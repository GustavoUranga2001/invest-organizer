#define _POSIX_C_SOURCE 200809L
#include "budget.h"
#include "monthly.h"
#include "safeio.h"
#include "storage.h"
#include "lock.h"
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static int count_problems = 0, count_restored = 0, count_checked = 0;

static int check_target(const char *title, const char *path,
                        SafeValidator validate, void *context, int recover) {
    char bak[PATH_MAX], error[256];
    struct stat st, bst;
    int primary, backup, primary_errno, backup_errno;
    if (snprintf(bak, sizeof(bak), "%s.bak", path) >= (int)sizeof(bak)) {
        fprintf(stderr, "%s: caminho longo demais\n", title);
        ++count_problems;return -1;
    }
    primary = lstat(path, &st) == 0; primary_errno = errno;
    backup = lstat(bak, &bst) == 0; backup_errno = errno;
    if ((!primary && primary_errno != ENOENT) || (!backup && backup_errno != ENOENT)) {
        fprintf(stderr, "%s: erro ao acessar arquivo principal\n", title);
        ++count_problems;return -1;
    }
    if (primary && (!S_ISREG(st.st_mode) || validate(path, context) != 0)) {
        printf("PROBLEMA: %s — arquivo principal inválido\n", title);
    } else if (primary) {
        printf("OK: %s\n", title);
        ++count_checked;return 0;
    } else if (!backup) {
        printf("—  %s: arquivo ainda não criado\n", title);
        return 0;
    } else {
        printf("AUSENTE: %s (existe backup)\n", title);
    }
    if (backup && S_ISREG(bst.st_mode) && validate(bak, context) == 0) {
        if (recover) {
            int rc = safeio_recover(path, validate, context, error, sizeof(error));
            if (rc == 1) {
                printf("RESTAURADO: %s (original inválido, se existia, guardado em .corrupt.*)\n", title);
                ++count_restored; ++count_checked;return 0;
            }
            fprintf(stderr, "FALHA: %s: %s\n", title, error);
        } else {
            printf("RECUPERÁVEL: %s — execute invest-doctor recover\n", title);
        }
    } else {
        fprintf(stderr, "SEM BACKUP VÁLIDO: %s; original preservado\n", title);
    }
    ++count_problems;
    return -1;
}

static int valid_month_filename(const char *name, int *year, int *month) {
    int y, m;
    if (strlen(name) != 11 ||
        name[4] != '-' || strcmp(name + 7, ".tsv") != 0) return 0;
    for (int i = 0; i < 4; ++i) if (name[i] < '0' || name[i] > '9') return 0;
    for (int i = 5; i < 7; ++i) if (name[i] < '0' || name[i] > '9') return 0;
    y = (name[0]-'0')*1000+(name[1]-'0')*100+(name[2]-'0')*10+name[3]-'0';
    m = (name[5]-'0')*10+name[6]-'0';
    if (y < 1900 || y > 2200 || m < 1 || m > 12) return 0;
    *year = y;*month = m;return 1;
}

int main(int argc, char **argv) {
    int recover;
    char scenarios[PATH_MAX], budget[PATH_MAX], directory[PATH_MAX];
    DIR *d;struct dirent *entry;
    if (argc != 2 || (strcmp(argv[1], "check") && strcmp(argv[1], "recover"))) {
        fprintf(stderr, "Uso: invest-doctor check | recover\n"
                        "  check: verifica arquivos sem modificá-los\n"
                        "  recover: restaura apenas arquivos inválidos com backups válidos\n");
        return 2;
    }
    recover = strcmp(argv[1], "recover") == 0;
    if (recover) {
        char err[180];
        if (invest_lock_acquire(err,sizeof(err))!=0) {
            fprintf(stderr,"invest-doctor: %s. Feche o invest antes de recuperar.\n",err);
            return 2;
        }
    }
    if (storage_path(scenarios, sizeof(scenarios)) != 0 ||
        budget_path(budget, sizeof(budget)) != 0) {
        fprintf(stderr, "HOME ou XDG_DATA_HOME inválido\n");return 2;
    }
    printf("Diagnóstico do invest (%s)\n", recover ? "recuperação explícita" : "somente leitura");
    check_target("Cenários", scenarios, storage_validate_file, NULL, recover);
    check_target("Orçamento legado", budget, budget_validate_file, NULL, recover);
    {
        char *slash;
        if (snprintf(directory, sizeof(directory), "%s", budget) >= (int)sizeof(directory)) return 2;
        slash = strrchr(directory, '/');
        if (!slash) return 2;
        strcpy(slash + 1, "meses");
    }
    d = opendir(directory);
    if (!d) {
        if (errno != ENOENT) {fprintf(stderr,"Não foi possível consultar meses: %s\n",directory);++count_problems;}
    } else {
        while ((entry = readdir(d))) {
            int year, month, date[2];char path[PATH_MAX], title[64];
            char name[32];size_t length = strlen(entry->d_name);
            /* Files ending in .bak should be included when their original is missing. */
            if (length > 4 && !strcmp(entry->d_name + length - 4, ".bak"))
                length -= 4;
            if (length >= sizeof(name)) continue;
            memcpy(name, entry->d_name, length);name[length] = '\0';
            if (!valid_month_filename(name, &year, &month)) continue;
            /* A month with both .tsv and .tsv.bak is checked just once. */
            if (entry->d_name[length] == '\0') {
                if (snprintf(path,sizeof(path),"%s/%s",directory,name)>=(int)sizeof(path)) continue;
            } else {
                struct stat st;
                if (snprintf(path,sizeof(path),"%s/%s",directory,name)>=(int)sizeof(path)) continue;
                if (lstat(path,&st)==0) continue;
            }
            snprintf(title,sizeof(title),"Mês %04d-%02d",year,month);
            date[0]=year;date[1]=month;
            check_target(title,path,monthly_validate_file,date,recover);
        }
        closedir(d);
    }
    printf("Conclusão: %d verificados, %d restaurados, %d problema(s) restantes.\n",
           count_checked,count_restored,count_problems);
    return count_problems ? 1 : 0;
}

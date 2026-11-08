#define _POSIX_C_SOURCE 200809L
#include "storage.h"
#include "utf8.h"
#include "safeio.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void fail(char *error, size_t size, const char *message)
{
    if (error && size) snprintf(error, size, "%s", message);
}

int storage_path(char *out, size_t size)
{
    const char *root = getenv("XDG_DATA_HOME");
    const char *home = getenv("HOME");
    int n;
    if (root && *root && root[0] == '/')
        n = snprintf(out, size, "%s/invest/cenarios.tsv", root);
    else if (home && *home)
        n = snprintf(out, size, "%s/.local/share/invest/cenarios.tsv", home);
    else return -1;
    return n >= 0 && (size_t)n < size ? 0 : -1;
}

static int create_parent(const char *path)
{
    char copy[PATH_MAX];
    size_t i, n = strlen(path);
    if (n >= sizeof(copy)) return -1;
    memcpy(copy, path, n + 1);
    for (i = 1; i < n; ++i) {
        if (copy[i] != '/') continue;
        copy[i] = 0;
        if (mkdir(copy, 0700) < 0 && errno != EEXIST) return -1;
        copy[i] = '/';
    }
    return 0;
}

static int parse_double(const char *s, double *out)
{
    char tail;
    return sscanf(s, "%lf%c", out, &tail) == 1 ? 0 : -1;
}

static int parse_int(const char *s, int *out)
{
    char tail;
    return sscanf(s, "%d%c", out, &tail) == 1 ? 0 : -1;
}

int storage_load_file(const char *path, ScenarioStore *store, char *error, size_t size)
{
    char line[8192];
    FILE *f;
    int version, selected;
    ScenarioStore result = {0};
    if (!store || !path) {
        fail(error, size, "Diretorio de dados indisponivel");
        return -1;
    }
    f = fopen(path, "r");
    if (!f && errno == ENOENT) {
        memset(store, 0, sizeof(*store));
        return 0;
    }
    if (!f) {
        fail(error, size, "Nao foi possivel abrir arquivo de cenarios");
        return -1;
    }
    if (!fgets(line, sizeof(line), f) ||
        sscanf(line, "INVEST\t%d\t%d", &version, &selected) != 2 ||
        (version != 1 && version != 2 && version != 3)) goto malformed;
    while (fgets(line, sizeof(line), f)) {
        char *fields[8], *cursor, *token;
        Scenario s = {0};
        int n = 0;
        if (!strchr(line, '\n') && !feof(f)) goto malformed;
        if (line[0] == '\n') continue;
        if (result.count >= INVEST_MAX_SCENARIOS) goto malformed;
        cursor = NULL;
        token = strtok_r(line, "\t\r\n", &cursor);
        while (token && n < 8) {
            fields[n++] = token;
            token = strtok_r(NULL, "\t\r\n", &cursor);
        }
        if (token || n != (version == 1 ? 5 : version == 2 ? 7 : 8) ||
            strlen(fields[0]) >= sizeof(s.name)) goto malformed;
        /* Repara em memoria nomes com bytes UTF-8 incompletos gravados por
         * versoes antigas. O arquivo original so muda apos salvar, e nesse
         * momento cenarios.tsv.bak preserva seus bytes originais. */
        utf8_clean(s.name, sizeof(s.name), fields[0]);
        if (parse_double(fields[1], &s.initial) < 0 ||
            parse_double(fields[2], &s.monthly) < 0 ||
            parse_double(fields[3], &s.rate_percent) < 0 ||
            parse_int(fields[4], &s.months) < 0) goto malformed;
        if (version == 2 &&
            (schedule_parse(fields[5], INVEST_RATES, s.months,
                            s.rates, &s.rate_count) < 0 ||
             schedule_parse(fields[6], INVEST_EXTRAS, s.months,
                            s.extras, &s.extra_count) < 0)) goto malformed;
        if (version == 3 &&
            (rules_parse(fields[5], INVEST_RATES, s.months,
                         s.rates, &s.rate_count) < 0 ||
             schedule_parse(fields[6], INVEST_EXTRAS, s.months,
                            s.extras, &s.extra_count) < 0 ||
             rules_parse(fields[7], INVEST_MONTHLY, s.months,
                         s.monthlies, &s.monthly_count) < 0)) goto malformed;
        if (!scenario_valid(&s)) goto malformed;
        result.items[result.count++] = s;
    }
    if (ferror(f)) goto malformed;
    fclose(f);
    result.selected = selected >= 0 && selected < result.count ? selected : 0;
    *store = result;
    return 0;
malformed:
    fclose(f);
    fail(error, size, "Arquivo de cenarios invalido: original preservado");
    return -1;
}

int storage_load(ScenarioStore *store, char *error, size_t size)
{
    char path[PATH_MAX];
    if (storage_path(path, sizeof(path)) != 0) {
        fail(error, size, "Diretorio de dados indisponivel");
        return -1;
    }
    return storage_load_file(path, store, error, size);
}

int storage_validate_file(const char *path, void *context)
{
    ScenarioStore *tmp = calloc(1, sizeof(*tmp));
    int rc;
    (void)context;
    if (!tmp) return -1;
    rc = storage_load_file(path, tmp, NULL, 0);
    free(tmp);
    return rc;
}

/* Na primeira migracao, preserva uma copia dos formatos INVEST 1 ou 2. */
static int backup_legacy(const char *path)
{
    char backup[PATH_MAX];
    FILE *in, *out;
    char header[64], buf[4096];
    size_t n;
    int rc = 0;
    in = fopen(path, "rb");
    if (!in) return errno == ENOENT ? 0 : -1;
    if (!fgets(header, sizeof(header), in)) {
        fclose(in);
        return -1;
    }
    /* Somente um original completo e válido pode virar backup legado. */
    if (storage_validate_file(path, NULL) != 0) {
        fclose(in);
        return -1;
    }
    {
        int version;
        if (sscanf(header, "INVEST\t%d", &version) != 1 || version == 3) {
            fclose(in);
            return 0;
        }
        if ((version != 1 && version != 2) ||
            snprintf(backup, sizeof(backup), "%s.v%d.bak", path, version) >=
                (int)sizeof(backup)) {
            fclose(in);
            return -1;
        }
    }
    if (access(backup, F_OK) == 0) {
        fclose(in);
        return 0;
    }
    rewind(in);
    {
        int backup_fd = open(backup, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
        if (backup_fd < 0) {
            fclose(in);
            return errno == EEXIST ? 0 : -1;
        }
        out = fdopen(backup_fd, "wb");
        if (!out) { close(backup_fd); unlink(backup); fclose(in); return -1; }
    }
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        if (fwrite(buf, 1, n, out) != n) { rc = -1; break; }
    if (ferror(in) || fflush(out) != 0 || fsync(fileno(out)) != 0) rc = -1;
    if (fclose(out) != 0) rc = -1;
    fclose(in);
    if (rc) unlink(backup);
    return rc;
}

int storage_save(const ScenarioStore *store, char *error, size_t size)
{
    char path[PATH_MAX], temp[PATH_MAX];
    FILE *f;
    int fd, i, n;
    if (!store || store->count < 1 || store->count > INVEST_MAX_SCENARIOS ||
        store->selected < 0 || store->selected >= store->count ||
        storage_path(path, sizeof(path)) < 0 || create_parent(path) < 0) {
        fail(error, size, "Diretorio ou cenarios invalidos");
        return -1;
    }
    for (i = 0; i < store->count; ++i)
        if (!scenario_valid(&store->items[i]) ||
            !utf8_valid(store->items[i].name)) {
            fail(error, size, "Cenario invalido; nada foi salvo");
            return -1;
        }
    if (backup_legacy(path) < 0) {
        fail(error, size, "Nao foi possivel criar backup da versao anterior");
        return -1;
    }
    n = snprintf(temp, sizeof(temp), "%s.tmp.XXXXXX", path);
    if (n < 0 || (size_t)n >= sizeof(temp)) {
        fail(error, size, "Caminho de dados muito longo");
        return -1;
    }
    fd = mkstemp(temp);
    if (fd < 0) {
        fail(error, size, "Nao foi possivel criar arquivo temporario");
        return -1;
    }
    f = fdopen(fd, "w");
    if (!f) {
        close(fd);
        unlink(temp);
        fail(error, size, "Erro de escrita");
        return -1;
    }
    if (fprintf(f, "INVEST\t3\t%d\n", store->selected) < 0) goto failed;
    for (i = 0; i < store->count; ++i) {
        const Scenario *s = &store->items[i];
        char rates[INVEST_MAX_EVENTS * 36 + 4];
        char extras[INVEST_MAX_EVENTS * 36 + 4];
        char monthly[INVEST_MAX_EVENTS * 48 + 4];
        if (rules_format(s->rates, s->rate_count, rates,
                         sizeof(rates)) < 0 ||
            schedule_format(s->extras, s->extra_count, extras,
                            sizeof(extras), 1) < 0 ||
            rules_format(s->monthlies, s->monthly_count, monthly,
                         sizeof(monthly)) < 0 ||
            fprintf(f, "%s\t%.17g\t%.17g\t%.17g\t%d\t%s\t%s\t%s\n",
                    s->name, s->initial, s->monthly, s->rate_percent,
                    s->months, rates, extras, monthly) < 0) goto failed;
    }
    if (fflush(f) != 0 || fsync(fd) != 0) goto failed;
    if (fclose(f) != 0) {
        unlink(temp);
        fail(error, size, "Falha ao fechar arquivo de dados");
        return -1;
    }
    if (safeio_commit(path, temp, storage_validate_file, NULL, error, size) != 0) {
        unlink(temp);
        return -1;
    }
    return 0;
failed:
    fclose(f);
    unlink(temp);
    fail(error, size, "Falha ao salvar cenarios");
    return -1;
}

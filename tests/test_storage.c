#define _POSIX_C_SOURCE 200809L
#include "storage.h"

#include <assert.h>
#include <stdio.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    char dir[] = "/tmp/invest-test-XXXXXX", path[4096], bak[4112];
    char error[200] = {0}, buf[8192];
    ScenarioStore store = {0}, restored = {0};
    FILE *f;
    assert(mkdtemp(dir));
    assert(setenv("XDG_DATA_HOME", dir, 1) == 0);
    assert(storage_path(path, sizeof(path)) == 0);
    /* Salva registro antigo no mesmo arquivo de dados. */
    assert(snprintf(bak, sizeof(bak), "%s.v1.bak", path) < (int)sizeof(bak));
    {
        char command[4096];
        snprintf(command, sizeof(command), "%s/invest", dir);
        assert(mkdir(command, 0700) == 0);
    }
    f = fopen(path, "w");
    assert(f);
    assert(fputs("INVEST\t1\t0\nCenario 1\t8440.66\t1000\t1.09\t24\n", f) >= 0);
    assert(fclose(f) == 0);
    assert(storage_load(&store, error, sizeof(error)) == 0);
    assert(store.count == 1 && store.items[0].rate_count == 0);
    assert(schedule_parse("6:1,4;12:0,5", INVEST_RATES, 24,
                          store.items[0].rates,
                          &store.items[0].rate_count) == 0);
    assert(schedule_parse("6:300", INVEST_EXTRAS, 24,
                          store.items[0].extras,
                          &store.items[0].extra_count) == 0);
    assert(storage_save(&store, error, sizeof(error)) == 0);
    f = fopen(bak, "r");
    assert(f);
    assert(fgets(buf, sizeof(buf), f));
    assert(strncmp(buf, "INVEST\t1\t", 9) == 0);
    fclose(f);
    f = fopen(path, "r");
    assert(f && fgets(buf, sizeof(buf), f));
    assert(strncmp(buf, "INVEST\t2\t", 9) == 0);
    fclose(f);
    assert(storage_load(&restored, error, sizeof(error)) == 0);
    assert(restored.items[0].rate_count == 2);
    assert(restored.items[0].extra_count == 1);
    assert(restored.items[0].rates[1].month == 12);
    assert(restored.items[0].extras[0].value == 300);
    /* Arquivo corrompido deve falhar em vez de descartar linhas. */
    f = fopen(path, "w");
    assert(f);
    assert(fputs("INVEST\t2\t0\nCenario invalido\n", f) >= 0);
    fclose(f);
    assert(storage_load(&restored, error, sizeof(error)) == -1);
    assert(strcmp(error, "Arquivo de cenarios invalido: original preservado") == 0);
    unlink(path);
    unlink(bak);
    { char sub[4096]; snprintf(sub, sizeof(sub), "%s/invest", dir); rmdir(sub); }
    rmdir(dir);
    puts("OK: migracao v1 -> v2, backup e recusa de arquivo corrompido");
    return 0;
}

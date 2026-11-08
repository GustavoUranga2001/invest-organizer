#define _POSIX_C_SOURCE 200809L
#include "safeio.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

static void fail(char *err, size_t n, const char *message) {
    if (err && n) snprintf(err, n, "%s", message);
}

/* Durable rename requires syncing the containing directory as well. */
static int sync_parent(const char *path) {
    char dir[PATH_MAX];
    const char *slash = strrchr(path, '/');
    int fd, rc;
    size_t len;
    if (!slash) return -1;
    len = (size_t)(slash - path);
    if (!len || len >= sizeof(dir)) return -1;
    memcpy(dir, path, len);
    dir[len] = '\0';
    fd = open(dir, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return -1;
    rc = fsync(fd);
    if (close(fd) != 0) rc = -1;
    return rc;
}

static int regular_file(const char *path, int *exists) {
    struct stat st;
    if (lstat(path, &st) == 0) {
        if (!S_ISREG(st.st_mode)) return -1; /* no symlinks or directories */
        *exists = 1;
        return 0;
    }
    if (errno == ENOENT) { *exists = 0; return 0; }
    return -1;
}

/* Do not alter destination until a complete, durable copy is ready. */
static int copy_atomic(const char *src, const char *dst) {
    char tmp[PATH_MAX];
    int source = -1, target = -1, rc = -1, exists;
    ssize_t n;
    char buf[8192];
    if (regular_file(src, &exists) < 0 || !exists ||
        snprintf(tmp, sizeof(tmp), "%s.tmp.XXXXXX", dst) >= (int)sizeof(tmp))
        return -1;
    source = open(src, O_RDONLY | O_CLOEXEC | O_NOFOLLOW);
    if (source < 0) return -1;
    target = mkstemp(tmp);
    if (target < 0) goto done;
    while ((n = read(source, buf, sizeof(buf))) > 0) {
        ssize_t written = 0;
        while (written < n) {
            ssize_t w = write(target, buf + written, (size_t)(n - written));
            if (w < 0 && errno == EINTR) continue;
            if (w <= 0) goto done;
            written += w;
        }
    }
    if (n < 0 || fsync(target) != 0) goto done;
    if (close(target) != 0) { target = -1; goto done; }
    target = -1;
    if (rename(tmp, dst) != 0) goto done;
    /* The rename succeeded; a failure to fsync is reported explicitly. */
    if (sync_parent(dst) != 0) goto done;
    rc = 0;
done:
    if (source >= 0) close(source);
    if (target >= 0) close(target);
    if (rc != 0) unlink(tmp);
    return rc;
}

int safeio_commit(const char *path, const char *temp,
                  SafeValidator validator, void *context,
                  char *error, size_t error_size) {
    char backup[PATH_MAX];
    int exists;
    if (!path || !temp || !validator || regular_file(path, &exists) < 0) {
        fail(error, error_size, "Arquivo de destino inseguro ou inacessível");
        return -1;
    }
    if (exists) {
        if (validator(path, context) != 0) {
            fail(error, error_size, "Arquivo original inválido; não foi sobrescrito. Use invest-doctor.");
            return -1;
        }
        if (snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup) ||
            copy_atomic(path, backup) != 0) {
            fail(error, error_size, "Não foi possível preservar backup; arquivo original mantido");
            return -1;
        }
    }
    if (rename(temp, path) != 0) {
        fail(error, error_size, "Falha ao substituir arquivo; original preservado");
        return -1;
    }
    if (sync_parent(path) != 0) {
        fail(error, error_size, "Gravação efetuada, mas sincronização do diretório falhou");
        return -1;
    }
    return 0;
}

int safeio_recover(const char *path, SafeValidator validator, void *context,
                   char *error, size_t error_size) {
    char backup[PATH_MAX], damaged[PATH_MAX];
    int exists, bak_exists;
    time_t now = time(NULL);
    if (!path || !validator || regular_file(path, &exists) < 0 ||
        snprintf(backup, sizeof(backup), "%s.bak", path) >= (int)sizeof(backup) ||
        regular_file(backup, &bak_exists) < 0) {
        fail(error, error_size, "Arquivo ou backup inseguro");
        return -1;
    }
    if (exists && validator(path, context) == 0) return 0;
    if (!bak_exists || validator(backup, context) != 0) {
        fail(error, error_size, "Nenhum backup válido; arquivo original não foi alterado");
        return -1;
    }
    if (exists) {
        /* Preserve the damaged bytes separately; no deletion/replacement. */
        int attempt, found = 0;
        for (attempt = 0; attempt < 100; ++attempt) {
            int clash;
            if (snprintf(damaged, sizeof(damaged), "%s.corrupt.%lld.%ld.%d",
                         path, (long long)now, (long)getpid(), attempt) >= (int)sizeof(damaged))
                break;
            if (regular_file(damaged, &clash) != 0) break;
            if (!clash) { found = 1; break; }
        }
        if (!found || copy_atomic(path, damaged) != 0) {
            fail(error, error_size, "Não foi possível preservar o arquivo danificado");
            return -1;
        }
    }
    if (copy_atomic(backup, path) != 0) {
        fail(error, error_size, "Falha ao restaurar backup; dados danificados preservados");
        return -1;
    }
    return 1;
}

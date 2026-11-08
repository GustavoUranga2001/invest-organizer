#ifndef INVEST_SAFEIO_H
#define INVEST_SAFEIO_H

#include <stddef.h>

/* Validator returns 0 only for a complete, valid file. */
typedef int (*SafeValidator)(const char *path, void *context);

/* Replace a complete, fsynced temporary file; first retain a verified
 * last-known-good .bak. Reject an unreadable or damaged existing file. */
int safeio_commit(const char *path, const char *temp,
                  SafeValidator validator, void *context,
                  char *error, size_t error_size);

/* Explicit recovery only. Returns 1 recovered, 0 already healthy/absent,
 * -1 error. Before recovery preserves damaged original as .corrupt.*. */
int safeio_recover(const char *path, SafeValidator validator, void *context,
                   char *error, size_t error_size);

#endif

#ifndef INVEST_STORAGE_H
#define INVEST_STORAGE_H

#include "finance.h"
#include <stddef.h>

/* Returns path of data file in the user's XDG data directory. */
int storage_path(char *out, size_t size);
/* Returns 0 if loaded (or if no file exists), -1 on failure.
 * If no file exists, 'store' is left with count == 0. */
int storage_load(ScenarioStore *store, char *error, size_t size);
/* Saves atomically by writing a new file and renaming it. */
int storage_save(const ScenarioStore *store, char *error, size_t size);

#endif

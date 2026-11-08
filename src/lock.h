#ifndef INVEST_LOCK_H
#define INVEST_LOCK_H
#include <stddef.h>
/* Holds a POSIX advisory lock in the user's private data directory.
 * Prevents two invest instances from silently overwriting each other. */
int invest_lock_acquire(char *error, size_t error_size);
void invest_lock_release(void);
#endif

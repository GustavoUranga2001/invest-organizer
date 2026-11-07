#ifndef INVEST_COMPOSE_H
#define INVEST_COMPOSE_H

#include <stddef.h>

/* Unicode composition fallback for X11 sessions without a working XIM.
 * mark is an ASCII character: ~ ' ` ^ " , or o (ring above).
 * Compose returns 1 on success, 0 if no mapping exists.
 */
int compose_letter(char mark, char base, char *out, size_t outcap);
/* Converts common decomposed Unicode forms to their precomposed UTF-8 form.
 * Input must be valid UTF-8; returns 0 on success, -1 on invalid/overflow.
 */
int compose_normalize(char *out, size_t outcap, const char *input);

/* Stateful keyboard-composition fallback for editable names. */
int compose_feed(char *pending, const char *input, char *out, size_t outcap);

#endif

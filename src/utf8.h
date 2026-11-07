#ifndef INVEST_UTF8_H
#define INVEST_UTF8_H

#include <stddef.h>

/* Helpers sem dependencias de interface: UTF-8 estrito e edicao por caractere.
 * Os deslocamentos de cursor sao offsets em bytes, SEMPRE em um limite UTF-8. */
int utf8_valid(const char *s);
size_t utf8_clean(char *dst, size_t capacity, const char *src);
size_t utf8_prev(const char *s, size_t cursor);
size_t utf8_next(const char *s, size_t cursor);
size_t utf8_prev_grapheme(const char *s, size_t cursor);
size_t utf8_next_grapheme(const char *s, size_t cursor);
int utf8_insert(char *s, size_t cap, size_t *cursor, int *select_all, const char *input);
int utf8_erase(char *s, size_t *cursor, int *select_all, int backwards);

#endif

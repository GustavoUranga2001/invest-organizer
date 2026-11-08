#ifndef INVEST_MARKET_H
#define INVEST_MARKET_H
#include <stddef.h>
#include <time.h>

enum MarketKind { MARKET_SELIC, MARKET_USD, MARKET_EUR, MARKET_BTC, MARKET_XMR, MARKET_COUNT };
typedef struct {
    double value;
    time_t fetched_at;
    char source_date[32];
    int valid;
} MarketQuote;
typedef struct { MarketQuote items[MARKET_COUNT]; } MarketData;

/* Local XDG cache. A missing cache is normal (all indicators unavailable). */
int market_load(MarketData *out);
/* Network access is performed only when called; run this in a child process. */
int market_refresh(void);
/* Last fetch timestamp among saved indicators. */
time_t market_latest(const MarketData *data);
/* Exposed for deterministic tests with recorded JSON fixtures. */
int market_parse_selic(const char *json, double *value, char *date, size_t cap);
int market_parse_ptax(const char *json, double *value, char *date, size_t cap);
int market_parse_crypto(const char *json, double *value, char *date, size_t cap);
#endif

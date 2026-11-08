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

typedef enum {
    MARKET_FETCH_OK=0, MARKET_FETCH_NO_CURL, MARKET_FETCH_DNS, MARKET_FETCH_TLS,
    MARKET_FETCH_HTTP, MARKET_FETCH_TIMEOUT, MARKET_FETCH_CONNECTION,
    MARKET_FETCH_PARSE, MARKET_FETCH_OTHER, MARKET_FETCH_SAVE, MARKET_FETCH_CONVERSION
} MarketFetchCode;
typedef struct {
    MarketFetchCode errors[MARKET_COUNT];
    int updated;
} MarketDiagnostics;
const char *market_fetch_error(MarketFetchCode code);
int market_refresh_detailed(MarketDiagnostics *diagnostics);
int market_diagnostics_load(MarketDiagnostics *diagnostics);
void market_diagnostics_summary(const MarketDiagnostics *diagnostics,char *output,size_t cap);
/* Local XDG cache. A missing cache is normal (all indicators unavailable). */
int market_load(MarketData *out);
/* Network access is performed only when called; run this in a child process. */
int market_refresh(void);
/* Last fetch timestamp among saved indicators. */
time_t market_latest(const MarketData *data);
/* Exposed for deterministic tests with recorded JSON fixtures. */
int market_parse_selic(const char *json, double *value, char *date, size_t cap);
int market_parse_ptax(const char *json, double *value, char *date, size_t cap);
/* Parse the ASCII/UTF-8 table from rate.sx (?qT&n=80); returns USD. */
int market_parse_rate_sx(const char *body,const char *symbol,double *value);
int market_parse_rate_sx_brl(const char *body,const char *symbol,double *value);
#endif

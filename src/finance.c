#include "finance.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

double annual_equivalent_rate(double monthly_percent)
{
    return 100.0 * (pow(1.0 + monthly_percent / 100.0, 12.0) - 1.0);
}

void scenario_default(Scenario *s)
{
    memset(s, 0, sizeof(*s));
    snprintf(s->name, sizeof(s->name), "Cenario 1");
    s->initial = 8440.66;
    s->monthly = 1000.0;
    s->rate_percent = 1.09;
    s->months = 24;
}

static int valid_event(const InvestEvent *e, int previous, int months,
                       enum InvestScheduleType type)
{
    if (e->month <= previous || e->month < 1 || e->month > months ||
        !isfinite(e->value)) return 0;
    if (type == INVEST_EXTRAS)
        return e->end_month == 0 && e->value >= 0.0 && e->value <= 1e12;
    if (e->end_month != 0 &&
        (e->end_month < e->month || e->end_month > months)) return 0;
    if (type == INVEST_RATES)
        return e->value > -100.0 && e->value <= 100.0;
    return e->value >= 0.0 && e->value <= 1e12;
}

int scenario_valid(const Scenario *s)
{
    int i, previous;
    if (!s || !s->name[0] || strchr(s->name, '\t') ||
        strchr(s->name, '\n') || strchr(s->name, '\r') ||
        !isfinite(s->initial) || !isfinite(s->monthly) ||
        !isfinite(s->rate_percent) || s->initial < 0.0 ||
        s->monthly < 0.0 || s->initial > 1e12 || s->monthly > 1e12 ||
        s->rate_percent <= -100.0 || s->rate_percent > 100.0 ||
        s->months < 1 || s->months > INVEST_MAX_MONTHS ||
        s->rate_count < 0 || s->rate_count > INVEST_MAX_EVENTS ||
        s->extra_count < 0 || s->extra_count > INVEST_MAX_EVENTS ||
        s->monthly_count < 0 || s->monthly_count > INVEST_MAX_EVENTS)
        return 0;
    previous = 0;
    for (i = 0; i < s->rate_count; ++i) {
        if (!valid_event(&s->rates[i], previous, s->months, INVEST_RATES))
            return 0;
        previous = s->rates[i].month;
    }
    previous = 0;
    for (i = 0; i < s->extra_count; ++i) {
        if (!valid_event(&s->extras[i], previous, s->months, INVEST_EXTRAS))
            return 0;
        previous = s->extras[i].month;
    }
    previous = 0;
    for (i = 0; i < s->monthly_count; ++i) {
        if (!valid_event(&s->monthlies[i], previous, s->months, INVEST_MONTHLY))
            return 0;
        previous = s->monthlies[i].month;
    }
    return 1;
}

/* As regras estao ordenadas pelo mes inicial: percorremos da mais recente
 * para a mais antiga e encontramos a primeira que ainda esta ativa.
 * Assim, uma regra temporaria nao apaga a regra que estava por baixo. */
static double effective_value(const InvestEvent *rules, int count,
                              int month, double base)
{
    int i;
    for (i = count - 1; i >= 0; --i)
        if (rules[i].month <= month &&
            (rules[i].end_month == 0 || month <= rules[i].end_month))
            return rules[i].value;
    return base;
}

int project_scenario(const Scenario *s, Projection *out,
                     double *balances, double *principal)
{
    double balance, capital, extra_sum = 0.0;
    int month, ie = 0;
    if (!scenario_valid(s)) return -1;
    balance = capital = s->initial;
    if (balances) balances[0] = balance;
    if (principal) principal[0] = capital;
    for (month = 1; month <= s->months; ++month) {
        double rate = effective_value(s->rates, s->rate_count,
                                      month, s->rate_percent);
        double monthly = effective_value(s->monthlies, s->monthly_count,
                                         month, s->monthly);
        double extra = 0.0;
        if (ie < s->extra_count && s->extras[ie].month == month)
            extra = s->extras[ie++].value;
        balance = balance * (1.0 + rate / 100.0) + monthly + extra;
        capital += monthly + extra;
        extra_sum += extra;
        if (!isfinite(balance) || !isfinite(capital) ||
            balance > 1e17 || capital > 1e17) return -1;
        if (balances) balances[month] = balance;
        if (principal) principal[month] = capital;
    }
    if (out) {
        out->final_balance = balance;
        out->contributions = capital;
        out->earnings = balance - capital;
        out->extra_contributions = extra_sum;
    }
    return 0;
}

/* Uma unica virgula decimal e aceita. Se ha virgula, os pontos representam
 * separadores de milhares. Para o formato em disco somente ponto decimal. */
static int parse_number(const char *s, size_t n, double *value)
{
    char buf[128], *end;
    size_t i, j = 0;
    int comma = 0;
    double v;
    if (!n || n >= sizeof(buf)) return -1;
    for (i = 0; i < n; ++i)
        if (s[i] == ',') comma = 1;
    for (i = 0; i < n; ++i) {
        unsigned char ch = (unsigned char)s[i];
        if (isspace(ch) || (comma && ch == '.')) continue;
        if (ch == ',') ch = '.';
        if (j >= sizeof(buf) - 1) return -1;
        buf[j++] = (char)ch;
    }
    buf[j] = '\0';
    if (!j) return -1;
    errno = 0;
    v = strtod(buf, &end);
    if (errno || *end || end == buf || !isfinite(v)) return -1;
    *value = v;
    return 0;
}

int schedule_parse(const char *input, enum InvestScheduleType kind,
                   int months, InvestEvent *out, int *count)
{
    const char *at;
    int n = 0, i;
    if (!input || !out || !count || months < 1 || months > INVEST_MAX_MONTHS)
        return -1;
    at = input;
    while (isspace((unsigned char)*at)) at++;
    if (!*at || (*at == '-' && !at[1])) {
        *count = 0;
        return 0;
    }
    while (*at) {
        char *end;
        long month;
        double val;
        const char *start;
        while (isspace((unsigned char)*at)) at++;
        errno = 0;
        month = strtol(at, &end, 10);
        if (errno || end == at || month < 1 || month > months) return -1;
        at = end;
        while (isspace((unsigned char)*at)) at++;
        if (*at != ':' && *at != '=') return -1;
        at++;
        start = at;
        while (*at && *at != ';') at++;
        if (parse_number(start, (size_t)(at - start), &val) < 0 ||
            n >= INVEST_MAX_EVENTS) return -1;
        out[n].month = (int)month;
        out[n].end_month = 0;
        out[n].value = val;
        ++n;
        if (!*at) break;
        at++;
        while (isspace((unsigned char)*at)) at++;
        if (!*at) return -1;
    }
    /* Ordenar por mes sem exigir ordem do usuario. */
    for (i = 1; i < n; ++i) {
        InvestEvent e = out[i];
        int j = i;
        while (j > 0 && out[j - 1].month > e.month) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = e;
    }
    for (i = 0; i < n; ++i)
        if (!valid_event(&out[i], i ? out[i - 1].month : 0,
                         months, kind)) return -1;
    *count = n;
    return 0;
}

int schedule_format(const InvestEvent *events, int count, char *out,
                    size_t size, int disk_format)
{
    int i;
    size_t used = 0;
    if (!out || size == 0 || count < 0 || count > INVEST_MAX_EVENTS)
        return -1;
    if (!count) {
        snprintf(out, size, "%s", disk_format ? "-" : "");
        return 0;
    }
    for (i = 0; i < count; ++i) {
        char tmp[90];
        int written;
        size_t j;
        /* %.17g para armazenamento sem perda na conversao double -> texto. */
        snprintf(tmp, sizeof(tmp), disk_format ? "%d:%.17g" : "%d:%.2f",
                 events[i].month, events[i].value);
        if (!disk_format)
            for (j = 0; tmp[j]; ++j) if (tmp[j] == '.') tmp[j] = ',';
        written = snprintf(out + used, size - used, "%s%s", i ? ";" : "", tmp);
        if (written < 0 || (size_t)written >= size - used) return -1;
        used += (size_t)written;
    }
    return 0;
}

/* INVEST 3: inicio:fim:valor, separados por ponto e virgula.
 * Nao e uma interface de usuario, apenas serializacao sem perda. */
int rules_parse(const char *input, enum InvestScheduleType kind,
                int months, InvestEvent *out, int *count)
{
    const char *at;
    int n = 0, i;
    if (!input || !out || !count || kind == INVEST_EXTRAS ||
        months < 1 || months > INVEST_MAX_MONTHS) return -1;
    at = input;
    while (isspace((unsigned char)*at)) ++at;
    if (!*at || (*at == '-' && !at[1])) { *count = 0; return 0; }
    while (*at) {
        char *end;
        long first, last;
        double value;
        const char *start;
        while (isspace((unsigned char)*at)) ++at;
        errno = 0;
        first = strtol(at, &end, 10);
        if (errno || end == at || *end != ':' || first < 1 || first > months)
            return -1;
        at = end + 1;
        errno = 0;
        last = strtol(at, &end, 10);
        if (errno || end == at || *end != ':' || last < 0 || last > months)
            return -1;
        at = end + 1;
        start = at;
        while (*at && *at != ';') ++at;
        if (parse_number(start, (size_t)(at - start), &value) < 0 ||
            n >= INVEST_MAX_EVENTS) return -1;
        out[n++] = (InvestEvent){(int)first, (int)last, value};
        if (!*at) break;
        ++at;
        while (isspace((unsigned char)*at)) ++at;
        if (!*at) return -1;
    }
    for (i = 1; i < n; ++i) {
        InvestEvent item = out[i];
        int j = i;
        while (j > 0 && out[j - 1].month > item.month) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = item;
    }
    for (i = 0; i < n; ++i)
        if (!valid_event(&out[i], i ? out[i-1].month : 0, months, kind))
            return -1;
    *count = n;
    return 0;
}

int rules_format(const InvestEvent *events, int count, char *out, size_t size)
{
    int i;
    size_t used = 0;
    if (!out || !size || count < 0 || count > INVEST_MAX_EVENTS) return -1;
    if (!count) return snprintf(out, size, "-") < (int)size ? 0 : -1;
    for (i = 0; i < count; ++i) {
        int n = snprintf(out + used, size - used, "%s%d:%d:%.17g",
                         i ? ";" : "", events[i].month,
                         events[i].end_month, events[i].value);
        if (n < 0 || (size_t)n >= size - used) return -1;
        used += (size_t)n;
    }
    return 0;
}

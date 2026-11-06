#ifndef INVEST_FINANCE_H
#define INVEST_FINANCE_H

#include <stddef.h>

#define INVEST_MAX_SCENARIOS 32
#define INVEST_MAX_MONTHS 600
#define INVEST_MAX_EVENTS 32
#define INVEST_NAME_SIZE 64

/* Uma taxa em determinado mes substitui a taxa mensal base daquele mes em
 * diante. Um aporte extra ocorre apenas no mes indicado, ao fim do mes. */
typedef struct {
    int month;
    double value;
} InvestEvent;

typedef struct {
    char name[INVEST_NAME_SIZE];
    double initial;
    double monthly;
    double rate_percent;
    int months;
    int rate_count;
    InvestEvent rates[INVEST_MAX_EVENTS];
    int extra_count;
    InvestEvent extras[INVEST_MAX_EVENTS];
} Scenario;

typedef struct {
    Scenario items[INVEST_MAX_SCENARIOS];
    int count;
    int selected;
} ScenarioStore;

typedef struct {
    double final_balance;
    double contributions;
    double earnings;
    double extra_contributions;
} Projection;

enum InvestScheduleType { INVEST_RATES, INVEST_EXTRAS };

void scenario_default(Scenario *s);
int scenario_valid(const Scenario *s);
/* Nao modifica o cenario. Cada vetor opcional tem s->months+1 elementos.
 * Ao fim de cada mes: saldo = saldo * (1+taxa/100) + mensal + extra. */
int project_scenario(const Scenario *s, Projection *out,
                     double *balances, double *principal);
/* Parseia '6:1,25; 12:0,85' ou '6=1.25; 12=0.85'.
 * Meses fora de ordem sao ordenados; meses duplicados sao rejeitados.
 * Cadeia vazia ou '-' significa sem eventos. */
int schedule_parse(const char *input, enum InvestScheduleType kind,
                   int months, InvestEvent *out, int *count);
/* Usa ponto decimal ao armazenar, virgula ao exibir. */
int schedule_format(const InvestEvent *events, int count, char *out,
                    size_t size, int disk_format);

#endif

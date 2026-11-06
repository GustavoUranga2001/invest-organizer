#ifndef INVEST_FINANCE_H
#define INVEST_FINANCE_H

#include <stddef.h>

#define INVEST_MAX_SCENARIOS 32
#define INVEST_MAX_MONTHS 600
#define INVEST_MAX_EVENTS 32
#define INVEST_NAME_SIZE 64

/* Regras (juros / aportes mensais): end_month == 0 indica que a regra
 * continua ate o fim, inclusive depois de sobreposicoes temporarias.
 * A regra ativa mais recente prevalece no mes considerado. Aportes extras
 * sao pontuais e ignoram end_month (sempre zero). */
typedef struct {
    int month;
    int end_month;
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
    int monthly_count;
    InvestEvent monthlies[INVEST_MAX_EVENTS];
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

enum InvestScheduleType { INVEST_RATES, INVEST_EXTRAS, INVEST_MONTHLY };

double annual_equivalent_rate(double monthly_percent);
void scenario_default(Scenario *s);
int scenario_valid(const Scenario *s);
/* Mes M: saldo = saldo*(1+taxa_efetiva(M)/100)+aporte_efetivo(M)+extra(M).
 * Um intervalo [inicio,fim] e inclusivo. Fim 0 = permanente.
 * Terminando um intervalo, a regra anterior ainda vigente e retomada. */
int project_scenario(const Scenario *s, Projection *out,
                     double *balances, double *principal);
/* Para arquivos historicos INVEST 2 (formato mes:valor) e extras pontuais. */
int schedule_parse(const char *input, enum InvestScheduleType kind,
                   int months, InvestEvent *out, int *count);
int schedule_format(const InvestEvent *events, int count, char *out,
                    size_t size, int disk_format);
/* Para INVEST 3: inicio:fim:valor; fim=0 quer dizer ate o fim.
 * Esses detalhes nao precisam ser digitados pelo usuario (somente disco). */
int rules_parse(const char *input, enum InvestScheduleType kind,
                int months, InvestEvent *out, int *count);
int rules_format(const InvestEvent *events, int count, char *out, size_t size);

#endif

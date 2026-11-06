#include "finance.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static int close_to(double a, double b)
{
    return fabs(a - b) < 0.005;
}

int main(void)
{
    Scenario s;
    Projection p;
    double balances[INVEST_MAX_MONTHS + 1];
    double principal[INVEST_MAX_MONTHS + 1];
    InvestEvent events[INVEST_MAX_EVENTS];
    char format[2048];
    int count;
    /* Equivalencia anual de juros compostos (nao 12 x taxa mensal). */
    assert(close_to(annual_equivalent_rate(1.09), 13.893347713));
    assert(close_to(annual_equivalent_rate(0.0), 0.0));
    assert(close_to(annual_equivalent_rate(2.0), 26.824179));
    scenario_default(&s);
    assert(scenario_valid(&s));
    assert(project_scenario(&s, &p, balances, principal) == 0);
    assert(close_to(p.final_balance, 38212.22));
    assert(close_to(p.contributions, 32440.66));
    assert(close_to(p.earnings, 5771.56));
    assert(close_to(p.extra_contributions, 0));
    assert(close_to(balances[1], 9532.66));
    assert(close_to(principal[24], 32440.66));

    /* Taxa 10% nos meses 1 e 2, 0% a partir do mes 3. */
    s.initial = 100;
    s.monthly = 10;
    s.rate_percent = 10;
    s.months = 4;
    assert(schedule_parse("3:0", INVEST_RATES, s.months,
                          s.rates, &s.rate_count) == 0);
    assert(schedule_parse("2:50;4:20", INVEST_EXTRAS, s.months,
                          s.extras, &s.extra_count) == 0);
    assert(project_scenario(&s, &p, balances, principal) == 0);
    assert(close_to(balances[1], 120));
    assert(close_to(balances[2], 192));  /* (120*1.1 + 10 + 50) */
    assert(close_to(balances[3], 202));
    assert(close_to(balances[4], 232));
    assert(close_to(p.contributions, 210));
    assert(close_to(p.extra_contributions, 70));
    assert(close_to(p.earnings, 22));
    assert(scenario_valid(&s));

    /* Eventos fora de ordem sao ordenados e formatados para persistencia. */
    assert(schedule_parse("4:2,50; 1:1,25; 3:0,5", INVEST_RATES, 12,
                          events, &count) == 0);
    assert(count == 3 && events[0].month == 1 && events[1].month == 3);
    assert(close_to(events[2].value, 2.5));
    assert(schedule_format(events, count, format, sizeof(format), 1) == 0);
    assert(strstr(format, "1:1.25;3:0.5;4:2.5") != NULL);
    assert(schedule_format(events, count, format, sizeof(format), 0) == 0);
    assert(strstr(format, "1:1,25") != NULL);

    assert(schedule_parse("", INVEST_RATES, 10, events, &count) == 0 && count == 0);
    assert(schedule_parse("-", INVEST_EXTRAS, 10, events, &count) == 0 && count == 0);
    assert(schedule_parse("2:1;2:2", INVEST_RATES, 10, events, &count) < 0);
    assert(schedule_parse("11:2", INVEST_RATES, 10, events, &count) < 0);
    assert(schedule_parse("1:-100", INVEST_RATES, 10, events, &count) < 0);
    assert(schedule_parse("1:-50", INVEST_EXTRAS, 10, events, &count) < 0);
    assert(schedule_parse("2:10;", INVEST_EXTRAS, 10, events, &count) < 0);
    assert(schedule_parse("2:NaN", INVEST_RATES, 10, events, &count) < 0);
    assert(schedule_parse("2:10%", INVEST_RATES, 10, events, &count) < 0);
    assert(schedule_parse("1:1000000,25", INVEST_EXTRAS, 10,
                          events, &count) == 0);
    assert(close_to(events[0].value, 1000000.25));
    assert(schedule_parse("1:1.000,25", INVEST_EXTRAS, 10,
                          events, &count) == 0);
    assert(close_to(events[0].value, 1000.25));

    scenario_default(&s);
    s.rate_percent = 0;
    assert(project_scenario(&s, &p, NULL, NULL) == 0);
    assert(close_to(p.final_balance, 32440.66));
    s.months = 1;
    s.initial = 100;
    s.monthly = 10;
    s.rate_percent = -50;
    assert(project_scenario(&s, &p, NULL, NULL) == 0);
    assert(close_to(p.final_balance, 60));
    assert(close_to(p.earnings, -50));
    s.rate_percent = -100;
    assert(project_scenario(&s, &p, NULL, NULL) < 0);
    s.rate_percent = 1;
    s.months = INVEST_MAX_MONTHS + 1;
    assert(project_scenario(&s, &p, NULL, NULL) < 0);

    /* Evento fora do novo prazo invalida o cenario, sem remover informacao. */
    scenario_default(&s);
    assert(schedule_parse("24:200", INVEST_EXTRAS, 24,
                          s.extras, &s.extra_count) == 0);
    assert(scenario_valid(&s));
    s.months = 12;
    assert(!scenario_valid(&s));

    puts("OK: formulas, taxas variaveis, aportes extras e validacao");
    return 0;
}

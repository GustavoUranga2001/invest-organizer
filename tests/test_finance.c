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

    /* Exemplo: aporte base R$100; R$50 durante os meses 3 e 4;
     * R$500 a partir do mes 5. Sem juros, total = 200+100+1500. */
    scenario_default(&s);
    s.initial = 0;
    s.rate_percent = 0;
    s.monthly = 100;
    s.months = 7;
    assert(rules_parse("3:4:50;5:0:500", INVEST_MONTHLY, 7,
                       s.monthlies, &s.monthly_count) == 0);
    assert(s.monthly_count == 2);
    assert(project_scenario(&s, &p, balances, principal) == 0);
    assert(close_to(balances[1], 100));
    assert(close_to(balances[2], 200));
    assert(close_to(balances[3], 250));
    assert(close_to(balances[4], 300));
    assert(close_to(balances[5], 800));
    assert(close_to(balances[6], 1300));
    assert(close_to(balances[7], 1800));
    assert(close_to(p.contributions, 1800));
    assert(close_to(p.earnings, 0));

    /* Um intervalo temporario volta ao valor base se nao houver outra regra. */
    s.monthly_count=1;
    assert(project_scenario(&s, &p, balances, NULL) == 0);
    assert(close_to(balances[5], 400));
    assert(close_to(balances[7], 600));

    /* Sobreposicao: regra permanente ativa no mes 2, substituida
     * temporariamente nos meses 4-5; no mes 6 retoma a permanente. */
    assert(rules_parse("2:0:300;4:5:50", INVEST_MONTHLY, 7,
                       s.monthlies, &s.monthly_count) == 0);
    assert(project_scenario(&s, &p, balances, NULL) == 0);
    assert(close_to(balances[1], 100));
    assert(close_to(balances[2], 400));
    assert(close_to(balances[3], 700));
    assert(close_to(balances[4], 750));
    assert(close_to(balances[5], 800));
    assert(close_to(balances[6], 1100));
    assert(close_to(balances[7], 1400));

    /* Taxas de juros variaveis e temporarias: 10%, 0%, 0%, 10%. */
    scenario_default(&s);
    s.initial=100;
    s.monthly=0;
    s.rate_percent=10;
    s.months=4;
    assert(rules_parse("2:3:0", INVEST_RATES, 4,
                       s.rates, &s.rate_count) == 0);
    assert(project_scenario(&s, &p, balances, NULL) == 0);
    assert(close_to(balances[1], 110));
    assert(close_to(balances[2], 110));
    assert(close_to(balances[3], 110));
    assert(close_to(balances[4], 121));
    assert(rules_parse("2:0:5;3:3:0", INVEST_RATES, 4,
                       s.rates, &s.rate_count) == 0);
    assert(project_scenario(&s, &p, balances, NULL) == 0);
    assert(close_to(balances[2], 115.5));
    assert(close_to(balances[3], 115.5));
    assert(close_to(balances[4], 121.275));

    /* Formato novo, eventos ordenados, validacao de periodos e duplicatas. */
    assert(rules_parse("5:0:500;3:4:50", INVEST_MONTHLY, 7,
                       events, &count) == 0);
    assert(events[0].month == 3 && events[0].end_month == 4);
    assert(rules_format(events, count, format, sizeof(format)) == 0);
    assert(strstr(format, "3:4:50;5:0:500") != NULL);
    assert(rules_parse("2:1:30", INVEST_MONTHLY, 7, events, &count) < 0);
    assert(rules_parse("2:8:30", INVEST_MONTHLY, 7, events, &count) < 0);
    assert(rules_parse("2:3:-1", INVEST_MONTHLY, 7, events, &count) < 0);
    assert(rules_parse("2:0:-100", INVEST_RATES, 7, events, &count) < 0);
    assert(rules_parse("2:0:10;2:0:20", INVEST_RATES, 7, events, &count) < 0);
    assert(rules_parse("1:0:NaN", INVEST_MONTHLY, 7, events, &count) < 0);
    assert(rules_parse("1:0:200;", INVEST_MONTHLY, 7, events, &count) < 0);
    assert(rules_parse("-", INVEST_MONTHLY, 7, events, &count) == 0 && count == 0);

    /* Nao descartar intervalos quando o prazo diminui. */
    scenario_default(&s);
    s.months=12;
    assert(rules_parse("8:12:100", INVEST_MONTHLY, 12,
                       s.monthlies, &s.monthly_count) == 0);
    assert(scenario_valid(&s));
    s.months=10;
    assert(!scenario_valid(&s));

    puts("OK: formulas, regras temporarias/permanentes, limites e validacao");
    return 0;
}

#define _POSIX_C_SOURCE 200809L
#include "budget.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void near(double a,double b){assert(fabs(a-b)<0.001);}
int main(void){
    char dir[]="/tmp/invest-budget-XXXXXX",path[4096],sub[4096],err[200]={0};
    BudgetStore data={0},copy={0};BudgetTotals totals;FILE*f;char line[256];
    assert(mkdtemp(dir));
    assert(setenv("XDG_DATA_HOME",dir,1)==0);
    assert(budget_load(&data,err,sizeof(err))==0 && data.count==0);
    data.items[0]=(BudgetItem){"Salario",4750.10,BUDGET_INCOME,CAT_OUTROS,1};
    data.items[1]=(BudgetItem){"Freelance",150.45,BUDGET_INCOME,CAT_OUTROS,1};
    data.items[2]=(BudgetItem){"Aluguel",1220.35,BUDGET_EXPENSE,CAT_MORADIA,1};
    data.items[3]=(BudgetItem){"Mercado",755.10,BUDGET_EXPENSE,CAT_ALIMENTACAO,0};
    data.items[4]=(BudgetItem){"Internet",115.00,BUDGET_EXPENSE,CAT_CONTAS,1};
    data.count=5;
    assert(budget_totals(&data,&totals)==0);
    near(totals.income,4900.55);near(totals.expenses,2090.45);
    near(totals.balance,2810.10);near(totals.balance_before_investment,2810.10);near(totals.monthly_investment,0);near(totals.fixed_expenses,1335.35);
    near(totals.variable_expenses,755.10);
    near(totals.by_category[CAT_MORADIA],1220.35);
    near(totals.by_category[CAT_ALIMENTACAO],755.10);
    /* Planejamento mensal nao e despesa nem aporte automatico. */
    data.monthly_investment=1000.70;
    assert(budget_totals(&data,&totals)==0);
    near(totals.monthly_investment,1000.70);
    near(totals.balance_before_investment,2810.10);
    near(totals.balance,1809.40);
    near(totals.expenses,2090.45);
    assert(budget_save(&data,err,sizeof(err))==0);
    assert(budget_load(&copy,err,sizeof(err))==0);
    assert(copy.count==5 && !strcmp(copy.items[2].name,"Aluguel"));
    near(copy.items[4].amount,115.00);
    near(copy.monthly_investment,1000.70);
    assert(budget_path(path,sizeof(path))==0);
    f=fopen(path,"r");assert(f && fgets(line,sizeof(line),f));
    assert(!strcmp(line,"INVEST_BUDGET\t2\n"));
    assert(fgets(line,sizeof(line),f)&&!strcmp(line,"INVESTMENT\t1000.70\n"));
    fclose(f);
    assert(snprintf(sub,sizeof(sub),"%s/invest",dir)<(int)sizeof(sub));
    assert(access(sub,F_OK)==0);
    /* Reserva maior que saldo anterior resulta em saldo negativo, sem perder dados. */
    data.monthly_investment=3000.00;
    assert(budget_totals(&data,&totals)==0);
    near(totals.balance,-189.90);
    /* Zero remove a reserva mensal planejada. */
    data.monthly_investment=0;
    assert(budget_totals(&data,&totals)==0);
    near(totals.balance,2810.10);
    /* Sem fundos, sem receitas e com investimento, o saldo tambem e negativo. */
    {
        BudgetStore no_income={0};
        no_income.monthly_investment=150.0;
        assert(budget_totals(&no_income,&totals)==0);
        near(totals.balance,-150.0);
    }
    /* Orcamento legado v1 deve abrir sem investimento planejado. */
    f=fopen(path,"w");assert(f);
    assert(fputs("INVEST_BUDGET\t1\n0\tSalario legado\t3000.00\t7\t1\n",f)>0);
    assert(fclose(f)==0);
    assert(budget_load(&copy,err,sizeof(err))==0);
    assert(copy.count==1);near(copy.monthly_investment,0);
    copy.monthly_investment=400.00;
    assert(budget_save(&copy,err,sizeof(err))==0);
    {
        char backup[4096];
        assert(snprintf(backup,sizeof(backup),"%s.v1.bak",path)<(int)sizeof(backup));
        f=fopen(backup,"r");assert(f && fgets(line,sizeof(line),f));
        assert(!strcmp(line,"INVEST_BUDGET\t1\n"));
        fclose(f);
    }
    assert(budget_load(&copy,err,sizeof(err))==0);
    near(copy.monthly_investment,400.00);
    assert(copy.count==1);
    /* Arquivo v2 invalido nao pode sobrescrever o orcamento em memoria. */
    f=fopen(path,"w");assert(f);
    assert(fputs("INVEST_BUDGET\t2\nINVESTMENT\t-1.00\n",f)>0);
    assert(fclose(f)==0);
    assert(budget_load(&copy,err,sizeof(err))<0);
    near(copy.monthly_investment,400.00);
    f=fopen(path,"w");assert(f);
    assert(fputs("INVEST_BUDGET\t2\nINVESTMENT\t100.00x\n",f)>0);
    assert(fclose(f)==0);
    assert(budget_load(&copy,err,sizeof(err))<0);
    near(copy.monthly_investment,400.00);
    /* Arquivo malformado nunca e regravado nem parcialmente carregado. */
    f=fopen(path,"w");assert(f);
    assert(fputs("INVEST_BUDGET\t1\n1\tGasto invalido\t-15\t7\t0\n",f)>0);
    assert(fclose(f)==0);
    assert(budget_load(&copy,err,sizeof(err))<0);
    assert(copy.count==1); /* Sem sobrescrever a estrutura antiga. */
    f=fopen(path,"r");assert(f && fgets(line,sizeof(line),f));
    assert(fgets(line,sizeof(line),f) && strstr(line,"-15"));fclose(f);
    data.items[5]=(BudgetItem){"Gasto invalido",-0.1,BUDGET_EXPENSE,CAT_OUTROS,0};
    data.count++;
    assert(budget_save(&data,err,sizeof(err))<0);
    data.count=0;data.monthly_investment=0;assert(budget_totals(&data,&totals)==0);
    near(totals.balance,0);
    assert(budget_save(&data,err,sizeof(err))==0);
    assert(budget_load(&copy,err,sizeof(err))==0 && copy.count==0);
    data.monthly_investment=-1.0;
    assert(budget_totals(&data,&totals)<0);
    assert(budget_save(&data,err,sizeof(err))<0);
    data.monthly_investment=INFINITY;
    assert(budget_totals(&data,&totals)<0);
    puts("test_budget: OK (reserva mensal, compatibilidade e saldo disponivel)");
    return 0;
}

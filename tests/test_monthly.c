#define _POSIX_C_SOURCE 200809L
#include "monthly.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static void item(BudgetStore*b,BudgetKind kind,const char *name,double amount){
    BudgetItem *it=&b->items[b->count++];
    memset(it,0,sizeof(*it));snprintf(it->name,sizeof(it->name),"%s",name);
    it->kind=kind;it->amount=amount;it->category=CAT_OUTROS;it->fixed=1;
}
int main(void){
    char tmp[]="/tmp/invest-monthly-test-XXXXXX",path[1024],err[256]={0};
    MonthlyBudget a={0},b={0};BudgetTotals t;
    assert(mkdtemp(tmp));assert(setenv("XDG_DATA_HOME",tmp,1)==0);
    assert(monthly_load(2026,11,&b,err,sizeof(err))==1);
    item(&a.planned,BUDGET_INCOME,"Salário",4500);
    item(&a.planned,BUDGET_EXPENSE,"Aluguel",1800);
    a.planned.monthly_investment=1000;
    item(&a.actual,BUDGET_INCOME,"Ação",4000);
    item(&a.actual,BUDGET_EXPENSE,"Saúde",900);
    a.actual.monthly_investment=850;
    assert(monthly_save(2026,11,&a,err,sizeof(err))==0);
    assert(monthly_load(2026,11,&b,err,sizeof(err))==0);
    assert(b.planned.count==2&&b.actual.count==2);
    assert(budget_totals(&b.planned,&t)==0&&t.balance==1700);
    assert(budget_totals(&b.actual,&t)==0&&t.balance==2250);
    assert(strcmp(b.actual.items[0].name,"Ação")==0);
    assert(monthly_load(2026,12,&b,err,sizeof(err))==1);
    assert(b.planned.count==0&&b.actual.count==0);
    assert(monthly_save(2026,11,&a,err,sizeof(err))==0);
    assert(monthly_path(2026,11,path,sizeof(path))==0);
    assert(strlen(path)+4<sizeof(path));strcat(path,".bak");
    assert(access(path,F_OK)==0);
    assert(monthly_path(2026,11,path,sizeof(path))==0);
    FILE*f=fopen(path,"w");assert(f);fputs("corrupt\n",f);fclose(f);
    assert(monthly_load(2026,11,&b,err,sizeof(err))==-1);
    assert(monthly_save(2026,13,&a,err,sizeof(err))==-1);
    puts("monthly budgets: OK");
    return 0;
}

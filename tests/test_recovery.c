#define _POSIX_C_SOURCE 200809L
#include "storage.h"
#include "monthly.h"
#include "budget.h"
#include "safeio.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void replace(const char *path,const char *value){
    FILE *f=fopen(path,"wb");assert(f);
    assert(fwrite(value,1,strlen(value),f)==strlen(value));
    assert(fclose(f)==0);
}
static int has_damaged(const char *path){
    char dir[4096],base[256];const char *slash=strrchr(path,'/');
    DIR*d;struct dirent*e;int found=0;
    assert(slash&&(size_t)(slash-path)<sizeof(dir));
    memcpy(dir,path,(size_t)(slash-path));dir[slash-path]=0;
    snprintf(base,sizeof(base),"%s.corrupt.",slash+1);
    d=opendir(dir);assert(d);
    while((e=readdir(d)))if(strncmp(e->d_name,base,strlen(base))==0)found=1;
    closedir(d);return found;
}
static void budget_item(BudgetStore *b,double v){
    BudgetItem *it=&b->items[b->count++];
    memset(it,0,sizeof(*it));strcpy(it->name,"Salário");
    it->kind=BUDGET_INCOME;it->amount=v;it->category=CAT_OUTROS;it->fixed=1;
}
int main(void){
    char root[]="/tmp/invest-recovery-XXXXXX",path[4096],backup[4096],err[256]={0};
    ScenarioStore s={0},s2={0};BudgetStore b={0},b2={0};MonthlyBudget m={0},m2={0};
    int date[2]={2027,1};int rc;
    assert(mkdtemp(root));assert(setenv("XDG_DATA_HOME",root,1)==0);
    scenario_default(&s.items[0]);s.count=1;s.selected=0;
    s.items[0].initial=1000;
    assert(storage_save(&s,err,sizeof(err))==0);
    s.items[0].initial=1200;
    assert(storage_save(&s,err,sizeof(err))==0);
    assert(storage_path(path,sizeof(path))==0);
    assert(snprintf(backup,sizeof(backup),"%s.bak",path)<(int)sizeof(backup));
    assert(storage_validate_file(backup,NULL)==0);
    replace(path,"INVEST\t3\t0\nconteúdo inválido\n");
    s.items[0].initial=1300;
    assert(storage_save(&s,err,sizeof(err))==-1); /* cannot clobber damage */
    assert(storage_validate_file(backup,NULL)==0);
    rc=safeio_recover(path,storage_validate_file,NULL,err,sizeof(err));
    assert(rc==1 && has_damaged(path));
    assert(storage_load(&s2,err,sizeof(err))==0);
    assert(s2.items[0].initial==s.items[0].initial-300); /* backup = first save */
    assert(safeio_recover(path,storage_validate_file,NULL,err,sizeof(err))==0);
    budget_item(&b,2500);
    assert(budget_save(&b,err,sizeof(err))==0);
    b.items[0].amount=3000;
    assert(budget_save(&b,err,sizeof(err))==0);
    assert(budget_path(path,sizeof(path))==0);
    assert(snprintf(backup,sizeof(backup),"%s.bak",path)<(int)sizeof(backup));
    assert(budget_validate_file(backup,NULL)==0);
    replace(path,"INVEST_BUDGET\t2\nINVESTMENT\t-9\n");
    b.items[0].amount=3500;
    assert(budget_save(&b,err,sizeof(err))<0);
    assert(safeio_recover(path,budget_validate_file,NULL,err,sizeof(err))==1);
    assert(budget_load(&b2,err,sizeof(err))==0 && b2.items[0].amount==2500);
    assert(has_damaged(path));
    budget_item(&m.planned,4500);
    assert(monthly_save(date[0],date[1],&m,err,sizeof(err))==0);
    m.planned.items[0].amount=5100;
    assert(monthly_save(date[0],date[1],&m,err,sizeof(err))==0);
    assert(monthly_path(date[0],date[1],path,sizeof(path))==0);
    assert(snprintf(backup,sizeof(backup),"%s.bak",path)<(int)sizeof(backup));
    assert(monthly_validate_file(backup,date)==0);
    replace(path,"INVEST_MONTH\t1\t2027\t01\nR_INVESTMENT\tbroken\n");
    assert(monthly_save(date[0],date[1],&m,err,sizeof(err))==-1);
    assert(monthly_validate_file(backup,date)==0);
    assert(safeio_recover(path,monthly_validate_file,date,err,sizeof(err))==1);
    assert(monthly_load(date[0],date[1],&m2,err,sizeof(err))==0);
    assert(m2.planned.items[0].amount==4500);
    assert(has_damaged(path));
    printf("test_recovery: OK (3 tipos, recusa sobrescrita, backup intacto e original danificado guardado)\n");
    return 0;
}

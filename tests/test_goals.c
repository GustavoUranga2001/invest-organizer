#define _POSIX_C_SOURCE 200809L
#include "goals.h"
#include "safeio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int main(void){
    char dir[]="/tmp/invest-goals-test-XXXXXX",path[1024],bak[1100],err[256];
    GoalsStore s={0},loaded={0};s.next_id=1;
    assert(mkdtemp(dir));assert(setenv("XDG_DATA_HOME",dir,1)==0);
    assert(goals_path(path,sizeof(path))==0);
    assert(goals_load(&loaded,err,sizeof(err))==1);
    int a=goals_add(&s,"Reserva de emergência",2000000,2028,12);
    int b=goals_add(&s,"Viagem à Europa",1200000,2028,6);
    assert(a>0&&b>a&&goals_valid(&s));
    assert(goals_allocate(&s,a,2026,11,60000,100000)==0);
    assert(goals_allocate(&s,b,2026,11,40000,100000)==0);
    assert(goals_allocate(&s,b,2026,11,40001,100000)==-2);
    assert(goals_month_total(&s,2026,11)==100000);
    assert(goals_progress(&s,a,2026,10)==0);
    assert(goals_progress(&s,a,2026,11)==60000);
    assert(goals_allocate(&s,a,2026,12,40000,50000)==0);
    assert(goals_progress(&s,a,2026,12)==100000);
    assert(goals_required_monthly(&s,a,2026,12)==(1900000+24-1)/24);
    assert(goals_required_monthly(&s,a,2029,1)==-1);
    assert(goals_delete(&s,a)==-2);
    assert(goals_save(&s,err,sizeof(err))==0);
    assert(goals_load(&loaded,err,sizeof(err))==0);
    assert(loaded.count==2&&loaded.allocation_count==3);
    assert(goals_progress(&loaded,a,2026,12)==100000);
    assert(goals_edit(&loaded,a,"Fundo de emergência",2500000,2029,1)==0);
    assert(goals_save(&loaded,err,sizeof(err))==0);
    snprintf(bak,sizeof(bak),"%s.bak",path);
    assert(access(bak,F_OK)==0);
    assert(goals_allocate(&loaded,a,2026,11,0,100000)==0);
    assert(goals_month_total(&loaded,2026,11)==40000);
    assert(goals_delete(&loaded,a)==-2);
    assert(goals_allocate(&loaded,a,2026,12,0,50000)==0);
    assert(goals_delete(&loaded,a)==0);
    assert(goals_valid(&loaded));
    assert(goals_save(&loaded,err,sizeof(err))==0);
    FILE *f=fopen(path,"w");assert(f);fputs("INVEST_GOALS\t1\nG\t1\tbroken\n",f);fclose(f);
    assert(goals_load(&loaded,err,sizeof(err))==-1);
    assert(goals_save(&s,err,sizeof(err))==-1); /* no overwrite corrupt original */
    assert(safeio_recover(path,goals_validate_file,NULL,err,sizeof(err))==1);
    assert(goals_load(&loaded,err,sizeof(err))==0);
    assert(loaded.count==2);
    printf("metas: integrações mensais, limites, UTF-8, backup e recuperação OK\n");
    return 0;
}

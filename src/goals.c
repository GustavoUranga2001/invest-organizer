#define _POSIX_C_SOURCE 200809L
#include "goals.h"
#include "budget.h"
#include "utf8.h"
#include "safeio.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define GOAL_MAX_CENTS 100000000000000LL
static void fail(char *err,size_t cap,const char *message){if(err&&cap)snprintf(err,cap,"%s",message);}
static int date_ok(int y,int m){return y>=1900&&y<=2200&&m>=1&&m<=12;}
static int before_eq(int y,int m,int yy,int mm){return y<yy||(y==yy&&m<=mm);}
static int name_ok(const char *s){size_t n;
    if(!s)return 0;
    n=strnlen(s,GOAL_NAME_MAX);
    return n>0&&n<GOAL_NAME_MAX&&utf8_valid(s)&&!strpbrk(s,"\t\r\n");
}
int goals_find(const GoalsStore *s,int id){if(!s)return -1;for(int i=0;i<s->count;i++)if(s->items[i].id==id)return i;return -1;}
long long goals_month_for(const GoalsStore *s,int id,int y,int m){
    if(!s)return 0;
    for(int i=0;i<s->allocation_count;i++){
        const GoalAllocation *a=&s->allocations[i];
        if(a->goal_id==id&&a->year==y&&a->month==m)return a->cents;
    }
    return 0;
}
long long goals_month_total(const GoalsStore *s,int y,int m){long long n=0;
    if(!s)return 0;
    for(int i=0;i<s->allocation_count;i++){
        const GoalAllocation *a=&s->allocations[i];
        if(a->year==y&&a->month==m)n+=a->cents;
    }return n;
}
long long goals_progress(const GoalsStore *s,int id,int y,int m){long long n=0;
    if(!s)return 0;
    for(int i=0;i<s->allocation_count;i++){
        const GoalAllocation *a=&s->allocations[i];
        if(a->goal_id==id&&before_eq(a->year,a->month,y,m))n+=a->cents;
    }return n;
}
long long goals_required_monthly(const GoalsStore *s,int id,int year,int month){
    int ix=goals_find(s,id);
    if(ix<0||!date_ok(year,month))return -1;
    const Goal *g=&s->items[ix];
    long long already=goals_progress(s,id,year,month);
    if(already>=g->target_cents)return 0;
    int months=(g->deadline_year-year)*12+(g->deadline_month-month);
    if(months<=0)return -1;
    long long missing=g->target_cents-already;
    return (missing+months-1)/months;
}
int goals_valid(const GoalsStore *s){
    if(!s||s->count<0||s->count>GOALS_MAX||s->allocation_count<0||s->allocation_count>GOALS_ALLOC_MAX)return 0;
    int max_id=0;
    for(int i=0;i<s->count;i++){
        const Goal *g=&s->items[i];
        if(g->id<1||!name_ok(g->name)||g->target_cents<=0||g->target_cents>GOAL_MAX_CENTS||
           !date_ok(g->deadline_year,g->deadline_month))return 0;
        if(g->id>max_id)max_id=g->id;
        for(int j=0;j<i;j++)if(s->items[j].id==g->id)return 0;
    }
    if(s->next_id<max_id+1||s->next_id<1)return 0;
    for(int i=0;i<s->allocation_count;i++){
        const GoalAllocation *a=&s->allocations[i];
        if(goals_find(s,a->goal_id)<0||!date_ok(a->year,a->month)||a->cents<=0||a->cents>GOAL_MAX_CENTS)return 0;
        for(int j=0;j<i;j++){
            const GoalAllocation *b=&s->allocations[j];
            if(a->goal_id==b->goal_id&&a->year==b->year&&a->month==b->month)return 0;
        }
    }
    return 1;
}
int goals_add(GoalsStore *s,const char *name,long long target,int year,int month){
    if(!s||s->count>=GOALS_MAX||s->next_id<=0||s->next_id>=INT_MAX||!name_ok(name)||
       target<=0||target>GOAL_MAX_CENTS||!date_ok(year,month))return -1;
    Goal *g=&s->items[s->count++];memset(g,0,sizeof(*g));
    g->id=s->next_id++;snprintf(g->name,sizeof(g->name),"%s",name);
    g->target_cents=target;g->deadline_year=year;g->deadline_month=month;
    return g->id;
}
int goals_edit(GoalsStore *s,int id,const char *name,long long target,int year,int month){
    int ix=goals_find(s,id);
    if(ix<0||!name_ok(name)||target<=0||target>GOAL_MAX_CENTS||!date_ok(year,month))return -1;
    Goal *g=&s->items[ix];snprintf(g->name,sizeof(g->name),"%s",name);
    g->target_cents=target;g->deadline_year=year;g->deadline_month=month;return 0;
}
int goals_delete(GoalsStore *s,int id){int ix=goals_find(s,id);
    if(ix<0)return -1;
    for(int i=0;i<s->allocation_count;i++)if(s->allocations[i].goal_id==id)return -2;
    for(int i=ix;i<s->count-1;i++)s->items[i]=s->items[i+1];
    s->count--;
    return 0;
}
int goals_allocate(GoalsStore *s,int id,int year,int month,long long cents,long long recorded){
    if(!s||goals_find(s,id)<0||!date_ok(year,month)||cents<0||cents>GOAL_MAX_CENTS||
       recorded<0||recorded>GOAL_MAX_CENTS)return -1;
    long long existing=goals_month_for(s,id,year,month);
    long long other=goals_month_total(s,year,month)-existing;
    if(cents>recorded-other)return -2;
    for(int i=0;i<s->allocation_count;i++){
        GoalAllocation *a=&s->allocations[i];
        if(a->goal_id==id&&a->year==year&&a->month==month){
            if(!cents){for(int j=i;j<s->allocation_count-1;j++)s->allocations[j]=s->allocations[j+1];s->allocation_count--;}
            else a->cents=cents;
            return 0;
        }
    }
    if(!cents)return 0;
    if(s->allocation_count>=GOALS_ALLOC_MAX)return -3;
    s->allocations[s->allocation_count++]=(GoalAllocation){id,year,month,cents};return 0;
}
int goals_path(char *out,size_t cap){char legacy[PATH_MAX];size_t n;
    if(!out||!cap||budget_path(legacy,sizeof(legacy))<0)return -1;
    n=strlen(legacy);if(n<strlen("orcamento.tsv")||strcmp(legacy+n-strlen("orcamento.tsv"),"orcamento.tsv"))return -1;
    legacy[n-strlen("orcamento.tsv")]=0;
    int k=snprintf(out,cap,"%smetas.tsv",legacy);
    return k>=0&&(size_t)k<cap?0:-1;
}
static int strict_ll(const char *text,long long *out){char tail;
    return text&&sscanf(text,"%lld%c",out,&tail)==1?0:-1;
}
static int strict_int(const char *text,int *out){char tail;
    return text&&sscanf(text,"%d%c",out,&tail)==1?0:-1;
}
static int split(char *s,char **out,int expected){int n=0;char *save=NULL;
    char *p=strtok_r(s,"\t\r\n",&save);
    while(p&&n<expected){out[n++]=p;p=strtok_r(NULL,"\t\r\n",&save);}
    return n==expected&&!p?0:-1;
}
int goals_load_file(const char *path,GoalsStore *out,char *err,size_t cap){
    char line[1024];FILE *f;GoalsStore *tmp;
    if(!out||!path)return -1;
    f=fopen(path,"r");
    if(!f){if(errno==ENOENT){memset(out,0,sizeof(*out));out->next_id=1;return 1;}fail(err,cap,"Não foi possível abrir metas");return -1;}
    tmp=calloc(1,sizeof(*tmp));if(!tmp){fclose(f);fail(err,cap,"Sem memória para metas");return -1;}
    tmp->next_id=1;
    if(!fgets(line,sizeof(line),f)||strcmp(line,"INVEST_GOALS\t1\n"))goto invalid;
    while(fgets(line,sizeof(line),f)){
        size_t len=strlen(line);char *parts[6];
        if(len==0||line[len-1]!='\n')goto invalid;
        if(line[0]=='G'&&line[1]=='\t'){
            Goal *g;int id,y,m;long long target;
            if(tmp->count>=GOALS_MAX||split(line,parts,6)<0||
               strict_int(parts[1],&id)<0||strict_ll(parts[3],&target)<0||
               strict_int(parts[4],&y)<0||strict_int(parts[5],&m)<0||
               id<1||id>=INT_MAX||!name_ok(parts[2])||target<=0||target>GOAL_MAX_CENTS||!date_ok(y,m))goto invalid;
            g=&tmp->items[tmp->count++];g->id=id;snprintf(g->name,sizeof(g->name),"%s",parts[2]);
            g->target_cents=target;g->deadline_year=y;g->deadline_month=m;
            if(tmp->next_id<=id)tmp->next_id=id+1;
        }else if(line[0]=='A'&&line[1]=='\t'){
            GoalAllocation *a;int id,y,m;long long cents;
            if(tmp->allocation_count>=GOALS_ALLOC_MAX||split(line,parts,5)<0||
               strict_int(parts[1],&id)<0||strict_int(parts[2],&y)<0||
               strict_int(parts[3],&m)<0||strict_ll(parts[4],&cents)<0||!date_ok(y,m))goto invalid;
            a=&tmp->allocations[tmp->allocation_count++];*a=(GoalAllocation){id,y,m,cents};
        }else goto invalid;
    }
    if(ferror(f)||!goals_valid(tmp))goto invalid;
    if(fclose(f)!=0){free(tmp);fail(err,cap,"Falha ao fechar metas");return -1;}
    *out=*tmp;free(tmp);return 0;
invalid:
    fclose(f);free(tmp);fail(err,cap,"Arquivo metas inválido; original preservado");return -1;
}
int goals_load(GoalsStore *s,char *err,size_t cap){char path[PATH_MAX];
    if(goals_path(path,sizeof(path))<0){fail(err,cap,"Caminho das metas inválido");return -1;}
    return goals_load_file(path,s,err,cap);
}
int goals_validate_file(const char *path,void *context){GoalsStore *tmp;int rc;
    (void)context;tmp=calloc(1,sizeof(*tmp));if(!tmp)return -1;
    rc=goals_load_file(path,tmp,NULL,0);free(tmp);return rc;
}
static int ensure_parent(const char *path){char buf[PATH_MAX];size_t len=strlen(path);
    if(len>=sizeof(buf))return -1;
    memcpy(buf,path,len+1);
    for(size_t i=1;i<len;i++)if(buf[i]=='/'){
        buf[i]=0;if(mkdir(buf,0700)<0&&errno!=EEXIST)return -1;buf[i]='/';
    }
    return 0;
}
int goals_save(const GoalsStore *s,char *err,size_t cap){
    char path[PATH_MAX],tmpname[PATH_MAX];int fd,k;FILE *f;
    if(!goals_valid(s)||goals_path(path,sizeof(path))<0||ensure_parent(path)<0){fail(err,cap,"Metas ou diretório inválidos");return -1;}
    k=snprintf(tmpname,sizeof(tmpname),"%s.tmp.XXXXXX",path);
    if(k<0||(size_t)k>=sizeof(tmpname)){fail(err,cap,"Caminho muito longo");return -1;}
    fd=mkstemp(tmpname);if(fd<0){fail(err,cap,"Não foi possível criar arquivo temporário");return -1;}
    f=fdopen(fd,"w");if(!f){close(fd);unlink(tmpname);return -1;}
    if(fputs("INVEST_GOALS\t1\n",f)==EOF)goto failed;
    for(int i=0;i<s->count;i++){
        const Goal *g=&s->items[i];
        if(fprintf(f,"G\t%d\t%s\t%lld\t%d\t%d\n",g->id,g->name,g->target_cents,g->deadline_year,g->deadline_month)<0)goto failed;
    }
    for(int i=0;i<s->allocation_count;i++){
        const GoalAllocation *a=&s->allocations[i];
        if(fprintf(f,"A\t%d\t%d\t%d\t%lld\n",a->goal_id,a->year,a->month,a->cents)<0)goto failed;
    }
    if(fflush(f)!=0||fsync(fd)!=0)goto failed;
    if(fclose(f)!=0){unlink(tmpname);fail(err,cap,"Falha ao fechar metas");return -1;}
    if(safeio_commit(path,tmpname,goals_validate_file,NULL,err,cap)<0){unlink(tmpname);return -1;}
    return 0;
failed:
    fclose(f);unlink(tmpname);fail(err,cap,"Falha na gravação das metas");return -1;
}

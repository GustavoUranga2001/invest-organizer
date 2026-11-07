#define _POSIX_C_SOURCE 200809L
#include "monthly.h"
#include "utf8.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
static void fail(char *err,size_t n,const char *message){if(err && n)snprintf(err,n,"%s",message);}
static int date_valid(int year,int month){return year>=1900&&year<=2200&&month>=1&&month<=12;}
int monthly_path(int year,int month,char*out,size_t cap){
    char root[PATH_MAX];int n;size_t length;
    if(!date_valid(year,month)||!out||!cap||budget_path(root,sizeof(root))<0)return -1;
    length=strlen(root);
    if(length<strlen("orcamento.tsv")||strcmp(root+length-strlen("orcamento.tsv"),"orcamento.tsv"))return -1;
    root[length-strlen("orcamento.tsv")]=0;
    n=snprintf(out,cap,"%smeses/%04d-%02d.tsv",root,year,month);
    return n>=0&&(size_t)n<cap?0:-1;
}
static int ensure_dirs(const char*path){
    char tmp[PATH_MAX];size_t n=strlen(path),i;
    if(n>=sizeof(tmp))return -1;
    memcpy(tmp,path,n+1);
    for(i=1;i<n;i++)if(tmp[i]=='/'){
        tmp[i]=0;if(mkdir(tmp,0700)<0&&errno!=EEXIST)return -1;tmp[i]='/';
    }
    return 0;
}
static int integer(const char*s,int*value){char c;return sscanf(s,"%d%c",value,&c)==1?0:-1;}
static int amount(const char*s,double*value){
    char *end;errno=0;*value=strtod(s,&end);
    if(end==s||errno||!isfinite(*value)||*value<0||*value>1e12)return -1;
    if(*end=='\n')end++;
    return *end==0?0:-1;
}
static int parse_item(char *line,BudgetStore *store){
    char *parts[5]={0},*save=NULL,*p;BudgetItem item={0};int n=0,kind,cat,fixed;
    p=strtok_r(line,"\t\n\r",&save);
    while(p&&n<5){parts[n++]=p;p=strtok_r(NULL,"\t\n\r",&save);}
    if(p||n!=5||store->count>=BUDGET_MAX_ITEMS||integer(parts[0],&kind)<0||
       strlen(parts[1])>=sizeof(item.name)||amount(parts[2],&item.amount)<0||
       integer(parts[3],&cat)<0||integer(parts[4],&fixed)<0)return -1;
    item.kind=(BudgetKind)kind;item.category=cat;item.fixed=fixed;
    /* Do not silently alter records: corrupted files must remain untouched. */
    if(!utf8_valid(parts[1]))return -1;
    memcpy(item.name,parts[1],strlen(parts[1])+1);
    if(!budget_valid_item(&item))return -1;
    store->items[store->count++]=item;
    return 0;
}
int monthly_load(int year,int month,MonthlyBudget*out,char*err,size_t err_size){
    char path[PATH_MAX],line[1024],header[100];int have_p=0,have_r=0;FILE*f;MonthlyBudget *temp;
    if(!out||monthly_path(year,month,path,sizeof(path))<0){fail(err,err_size,"Data mensal inválida");return -1;}
    f=fopen(path,"r");
    if(!f){if(errno==ENOENT){memset(out,0,sizeof(*out));return 1;}fail(err,err_size,"Falha ao abrir orçamento mensal");return -1;}
    temp=calloc(1,sizeof(*temp));
    if(!temp){fclose(f);fail(err,err_size,"Sem memória para abrir orçamento");return -1;}
    snprintf(header,sizeof(header),"INVEST_MONTH\t1\t%04d\t%02d\n",year,month);
    if(!fgets(line,sizeof(line),f)||strcmp(line,header))goto invalid;
    while(fgets(line,sizeof(line),f)){
        BudgetStore *target=NULL;double v;
        size_t n=strlen(line);
        if(!n||line[n-1]!='\n')goto invalid;
        if(!strncmp(line,"P_INVESTMENT\t",13)){
            if(have_p++||amount(line+13,&v)<0)goto invalid;
            temp->planned.monthly_investment=v;
        }else if(!strncmp(line,"R_INVESTMENT\t",13)){
            if(have_r++||amount(line+13,&v)<0)goto invalid;
            temp->actual.monthly_investment=v;
        }else if(!strncmp(line,"P\t",2)){
            target=&temp->planned;
            if(parse_item(line+2,target)<0)goto invalid;
        }else if(!strncmp(line,"R\t",2)){
            target=&temp->actual;
            if(parse_item(line+2,target)<0)goto invalid;
        }else goto invalid;
    }
    if(ferror(f)||!have_p||!have_r)goto invalid;
    {BudgetTotals total;
     if(budget_totals(&temp->planned,&total)<0||budget_totals(&temp->actual,&total)<0)goto invalid;}
    if(fclose(f)!=0){free(temp);fail(err,err_size,"Falha ao fechar orçamento");return -1;}
    *out=*temp;free(temp);return 0;
invalid:
    fclose(f);free(temp);fail(err,err_size,"Arquivo mensal inválido; original preservado");return -1;
}
int monthly_save(int year,int month,const MonthlyBudget*data,char*err,size_t err_size){
    char path[PATH_MAX],temp[PATH_MAX],backup[PATH_MAX];BudgetTotals totals;
    int fd,i,n;FILE*f;
    if(!data||budget_totals(&data->planned,&totals)<0||budget_totals(&data->actual,&totals)<0||
       monthly_path(year,month,path,sizeof(path))<0||ensure_dirs(path)<0){
        fail(err,err_size,"Orçamento mensal ou caminho inválido");return -1;
    }
    n=snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path);
    if(n<0||(size_t)n>=sizeof(temp)){fail(err,err_size,"Caminho muito longo");return -1;}
    fd=mkstemp(temp);
    if(fd<0){fail(err,err_size,"Não foi possível criar arquivo temporário");return -1;}
    f=fdopen(fd,"w");
    if(!f){close(fd);unlink(temp);fail(err,err_size,"Falha de escrita");return -1;}
    if(fprintf(f,"INVEST_MONTH\t1\t%04d\t%02d\n",year,month)<0||
       fprintf(f,"P_INVESTMENT\t%.2f\n",data->planned.monthly_investment)<0||
       fprintf(f,"R_INVESTMENT\t%.2f\n",data->actual.monthly_investment)<0)goto failed;
    for(int mode=0;mode<2;mode++){
        const BudgetStore *b=mode?&data->actual:&data->planned;
        for(i=0;i<b->count;i++){
            const BudgetItem *it=&b->items[i];
            if(fprintf(f,"%c\t%d\t%s\t%.2f\t%d\t%d\n",mode?'R':'P',it->kind,
                       it->name,it->amount,it->category,it->fixed)<0)goto failed;
        }
    }
    if(fflush(f)!=0||fsync(fd)!=0)goto failed;
    if(fclose(f)!=0){unlink(temp);fail(err,err_size,"Falha ao fechar arquivo mensal");return -1;}
    /* Back up the last valid version before replacement (source file stays intact). */
    {struct stat st;
     if(stat(path,&st)==0){
         int bn=snprintf(backup,sizeof(backup),"%s.bak",path);
         if(bn<0||(size_t)bn>=sizeof(backup)){unlink(temp);fail(err,err_size,"Backup: caminho muito longo");return -1;}
         if(unlink(backup)<0&&errno!=ENOENT){unlink(temp);fail(err,err_size,"Falha ao renovar backup");return -1;}
         if(link(path,backup)<0){unlink(temp);fail(err,err_size,"Falha ao criar backup");return -1;}
     }else if(errno!=ENOENT){unlink(temp);fail(err,err_size,"Falha ao verificar arquivo original");return -1;}}
    if(rename(temp,path)!=0){unlink(temp);fail(err,err_size,"Falha ao gravar mês");return -1;}
    return 0;
failed:
    fclose(f);unlink(temp);fail(err,err_size,"Falha ao gravar mês");return -1;
}

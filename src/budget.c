#define _POSIX_C_SOURCE 200809L
#include "budget.h"
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

const char *const budget_categories[BUDGET_CATEGORY_COUNT] = {
    "Moradia", "Alimentacao", "Transporte", "Saude",
    "Educacao", "Lazer", "Contas", "Outros"
};

static void fail(char *err, size_t len, const char *msg) {
    if(err && len)snprintf(err,len,"%s",msg);
}

int budget_valid_item(const BudgetItem *item) {
    size_t len;
    if(!item)return 0;
    len=strnlen(item->name,sizeof(item->name));
    if(len<1 || len>=sizeof(item->name) ||
       strpbrk(item->name,"\n\r\t") ||
       !isfinite(item->amount) || item->amount<0.0 || item->amount>1e12 ||
       (item->kind!=BUDGET_INCOME && item->kind!=BUDGET_EXPENSE) ||
       item->category<0 || item->category>=BUDGET_CATEGORY_COUNT ||
       (item->fixed!=0 && item->fixed!=1))return 0;
    return 1;
}

/* Somar centavos inteiros evita erros de acumulacao monetaria no resumo. */
int budget_totals(const BudgetStore *store, BudgetTotals *out){
    long long in=0,expenses=0,fixed=0,variable=0,categories[BUDGET_CATEGORY_COUNT]={0};
    int i;
    if(!store||!out||store->count<0||store->count>BUDGET_MAX_ITEMS||
       !isfinite(store->monthly_investment)||store->monthly_investment<0.0||
       store->monthly_investment>1e12)return -1;
    for(i=0;i<store->count;i++){
        const BudgetItem *item=&store->items[i];
        long long c;
        if(!budget_valid_item(item))return -1;
        c=llround(item->amount*100.0);
        if(item->kind==BUDGET_INCOME)in+=c;
        else{
            expenses+=c;
            if(item->fixed)fixed+=c;
            else variable+=c;
            categories[item->category]+=c;
        }
    }
    memset(out,0,sizeof(*out));
    out->income=in/100.0;out->expenses=expenses/100.0;
    out->fixed_expenses=fixed/100.0;out->variable_expenses=variable/100.0;
    {
        long long allocated=llround(store->monthly_investment*100.0);
        out->monthly_investment=allocated/100.0;
        out->balance_before_investment=(in-expenses)/100.0;
        out->balance=(in-expenses-allocated)/100.0;
    }
    for(i=0;i<BUDGET_CATEGORY_COUNT;i++)out->by_category[i]=categories[i]/100.0;
    return 0;
}

int budget_path(char *out,size_t size){
    const char *root=getenv("XDG_DATA_HOME"),*home=getenv("HOME");
    int n;
    if(root && root[0]=='/')n=snprintf(out,size,"%s/invest/orcamento.tsv",root);
    else if(home && *home)n=snprintf(out,size,"%s/.local/share/invest/orcamento.tsv",home);
    else return -1;
    return n>=0&&(size_t)n<size?0:-1;
}
static int ensure_parent(const char *path){
    char buf[PATH_MAX];size_t n=strlen(path),i;
    if(n>=sizeof(buf))return -1;
    memcpy(buf,path,n+1);
    for(i=1;i<n;i++)if(buf[i]=='/'){
        buf[i]=0;
        if(mkdir(buf,0700)<0&&errno!=EEXIST)return -1;
        buf[i]='/';
    }
    return 0;
}
static int parse_int_strict(const char *s,int *n){
    char tail;
    return sscanf(s,"%d%c",n,&tail)==1?0:-1;
}
static int parse_amount_strict(const char *s,double *n){
    char tail;
    return sscanf(s,"%lf%c",n,&tail)==1?0:-1;
}

int budget_load(BudgetStore *store,char *err,size_t err_size){
    BudgetStore tmp={0};
    char path[PATH_MAX],line[1024];FILE*f;
    if(!store||budget_path(path,sizeof(path))<0){fail(err,err_size,"Pasta de orcamento indisponivel");return -1;}
    f=fopen(path,"r");
    if(!f && errno==ENOENT){memset(store,0,sizeof(*store));return 0;}
    if(!f){fail(err,err_size,"Falha ao abrir orcamento");return -1;}
    if(!fgets(line,sizeof(line),f))goto invalid;
    if(strcmp(line,"INVEST_BUDGET\t1\n")==0){
        /* Orcamentos da v0.3.3 continuam validos, com reserva inicial zero. */
        tmp.monthly_investment=0.0;
    }else if(strcmp(line,"INVEST_BUDGET\t2\n")==0){
        double planned;char ch;
        if(!fgets(line,sizeof(line),f)||
           sscanf(line,"INVESTMENT\t%lf%c",&planned,&ch)!=2||ch!='\n'||
           !isfinite(planned)||planned<0||planned>1e12)goto invalid;
        tmp.monthly_investment=planned;
    }else goto invalid;
    while(fgets(line,sizeof(line),f)){
        BudgetItem item={0};char *fields[5],*saveptr=NULL,*part;int n=0,kind,cat,fixed;
        if(!strchr(line,'\n'))goto invalid;
        part=strtok_r(line,"\t\r\n",&saveptr);
        while(part && n<5){fields[n++]=part;part=strtok_r(NULL,"\t\r\n",&saveptr);}
        if(part||n!=5||tmp.count==BUDGET_MAX_ITEMS||
           parse_int_strict(fields[0],&kind)<0||
           strlen(fields[1])>=sizeof(item.name)||
           parse_amount_strict(fields[2],&item.amount)<0||
           parse_int_strict(fields[3],&cat)<0||
           parse_int_strict(fields[4],&fixed)<0)goto invalid;
        item.kind=(BudgetKind)kind;item.category=cat;item.fixed=fixed;
        snprintf(item.name,sizeof(item.name),"%s",fields[1]);
        if(!budget_valid_item(&item))goto invalid;
        tmp.items[tmp.count++]=item;
    }
    if(ferror(f))goto invalid;
    fclose(f);*store=tmp;return 0;
invalid:
    fclose(f);fail(err,err_size,"Orcamento invalido; arquivo original preservado");return -1;
}

int budget_save(const BudgetStore *store,char *err,size_t err_size){
    char path[PATH_MAX],temp[PATH_MAX];
    BudgetTotals totals;FILE*f;int fd,i,n;
    if(budget_totals(store,&totals)<0 || budget_path(path,sizeof(path))<0 ||
       ensure_parent(path)<0){fail(err,err_size,"Orcamento ou pasta invalida");return -1;}
    n=snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path);
    if(n<0||(size_t)n>=sizeof(temp)){fail(err,err_size,"Caminho de orcamento longo demais");return -1;}
    fd=mkstemp(temp);
    if(fd<0){fail(err,err_size,"Nao foi possivel criar arquivo temporario");return -1;}
    f=fdopen(fd,"w");
    if(!f){close(fd);unlink(temp);fail(err,err_size,"Erro de escrita");return -1;}
    if(fprintf(f,"INVEST_BUDGET\t2\nINVESTMENT\t%.2f\n",store->monthly_investment)<0)goto invalid;
    for(i=0;i<store->count;i++){
        const BudgetItem*it=&store->items[i];
        if(fprintf(f,"%d\t%s\t%.2f\t%d\t%d\n",it->kind,it->name,it->amount,it->category,it->fixed)<0)
            goto invalid;
    }
    if(fflush(f)!=0 || fsync(fd)!=0)goto invalid;
    if(fclose(f)!=0){unlink(temp);fail(err,err_size,"Falha ao fechar orcamento");return -1;}
    /* Na migracao, preservar uma copia do orcamento original, sem substituir backups. */
    {
        FILE *original=fopen(path,"r");
        if(original){
            char header[64];
            if(fgets(header,sizeof(header),original) && !strcmp(header,"INVEST_BUDGET\t1\n")){
                char backup[PATH_MAX];int bn=snprintf(backup,sizeof(backup),"%s.v1.bak",path);
                if(bn<0||(size_t)bn>=sizeof(backup)){fclose(original);unlink(temp);fail(err,err_size,"Caminho do backup longo demais");return -1;}
                int bfd=open(backup,O_WRONLY|O_CREAT|O_EXCL,0600);
                if(bfd>=0){
                    char buffer[4096];size_t got;int good=1;
                    rewind(original);
                    while((got=fread(buffer,1,sizeof(buffer),original))>0){
                        size_t off=0;
                        while(off<got){ssize_t done=write(bfd,buffer+off,got-off);
                            if(done<=0){good=0;break;}off+=(size_t)done;}
                        if(!good)break;
                    }
                    if(ferror(original)||fsync(bfd)<0)good=0;
                    if(close(bfd)<0)good=0;
                    if(!good){unlink(backup);fclose(original);unlink(temp);fail(err,err_size,"Falha ao criar backup v1");return -1;}
                }else if(errno!=EEXIST){
                    fclose(original);unlink(temp);fail(err,err_size,"Falha ao abrir backup v1");return -1;
                }
            }
            fclose(original);
        }else if(errno!=ENOENT){
            unlink(temp);fail(err,err_size,"Falha ao ler orcamento antigo");return -1;
        }
    }
    if(rename(temp,path)!=0){unlink(temp);fail(err,err_size,"Falha ao substituir orcamento");return -1;}
    return 0;
invalid:
    fclose(f);unlink(temp);fail(err,err_size,"Falha ao salvar orcamento");return -1;
}

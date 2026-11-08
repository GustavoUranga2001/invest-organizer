#define _POSIX_C_SOURCE 200809L
#include "market.h"
#include "storage.h"
#include "safeio.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define RESPONSE_MAX 65536

/* No JSON or HTTP runtime dependency. The following narrowly scoped parser
 * reads only documented keys and refuses malformed/nonpositive values. */
static const char *skip_ws(const char *p) {
    while (*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
    return p;
}
static const char *json_find_key(const char *first,const char *limit,const char *key){
    char pattern[80]; const char *p=first;int n=snprintf(pattern,sizeof(pattern),"\"%s\"",key);
    if(n<=0||(size_t)n>=sizeof(pattern))return NULL;
    while(p && (p=strstr(p,pattern))){
        if(limit && p>=limit)return NULL;
        const char *c=skip_ws(p+n);
        if(*c==':' && (!limit || c<limit))return skip_ws(c+1);
        p++;
    }
    return NULL;
}
static int json_price(const char *p,double *out){
    char *end;double v;
    if(!p)return -1;
    if(*p=='\"')p++;
    errno=0;v=strtod(p,&end);
    if(errno||p==end||!isfinite(v)||v<=0||v>1e15)return -1;
    if(*end!='\"'&&*end!=','&&*end!='}'&&*end!=']'&&!isspace((unsigned char)*end))return -1;
    *out=v;return 0;
}
static int json_date(const char*p,char*dst,size_t cap){
    size_t i=0;
    if(!p||!dst||cap<2||*p!='\"')return -1;
    p++;
    while(p[i]&&p[i]!='\"'&&i+1<cap){
        char c=p[i];
        if(!isdigit((unsigned char)c)&&c!='/'&&c!='-'&&c!=':'&&c!=' '&&c!='T'&&c!='Z'&&c!='.')return -1;
        dst[i]=c;i++;
    }
    if(p[i]!='\"')return -1;
    dst[i]=0;return i?0:-1;
}
int market_parse_selic(const char*json,double*value,char*date,size_t cap){
    if(!json||*skip_ws(json)!='[')return -1;
    const char*v=json_find_key(json,NULL,"valor");
    const char*d=json_find_key(json,NULL,"data");
    if(json_price(v,value)<0||json_date(d,date,cap)<0||*value>1000)return -1;
    return 0;
}
int market_parse_ptax(const char*json,double*value,char*date,size_t cap){
    if(!json||*skip_ws(json)!='{')return -1;
    const char *array=json_find_key(json,NULL,"value");
    if(!array||*array!='['||!strchr(array,']'))return -1;
    const char *v=json_find_key(array,NULL,"cotacaoVenda");
    const char *d=json_find_key(array,NULL,"dataHoraCotacao");
    if(json_price(v,value)<0||json_date(d,date,cap)<0||*value>1000)return -1;
    return 0;
}
int market_parse_crypto(const char*json,double*value,char*date,size_t cap){
    if(!json||*skip_ws(json)!='{')return -1;
    const char *q=json_find_key(json,NULL,"quotes");
    const char *brl=q?json_find_key(q,NULL,"BRL"):NULL;
    if(!brl||*brl!='{')return -1;
    const char *end=strchr(brl,'}');
    if(!end)return -1;
    const char *price=json_find_key(brl,end,"price");
    if(!price||json_price(price,value)<0)return -1;
    const char *dt=json_find_key(json,NULL,"last_updated");
    if(dt && json_date(dt,date,cap)==0)return 0;
    if(cap)date[0]=0;
    return 0;
}
static int market_path(char*out,size_t cap){
    char path[PATH_MAX];size_t n;
    if(storage_path(path,sizeof(path))<0)return -1;
    n=strlen(path);
    if(n<strlen("cenarios.tsv")||strcmp(path+n-strlen("cenarios.tsv"),"cenarios.tsv"))return -1;
    path[n-strlen("cenarios.tsv")]=0;
    return snprintf(out,cap,"%smercado.tsv",path)<(int)cap?0:-1;
}
static int market_file(const char *path,MarketData*out){
    FILE*f=fopen(path,"r");char line[256];int previous=-1,count=0;
    if(!f)return errno==ENOENT?1:-1;
    if(!fgets(line,sizeof(line),f)||strcmp(line,"INVEST_MARKET\t1\n"))goto invalid;
    memset(out,0,sizeof(*out));
    while(fgets(line,sizeof(line),f)){
        int idx,pos=0;long long stamp;double v;char date[32];
        if(!strchr(line,'\n')||sscanf(line,"%d\t%lf\t%lld\t%31[^\t\n]%n",&idx,&v,&stamp,date,&pos)!=4 || pos<1 || line[pos]!='\n' || line[pos+1]!=0)goto invalid;
        if(idx<=previous||idx<0||idx>=MARKET_COUNT||!isfinite(v)||v<=0||v>1e15||stamp<=0)goto invalid;
        if(strlen(date)>30)goto invalid;
        for(size_t j=0;date[j];j++){
            unsigned char c=(unsigned char)date[j];
            if(!isdigit(c)&&c!='-'&&c!='/'&&c!=':'&&c!='T'&&c!='Z'&&c!='.'&&c!=' ')goto invalid;
        }
        out->items[idx].value=v;out->items[idx].fetched_at=(time_t)stamp;
        snprintf(out->items[idx].source_date,sizeof(out->items[idx].source_date),"%s",date);
        out->items[idx].valid=1;previous=idx;count++;
    }
    if(ferror(f))goto invalid;
    fclose(f);return count?0:1;
invalid:
    fclose(f);return -1;
}
int market_load(MarketData *out){
    char path[PATH_MAX];
    if(!out)return -1;
    memset(out,0,sizeof(*out));
    if(market_path(path,sizeof(path))<0)return -1;
    return market_file(path,out);
}
static int market_validator(const char*path,void*unused){
    MarketData check; (void)unused;
    return market_file(path,&check)==0?0:-1;
}
static int mkdir_parent(const char *path){
    char copy[PATH_MAX];size_t n=strlen(path);
    if(n>=sizeof(copy))return -1;
    memcpy(copy,path,n+1);
    for(size_t i=1;i<n;i++)if(copy[i]=='/'){
        copy[i]=0;
        if(mkdir(copy,0700)<0&&errno!=EEXIST)return -1;
        copy[i]='/';
    }
    return 0;
}
static int market_save(const MarketData *data){
    char path[PATH_MAX],temp[PATH_MAX],err[200];int fd;FILE*f;
    if(market_path(path,sizeof(path))<0||mkdir_parent(path)<0)return -1;
    if(snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path)>=(int)sizeof(temp))return -1;
    fd=mkstemp(temp);if(fd<0)return -1;
    f=fdopen(fd,"w");if(!f){close(fd);unlink(temp);return -1;}
    if(fprintf(f,"INVEST_MARKET\t1\n")<0)goto fail;
    for(int i=0;i<MARKET_COUNT;i++){
        const MarketQuote *q=&data->items[i];
        if(q->valid&&fprintf(f,"%d\t%.10f\t%lld\t%s\n",i,q->value,
                   (long long)q->fetched_at,q->source_date[0]?q->source_date:"-")<0)goto fail;
    }
    if(fflush(f)<0||fsync(fd)<0)goto fail;
    if(fclose(f)<0){unlink(temp);return -1;}
    if(safeio_commit(path,temp,market_validator,NULL,err,sizeof(err))!=0){unlink(temp);return -1;}
    return 0;
fail:
    fclose(f);unlink(temp);return -1;
}
static int curl_get(const char *url,char *output,size_t cap){
    int pipefd[2],status=0;pid_t pid;size_t used=0;int exceeded=0;
    if(!output||cap<2||pipe(pipefd)<0)return -1;
    pid=fork();
    if(pid<0){close(pipefd[0]);close(pipefd[1]);return -1;}
    if(pid==0){
        int devnull=open("/dev/null",O_WRONLY);
        if(devnull>=0){dup2(devnull,STDERR_FILENO);close(devnull);}
        dup2(pipefd[1],STDOUT_FILENO);close(pipefd[0]);close(pipefd[1]);
        execlp("curl","curl","--fail","--silent","--show-error","--location",
               "--connect-timeout","4","--max-time","8","--proto","=https",url,(char*)NULL);
        _exit(127);
    }
    close(pipefd[1]);
    for(;;){
        char chunk[4096];ssize_t n=read(pipefd[0],chunk,sizeof(chunk));
        if(n<0&&errno==EINTR)continue;
        if(n<=0)break;
        if((size_t)n>cap-1-used){exceeded=1;continue;}
        memcpy(output+used,chunk,(size_t)n);used+=(size_t)n;
    }
    close(pipefd[0]);
    while(waitpid(pid,&status,0)<0){if(errno!=EINTR)return -1;}
    output[used]=0;
    if(exceeded||!WIFEXITED(status)||WEXITSTATUS(status)!=0||!used)return -1;
    return 0;
}
static int ptax_url(char*out,size_t cap,const char *ticker){
    time_t now=time(NULL);struct tm end={0},begin={0};char from[32],to[32];
    if(!localtime_r(&now,&end))return -1;
    now-=(time_t)(14*24*60*60);
    if(!localtime_r(&now,&begin))return -1;
    if(!strftime(from,sizeof(from),"%m-%d-%Y",&begin)||!strftime(to,sizeof(to),"%m-%d-%Y",&end))return -1;
    int n=snprintf(out,cap,"https://olinda.bcb.gov.br/olinda/servico/PTAX/versao/v1/odata/"
        "CotacaoMoedaPeriodo(moeda=@moeda,dataInicial=@dataInicial,dataFinalCotacao=@dataFinalCotacao)"
        "?%%40moeda=%%27%s%%27&%%40dataInicial=%%27%s%%27&%%40dataFinalCotacao=%%27%s%%27"
        "&%%24format=json&%%24orderby=dataHoraCotacao%%20desc&%%24top=1",ticker,from,to);
    return n>0&&(size_t)n<cap?0:-1;
}
static int fetch_one(int kind,double*value,char*date,size_t cap){
    char url[1024],body[RESPONSE_MAX];int rc;
    if(kind==MARKET_SELIC){
        snprintf(url,sizeof(url),"https://api.bcb.gov.br/dados/serie/bcdata.sgs.432/dados/ultimos/1?formato=json");
    }else if(kind==MARKET_USD||kind==MARKET_EUR){
        if(ptax_url(url,sizeof(url),kind==MARKET_USD?"USD":"EUR")<0)return -1;
    }else if(kind==MARKET_BTC||kind==MARKET_XMR){
        snprintf(url,sizeof(url),"https://api.coinpaprika.com/v1/tickers/%s?quotes=BRL",
                 kind==MARKET_BTC?"btc-bitcoin":"xmr-monero");
    }else return -1;
    if(curl_get(url,body,sizeof(body))<0)return -1;
    if(kind==MARKET_SELIC)rc=market_parse_selic(body,value,date,cap);
    else if(kind==MARKET_USD||kind==MARKET_EUR)rc=market_parse_ptax(body,value,date,cap);
    else rc=market_parse_crypto(body,value,date,cap);
    return rc;
}
int market_refresh(void){
    MarketData data={0};int changed=0;
    if(market_load(&data)<0)return -1; /* Do not overwrite damaged data. */
    for(int i=0;i<MARKET_COUNT;i++){
        double value;char date[32]={0};
        if(fetch_one(i,&value,date,sizeof(date))<0)continue;
        data.items[i].value=value;
        data.items[i].fetched_at=time(NULL);
        data.items[i].valid=1;
        snprintf(data.items[i].source_date,sizeof(data.items[i].source_date),"%s",date[0]?date:"-");
        changed++;
    }
    return changed ? market_save(&data) : -1;
}
time_t market_latest(const MarketData*data){
    time_t latest=0;
    if(!data)return 0;
    for(int i=0;i<MARKET_COUNT;i++)if(data->items[i].valid&&data->items[i].fetched_at>latest)
        latest=data->items[i].fetched_at;
    return latest;
}

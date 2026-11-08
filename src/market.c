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
/* rate.sx returns a text table (not JSON). The qT query disables ANSI
 * control sequences. Require the exact Price (USD) or Price (BRL) header
 * as appropriate. Columns are Rank | Coin | Price | ... . */
static size_t table_bar(const char *p, const char *end){
    if(p<end && *p=='|')return 1;
    if(end-p>=3 && (unsigned char)p[0]==0xe2 &&
       (unsigned char)p[1]==0x94 && (unsigned char)p[2]==0x82)return 3;
    return 0;
}
static void trim_field(const char **begin,const char **end){
    while(*begin<*end && isspace((unsigned char)**begin))++*begin;
    while(*end>*begin && isspace((unsigned char)(*end)[-1]))--*end;
}
static int parse_rate_sx_currency(const char *body,const char *symbol,const char *currency,double *out){
    const char *line;
    char header[24];
    if(!body||!symbol||!out||!currency)return -1;
    if(snprintf(header,sizeof(header),"Price (%s)",currency)<0 ||
       !strstr(body,header)||!strstr(body,"Coin"))return -1;
    if(strcmp(symbol,"BTC")&&strcmp(symbol,"XMR"))return -1;
    line=body;
    while(*line){
        const char *end=strchr(line,'\n');
        const char *p, *begin=NULL;
        char coin[10]={0};double price=0;int col=0,have_price=0;
        if(!end)end=line+strlen(line);
        for(p=line;p<end;){
            size_t sep=table_bar(p,end);
            if(sep){
                if(begin){
                    const char *a=begin,*b=p;
                    trim_field(&a,&b);
                    col++;
                    if(col==2 && b-a>0 && b-a<(ptrdiff_t)sizeof(coin)){
                        memcpy(coin,a,(size_t)(b-a));coin[b-a]=0;
                    }
                    if(col==3 && a<b && b-a<64){
                        char value[64],*tail;
                        memcpy(value,a,(size_t)(b-a));value[b-a]=0;
                        errno=0;price=strtod(value,&tail);
                        have_price=(tail!=value && !*tail && errno==0 &&
                                    isfinite(price)&&price>0&&price<1e15);
                    }
                }
                begin=p+sep;p+=sep;
            }else p++;
        }
        if(!strcmp(coin,symbol)){
            if(!have_price)return -1;
            *out=price;return 0;
        }
        line= *end ? end+1 : end;
    }
    return -1;
}
int market_parse_rate_sx(const char *body,const char *symbol,double *out){
    return parse_rate_sx_currency(body,symbol,"USD",out);
}
int market_parse_rate_sx_brl(const char *body,const char *symbol,double *out){
    return parse_rate_sx_currency(body,symbol,"BRL",out);
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
/* Individual error categories make an online-but-unreachable provider distinguishable
 * from a machine with no internet. Never disable TLS certificate checks. */
static MarketFetchCode curl_code(int exit_code){
    switch(exit_code){
        case 127: return MARKET_FETCH_NO_CURL;
        case 6: return MARKET_FETCH_DNS;
        case 60: case 51: return MARKET_FETCH_TLS;
        case 22: return MARKET_FETCH_HTTP;
        case 28: return MARKET_FETCH_TIMEOUT;
        case 5: case 7: case 35: case 52: case 56: return MARKET_FETCH_CONNECTION;
        default: return MARKET_FETCH_OTHER;
    }
}
const char *market_fetch_error(MarketFetchCode code){
    switch(code){
        case MARKET_FETCH_OK: return "OK";
        case MARKET_FETCH_NO_CURL: return "curl não instalado/não encontrado no PATH";
        case MARKET_FETCH_DNS: return "DNS: servidor não encontrado";
        case MARKET_FETCH_TLS: return "certificado/TLS HTTPS";
        case MARKET_FETCH_HTTP: return "HTTP: API recusou a consulta ou está indisponível";
        case MARKET_FETCH_TIMEOUT: return "tempo limite da conexão";
        case MARKET_FETCH_CONNECTION: return "não foi possível estabelecer a conexão";
        case MARKET_FETCH_PARSE: return "resposta inesperada do fornecedor";
        case MARKET_FETCH_CONVERSION: return "USD/BRL (PTAX) indisponível para conversão";
        case MARKET_FETCH_SAVE: return "não foi possível salvar o cache";
        default: return "outra falha de rede/curl";
    }
}
static int curl_get(const char *url,char *output,size_t cap,MarketFetchCode *error){
    int pipefd[2],status=0;pid_t pid;size_t used=0;int exceeded=0;
    *error=MARKET_FETCH_OTHER;
    if(!output||cap<2||pipe(pipefd)<0)return -1;
    pid=fork();
    if(pid<0){close(pipefd[0]);close(pipefd[1]);return -1;}
    if(pid==0){
        int devnull=open("/dev/null",O_WRONLY);
        if(devnull>=0){dup2(devnull,STDERR_FILENO);close(devnull);}
        dup2(pipefd[1],STDOUT_FILENO);close(pipefd[0]);close(pipefd[1]);
        execlp("curl","curl","--fail","--silent","--show-error","--location",
               "--connect-timeout","6","--max-time","12","--proto","=https",
               "--proto-redir","=https",url,(char*)NULL);
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
    if(!WIFEXITED(status))return -1;
    if(WEXITSTATUS(status)!=0){*error=curl_code(WEXITSTATUS(status));return -1;}
    if(exceeded||!used){*error=MARKET_FETCH_PARSE;return -1;}
    *error=MARKET_FETCH_OK;
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
static int fetch_one(int kind,double*value,char*date,size_t cap,MarketFetchCode*error){
    char url[1024],body[RESPONSE_MAX];int rc;
    *error=MARKET_FETCH_OTHER;
    if(kind==MARKET_SELIC){
        snprintf(url,sizeof(url),"https://api.bcb.gov.br/dados/serie/bcdata.sgs.432/dados/ultimos/1?formato=json");
    }else if(kind==MARKET_USD||kind==MARKET_EUR){
        if(ptax_url(url,sizeof(url),kind==MARKET_USD?"USD":"EUR")<0)return -1;
    }else return -1;
    if(curl_get(url,body,sizeof(body),error)<0)return -1;
    if(kind==MARKET_SELIC)rc=market_parse_selic(body,value,date,cap);
    else rc=market_parse_ptax(body,value,date,cap);
    if(rc<0){*error=MARKET_FETCH_PARSE;return -1;}
    *error=MARKET_FETCH_OK;
    return 0;
}

/* Small, separate diagnostic state. Failed feeds never delete good quotes.
 * File contains only fixed numeric error categories, not URLs or private data. */
static int status_path(char *dest,size_t cap){
    char cache[PATH_MAX];size_t n;
    if(market_path(cache,sizeof(cache))<0)return -1;
    n=strlen(cache);
    if(n<strlen("mercado.tsv")||strcmp(cache+n-strlen("mercado.tsv"),"mercado.tsv"))return -1;
    cache[n-strlen("mercado.tsv")]=0;
    return snprintf(dest,cap,"%smercado-estado.tsv",cache)<(int)cap?0:-1;
}
static int status_save(const MarketDiagnostics *diag){
    char path[PATH_MAX],temp[PATH_MAX];int fd;FILE *f;
    if(status_path(path,sizeof(path))<0||mkdir_parent(path)<0||
       snprintf(temp,sizeof(temp),"%s.tmp.XXXXXX",path)>=(int)sizeof(temp))return -1;
    fd=mkstemp(temp);if(fd<0)return -1;
    f=fdopen(fd,"w");if(!f){close(fd);unlink(temp);return -1;}
    if(fprintf(f,"INVEST_MARKET_STATUS\t1\n%d\n",diag->updated)<0)goto fail;
    for(int i=0;i<MARKET_COUNT;i++)if(fprintf(f,"%d\t%d\n",i,(int)diag->errors[i])<0)goto fail;
    if(fflush(f)!=0||fsync(fd)!=0)goto fail;
    if(fclose(f)!=0){unlink(temp);return -1;}
    if(rename(temp,path)!=0){unlink(temp);return -1;}
    return 0;
fail:
    fclose(f);unlink(temp);return -1;
}
int market_diagnostics_load(MarketDiagnostics *diag){
    char path[PATH_MAX],line[128];FILE *f;int count;
    if(!diag||status_path(path,sizeof(path))<0)return -1;
    memset(diag,0,sizeof(*diag));
    f=fopen(path,"r");if(!f)return -1;
    if(!fgets(line,sizeof(line),f)||strcmp(line,"INVEST_MARKET_STATUS\t1\n"))goto fail;
    if(!fgets(line,sizeof(line),f)||sscanf(line,"%d",&count)!=1||count<0||count>MARKET_COUNT)goto fail;
    diag->updated=count;
    for(int i=0;i<MARKET_COUNT;i++){
        int idx,code;
        if(!fgets(line,sizeof(line),f)||sscanf(line,"%d\t%d",&idx,&code)!=2||idx!=i||code<0||code>MARKET_FETCH_CONVERSION)goto fail;
        diag->errors[i]=(MarketFetchCode)code;
    }
    if(fgets(line,sizeof(line),f))goto fail;
    fclose(f);return 0;
fail:
    fclose(f);return -1;
}
void market_diagnostics_summary(const MarketDiagnostics *diag,char *out,size_t cap){
    int first=-1;
    if(!out||!cap)return;
    if(!diag){snprintf(out,cap,"Diagnóstico indisponível");return;}
    if(diag->updated==MARKET_COUNT){snprintf(out,cap,"Todos os 5 indicadores atualizados");return;}
    for(int i=0;i<MARKET_COUNT;i++)if(diag->errors[i]!=MARKET_FETCH_OK){first=i;break;}
    if(first<0){snprintf(out,cap,"Falha no salvamento dos indicadores");return;}
    if(diag->updated>0)snprintf(out,cap,"Atualização parcial (%d/5): %s",diag->updated,market_fetch_error(diag->errors[first]));
    else snprintf(out,cap,"Sem novos dados: %s",market_fetch_error(diag->errors[first]));
}
int market_refresh_detailed(MarketDiagnostics *diag){
    MarketData data={0};MarketDiagnostics state={0};int saved=0;
    if(market_load(&data)<0){
        for(int i=0;i<MARKET_COUNT;i++)state.errors[i]=MARKET_FETCH_SAVE;
        (void)status_save(&state);
        if(diag)*diag=state;
        return -1; /* Do not overwrite damaged data. */
    }
    /* Keep the BCB feeds independent of rate.sx availability. */
    for(int i=MARKET_SELIC;i<=MARKET_EUR;i++){
        double value;char date[32]={0};MarketFetchCode code;
        if(fetch_one(i,&value,date,sizeof(date),&code)<0){state.errors[i]=code;continue;}
        data.items[i].value=value;
        data.items[i].fetched_at=time(NULL);
        data.items[i].valid=1;
        snprintf(data.items[i].source_date,sizeof(data.items[i].source_date),"%s",date[0]?date:"-");
        state.errors[i]=MARKET_FETCH_OK;
        state.updated++;
    }
    /* Prefer rate.sx in BRL directly, but do not trust a silent fallback
     * to USD: validate the exact header. If BRL is unavailable, obtain the
     * USD table from rate.sx and convert via a fresh BCB PTAX reference.
     * Never convert with an old cached FX value. */
    {
        char body[RESPONSE_MAX];MarketFetchCode code=MARKET_FETCH_OTHER;
        int direct=(curl_get("https://brl.rate.sx/?qT&n=80",body,sizeof(body),&code)==0);
        MarketFetchCode direct_error=direct?MARKET_FETCH_PARSE:code;
        for(int i=MARKET_BTC;i<=MARKET_XMR;i++){
            double brl;
            if(direct && market_parse_rate_sx_brl(body,i==MARKET_BTC?"BTC":"XMR",&brl)==0){
                data.items[i].value=brl;
                data.items[i].fetched_at=time(NULL);
                data.items[i].valid=1;
                snprintf(data.items[i].source_date,sizeof(data.items[i].source_date),"%s","-");
                state.errors[i]=MARKET_FETCH_OK;
                state.updated++;
            }else state.errors[i]=direct_error;
        }
        if(state.errors[MARKET_BTC]!=MARKET_FETCH_OK||
           state.errors[MARKET_XMR]!=MARKET_FETCH_OK){
            if(state.errors[MARKET_USD]!=MARKET_FETCH_OK){
                for(int i=MARKET_BTC;i<=MARKET_XMR;i++)
                    if(state.errors[i]!=MARKET_FETCH_OK && direct)
                        state.errors[i]=MARKET_FETCH_CONVERSION;
            }else{
                MarketFetchCode fallback_err=MARKET_FETCH_OTHER;
                if(curl_get("https://rate.sx/?qT&n=80",body,sizeof(body),&fallback_err)<0){
                    for(int i=MARKET_BTC;i<=MARKET_XMR;i++)
                        if(state.errors[i]!=MARKET_FETCH_OK)state.errors[i]=fallback_err;
                }else for(int i=MARKET_BTC;i<=MARKET_XMR;i++){
                    double usd,brl;
                    if(state.errors[i]==MARKET_FETCH_OK)continue;
                    if(market_parse_rate_sx(body,i==MARKET_BTC?"BTC":"XMR",&usd)!=0 ||
                       !isfinite(brl=usd*data.items[MARKET_USD].value) || brl<=0 || brl>=1e15){
                        state.errors[i]=MARKET_FETCH_PARSE;
                        continue;
                    }
                    data.items[i].value=brl;
                    data.items[i].fetched_at=time(NULL);
                    data.items[i].valid=1;
                    snprintf(data.items[i].source_date,sizeof(data.items[i].source_date),"%s","-");
                    state.errors[i]=MARKET_FETCH_OK;
                    state.updated++;
                }
            }
        }
    }
    if(state.updated>0){
        if(market_save(&data)!=0){
            for(int i=0;i<MARKET_COUNT;i++)if(state.errors[i]==MARKET_FETCH_OK)state.errors[i]=MARKET_FETCH_SAVE;
            state.updated=0;
        }else saved=1;
    }
    (void)status_save(&state);
    if(diag)*diag=state;
    return saved ? 0 : -1;
}
int market_refresh(void){return market_refresh_detailed(NULL);}

time_t market_latest(const MarketData*data){
    time_t latest=0;
    if(!data)return 0;
    for(int i=0;i<MARKET_COUNT;i++)
        if(data->items[i].valid&&data->items[i].fetched_at>latest)
            latest=data->items[i].fetched_at;
    return latest;
}

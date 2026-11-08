#define _POSIX_C_SOURCE 200809L
#include "market.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int close_enough(double a,double b){return fabs(a-b)<0.00001;}
int main(void){
    double v=0;char date[32];
    assert(market_parse_selic("[{\"data\":\"07/11/2026\",\"valor\":\"12.75\"}]",&v,date,sizeof(date))==0);
    assert(close_enough(v,12.75)); assert(!strcmp(date,"07/11/2026"));
    assert(market_parse_selic("<html>error</html>",&v,date,sizeof(date))<0);
    assert(market_parse_selic("[{\"data\":\"07/11/2026\",\"valor\":\"nan\"}]",&v,date,sizeof(date))<0);
    assert(market_parse_ptax("{\"value\":[{\"cotacaoCompra\":5.001,\"cotacaoVenda\":5.0123,\"dataHoraCotacao\":\"2026-11-06 13:10:10.000\"}]}",&v,date,sizeof(date))==0);
    assert(close_enough(v,5.0123));assert(!strcmp(date,"2026-11-06 13:10:10.000"));
    assert(market_parse_ptax("{\"value\":[]}",&v,date,sizeof(date))<0);
    const char *rates=
      "Market Cap: $2,500,000,000,000\n"
      "│ Rank │ Coin │ Price (USD) │ Change (24H) │\n"
      "│ 1    │ BTC  │ 75683.0     │ -0.10%       │\n"
      "│ 2    │ ETH  │ 2227.62     │  0.00%       │\n"
      "│ 11   │ XMR  │ 599.125     │ +2.00%       │\n"
      "2026-11-08 17:52:09 UTC\n";
    assert(market_parse_rate_sx_brl(rates,"BTC",&v)<0);
    assert(market_parse_rate_sx_brl("│ Rank │ Coin │ Price (BRL) │\n│ 1 │ BTC │ 420000.50 │\n","BTC",&v)==0 && close_enough(v,420000.5));
    assert(market_parse_rate_sx(rates,"BTC",&v)==0 && close_enough(v,75683));
    assert(market_parse_rate_sx(rates,"XMR",&v)==0 && close_enough(v,599.125));
    assert(market_parse_rate_sx(rates,"ADA",&v)<0);
    assert(market_parse_rate_sx("<html>error</html>","BTC",&v)<0);
    assert(market_parse_rate_sx("│ Rank │ Coin │ Price (BRL) │\n│ 1 │ BTC │ 250000 │\n","BTC",&v)<0);
    assert(market_parse_rate_sx("│ Rank │ Coin │ Price (USD) │\n│ 1 │ BTC │ -1 │\n","BTC",&v)<0);
    assert(market_parse_rate_sx("│ Rank │ Coin │ Price (USD) │\n│ 1 │ BTC │ NaN │\n","BTC",&v)<0);
    assert(market_parse_rate_sx("│ Rank │ Coin │ Price (USD) │\n│ 1 │ BTC │ 2%;bad │\n","BTC",&v)<0);
    assert(market_parse_rate_sx("| Rank | Coin | Price (USD) |\n| 2 | XMR | 555.5 |\n","XMR",&v)==0 && close_enough(v,555.5));
    char tmp[]="/tmp/invest-market-tests-XXXXXX";assert(mkdtemp(tmp));
    assert(setenv("XDG_DATA_HOME",tmp,1)==0);
    MarketData m;assert(market_load(&m)==1);assert(!market_latest(&m));
    char path[1024],folder[1024];snprintf(folder,sizeof(folder),"%s/invest",tmp);
    assert(mkdir(folder,0700)==0);assert(strlen(folder)+strlen("/mercado.tsv")<sizeof(path));strcpy(path,folder);strcat(path,"/mercado.tsv");
    FILE *f=fopen(path,"w");assert(f);
    assert(fputs("INVEST_MARKET\t1\n0\t12.5000000000\t1794000000\t07/11/2026\n3\t450000.0000000000\t1794000001\t2026-11-07T12:00:00Z\n",f)>=0);
    fclose(f);
    assert(market_load(&m)==0);
    assert(m.items[MARKET_SELIC].valid&&close_enough(m.items[MARKET_SELIC].value,12.5));
    assert(!m.items[MARKET_EUR].valid);
    assert(market_latest(&m)==1794000001);
    f=fopen(path,"a");assert(f);fputs("0\t99\t1794000002\tduplicate\n",f);fclose(f);
    assert(market_load(&m)<0);
    unlink(path);rmdir(folder);rmdir(tmp);
    puts("Mercado: testes de API, cache e validacao OK");
    return 0;
}

/* Standalone market diagnostics: no X11, no Cairo, no account credentials. */
#include "market.h"
#include <stdio.h>
#include <stdlib.h>

int main(void){
    static const char*name[MARKET_COUNT]={"Selic (BCB)","USD PTAX (BCB)","EUR PTAX (BCB)","BTC (rate.sx)","XMR (rate.sx)"};
    MarketDiagnostics diag;
    int rc;
    puts("invest-market-check — diagnóstico de dados de mercado\n");
    rc=market_refresh_detailed(&diag);
    for(int i=0;i<MARKET_COUNT;i++){
        printf("  %-20s  %s\n",name[i],market_fetch_error(diag.errors[i]));
    }
    printf("\nAtualizados nesta tentativa: %d de %d\n",diag.updated,MARKET_COUNT);
    puts("Observação: disponibilidade de internet não garante acesso a cada API.");
    puts("Certificados HTTPS continuam sendo verificados; nenhuma opção insegura foi usada.");
    return rc==0?0:1;
}

#include "utf8.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_accent(void){
    char t[128]="Cenario";size_t at=strlen(t);int select=0;
    /* XIM: o acento e emitido como um ponto UTF-8 completo. */
    assert(utf8_insert(t,sizeof(t),&at,&select,"á")==0);
    assert(utf8_valid(t)&&strcmp(t,"Cenarioá")==0);
    assert(at==strlen(t));
    assert(utf8_erase(t,&at,&select,1)==1);
    assert(!strcmp(t,"Cenario")&&at==strlen(t));
    /* Composicao Unicode: e + acento combinante tem dois pontos Unicode. */
    assert(utf8_insert(t,sizeof(t),&at,&select,"e\xcc\x81")==0);
    assert(utf8_valid(t));
    assert(utf8_prev_grapheme(t,at)==strlen("Cenario"));
    assert(utf8_erase(t,&at,&select,1)==1);
    assert(!strcmp(t,"Cenario"));
    /* Cursor e Delete nao podem remover apenas parte de um caractere. */
    assert(utf8_insert(t,sizeof(t),&at,&select," çãõ")==0);
    at=strlen("Cenario ");
    assert(utf8_erase(t,&at,&select,0)==1);
    assert(!strcmp(t,"Cenario ãõ"));
    assert(utf8_valid(t));
}
static void test_bad_data(void){
    char bad[]={ 'a',(char)0xC3,0 }; /* sequencia interrompida */
    char cleaned[32],t[32]="Seguro";size_t at=6;int all=0;
    assert(!utf8_valid(bad));
    assert(!utf8_valid("\xC0\xAF")); /* overlong */
    assert(!utf8_valid("\xED\xA0\x80")); /* surrogate */
    assert(!utf8_valid("\xF4\x90\x80\x80")); /* U+110000 */
    assert(utf8_insert(t,sizeof(t),&at,&all,bad)<0);
    assert(!strcmp(t,"Seguro")&&at==6);
    assert(utf8_clean(cleaned,sizeof(cleaned),bad)==2);
    assert(!strcmp(cleaned,"a?")&&utf8_valid(cleaned));
    assert(utf8_clean(cleaned,3,"áé")==2); /* nao corta caracteres */
    assert(!strcmp(cleaned,"á"));
}
static void test_selection(void){
    char t[32]="Original";size_t at=strlen(t);int all=1;
    /* Sem texto composto, tecla de acento nao deve apagar a selecao. */
    assert(utf8_insert(t,sizeof(t),&at,&all,"")==0);
    assert(all==1 && !strcmp(t,"Original"));
    assert(utf8_insert(t,sizeof(t),&at,&all,"Éxito")==0);
    assert(!strcmp(t,"Éxito")&&utf8_valid(t));
    assert(utf8_erase(t,&at,&all,1)==1);
    assert(!strcmp(t,"Éxit"));
    all=1;
    assert(utf8_erase(t,&at,&all,1)==1);
    assert(!strcmp(t,"")&&at==0);
}
int main(void){
    test_accent();test_bad_data();test_selection();
    puts("test_utf8: OK (acento, Backspace, selecao, dados invalidos)");return 0;
}

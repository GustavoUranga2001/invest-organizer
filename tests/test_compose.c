#include "compose.h"
#include "utf8.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void check(char mark,char base,const char *expected){
    char out[16];
    assert(compose_letter(mark,base,out,sizeof(out))==1);
    assert(strcmp(out,expected)==0);
    assert(utf8_valid(out));
}
static void test_portuguese_and_others(void){
    check('~','a',"ã");check('~','A',"Ã");check('~','o',"õ");check('~','O',"Õ");
    check('\'','a',"á");check('\'','e',"é");check('\'','i',"í");
    check('\'','o',"ó");check('\'','u',"ú");check('`','a',"à");
    check('^','a',"â");check('^','e',"ê");check('^','o',"ô");
    check(',','c',"ç");check(',','C',"Ç");
    check('"','u',"ü");check('"','U',"Ü");check('~','n',"ñ");
    check('o','a',"å");
    char out[16];
    assert(compose_letter('~','x',out,sizeof(out))==0);
    assert(compose_letter('~','a',out,2)==0);
}
static void test_insertion(void){
    char pending=0,output[128],name[128]="Simulacao ";
    size_t cursor=strlen(name);int all=0;
    assert(compose_feed(&pending,"~",output,sizeof(output))==0 && pending=='~');
    assert(compose_feed(&pending,"a",output,sizeof(output))==1);
    assert(!strcmp(output,"ã") && pending==0);
    assert(utf8_insert(name,sizeof(name),&cursor,&all,output)==0);
    assert(!strcmp(name,"Simulacao ã"));
    assert(utf8_erase(name,&cursor,&all,1)==1);
    assert(!strcmp(name,"Simulacao "));
    /* Dead acute -> i; dead cedilla -> c; precomposed XIM output. */
    pending='\'';
    assert(compose_feed(&pending,"i",output,sizeof(output))==1 && !strcmp(output,"í"));
    pending=',';
    assert(compose_feed(&pending,"c",output,sizeof(output))==1 && !strcmp(output,"ç"));
    pending='~';
    assert(compose_feed(&pending,"ã",output,sizeof(output))==1 && !strcmp(output,"ã"));
    assert(pending==0);
    /* Raw ~~ or tilde then space is a deliberate literal tilde. */
    assert(compose_feed(&pending,"~",output,sizeof(output))==0);
    assert(compose_feed(&pending,"~",output,sizeof(output))==1 && !strcmp(output,"~"));
    assert(compose_feed(&pending,"~",output,sizeof(output))==0);
    assert(compose_feed(&pending," ",output,sizeof(output))==1 && !strcmp(output,"~"));
    /* Uncomposable pairing is preserved without losing the second character. */
    assert(compose_feed(&pending,"~",output,sizeof(output))==0);
    assert(compose_feed(&pending,"x",output,sizeof(output))==1 && !strcmp(output,"~x"));
    assert(compose_feed(&pending,"ó",output,sizeof(output))==1 && !strcmp(output,"ó"));
    /* Spacing accents may be emitted even with no XIM. */
    assert(compose_feed(&pending,"´",output,sizeof(output))==0);
    assert(compose_feed(&pending,"a",output,sizeof(output))==1 && !strcmp(output,"á"));
    assert(compose_feed(&pending,"˜",output,sizeof(output))==0);
    assert(compose_feed(&pending,"o",output,sizeof(output))==1 && !strcmp(output,"õ"));
    assert(compose_feed(&pending,"¨",output,sizeof(output))==0);
    assert(compose_feed(&pending,"u",output,sizeof(output))==1 && !strcmp(output,"ü"));
    assert(compose_feed(&pending,"¸",output,sizeof(output))==0);
    assert(compose_feed(&pending,"c",output,sizeof(output))==1 && !strcmp(output,"ç"));
}
static void test_normalization(void){
    char result[128];
    assert(compose_normalize(result,sizeof(result),"a\xcc\x83 o\xcc\x83 i\xcc\x81 c\xcc\xa7") == 0);
    assert(!strcmp(result,"ã õ í ç"));
    assert(compose_normalize(result,sizeof(result),"Dívida, ação, informação, vôo")==0);
    assert(!strcmp(result,"Dívida, ação, informação, vôo"));
    assert(compose_normalize(result,3,"ãõ")==-1);
    assert(compose_normalize(result,sizeof(result),"\xc3")==-1);
}
int main(void){
    test_portuguese_and_others();test_insertion();test_normalization();
    puts("test_compose: OK (acentos, tilde, cedilha, XIM, NFC, Backspace)");
    return 0;
}

#include "compose.h"
#include "utf8.h"
#include <string.h>

/* Compact Latin-1 and Latin Extended-A mapping for common dead keys.
 * UTF-8 literals avoid nonportable character-set assumptions. */
typedef struct { char base,mark; const char *result; } ComposePair;
static const ComposePair pairs[] = {
    {'a','\'',"á"},{'A','\'',"Á"},{'e','\'',"é"},{'E','\'',"É"},
    {'i','\'',"í"},{'I','\'',"Í"},{'o','\'',"ó"},{'O','\'',"Ó"},
    {'u','\'',"ú"},{'U','\'',"Ú"},{'y','\'',"ý"},{'Y','\'',"Ý"},
    {'c','\'',"ć"},{'C','\'',"Ć"},{'n','\'',"ń"},{'N','\'',"Ń"},
    {'s','\'',"ś"},{'S','\'',"Ś"},{'z','\'',"ź"},{'Z','\'',"Ź"},
    {'a','`',"à"},{'A','`',"À"},{'e','`',"è"},{'E','`',"È"},
    {'i','`',"ì"},{'I','`',"Ì"},{'o','`',"ò"},{'O','`',"Ò"},
    {'u','`',"ù"},{'U','`',"Ù"},
    {'a','^',"â"},{'A','^',"Â"},{'e','^',"ê"},{'E','^',"Ê"},
    {'i','^',"î"},{'I','^',"Î"},{'o','^',"ô"},{'O','^',"Ô"},
    {'u','^',"û"},{'U','^',"Û"},
    {'a','~',"ã"},{'A','~',"Ã"},{'o','~',"õ"},{'O','~',"Õ"},
    {'n','~',"ñ"},{'N','~',"Ñ"},{'i','~',"ĩ"},{'I','~',"Ĩ"},
    {'u','~',"ũ"},{'U','~',"Ũ"},{'e','~',"ẽ"},{'E','~',"Ẽ"},
    {'a','"',"ä"},{'A','"',"Ä"},{'e','"',"ë"},{'E','"',"Ë"},
    {'i','"',"ï"},{'I','"',"Ï"},{'o','"',"ö"},{'O','"',"Ö"},
    {'u','"',"ü"},{'U','"',"Ü"},{'y','"',"ÿ"},{'Y','"',"Ÿ"},
    {'c',',',"ç"},{'C',',',"Ç"},{'s',',',"ş"},{'S',',',"Ş"},
    {'a','o',"å"},{'A','o',"Å"},{'u','o',"ů"},{'U','o',"Ů"},
};
int compose_letter(char mark,char base,char*out,size_t cap){
    size_t i;
    if(!out||!cap)return 0;
    for(i=0;i<sizeof(pairs)/sizeof(pairs[0]);i++){
        if(pairs[i].mark==mark&&pairs[i].base==base){
            size_t n=strlen(pairs[i].result);
            if(n+1>cap)return 0;
            memcpy(out,pairs[i].result,n+1);
            return 1;
        }
    }
    return 0;
}
/* Unicode combining marks U+0301 / 0300 / 0302 / 0303 / 0308 / 0327 / 030A.
 * Normalize common ASCII letter + mark combinations without a heavy library. */
static char combining_mark(const unsigned char *p,size_t left){
    if(left<2)return 0;
    if(p[0]==0xCC){
        switch(p[1]){
        case 0x80:return '`';case 0x81:return '\'';
        case 0x82:return '^';case 0x83:return '~';
        case 0x88:return '"';case 0x8A:return 'o';
        default:break;
        }
    }
    if(p[0]==0xCC && p[1]==0xA7)return ',';
    return 0;
}
int compose_normalize(char*out,size_t cap,const char*input){
    size_t i,j=0,n;
    if(!out||!input||cap==0||!utf8_valid(input))return -1;
    n=strlen(input);
    for(i=0;i<n;){
        if((unsigned char)input[i]<0x80 && i+2<n){
            char mark=combining_mark((const unsigned char*)input+i+1,n-i-1);
            char composed[8];
            if(mark && compose_letter(mark,input[i],composed,sizeof(composed))){
                size_t l=strlen(composed);
                if(j+l>=cap)return -1;
                memcpy(out+j,composed,l);j+=l;i+=3;continue;
            }
        }
        /* Preserve complete UTF-8 codepoints on overflow. */
        size_t count=1;
        unsigned char b=(unsigned char)input[i];
        if(b>=0xF0)count=4;else if(b>=0xE0)count=3;else if(b>=0xC0)count=2;
        if(j+count>=cap)return -1;
        memcpy(out+j,input+i,count);j+=count;i+=count;
    }
    out[j]=0;return 0;
}

/* Text-field fallback: dead accent followed by letter becomes a single glyph.
 * Also handles literal ~ followed by a on layouts where ~ is not a dead key.
 * Return 1 for output, 0 if still waiting, -1 for invalid input. */
static char input_mark(const char *s){
    if(s[0] && !s[1]){
        char c=s[0];
        if(c=='~'||c=='\''||c=='`'||c=='^'||c=='"')return c;
    }
    /* Some keyboard layouts emit spacing accents instead of dead keysyms. */
    if(!strcmp(s,"´"))return '\'';
    if(!strcmp(s,"˜"))return '~';
    if(!strcmp(s,"¨"))return '"';
    if(!strcmp(s,"¸"))return ',';
    return 0;
}
int compose_feed(char *pending,const char *input,char *out,size_t cap){
    char tmp[32];size_t n;
    if(!pending||!input||!out||cap==0||!utf8_valid(input))return -1;
    n=strlen(input);
    if(n==0){out[0]=0;return 0;}
    char mark=input_mark(input);
    if(mark){
        if(!*pending){*pending=mark;out[0]=0;return 0;}
        if(cap<2)return -1;
        if(*pending==mark){
            out[0]=*pending;out[1]=0;*pending=0;
            return 1;
        }
        out[0]=*pending;out[1]=0;*pending=mark;
        return 1;
    }
    if(*pending){
        char mark=*pending;
        *pending=0;
        if(n==1 && compose_letter(mark,input[0],tmp,sizeof(tmp)))
            return compose_normalize(out,cap,tmp)==0?1:-1;
        if(n==1 && input[0]==' '){
            if(cap<2)return -1;
            out[0]=mark;out[1]=0;return 1;
        }
        /* XIM might have emitted a full accented character already. */
        if((unsigned char)input[0]>=0x80)
            return compose_normalize(out,cap,input)==0?1:-1;
        if(n+2>cap)return -1;
        out[0]=mark;memcpy(out+1,input,n+1);return 1;
    }
    return compose_normalize(out,cap,input)==0?1:-1;
}

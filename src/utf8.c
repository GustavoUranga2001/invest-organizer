#include "utf8.h"
#include <stdint.h>
#include <string.h>

/* Quantidade de bytes de um ponto Unicode valido. Zero = sequência incorreta.
 * Rejeita overlong, surrogates, codepoints acima de U+10FFFF e truncamentos. */
static size_t decode(const unsigned char *p,size_t left,uint32_t *cp){
    unsigned char a,b,c,d;
    if(!left)return 0;
    a=p[0];
    if(a<0x80){*cp=a;return 1;}
    if(a<0xC2||a>0xF4||left<2)return 0;
    b=p[1];if(b<0x80||b>0xBF)return 0;
    if(a<=0xDF){*cp=((uint32_t)(a&0x1F)<<6)|(b&0x3F);return 2;}
    if(left<3)return 0;
    c=p[2];if(c<0x80||c>0xBF)return 0;
    if(a==0xE0&&b<0xA0)return 0;
    if(a==0xED&&b>0x9F)return 0;
    if(a<=0xEF){*cp=((uint32_t)(a&15)<<12)|((uint32_t)(b&63)<<6)|(c&63);return 3;}
    if(left<4)return 0;
    d=p[3];if(d<0x80||d>0xBF)return 0;
    if(a==0xF0&&b<0x90)return 0;
    if(a==0xF4&&b>0x8F)return 0;
    *cp=((uint32_t)(a&7)<<18)|((uint32_t)(b&63)<<12)|((uint32_t)(c&63)<<6)|(d&63);
    return 4;
}
int utf8_valid(const char *s){
    size_t i=0,n;uint32_t cp;
    if(!s)return 0;
    n=strlen(s);
    while(i<n){size_t take=decode((const unsigned char*)s+i,n-i,&cp);if(!take)return 0;i+=take;}
    return 1;
}
size_t utf8_clean(char *dst,size_t capacity,const char *src){
    size_t i=0,j=0,n;
    uint32_t cp;
    if(!dst||!capacity)return 0;
    if(!src){dst[0]=0;return 0;}
    n=strlen(src);
    while(i<n){
        size_t take=decode((const unsigned char*)src+i,n-i,&cp);
        if(!take){if(j+1>=capacity)break;dst[j++]='?';i++;continue;}
        if(j+take>=capacity)break;
        memcpy(dst+j,src+i,take);j+=take;i+=take;
    }
    dst[j]=0;return j;
}
size_t utf8_prev(const char *s,size_t cursor){
    if(!s)return 0;
    size_t n=strlen(s);if(cursor>n)cursor=n;
    if(!cursor)return 0;
    --cursor;
    while(cursor>0&&(((unsigned char)s[cursor]&0xC0)==0x80))--cursor;
    return cursor;
}
size_t utf8_next(const char *s,size_t cursor){
    if(!s)return 0;
    size_t n=strlen(s);if(cursor>=n)return n;
    uint32_t cp;size_t take=decode((const unsigned char*)s+cursor,n-cursor,&cp);
    return cursor+(take?take:1);
}
static int is_combining(uint32_t cp){
    return (cp>=0x0300&&cp<=0x036F)||(cp>=0x1AB0&&cp<=0x1AFF)||
           (cp>=0x1DC0&&cp<=0x1DFF)||(cp>=0x20D0&&cp<=0x20FF)||
           (cp>=0xFE20&&cp<=0xFE2F);
}
size_t utf8_prev_grapheme(const char *s,size_t cursor){
    size_t at=cursor;
    if(!at)return 0;
    do{
        uint32_t cp=0;
        at=utf8_prev(s,at);
        (void)decode((const unsigned char*)s+at,strlen(s)-at,&cp);
        if(!is_combining(cp)||at==0)break;
    }while(at>0);
    return at;
}
size_t utf8_next_grapheme(const char *s,size_t cursor){
    size_t at=utf8_next(s,cursor),n=strlen(s);
    while(at<n){
        uint32_t cp=0;
        (void)decode((const unsigned char*)s+at,n-at,&cp);
        if(!is_combining(cp))break;
        at=utf8_next(s,at);
    }
    return at;
}
int utf8_insert(char *s,size_t cap,size_t *cursor,int *select_all,const char *input){
    size_t n,len,at;
    if(!s||!cursor||!select_all||!input||!utf8_valid(input))return -1;
    n=strlen(s);len=strlen(input);
    if(!len)return 0;
    at=*select_all?0:*cursor;
    if(at>n)return -1;
    if(len+( *select_all?0:n )>=cap)return -1;
    if(*select_all){s[0]=0;n=0;*select_all=0;}
    memmove(s+at+len,s+at,n-at+1);
    memcpy(s+at,input,len);
    *cursor=at+len;
    return 0;
}
int utf8_erase(char *s,size_t *cursor,int *select_all,int backwards){
    size_t n,a,b;
    if(!s||!cursor||!select_all||!utf8_valid(s))return -1;
    n=strlen(s);
    if(*select_all){s[0]=0;*cursor=0;*select_all=0;return 1;}
    if(*cursor>n)return -1;
    a=backwards?utf8_prev_grapheme(s,*cursor):*cursor;
    b=backwards?*cursor:utf8_next_grapheme(s,*cursor);
    if(a==b)return 0;
    memmove(s+a,s+b,n-b+1);*cursor=a;
    return 1;
}

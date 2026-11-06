#define _POSIX_C_SOURCE 200809L
#include "finance.h"
#include "storage.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Invest v0.3.2: janela X11 nativa, campos e botoes desenhados diretamente.
 * Nao ha toolkit, browser, rede ou formatos proprietarios. */
#define APP_TITLE "invest 0.3.2"
#define SIDE 242
#define MAX_CLICKS 240
#define MAX_EDIT 128
#define MIN_W 940
#define MIN_H 740

typedef struct { int x,y,w,h; } Rect;
typedef enum {
    BG, WHITE, INK, MUTED, BORDER, BLUE, BLUE_PALE, GREEN,
    GREEN_PALE, RED, RED_PALE, LIGHT, SIDE_BG, GRID, SHADOW, PURPLE,
    SIDE_ACTIVE, SIDE_TEXT, COLOR_COUNT
} Color;
typedef enum { SIMULATE, EVENTS, COMPARE } AppView;
typedef enum { MODAL_NONE, MODAL_EVENT_EDIT, MODAL_CONFIRM } Modal;
typedef enum { F_NONE = -1, F_NAME, F_INITIAL, F_MONTHLY, F_RATE, F_MONTHS, F_EVENT_MONTH, F_EVENT_VALUE, F_EVENT_DURATION } Field;
typedef enum {
    A_NOTHING, A_SCREEN_SIM, A_SCREEN_EVENTS, A_SCREEN_COMPARE, A_NEW, A_DUPLICATE, A_DELETE,
    A_CHOOSE, A_SAVE, A_OPEN_EVENTS, A_CLOSE_MODAL, A_EVENT_RATE_TAB, A_EVENT_MONTHLY_TAB,
    A_EVENT_EXTRA_TAB, A_EVENT_NEW, A_EVENT_EDIT, A_EVENT_DELETE, A_EVENT_SAVE,
    A_EVENT_CANCEL, A_EVENT_FOREVER, A_EVENT_TEMP, A_CONFIRM_OK, A_CONFIRM_CANCEL, A_CMP_A, A_CMP_B,
    A_FIELD, A_LIST_PREV, A_LIST_NEXT, A_EVENTS_PREV, A_EVENTS_NEXT
} Action;
typedef enum { CONFIRM_SCENARIO, CONFIRM_EVENT } ConfirmKind;

typedef struct { Rect box; Action action; int arg; } Hit;

typedef struct {
    Display *dpy;
    Window win;
    int screen;
    GC gc;
    Pixmap back;
    Atom wm_delete;
    XFontStruct *f_small,*f_normal,*f_bold,*f_title,*f_number;
    unsigned long colors[COLOR_COUNT];
    int w,h,running,dirty;
    ScenarioStore store;
    AppView view;
    Modal modal;
    ConfirmKind confirm;
    enum InvestScheduleType event_type;
    int event_edit_index; /* -1 = novo */
    int event_limited; /* 0 = ate o fim; 1 = por N meses */
    int event_scroll;
    int sidebar_scroll;
    int cmp_a,cmp_b;
    int plot_hover;
    Field focus;
    char input[MAX_EDIT];
    int caret,sel_all;
    char status[240];
    int error;
    Hit hits[MAX_CLICKS];
    int hit_n;
} App;

static App app;
/* Paleta original v0.2: cabecalho/lateral escuros, conteudo claro, acento verde.
 * Mantemos os nomes internos por compatibilidade com os componentes v0.3. */
static const char *color_names[COLOR_COUNT] = {
    "#F4F5F6", "#FFFFFF", "#182322", "#71817D", "#DDE3E1",
    "#167259", "#DFF1E9", "#167259", "#DFF1E9", "#B83B40",
    "#F9E9E9", "#EDF0F0", "#171F23", "#E6EBE9", "#E1E8E6", "#416CB7",
    "#2A383C", "#B9CBC8"
};

static int inside(int x,int y,Rect r) { return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static int imin(int a,int b) { return a<b?a:b; }
static int imax(int a,int b) { return a>b?a:b; }
static void foreground(Color c) { XSetForeground(app.dpy,app.gc,app.colors[c]); }
static void fill(Rect r,Color c) {
    if (r.w<=0 || r.h<=0) return;
    foreground(c);
    XFillRectangle(app.dpy,app.back,app.gc,r.x,r.y,(unsigned)r.w,(unsigned)r.h);
}
static void line(int x1,int y1,int x2,int y2,Color c) {
    foreground(c);XDrawLine(app.dpy,app.back,app.gc,x1,y1,x2,y2);
}
static void rounded(Rect r,int radius,Color c) {
    int d;
    if(r.w<=0 || r.h<=0) return;
    radius=imin(radius,imin(r.w/2,r.h/2));
    if(radius==0){fill(r,c);return;}
    d=radius*2;foreground(c);
    XFillRectangle(app.dpy,app.back,app.gc,r.x+radius,r.y,(unsigned)(r.w-d),(unsigned)r.h);
    XFillRectangle(app.dpy,app.back,app.gc,r.x,r.y+radius,(unsigned)r.w,(unsigned)(r.h-d));
    XFillArc(app.dpy,app.back,app.gc,r.x,r.y,d,d,90*64,90*64);
    XFillArc(app.dpy,app.back,app.gc,r.x+r.w-d,r.y,d,d,0,90*64);
    XFillArc(app.dpy,app.back,app.gc,r.x,r.y+r.h-d,d,d,180*64,90*64);
    XFillArc(app.dpy,app.back,app.gc,r.x+r.w-d,r.y+r.h-d,d,d,270*64,90*64);
}
static void card(Rect r) {
    rounded((Rect){r.x,r.y+3,r.w,r.h},13,SHADOW);
    rounded(r,13,WHITE);
}
/* X core fonts recebem Latin-1; os arquivos em disco permanecem UTF-8. */
static int latin1(const char *utf8,char *out,size_t cap){
    size_t i=0,j=0;unsigned char c;
    while((c=(unsigned char)utf8[i])!=0 && j+1<cap){
        if(c<0x80){out[j++]=(char)c;i++;continue;}
        if((c==0xC2 || c==0xC3) && utf8[i+1] && j+1<cap){
            unsigned char q=(unsigned char)utf8[i+1];
            if(q>=0x80 && q<=0xBF){
                out[j++]=(char)((c==0xC2?0x80:0xC0)|(q&0x3F));i+=2;continue;
            }
        }
        /* Outros caracteres UTF-8 viram '?', sem corromper a tela. */
        if(c>=0x80 && c<0xC0){i++;continue;}
        out[j++]='?';i++;
    }
    out[j]=0;return (int)j;
}
static void txt(int x,int y,const char *s,Color color,XFontStruct *font) {
    char ansi[512];int n;
    if(!s||!font)return;
    n=latin1(s,ansi,sizeof(ansi));
    foreground(color);XSetFont(app.dpy,app.gc,font->fid);
    XDrawString(app.dpy,app.back,app.gc,x,y,ansi,n);
}
static int tw(const char *s,XFontStruct *f){
    char ansi[512];int n;
    if(!f)return 0;
    n=latin1(s,ansi,sizeof(ansi));return XTextWidth(f,ansi,n);
}
static void text_right(int x,int y,const char*s,Color c,XFontStruct*f){txt(x-tw(s,f),y,s,c,f);}
static void text_center(Rect r,int y,const char*s,Color c,XFontStruct*f){txt(r.x+(r.w-tw(s,f))/2,y,s,c,f);}
static void cut_text(char *target,size_t n,const char *s,int px,XFontStruct*f) {
    size_t k;
    if(!n)return;
    snprintf(target,n,"%s",s);
    if(tw(target,f)<=px)return;
    k=strlen(target);
    while(k>0 && tw(target,f)+tw("...",f)>px){
        target[--k]=0;
        while(k>0&&((unsigned char)target[k]&0xC0)==0x80)target[--k]=0;
    }
    if(k+4<n)strcat(target,"...");
}
static void money(double v,char*out,size_t size){
    char raw[48],grp[64];size_t i,j=0,l;unsigned long long cents,k;
    if(!isfinite(v)||fabs(v)>9e15){snprintf(out,size,"Fora de escala");return;}
    cents=(unsigned long long)llround(fabs(v)*100.0);k=cents/100;
    snprintf(raw,sizeof(raw),"%llu",k);l=strlen(raw);
    for(i=0;i<l&&j+2<sizeof(grp);i++){if(i&&(l-i)%3==0)grp[j++]='.';grp[j++]=raw[i];}grp[j]=0;
    snprintf(out,size,"%sR$ %s,%02u",v<-.005?"-":"",grp,(unsigned)(cents%100));
}
static void number(double v,char*out,size_t size) {
    size_t i;snprintf(out,size,"%.2f",v);
    for(i=0;out[i];i++)if(out[i]=='.')out[i]=',';
}
/* Numeros no formato brasileiro e decimal com ponto tambem sao aceitos. */
static int parse_number(const char*s,double*out) {
    char tmp[MAX_EDIT],*end;int comma=strchr(s,',')!=NULL;size_t i,j=0;double v;
    for(i=0;s[i];i++){
        unsigned char c=(unsigned char)s[i];
        if(c==' ')continue;
        if(comma&&c=='.')continue;
        if(c==',')c='.';
        if(j+1>=sizeof(tmp))return -1;
        tmp[j++]=(char)c;
    }
    tmp[j]=0;if(!j)return -1;
    errno=0;v=strtod(tmp,&end);
    if(errno||end==tmp||*end||!isfinite(v))return -1;
    *out=v;return 0;
}
static int parse_month(const char *s,int *result) {
    char *end;long n;
    if(!*s)return -1;
    errno=0;n=strtol(s,&end,10);
    if(errno||end==s||*end||n<1||n>INVEST_MAX_MONTHS)return -1;
    *result=(int)n;return 0;
}
static void status(const char*msg,int error){snprintf(app.status,sizeof(app.status),"%s",msg);app.error=error;}
static Scenario* current(void){return &app.store.items[app.store.selected];}
static int save(void) {
    char err[200];
    if(storage_save(&app.store,err,sizeof(err))<0){status(err,1);return -1;}
    app.dirty=0;status("Alteracoes salvas",0);return 0;
}
static void modified(void){app.dirty=1;}
static int add_hit(Rect r,Action a,int arg) {
    if(app.hit_n>=MAX_CLICKS)return -1;
    app.hits[app.hit_n++]=(Hit){r,a,arg};return 0;
}
static void button(Rect r,const char*title,int style,Action action,int arg) {
    char clipped[160];
    Color bg=style==1?BLUE:(style==2?BLUE_PALE:(style==3?RED_PALE:LIGHT));
    Color fg=style==1?WHITE:(style==3?RED:(style==2?BLUE:INK));
    rounded(r,9,bg);
    cut_text(clipped,sizeof(clipped),title,r.w-20,app.f_bold);
    text_center(r,r.y+r.h/2+5,clipped,fg,app.f_bold);
    add_hit(r,action,arg);
}
static void begin_focus(Field f);
static int end_focus(void);
static void display_field(Rect r,const char*name,const char*value,Field field,const char *suffix) {
    char val[160];int active=app.focus==field;
    txt(r.x,r.y-11,name,MUTED,app.f_small);
    rounded(r,9,active?BLUE_PALE:LIGHT);
    if(active) {
        rounded((Rect){r.x+2,r.y+2,r.w-4,r.h-4},8,WHITE);
        snprintf(val,sizeof(val),"%s",app.input);
    } else snprintf(val,sizeof(val),"%s",value);
    if(tw(val,app.f_bold)>r.w-(suffix?78:30)){
        char clipped[160];cut_text(clipped,sizeof(clipped),val,r.w-(suffix?78:30),app.f_bold);
        snprintf(val,sizeof(val),"%s",clipped);
    }
    txt(r.x+14,r.y+r.h/2+5,val,INK,app.f_bold);
    if(suffix)text_right(r.x+r.w-14,r.y+r.h/2+5,suffix,MUTED,app.f_small);
    if(active){
        char prefix[MAX_EDIT];size_t pos=(size_t)imin(app.caret,(int)strlen(val));
        memcpy(prefix,val,pos);prefix[pos]=0;
        int cursor_x=r.x+14+tw(prefix,app.f_bold);
        cursor_x=imin(cursor_x,r.x+r.w-(suffix?58:10));
        line(cursor_x,r.y+12,cursor_x,r.y+r.h-10,BLUE);
    }
    add_hit(r,A_FIELD,(int)field);
}
static void get_field_text(Field f,char*out,size_t n) {
    Scenario*s=current();
    switch(f){
        case F_NAME:snprintf(out,n,"%s",s->name);break;
        case F_INITIAL:number(s->initial,out,n);break;
        case F_MONTHLY:number(s->monthly,out,n);break;
        case F_RATE:number(s->rate_percent,out,n);break;
        case F_MONTHS:snprintf(out,n,"%d",s->months);break;
        case F_EVENT_MONTH: {
            const InvestEvent*arr=app.event_type==INVEST_RATES?s->rates:
                                   app.event_type==INVEST_MONTHLY?s->monthlies:s->extras;
            if(app.event_edit_index>=0)snprintf(out,n,"%d",arr[app.event_edit_index].month);
            else out[0]=0;
            break;
        }
        case F_EVENT_VALUE: {
            const InvestEvent*arr=app.event_type==INVEST_RATES?s->rates:
                                   app.event_type==INVEST_MONTHLY?s->monthlies:s->extras;
            if(app.event_edit_index>=0)number(arr[app.event_edit_index].value,out,n);
            else out[0]=0;
            break;
        }
        case F_EVENT_DURATION: {
            const InvestEvent *arr=app.event_type==INVEST_RATES?s->rates:
                                   app.event_type==INVEST_MONTHLY?s->monthlies:s->extras;
            if(app.event_edit_index>=0 && arr[app.event_edit_index].end_month>0)
                snprintf(out,n,"%d",arr[app.event_edit_index].end_month-arr[app.event_edit_index].month+1);
            else snprintf(out,n,"2");
            break;
        }
        default:out[0]=0;
    }
}
/* O editor de eventos mantem valores em strings independentes. */
static char event_month_text[MAX_EDIT],event_value_text[MAX_EDIT],event_duration_text[MAX_EDIT];
static void event_editor_load(void){
    get_field_text(F_EVENT_MONTH,event_month_text,sizeof(event_month_text));
    get_field_text(F_EVENT_VALUE,event_value_text,sizeof(event_value_text));
    get_field_text(F_EVENT_DURATION,event_duration_text,sizeof(event_duration_text));
    if(app.event_type==INVEST_EXTRAS) app.event_limited=0;
    else if(app.event_edit_index<0) app.event_limited=0;
    else {
        const InvestEvent *arr=app.event_type==INVEST_RATES?current()->rates:current()->monthlies;
        app.event_limited=arr[app.event_edit_index].end_month>0;
    }
}
static InvestEvent *get_events(Scenario *s, enum InvestScheduleType type,
                               int **count)
{
    if(type==INVEST_RATES){*count=&s->rate_count;return s->rates;}
    if(type==INVEST_MONTHLY){*count=&s->monthly_count;return s->monthlies;}
    *count=&s->extra_count;return s->extras;
}
static int update_event(void) {
    Scenario candidate=*current();
    int *count, i, pos, duration;
    InvestEvent *arr=get_events(&candidate,app.event_type,&count);
    InvestEvent e={0};
    if(parse_month(event_month_text,&e.month)<0||parse_number(event_value_text,&e.value)<0){
        status("Informe o mes e o valor corretamente.",1);return -1;
    }
    if(e.month>candidate.months){status("O mes ultrapassa o prazo do cenario.",1);return -1;}
    if(app.event_type!=INVEST_EXTRAS&&app.event_limited){
        if(parse_month(event_duration_text,&duration)<0 ||
           duration>candidate.months-e.month+1){
            status("Duracao invalida: verifique o prazo do cenario.",1);return -1;
        }
        e.end_month=e.month+duration-1;
    }
    if(app.event_type==INVEST_RATES && !(e.value>-100.0&&e.value<=100.0)){
        status("A taxa deve estar acima de -100% e ate 100%.",1);return -1;
    }
    if(app.event_type!=INVEST_RATES && (e.value<0||e.value>1e12)){
        status("Informe um aporte entre zero e 1 trilhao.",1);return -1;
    }
    if(app.event_edit_index>=0){
        for(i=app.event_edit_index;i<*count-1;i++)arr[i]=arr[i+1];
        (*count)--;
    }
    if(*count>=INVEST_MAX_EVENTS){status("Limite de 32 eventos por tipo.",1);return -1;}
    pos=0;while(pos<*count&&arr[pos].month<e.month)pos++;
    if(pos<*count&&arr[pos].month==e.month){
        status("Esse mes ja tem uma regra desse tipo.",1);return -1;
    }
    for(i=*count;i>pos;i--)arr[i]=arr[i-1];
    arr[pos]=e;(*count)++;
    if(!scenario_valid(&candidate)||project_scenario(&candidate,NULL,NULL,NULL)<0){
        status("Regra invalida ou projecao fora de escala.",1);return -1;
    }
    *current()=candidate;modified();app.modal=MODAL_NONE;
    status("Regra atualizada.",0);return 0;
}
static void remove_event(void){
    Scenario *s=current();int *count;
    InvestEvent *arr=get_events(s,app.event_type,&count);
    int i=app.event_edit_index;
    if(i<0||i>=*count)return;
    for(;i<*count-1;i++)arr[i]=arr[i+1];
    (*count)--;modified();status("Regra removida.",0);
}
static int apply_field(Field f,const char*text){
    Scenario candidate=*current();double v;int n;
    if(f==F_NAME){
        if(!text[0]||strlen(text)>=sizeof(candidate.name)||strchr(text,'\t')||strchr(text,'\n')){
            status("Use um nome de 1 a 63 caracteres.",1);return -1;
        }
        snprintf(candidate.name,sizeof(candidate.name),"%s",text);
    }else if(f==F_MONTHS){
        if(parse_month(text,&n)<0){status("O prazo deve estar entre 1 e 600 meses.",1);return -1;}
        candidate.months=n;
    }else if(f==F_INITIAL||f==F_MONTHLY||f==F_RATE){
        if(parse_number(text,&v)<0){status("Informe um numero valido.",1);return -1;}
        if(f==F_INITIAL)candidate.initial=v;
        if(f==F_MONTHLY)candidate.monthly=v;
        if(f==F_RATE)candidate.rate_percent=v;
    }
    if(!scenario_valid(&candidate)||project_scenario(&candidate,NULL,NULL,NULL)<0){
        status("Valor invalido, prazo incompativel ou projecao fora de escala.",1);
        return -1;
    }
    if(memcmp(&candidate,current(),sizeof(candidate))){*current()=candidate;modified();}
    status("",0);return 0;
}
static void begin_focus(Field f){
    char buf[MAX_EDIT]={0};
    if(app.focus==f)return;
    if(end_focus()<0)return;
    app.focus=f;
    if(f==F_EVENT_MONTH)snprintf(buf,sizeof(buf),"%s",event_month_text);
    else if(f==F_EVENT_VALUE)snprintf(buf,sizeof(buf),"%s",event_value_text);
    else if(f==F_EVENT_DURATION)snprintf(buf,sizeof(buf),"%s",event_duration_text);
    else get_field_text(f,buf,sizeof(buf));
    snprintf(app.input,sizeof(app.input),"%s",buf);
    app.caret=(int)strlen(app.input);app.sel_all=1;
}
static int end_focus(void){
    Field f=app.focus;
    if(f==F_NONE)return 0;
    if(f==F_EVENT_MONTH)snprintf(event_month_text,sizeof(event_month_text),"%s",app.input);
    else if(f==F_EVENT_VALUE)snprintf(event_value_text,sizeof(event_value_text),"%s",app.input);
    else if(f==F_EVENT_DURATION)snprintf(event_duration_text,sizeof(event_duration_text),"%s",app.input);
    else if(apply_field(f,app.input)<0)return -1;
    app.focus=F_NONE;app.sel_all=0;return 0;
}
static void scenario_select(int n){
    if(n<0||n>=app.store.count)return;
    end_focus();app.store.selected=n;modified();status("",0);
}
static int list_visible(void);
static void scenario_new(int duplicate){
    Scenario s;int num=1,i,used;
    end_focus();if(app.store.count==INVEST_MAX_SCENARIOS){status("Limite de 32 cenarios.",1);return;}
    s=duplicate?*current():(Scenario){0};
    if(!duplicate)scenario_default(&s);
    do {
        char name[64];used=0;snprintf(name,sizeof(name),"Cenario %d",num);
        for(i=0;i<app.store.count;i++)if(!strcmp(app.store.items[i].name,name))used=1;
        if(!used)snprintf(s.name,sizeof(s.name),"%s",name);
        num++;
    }while(used&&num<10000);
    app.store.items[app.store.count]=s;
    app.store.selected=app.store.count++;
    app.sidebar_scroll=imax(0,app.store.selected-list_visible()+1);
    if(app.cmp_b==app.cmp_a)app.cmp_b=app.store.selected;
    modified();status(duplicate?"Cenario duplicado.":"Cenario criado.",0);
}
static void scenario_remove(void){
    int i,n=app.store.selected;
    if(app.store.count<=1){status("Pelo menos um cenario precisa permanecer.",1);return;}
    for(i=n;i<app.store.count-1;i++)app.store.items[i]=app.store.items[i+1];
    app.store.count--;
    if(app.store.selected>=app.store.count)app.store.selected=app.store.count-1;
    app.cmp_a=0;app.cmp_b=app.store.count>1?1:0;
    modified();status("Cenario excluido.",0);
}
static int list_visible(void){return imax(1,(app.h-142-145)/62);}
static void clamp_list(void){
    int max=imax(0,app.store.count-list_visible());
    if(app.sidebar_scroll>max)app.sidebar_scroll=max;
    if(app.sidebar_scroll<0)app.sidebar_scroll=0;
}
/* As abas voltam para a faixa superior, como na versao 0.2. */
static void header_tab(Rect r,const char *name,int selected,Action action){
    rounded(r,3,selected?BLUE:SIDE_ACTIVE);
    text_center(r,r.y+r.h/2+5,name,WHITE,app.f_bold);
    add_hit(r,action,0);
}
static void draw_header(void){
    Rect savebtn={app.w-159,18,132,38};
    fill((Rect){0,0,app.w,74},SIDE_BG);
    txt(22,46,"invest",WHITE,app.f_title);
    txt(106,42,"v0.3.2",SIDE_TEXT,app.f_small);
    header_tab((Rect){251,17,131,39},"SIMULADOR",app.view==SIMULATE,A_SCREEN_SIM);
    header_tab((Rect){391,17,124,39},"EVENTOS",app.view==EVENTS,A_SCREEN_EVENTS);
    header_tab((Rect){524,17,132,39},"COMPARAR",app.view==COMPARE,A_SCREEN_COMPARE);
    button(savebtn,app.dirty?"Salvar agora":"Salvar",1,A_SAVE,0);
}
static void draw_side(void){
    int y,i,visible;
    fill((Rect){0,74,SIDE,app.h-74},SIDE_BG);
    line(SIDE-1,74,SIDE-1,app.h,SIDE_ACTIVE);
    txt(23,116,"CENARIOS",SIDE_TEXT,app.f_bold);
    line(22,128,SIDE-22,128,SIDE_ACTIVE);
    visible=list_visible();clamp_list();
    for(i=0;i<visible&&i+app.sidebar_scroll<app.store.count;i++){
        int n=i+app.sidebar_scroll;Rect r={13,142+i*62,SIDE-26,54};
        Scenario*s=&app.store.items[n];char title[80],sub[100];
        if(n==app.store.selected){
            rounded(r,3,SIDE_ACTIVE);
            fill((Rect){r.x,r.y,4,r.h},BLUE);
        }
        cut_text(title,sizeof(title),s->name,r.w-28,app.f_bold);
        txt(r.x+13,r.y+23,title,WHITE,app.f_bold);
        snprintf(sub,sizeof(sub),"%d meses  |  %.2f%% ao mes",s->months,s->rate_percent);
        txt(r.x+13,r.y+43,sub,SIDE_TEXT,app.f_small);
        add_hit(r,A_CHOOSE,n);
    }
    y=app.h-87;
    if(app.store.count>visible){
        button((Rect){23,y-41,87,29},"Anterior",0,A_LIST_PREV,0);
        button((Rect){132,y-41,87,29},"Proximo",0,A_LIST_NEXT,0);
    }
    button((Rect){18,y,SIDE-36,39},"+ Novo cenario",1,A_NEW,0);
    txt(22,app.h-18,"Dados locais  |  Linux",SIDE_TEXT,app.f_small);
}
static void draw_footer(void){
    fill((Rect){SIDE,app.h-36,app.w-SIDE,36},BG);
    line(SIDE,app.h-36,app.w,app.h-36,BORDER);
    char msg[240];
    cut_text(msg,sizeof(msg),app.status[0]?app.status:"Dados locais  |  Ctrl+S para salvar",app.w-SIDE-224,app.f_small);
    txt(SIDE+28,app.h-13,msg,app.error?RED:MUTED,app.f_small);
    text_right(app.w-27,app.h-13,app.dirty?"Alteracoes pendentes":"Tudo salvo",app.dirty?BLUE:GREEN,app.f_small);
}
static void draw_graph(Rect r,const Scenario *a,const Scenario *b) {
    double balancesA[INVEST_MAX_MONTHS+1],capitalA[INVEST_MAX_MONTHS+1];
    double balancesB[INVEST_MAX_MONTHS+1],capitalB[INVEST_MAX_MONTHS+1];
    Projection p;double ymax=1;int i;
    int plotx=r.x+74,ploty=r.y+54,plotw=r.w-103,ploth=r.h-98;
    int end=b?imax(a->months,b->months):a->months;
    int tick_count=ploth<90?2:4;
    char txtbuf[80];
    if(plotw<20||ploth<20)return;
    if(project_scenario(a,&p,balancesA,capitalA)<0)return;
    if(b && project_scenario(b,&p,balancesB,capitalB)<0)return;
    for(i=0;i<=a->months;i++)ymax=fmax(ymax,balancesA[i]);
    if(b)for(i=0;i<=b->months;i++)ymax=fmax(ymax,balancesB[i]);
    ymax*=1.10;
    for(i=0;i<=tick_count;i++){
        int yy=ploty+ploth-(ploth*i)/tick_count;
        line(plotx,yy,plotx+plotw,yy,GRID);
        snprintf(txtbuf,sizeof(txtbuf),"%.0f mil",(ymax*i)/(1000.0*tick_count));
        text_right(plotx-12,yy+4,txtbuf,MUTED,app.f_small);
    }
    for(i=0;i<=4;i++){
        int xx=plotx+plotw*i/4;
        snprintf(txtbuf,sizeof(txtbuf),"%d m",end*i/4);
        text_center((Rect){xx-28,0,56,0},ploty+ploth+24,txtbuf,MUTED,app.f_small);
    }
    for(i=1;i<=a->months;i++){
        int x1=plotx+plotw*(i-1)/end,x2=plotx+plotw*i/end;
        int y1=ploty+ploth-(int)(balancesA[i-1]/ymax*ploth);
        int y2=ploty+ploth-(int)(balancesA[i]/ymax*ploth);
        line(x1,y1,x2,y2,BLUE);
        if(!b){
            y1=ploty+ploth-(int)(capitalA[i-1]/ymax*ploth);
            y2=ploty+ploth-(int)(capitalA[i]/ymax*ploth);
            line(x1,y1,x2,y2,PURPLE);
        }
    }
    if(b)for(i=1;i<=b->months;i++){
        int x1=plotx+plotw*(i-1)/end,x2=plotx+plotw*i/end;
        int y1=ploty+ploth-(int)(balancesB[i-1]/ymax*ploth);
        int y2=ploty+ploth-(int)(balancesB[i]/ymax*ploth);
        line(x1,y1,x2,y2,PURPLE);
    }
    txt(r.x+25,r.y+27,b?"Evolucao dos dois cenarios":"Evolucao do patrimonio",INK,app.f_bold);
    rounded((Rect){r.x+r.w-271,r.y+15,11,11},5,BLUE);
    txt(r.x+r.w-252,r.y+25,b?"Cenario A":"Patrimonio",MUTED,app.f_small);
    rounded((Rect){r.x+r.w-131,r.y+15,11,11},5,PURPLE);
    txt(r.x+r.w-112,r.y+25,b?"Cenario B":"Investido",MUTED,app.f_small);
}
static void draw_sim(void){
    int x=SIDE+30,w=app.w-SIDE-60,half=(w-51)/2;
    Rect r;char b1[80],b2[80],b3[80],b4[80];
    Scenario*s=current();Projection p;
    if(project_scenario(s,&p,NULL,NULL)<0){
        memset(&p,0,sizeof(p));status("Projecao fora da escala numerica.",1);
    }
    display_field((Rect){x,110,w-268,43},"Nome do cenario",s->name,F_NAME,NULL);
    button((Rect){x+w-236,112,112,39},"Duplicar",0,A_DUPLICATE,0);
    button((Rect){x+w-112,112,112,39},"Excluir",3,A_DELETE,0);
    txt(x,176,"Preencha os campos e veja sua projecao em tempo real.",MUTED,app.f_normal);
    r=(Rect){x,192,w,214};card(r);
    txt(x+22,218,"Dados do investimento",INK,app.f_bold);
    button((Rect){x+w-212,200,189,32},"Eventos e aportes  >",2,A_OPEN_EVENTS,0);
    number(s->initial,b1,sizeof(b1));number(s->monthly,b2,sizeof(b2));
    number(s->rate_percent,b3,sizeof(b3));snprintf(b4,sizeof(b4),"%d",s->months);
    display_field((Rect){x+24,250,half-6,41},"Valor inicial",b1,F_INITIAL,"R$");
    display_field((Rect){x+31+half,250,half-6,41},"Aporte mensal",b2,F_MONTHLY,"R$");
    display_field((Rect){x+24,322,half-6,41},"Juros por mes",b3,F_RATE,"%");
    display_field((Rect){x+31+half,322,half-6,41},"Prazo",b4,F_MONTHS,"meses");
    /* Equivalencia composta: (1 + taxa mensal/100)^12 - 1. Taxa BASE,
       mesmo quando o cenario inclui mudancas futuras na aba Eventos. */
    {
        double annual=annual_equivalent_rate(s->rate_percent);
        char percent[40];
        number(annual,percent,sizeof(percent));
        txt(x+24,389,s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",MUTED,app.f_small);
        txt(x+24+tw(s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",app.f_small)+8,
            389,percent,GREEN,app.f_bold);
        txt(x+24+tw(s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",app.f_small)+
            tw(percent,app.f_bold)+12,389,"% a.a.",GREEN,app.f_small);
        if(s->rate_count||s->monthly_count||s->extra_count){
            char changes[96];
            snprintf(changes,sizeof(changes),"%d juros | %d mensais | %d extras",s->rate_count,s->monthly_count,s->extra_count);
            txt(x+31+half,389,changes,MUTED,app.f_small);
        }
    }
    r=(Rect){x,418,w,124};card(r);
    txt(x+24,443,"Resultado projetado",INK,app.f_bold);
    money(p.final_balance,b1,sizeof(b1));
    txt(x+24,483,b1,INK,app.f_number);
    txt(x+24,515,"Montante final estimado",MUTED,app.f_small);
    line(x+w/2,443,x+w/2,528,BORDER);
    money(p.contributions,b2,sizeof(b2));money(p.earnings,b3,sizeof(b3));
    txt(x+w/2+20,455,"Total investido",MUTED,app.f_small);
    txt(x+w/2+20,479,b2,INK,app.f_bold);
    txt(x+w/2+20,501,"Juros acumulados",MUTED,app.f_small);
    txt(x+w/2+20,526,b3,p.earnings>=0?GREEN:RED,app.f_bold);
    r=(Rect){x,555,w,app.h-607};card(r);
    draw_graph(r,s,NULL);
}
static void draw_compare(void){
    int x=SIDE+30,w=app.w-SIDE-60,top=123;Rect r;
    const Scenario *a=&app.store.items[app.cmp_a];
    const Scenario *b=&app.store.items[app.cmp_b];
    Projection pa,pb;char v[96],text[120];int split=(w-14)/2;
    if(project_scenario(a,&pa,NULL,NULL)<0)memset(&pa,0,sizeof(pa));
    if(project_scenario(b,&pb,NULL,NULL)<0)memset(&pb,0,sizeof(pb));
    txt(x,top,"Comparar cenarios",INK,app.f_title);
    txt(x,top+25,app.store.count>1?"Veja os resultados e as curvas lado a lado.":
        "Crie mais um cenario para comparar resultados diferentes.",MUTED,app.f_normal);
    button((Rect){x,181,split,42},a->name,2,A_CMP_A,0);
    button((Rect){x+split+14,181,split,42},b->name,0,A_CMP_B,0);
    for(int i=0;i<2;i++){
        const Projection*p=i?&pb:&pa;const Scenario*s=i?b:a;
        int colx=x+i*(split+14);
        r=(Rect){colx,240,split,153};card(r);
        txt(colx+20,269,i?"CENARIO B":"CENARIO A",MUTED,app.f_small);
        money(p->final_balance,v,sizeof(v));txt(colx+20,309,v,INK,app.f_number);
        money(p->earnings,v,sizeof(v));
        snprintf(text,sizeof(text),"Juros: %s",v);txt(colx+20,343,text,p->earnings>=0?GREEN:RED,app.f_bold);
        snprintf(text,sizeof(text),"%d meses | %.2f%% ao mes",s->months,s->rate_percent);
        txt(colx+20,370,text,MUTED,app.f_small);
    }
    r=(Rect){x,409,w,87};card(r);
    txt(x+20,441,"Diferenca entre os montantes finais",MUTED,app.f_small);
    money(fabs(pa.final_balance-pb.final_balance),v,sizeof(v));
    txt(x+20,473,v,INK,app.f_title);
    if(a->months!=b->months) {
        txt(x+w/2,460,"Atencao: os prazos sao diferentes.",RED,app.f_small);
    }else{
        const char *winner=pa.final_balance>pb.final_balance?"Cenario A tem maior montante":
                           pa.final_balance<pb.final_balance?"Cenario B tem maior montante":"Montantes equivalentes";
        txt(x+w/2,460,winner,MUTED,app.f_normal);
    }
    r=(Rect){x,512,w,app.h-571};card(r);
    draw_graph(r,a,b);
    txt(x,app.h-51,"Comparacao nominal, sem impostos ou inflacao.",MUTED,app.f_small);
}
static Rect modal_box(void){
    int w=imin(740,app.w-110);int h=imin(606,app.h-95);
    return (Rect){(app.w-w)/2,(app.h-h)/2,w,h};
}
static void draw_modal_bg(void){
    fill((Rect){0,0,app.w,app.h},SHADOW);
}
/* Aba Eventos: lista legivel e formularios simples, sem sintaxe de planilha. */
static Rect events_area(void){
    return (Rect){SIDE+30,171,app.w-SIDE-60,app.h-225};
}
static void draw_events(void){
    int x=SIDE+30,w=app.w-SIDE-60,i,n;
    Scenario *s=current();Rect r=events_area();int *countp;
    const InvestEvent *arr=get_events(s,app.event_type,&countp);
    int count=*countp;
    char heading[128],desc[200],val[80];
    const char *title=app.event_type==INVEST_RATES?"Mudancas de juros":
                      app.event_type==INVEST_MONTHLY?"Mudancas de aporte mensal":
                      "Aportes extraordinarios";
    txt(x,119,"Eventos do cenario",INK,app.f_title);
    txt(x,146,"Programe taxas e aportes temporarios ou permanentes.",MUTED,app.f_normal);
    card(r);
    button((Rect){x+20,r.y+21,165,40},"Juros",app.event_type==INVEST_RATES?1:0,A_EVENT_RATE_TAB,0);
    button((Rect){x+194,r.y+21,205,40},"Aportes mensais",app.event_type==INVEST_MONTHLY?1:0,A_EVENT_MONTHLY_TAB,0);
    button((Rect){x+408,r.y+21,175,40},"Aportes extras",app.event_type==INVEST_EXTRAS?1:0,A_EVENT_EXTRA_TAB,0);
    line(x+23,r.y+78,x+w-23,r.y+78,BORDER);
    txt(x+25,r.y+115,title,INK,app.f_bold);
    button((Rect){x+w-199,r.y+92,175,41},"+ Adicionar",1,A_EVENT_NEW,0);
    txt(x+25,r.y+147,app.event_type==INVEST_RATES?
        "Defina uma taxa por alguns meses ou ate o fim.":
        app.event_type==INVEST_MONTHLY?
        "Defina um novo aporte mensal por alguns meses ou ate o fim.":
        "Acrescente um investimento uma unica vez.",MUTED,app.f_small);
    line(x+23,r.y+161,x+w-23,r.y+161,BORDER);
    n=imax(1,(r.h-266)/65);
    if(app.event_scroll>imax(0,count-n))app.event_scroll=imax(0,count-n);
    if(count==0){
        txt(x+29,r.y+218,"Nenhuma alteracao cadastrada.",INK,app.f_bold);
        txt(x+29,r.y+246,app.event_type==INVEST_RATES?
            "A taxa base vale durante todo o periodo.":
            app.event_type==INVEST_MONTHLY?
            "O aporte base vale durante todo o periodo.":
            "Nenhum aporte pontual foi adicionado.",MUTED,app.f_normal);
    }
    for(i=0;i<n&&i+app.event_scroll<count;i++){
        int idx=i+app.event_scroll;Rect item={x+23,r.y+172+i*65,w-46,58};
        rounded(item,7,LIGHT);
        if(app.event_type==INVEST_EXTRAS)
            snprintf(heading,sizeof(heading),"No mes %d",arr[idx].month);
        else if(arr[idx].end_month==0)
            snprintf(heading,sizeof(heading),"A partir do mes %d",arr[idx].month);
        else
            snprintf(heading,sizeof(heading),"Do mes %d ao %d",arr[idx].month,arr[idx].end_month);
        txt(item.x+17,item.y+23,heading,INK,app.f_bold);
        if(app.event_type==INVEST_RATES){
            number(arr[idx].value,val,sizeof(val));
            snprintf(desc,sizeof(desc),"Taxa mensal: %s%%",val);
        }else{
            money(arr[idx].value,val,sizeof(val));
            snprintf(desc,sizeof(desc),app.event_type==INVEST_MONTHLY?
                     "Aporte mensal: %s":"Aporte unico: %s",val);
        }
        txt(item.x+17,item.y+44,desc,MUTED,app.f_small);
        button((Rect){item.x+item.w-220,item.y+11,94,36},"Editar",0,A_EVENT_EDIT,idx);
        button((Rect){item.x+item.w-116,item.y+11,105,36},"Remover",3,A_EVENT_DELETE,idx);
    }
    if(count>n){
        button((Rect){x+27,r.y+r.h-77,101,33},"Anterior",0,A_EVENTS_PREV,0);
        button((Rect){x+136,r.y+r.h-77,101,33},"Proximo",0,A_EVENTS_NEXT,0);
        snprintf(heading,sizeof(heading),"Exibindo %d-%d de %d",app.event_scroll+1,
                 imin(count,app.event_scroll+n),count);
        text_right(x+w-27,r.y+r.h-54,heading,MUTED,app.f_small);
    }
    line(x+23,r.y+r.h-38,x+w-23,r.y+r.h-38,BORDER);
    txt(x+25,r.y+r.h-15,"Depois de um periodo temporario, vale a regra anterior.",MUTED,app.f_small);
}
static void draw_event_editor(void){
    Rect r=modal_box();Rect d={r.x+63,r.y+52,r.w-126,492};char text[160];
    card(r);card(d);
    txt(d.x+25,d.y+38,app.event_edit_index<0?"Nova alteracao":"Editar alteracao",INK,app.f_title);
    txt(d.x+25,d.y+68,app.event_type==INVEST_RATES?
       "Escolha quando os novos juros comecam.":
       app.event_type==INVEST_MONTHLY?
       "Escolha quando o novo aporte mensal comeca.":
       "Defina um aporte extra para um unico mes.",MUTED,app.f_normal);
    snprintf(text,sizeof(text),"%s",event_month_text);
    display_field((Rect){d.x+25,d.y+114,d.w-50,43},
                  app.event_type==INVEST_EXTRAS?"Mes do aporte":"Mes inicial",text,F_EVENT_MONTH,"mes");
    snprintf(text,sizeof(text),"%s",event_value_text);
    display_field((Rect){d.x+25,d.y+198,d.w-50,43},
                  app.event_type==INVEST_RATES?"Nova taxa mensal":
                  app.event_type==INVEST_MONTHLY?"Novo aporte mensal":"Aporte extra",
                  text,F_EVENT_VALUE,app.event_type==INVEST_RATES?"%":"R$");
    if(app.event_type!=INVEST_EXTRAS){
        txt(d.x+25,d.y+267,"Por quanto tempo?",INK,app.f_bold);
        button((Rect){d.x+25,d.y+283,190,39},"Ate o fim",app.event_limited?0:1,A_EVENT_FOREVER,0);
        button((Rect){d.x+225,d.y+283,207,39},"Por alguns meses",app.event_limited?1:0,A_EVENT_TEMP,0);
        if(app.event_limited){
            snprintf(text,sizeof(text),"%s",event_duration_text);
            display_field((Rect){d.x+25,d.y+362,225,43},"Duracao",text,F_EVENT_DURATION,"meses");
            txt(d.x+272,d.y+389,"Depois, retoma a regra",MUTED,app.f_small);
            txt(d.x+272,d.y+405,"anterior ainda vigente.",MUTED,app.f_small);
        }else{
            txt(d.x+25,d.y+365,"A nova regra vale ate o fim, exceto quando",MUTED,app.f_small);
            txt(d.x+25,d.y+387,"outra alteracao estiver ativa.",MUTED,app.f_small);
        }
    }else{
        txt(d.x+25,d.y+293,"Aporte unico: nao altera o aporte mensal regular.",MUTED,app.f_small);
    }
    button((Rect){d.x+25,d.y+432,122,42},"Cancelar",0,A_EVENT_CANCEL,0);
    button((Rect){d.x+d.w-165,d.y+432,140,42},"Salvar",1,A_EVENT_SAVE,0);
}
static void draw_confirm(void){
    Rect outer=modal_box();Rect r={outer.x+110,outer.y+115,outer.w-220,286};
    card(outer);card(r);
    txt(r.x+24,r.y+51,"Confirmar exclusao",INK,app.f_title);
    txt(r.x+24,r.y+105,app.confirm==CONFIRM_EVENT?
      "Remover este evento do cenario?":"Excluir este cenario e seus eventos?",INK,app.f_normal);
    txt(r.x+24,r.y+134,"Esta acao nao pode ser desfeita.",MUTED,app.f_small);
    button((Rect){r.x+24,r.y+205,138,42},"Cancelar",0,A_CONFIRM_CANCEL,0);
    button((Rect){r.x+r.w-161,r.y+205,137,42},"Excluir",3,A_CONFIRM_OK,0);
}
static void paint(void){
    app.hit_n=0;
    fill((Rect){0,0,app.w,app.h},BG);
    draw_header();draw_side();
    if(app.view==SIMULATE)draw_sim();
    else if(app.view==EVENTS)draw_events();
    else draw_compare();
    draw_footer();
    if(app.modal!=MODAL_NONE){
        /* Modal bloqueia completamente cliques na tela principal. */
        app.hit_n=0;draw_modal_bg();
        if(app.modal==MODAL_EVENT_EDIT)draw_event_editor();
        if(app.modal==MODAL_CONFIRM)draw_confirm();
        if(app.status[0]){
            Rect r=modal_box();
            txt(r.x+30,r.y+r.h-62,app.status,app.error?RED:GREEN,app.f_small);
        }
    }
    XCopyArea(app.dpy,app.back,app.win,app.gc,0,0,(unsigned)app.w,(unsigned)app.h,0,0);
    XFlush(app.dpy);
}
static void launch_event_editor(int idx){
    app.event_edit_index=idx;
    app.modal=MODAL_EVENT_EDIT;event_editor_load();
    app.focus=F_NONE;status("",0);
}
static void execute(Action a,int arg){
    int modal_is_edit=app.modal==MODAL_EVENT_EDIT;
    /* Cliques em campos so trocam o foco; os demais confirmam a edicao. */
    if(a==A_FIELD){begin_focus((Field)arg);return;}
    if(app.focus!=F_NONE&&end_focus()<0)return;
    switch(a){
    case A_SCREEN_SIM:app.view=SIMULATE;break;
    case A_SCREEN_EVENTS:app.view=EVENTS;app.event_scroll=0;break;
    case A_SCREEN_COMPARE:app.view=COMPARE;break;
    case A_NEW:scenario_new(0);break;
    case A_DUPLICATE:scenario_new(1);break;
    case A_DELETE:if(app.store.count>1){app.confirm=CONFIRM_SCENARIO;app.modal=MODAL_CONFIRM;}else status("Mantenha pelo menos um cenario.",1);break;
    case A_CHOOSE:scenario_select(arg);break;
    case A_SAVE:(void)save();break;
    case A_OPEN_EVENTS:app.view=EVENTS;app.event_scroll=0;status("",0);break;
    case A_CLOSE_MODAL:app.modal=MODAL_NONE;status("",0);break;
    case A_EVENT_RATE_TAB:app.event_type=INVEST_RATES;app.event_scroll=0;status("",0);break;
    case A_EVENT_EXTRA_TAB:app.event_type=INVEST_EXTRAS;app.event_scroll=0;status("",0);break;
    case A_EVENT_MONTHLY_TAB:app.event_type=INVEST_MONTHLY;app.event_scroll=0;status("",0);break;
    case A_EVENT_NEW:launch_event_editor(-1);break;
    case A_EVENT_EDIT:launch_event_editor(arg);break;
    case A_EVENT_DELETE:app.event_edit_index=arg;app.confirm=CONFIRM_EVENT;app.modal=MODAL_CONFIRM;break;
    case A_EVENT_SAVE:if(modal_is_edit){(void)update_event();}break;
    case A_EVENT_CANCEL:app.modal=MODAL_NONE;status("",0);break;
    case A_EVENT_FOREVER:app.event_limited=0;status("",0);break;
    case A_EVENT_TEMP:app.event_limited=1;status("",0);break;
    case A_CONFIRM_OK:
        if(app.confirm==CONFIRM_SCENARIO){scenario_remove();app.modal=MODAL_NONE;}
        else{remove_event();app.modal=MODAL_NONE;}
        break;
    case A_CONFIRM_CANCEL:app.modal=MODAL_NONE;status("",0);break;
    case A_CMP_A:app.cmp_a=(app.cmp_a+1)%app.store.count;break;
    case A_CMP_B:app.cmp_b=(app.cmp_b+1)%app.store.count;break;
    case A_LIST_PREV:app.sidebar_scroll=imax(0,app.sidebar_scroll-2);break;
    case A_LIST_NEXT:app.sidebar_scroll=imin(imax(0,app.store.count-list_visible()),app.sidebar_scroll+2);break;
    case A_EVENTS_PREV:app.event_scroll=imax(0,app.event_scroll-2);break;
    case A_EVENTS_NEXT:app.event_scroll+=2;break;
    default:break;
    }
}
static void mouse_press(int x,int y,unsigned button){
    if(button==4 || button==5){
        if(app.modal==MODAL_NONE&&app.view==EVENTS&&inside(x,y,events_area())){
            app.event_scroll=imax(0,app.event_scroll+(button==4?-1:1));
        }else if(app.modal==MODAL_NONE&&inside(x,y,(Rect){0,130,SIDE,app.h-130})){
            app.sidebar_scroll=imax(0,app.sidebar_scroll+(button==4?-1:1));clamp_list();
        }return;
    }
    if(button!=1)return;
    /* Ultimo botao registrado tem prioridade, com hit testing reverso. */
    for(int i=app.hit_n-1;i>=0;i--){
        if(inside(x,y,app.hits[i].box)){
            execute(app.hits[i].action,app.hits[i].arg);return;
        }
    }
    (void)end_focus();
}
static void handle_key(XKeyEvent *ev){
    KeySym sym;char text[32];int len=XLookupString(ev,text,sizeof(text)-1,&sym,NULL);
    int ctrl=ev->state & ControlMask;
    if(ctrl && (sym==XK_s||sym==XK_S)){end_focus();(void)save();return;}
    if(sym==XK_Escape){
        if(app.focus!=F_NONE){app.focus=F_NONE;status("Edicao cancelada.",0);}
        else if(app.modal==MODAL_EVENT_EDIT||app.modal==MODAL_CONFIRM){app.modal=MODAL_NONE;}
        return;
    }
    if(app.focus==F_NONE){
        if(sym==XK_F1 && app.modal==MODAL_NONE)app.view=SIMULATE;
        if(sym==XK_F2 && app.modal==MODAL_NONE)app.view=EVENTS;
        if(sym==XK_F3 && app.modal==MODAL_NONE)app.view=COMPARE;
        return;
    }
    if(sym==XK_Return||sym==XK_KP_Enter){end_focus();return;}
    if(sym==XK_Tab){
        Field next=app.focus;
        if(app.modal==MODAL_EVENT_EDIT)
            next=app.focus==F_EVENT_MONTH?F_EVENT_VALUE:
                 app.focus==F_EVENT_VALUE&&app.event_limited&&app.event_type!=INVEST_EXTRAS?
                 F_EVENT_DURATION:F_EVENT_MONTH;
        else next=app.focus==F_INITIAL?F_MONTHLY:app.focus==F_MONTHLY?F_RATE:app.focus==F_RATE?F_MONTHS:F_INITIAL;
        begin_focus(next);return;
    }
    if(ctrl && (sym==XK_a||sym==XK_A)){app.sel_all=1;app.caret=(int)strlen(app.input);return;}
    if(sym==XK_Home){app.caret=0;app.sel_all=0;return;}
    if(sym==XK_End){app.caret=(int)strlen(app.input);app.sel_all=0;return;}
    if(sym==XK_Left){app.caret=imax(0,app.caret-1);app.sel_all=0;return;}
    if(sym==XK_Right){app.caret=imin((int)strlen(app.input),app.caret+1);app.sel_all=0;return;}
    if(sym==XK_BackSpace||sym==XK_Delete){
        int n=(int)strlen(app.input);
        if(app.sel_all){app.input[0]=0;app.caret=0;app.sel_all=0;return;}
        if(sym==XK_BackSpace&&app.caret>0){
            memmove(app.input+app.caret-1,app.input+app.caret,(size_t)(n-app.caret+1));app.caret--;
        }else if(sym==XK_Delete&&app.caret<n){
            memmove(app.input+app.caret,app.input+app.caret+1,(size_t)(n-app.caret));
        }return;
    }
    if(ctrl||len<=0)return;
    for(int i=0;i<len;i++){
        unsigned char ch=(unsigned char)text[i];int n=(int)strlen(app.input);
        if(ch>=0xA0 && app.focus==F_NAME){
            int new_len=(int)strlen(app.input);
            unsigned char c1=(unsigned char)(0xC0|(ch>>6));
            unsigned char c2=(unsigned char)(0x80|(ch&0x3F));
            if(app.sel_all){app.input[0]=0;app.caret=0;app.sel_all=0;new_len=0;}
            if(new_len<(int)sizeof(app.input)-3){
                memmove(app.input+app.caret+2,app.input+app.caret,(size_t)(new_len-app.caret+1));
                app.input[app.caret++]=(char)c1;app.input[app.caret++]=(char)c2;
            }
            continue;
        }
        if(ch<32||ch>126)continue;
        if(app.focus!=F_NAME && !(isdigit(ch)||ch=='.'||ch==','||ch=='-'||ch==' '))continue;
        if(app.sel_all){app.input[0]=0;app.caret=0;app.sel_all=0;n=0;}
        if(n>=(int)sizeof(app.input)-2)break;
        memmove(app.input+app.caret+1,app.input+app.caret,(size_t)(n-app.caret+1));
        app.input[app.caret++]=(char)ch;
    }
}
static XFontStruct *load_font(const char *one,const char*two,const char*three){
    XFontStruct*f=XLoadQueryFont(app.dpy,one);
    if(!f)f=XLoadQueryFont(app.dpy,two);
    if(!f)f=XLoadQueryFont(app.dpy,three);
    if(!f)f=XLoadQueryFont(app.dpy,"fixed");
    return f;
}
static void shutdown(void){
    if(app.back)XFreePixmap(app.dpy,app.back);
    if(app.gc)XFreeGC(app.dpy,app.gc);
    if(app.f_small)XFreeFont(app.dpy,app.f_small);
    if(app.f_normal)XFreeFont(app.dpy,app.f_normal);
    if(app.f_bold)XFreeFont(app.dpy,app.f_bold);
    if(app.f_title)XFreeFont(app.dpy,app.f_title);
    if(app.f_number)XFreeFont(app.dpy,app.f_number);
    if(app.win)XDestroyWindow(app.dpy,app.win);
    if(app.dpy)XCloseDisplay(app.dpy);
}
static int setup(void){
    XSizeHints hints={0};XColor exact,actual;
    app.dpy=XOpenDisplay(NULL);
    if(!app.dpy){fprintf(stderr,"invest: nao foi possivel abrir o X11 (DISPLAY)\n");return -1;}
    app.screen=DefaultScreen(app.dpy);
    app.w=1120;app.h=832;
    app.win=XCreateSimpleWindow(app.dpy,RootWindow(app.dpy,app.screen),80,60,
                                (unsigned)app.w,(unsigned)app.h,0,0,WhitePixel(app.dpy,app.screen));
    XStoreName(app.dpy,app.win,APP_TITLE);
    hints.flags=PMinSize;hints.min_width=MIN_W;hints.min_height=MIN_H;
    XSetWMNormalHints(app.dpy,app.win,&hints);
    XSelectInput(app.dpy,app.win,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask);
    app.wm_delete=XInternAtom(app.dpy,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(app.dpy,app.win,&app.wm_delete,1);
    app.gc=XCreateGC(app.dpy,app.win,0,NULL);
    for(int i=0;i<COLOR_COUNT;i++){
        if(XAllocNamedColor(app.dpy,DefaultColormap(app.dpy,app.screen),color_names[i],&actual,&exact))
            app.colors[i]=actual.pixel;
        else app.colors[i]=BlackPixel(app.dpy,app.screen);
    }
    app.f_small=load_font("-adobe-helvetica-medium-r-normal--13-130-75-75-p-*-iso8859-1","7x13","fixed");
    app.f_normal=load_font("-adobe-helvetica-medium-r-normal--16-160-75-75-p-*-iso8859-1","9x15","fixed");
    app.f_bold=load_font("-adobe-helvetica-bold-r-normal--16-160-75-75-p-*-iso8859-1","9x15bold","9x15");
    app.f_title=load_font("-adobe-helvetica-bold-r-normal--23-230-75-75-p-*-iso8859-1","10x20","9x15bold");
    app.f_number=load_font("-adobe-helvetica-bold-r-normal--25-250-75-75-p-*-iso8859-1","10x20","9x15bold");
    app.back=XCreatePixmap(app.dpy,app.win,(unsigned)app.w,(unsigned)app.h,
                           (unsigned)DefaultDepth(app.dpy,app.screen));
    XMapWindow(app.dpy,app.win);
    return 0;
}
int main(void){
    XEvent ev;char err[180];
    memset(&app,0,sizeof(app));app.focus=F_NONE;
    if(storage_load(&app.store,err,sizeof(err))<0){
        fprintf(stderr,"invest: %s\nNenhum arquivo existente foi alterado.\n",err);
        return EXIT_FAILURE;
    }
    if(!app.store.count){scenario_default(&app.store.items[0]);app.store.count=1;app.store.selected=0;app.dirty=1;}
    app.cmp_a=0;app.cmp_b=app.store.count>1?1:0;
    app.running=1;
    if(setup()<0){if(app.dpy)shutdown();return EXIT_FAILURE;}
    while(app.running){
        XNextEvent(app.dpy,&ev);
        switch(ev.type){
        case Expose:if(!ev.xexpose.count)paint();break;
        case ButtonPress:mouse_press(ev.xbutton.x,ev.xbutton.y,ev.xbutton.button);paint();break;
        case KeyPress:handle_key(&ev.xkey);paint();break;
        case ConfigureNotify:
            if(app.w!=ev.xconfigure.width||app.h!=ev.xconfigure.height){
                app.w=ev.xconfigure.width;app.h=ev.xconfigure.height;
                XFreePixmap(app.dpy,app.back);
                app.back=XCreatePixmap(app.dpy,app.win,(unsigned)app.w,(unsigned)app.h,
                                       (unsigned)DefaultDepth(app.dpy,app.screen));
                paint();
            }break;
        case ClientMessage:
            if((Atom)ev.xclient.data.l[0]==app.wm_delete){
                if(end_focus()<0)break;
                if(app.dirty&&save()<0)break;
                app.running=0;
            }break;
        default:break;
        }
    }
    shutdown();return EXIT_SUCCESS;
}

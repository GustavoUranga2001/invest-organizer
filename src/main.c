#define _POSIX_C_SOURCE 200809L
#include "finance.h"
#include "storage.h"
#include "lock.h"
#include "budget.h"
#include "monthly.h"
#include <time.h>
#include "utf8.h"
#include "compose.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xlib.h>
#include <ctype.h>
#include <errno.h>
#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* invest v0.3.11: Dark Elegant + renderizacao orientada a eventos.
 * C11, Xlib e Cairo. Sem GTK, Qt, browser, timers ou polling. */
#define APP_TITLE "invest 0.3.11"
#define SIDE 242
#define MAX_CLICKS 240
#define MAX_EDIT 128
#define MIN_W 940
#define MIN_H 740

typedef struct { int x,y,w,h; } Rect;
typedef struct { double size; int bold; } AppFont;
typedef enum {
    BG, CARD, INK, MUTED, BORDER, BLUE, BLUE_PALE, GREEN,
    GREEN_PALE, RED, RED_PALE, LIGHT, SIDE_BG, GRID, SHADOW, PURPLE,
    SIDE_ACTIVE, SIDE_TEXT, COLOR_COUNT
} Color;
typedef enum { SIMULATE, EVENTS, COMPARE, FINANCES } AppView;
typedef enum { MODAL_NONE, MODAL_EVENT_EDIT, MODAL_BUDGET_EDIT, MODAL_INVESTMENT_EDIT, MODAL_CONFIRM } Modal;
typedef enum { F_NONE = -1, F_NAME, F_INITIAL, F_MONTHLY, F_RATE, F_MONTHS, F_EVENT_MONTH, F_EVENT_VALUE, F_EVENT_DURATION, F_BUDGET_NAME, F_BUDGET_AMOUNT, F_INVESTMENT_AMOUNT } Field;
typedef enum {
    A_NOTHING, A_SCREEN_SIM, A_SCREEN_EVENTS, A_SCREEN_COMPARE, A_SCREEN_FINANCES, A_NEW, A_DUPLICATE, A_DELETE,
    A_CHOOSE, A_SAVE, A_OPEN_EVENTS, A_CLOSE_MODAL, A_EVENT_RATE_TAB, A_EVENT_MONTHLY_TAB,
    A_EVENT_EXTRA_TAB, A_EVENT_NEW, A_EVENT_EDIT, A_EVENT_DELETE, A_EVENT_SAVE,
    A_EVENT_CANCEL, A_EVENT_FOREVER, A_EVENT_TEMP, A_CONFIRM_OK, A_CONFIRM_CANCEL, A_CMP_A, A_CMP_B,
    A_FIELD, A_LIST_PREV, A_LIST_NEXT, A_EVENTS_PREV, A_EVENTS_NEXT,
    A_BUDGET_INCOME, A_BUDGET_EXPENSE, A_BUDGET_NEW, A_BUDGET_EDIT, A_BUDGET_DELETE,
    A_BUDGET_CATEGORY, A_BUDGET_FIXED, A_BUDGET_VARIABLE, A_BUDGET_SAVE, A_BUDGET_CANCEL,
    A_BUDGET_PREV, A_BUDGET_NEXT, A_INVESTMENT_EDIT, A_INVESTMENT_SAVE, A_INVESTMENT_CANCEL,
    A_MONTH_PREV, A_MONTH_NEXT, A_MONTH_PLAN, A_MONTH_ACTUAL, A_MONTH_COPY, A_MONTH_YEAR, A_MONTH_CHART
} Action;
typedef enum { CONFIRM_SCENARIO, CONFIRM_EVENT, CONFIRM_BUDGET } ConfirmKind;

typedef struct { Rect box; Action action; int arg; } Hit;

typedef struct {
    Display *dpy;
    XIM im;
    XIC ic;
    char dead_pending; /* acento aguardando composicao em campo de texto */
    Window win;
    int screen;
    GC gc;
    Pixmap back;
    Atom wm_delete;
    AppFont *f_small,*f_normal,*f_bold,*f_title,*f_number;
    cairo_surface_t *surface;
    cairo_t *cr;
    int w,h,running,dirty,budget_dirty,frame_ready;
    const AppFont *last_font;
    struct {
        int valid, year;
        double values[2][12][4];
        signed char present[12], have[2][12];
    } annual_cache;
    ScenarioStore store;
    BudgetStore budget; /* Active planned or actual budget. */
    MonthlyBudget monthly;
    int budget_year, budget_month, budget_mode; /* 0 planned, 1 actual */
    int year_view, chart_metric;
    BudgetKind budget_filter;
    int budget_edit_index,budget_category,budget_fixed,budget_scroll;
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
/* Dark Elegant: cores opacas e sem sombras/blur.
 * As cores sao convertidas em RGB uma unica vez no inicio. */
static const char *color_names[COLOR_COUNT] = {
    "#111A22", /* BG */
    "#1C2A34", /* CARD */
    "#F1F4F5", /* INK */
    "#A7B7C1", /* MUTED */
    "#354751", /* BORDER */
    "#79C8A9", /* BLUE: acento da interface */
    "#2F5149", /* BLUE_PALE */
    "#8DD7B2", /* GREEN */
    "#244139", /* GREEN_PALE */
    "#F19AA2", /* RED */
    "#4B2D36", /* RED_PALE */
    "#253641", /* LIGHT: campos */
    "#17232D", /* SIDE_BG */
    "#293943", /* GRID */
    "#111A22", /* SHADOW (sem sombra real) */
    "#8DAAD8", /* PURPLE: segundo cenario */
    "#29423F", /* SIDE_ACTIVE */
    "#BCCBD1"  /* SIDE_TEXT */
};
static double palette[COLOR_COUNT][3];
static void palette_init(void){
    for(int i=0;i<COLOR_COUNT;i++){
        unsigned int r=0,g=0,b=0;
        if(sscanf(color_names[i],"#%02x%02x%02x",&r,&g,&b)==3){
            palette[i][0]=r/255.0; palette[i][1]=g/255.0; palette[i][2]=b/255.0;
        }
    }
}
static int inside(int x,int y,Rect r) { return x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h; }
static int imin(int a,int b) { return a<b?a:b; }
static int imax(int a,int b) { return a>b?a:b; }
static void source_color(Color c) {
    if(c<0 || c>=COLOR_COUNT) c=INK;
    cairo_set_source_rgb(app.cr,palette[c][0],palette[c][1],palette[c][2]);
}
static void fill(Rect r,Color c) {
    if(r.w<=0 || r.h<=0)return;
    source_color(c);
    cairo_rectangle(app.cr,r.x,r.y,r.w,r.h);
    cairo_fill(app.cr);
}
static void line(int x1,int y1,int x2,int y2,Color c) {
    source_color(c);
    cairo_set_line_width(app.cr,1.0);
    cairo_set_line_cap(app.cr,CAIRO_LINE_CAP_BUTT);
    /* Half-pixel alignment keeps horizontal and vertical separators sharp. */
    double offset=(x1==x2 || y1==y2)?0.5:0.0;
    cairo_move_to(app.cr,x1+offset,y1+offset);
    cairo_line_to(app.cr,x2+offset,y2+offset);
    cairo_stroke(app.cr);
}
static void rounded_path(Rect r,double radius) {
    double x=r.x,y=r.y,w=r.w,h=r.h;
    double rr=fmax(0.0,fmin(radius,fmin(w,h)*0.5));
    if(rr<0.5){cairo_rectangle(app.cr,x,y,w,h);return;}
    cairo_new_sub_path(app.cr);
    cairo_arc(app.cr,x+w-rr,y+rr,rr,-1.5707963267948966,0.0);
    cairo_arc(app.cr,x+w-rr,y+h-rr,rr,0.0,1.5707963267948966);
    cairo_arc(app.cr,x+rr,y+h-rr,rr,1.5707963267948966,3.141592653589793);
    cairo_arc(app.cr,x+rr,y+rr,rr,3.141592653589793,4.71238898038469);
    cairo_close_path(app.cr);
}
static void rounded(Rect r,int radius,Color c) {
    if(r.w<=0 || r.h<=0)return;
    source_color(c);
    rounded_path(r,radius);
    cairo_fill(app.cr);
}
static void card(Rect r) {
    if(r.w<=0 || r.h<=0)return;
    rounded(r,11,CARD);
    /* Borda simples e discreta, sem sombras ou efeitos custosos. */
    source_color(BORDER);
    cairo_set_line_width(app.cr,1.0);
    rounded_path((Rect){r.x+1,r.y+1,r.w-2,r.h-2},10);
    cairo_stroke(app.cr);
}
static void font_set(const AppFont *font) {
    if(font==app.last_font)return;
    cairo_select_font_face(app.cr,"Inter",CAIRO_FONT_SLANT_NORMAL,
                           font && font->bold?CAIRO_FONT_WEIGHT_BOLD:CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(app.cr,font?font->size:14.0);
    app.last_font=font;
}
static void txt(int x,int y,const char *s,Color color,AppFont *font) {
    if(!s||!font)return;
    font_set(font);
    source_color(color);
    cairo_move_to(app.cr,x,y);
    /* Cairo accepts UTF-8 directly, including accents and currency symbols. */
    /* Cairo entra num estado de erro persistente com texto UTF-8 quebrado.
     * Sanitizar aqui impede que uma string ruim apague a interface inteira. */
    char safe[4096];
    utf8_clean(safe,sizeof(safe),s);
    cairo_show_text(app.cr,safe);
}
static int tw(const char *s,AppFont *f) {
    cairo_text_extents_t ext;
    if(!s||!f)return 0;
    font_set(f);
    char safe[4096];
    utf8_clean(safe,sizeof(safe),s);
    cairo_text_extents(app.cr,safe,&ext);
    return (int)ceil(ext.x_advance);
}
static void text_right(int x,int y,const char*s,Color c,AppFont*f){txt(x-tw(s,f),y,s,c,f);}
static void text_center(Rect r,int y,const char*s,Color c,AppFont*f){txt(r.x+(r.w-tw(s,f))/2,y,s,c,f);}
static void cut_text(char *target,size_t n,const char *s,int px,AppFont*f) {
    size_t k;
    if(!n)return;
    utf8_clean(target,n,s);
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
    if(app.dirty){
        if(storage_save(&app.store,err,sizeof(err))<0){status(err,1);return -1;}
        app.dirty=0;
    }
    if(app.budget_dirty){
        if(app.budget_mode)app.monthly.actual=app.budget;
        else app.monthly.planned=app.budget;
        if(monthly_save(app.budget_year,app.budget_month,&app.monthly,err,sizeof(err))<0){
            status(err,1);return -1;
        }
        app.budget_dirty=0;
    }
    status("Alterações salvas",0);return 0;
}
static void modified(void){app.dirty=1;}
static void budget_modified(void){
    if(app.budget_mode)app.monthly.actual=app.budget;
    else app.monthly.planned=app.budget;
    app.budget_dirty=1;
    app.annual_cache.valid=0;
}
static void month_mode_set(int mode){
    if(mode==app.budget_mode)return;
    if(app.budget_mode)app.monthly.actual=app.budget;
    else app.monthly.planned=app.budget;
    app.budget_mode=mode;
    app.budget=mode?app.monthly.actual:app.monthly.planned;
    app.budget_scroll=0;
    status("",0);
}
static int month_open(int year,int month){
    MonthlyBudget next;char err[200];int rc;
    if(year<1900||year>2200||month<1||month>12)return -1;
    if(save()<0)return -1;
    rc=monthly_load(year,month,&next,err,sizeof(err));
    if(rc<0){status(err,1);return -1;}
    app.monthly=next;app.budget_year=year;app.budget_month=month;
    app.budget=app.budget_mode?app.monthly.actual:app.monthly.planned;
    app.budget_scroll=0;
    app.annual_cache.valid=0;
    status(rc==1?"Mês sem registros. Use + Adicionar ou Copiar anterior.":"",0);
    return 0;
}
static void month_shift(int delta){
    int y=app.budget_year,m=app.budget_month+delta;
    if(m<1){m=12;y--;}if(m>12){m=1;y++;}
    (void)month_open(y,m);
}
static void month_copy_previous(void){
    int m=app.budget_month-1,y=app.budget_year,rc;MonthlyBudget prev;char err[200];
    BudgetTotals totals;
    if(app.budget_mode){status("Mude para Planejado antes de copiar.",1);return;}
    if(app.budget.count||app.budget.monthly_investment>0.0){
        status("O planejamento deste mês já tem dados; cópia cancelada.",1);return;
    }
    if(m<1){m=12;y--;}
    rc=monthly_load(y,m,&prev,err,sizeof(err));
    if(rc<0){status(err,1);return;}
    if(rc==1||(prev.planned.count==0&&prev.planned.monthly_investment==0)){
        status("O mês anterior não tem planejamento para copiar.",1);return;
    }
    if(budget_totals(&prev.planned,&totals)<0){status("Orçamento anterior inválido.",1);return;}
    app.budget=prev.planned;budget_modified();app.budget_scroll=0;
    status("Planejamento copiado; ajuste os valores deste mês.",0);
}
static int add_hit(Rect r,Action a,int arg) {
    if(app.hit_n>=MAX_CLICKS)return -1;
    app.hits[app.hit_n++]=(Hit){r,a,arg};return 0;
}
static void button(Rect r,const char*title,int style,Action action,int arg) {
    char clipped[160];
    Color bg=style==1?BLUE:(style==2?BLUE_PALE:(style==3?RED_PALE:LIGHT));
    Color fg=style==1?BG:(style==3?RED:(style==2?GREEN:INK));
    rounded(r,9,bg);
    cut_text(clipped,sizeof(clipped),title,r.w-20,app.f_bold);
    text_center(r,r.y+r.h/2+5,clipped,fg,app.f_bold);
    add_hit(r,action,arg);
}
static void reset_dead_composition(void);
static void begin_focus(Field f);
static int end_focus(void);
static void display_field(Rect r,const char*name,const char*value,Field field,const char *suffix) {
    char val[160];int active=app.focus==field;
    txt(r.x,r.y-11,name,MUTED,app.f_small);
    rounded(r,8,active?BLUE_PALE:LIGHT);
    if(active) {
        rounded((Rect){r.x+2,r.y+2,r.w-4,r.h-4},7,LIGHT);
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
static char budget_name_text[MAX_EDIT],budget_amount_text[MAX_EDIT];
static char investment_amount_text[MAX_EDIT];
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
        status("Informe o mês e o valor corretamente.",1);return -1;
    }
    if(e.month>candidate.months){status("O mês ultrapassa o prazo do cenário.",1);return -1;}
    if(app.event_type!=INVEST_EXTRAS&&app.event_limited){
        if(parse_month(event_duration_text,&duration)<0 ||
           duration>candidate.months-e.month+1){
            status("Duração inválida: verifique o prazo do cenário.",1);return -1;
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
        status("Esse mês já tem uma regra desse tipo.",1);return -1;
    }
    for(i=*count;i>pos;i--)arr[i]=arr[i-1];
    arr[pos]=e;(*count)++;
    if(!scenario_valid(&candidate)||project_scenario(&candidate,NULL,NULL,NULL)<0){
        status("Regra inválida ou projeção fora de escala.",1);return -1;
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
        if(!text[0]||!utf8_valid(text)||strlen(text)>=sizeof(candidate.name)||strchr(text,'\t')||strchr(text,'\n')){
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
        status("Valor inválido, prazo incompatível ou projeção fora de escala.",1);
        return -1;
    }
    if(memcmp(&candidate,current(),sizeof(candidate))){*current()=candidate;modified();}
    status("",0);return 0;
}
/* O orcamento mensal e independente dos cenarios de investimento. */
static int budget_index(int ordinal){
    int i;
    for(i=0;i<app.budget.count;i++)if(app.budget.items[i].kind==app.budget_filter){
        if(ordinal--==0)return i;
    }
    return -1;
}
static int budget_count_kind(BudgetKind kind){
    int n=0,i;
    for(i=0;i<app.budget.count;i++)if(app.budget.items[i].kind==kind)n++;
    return n;
}
static void launch_budget_editor(int idx){
    app.budget_edit_index=idx;
    app.budget_category=CAT_OUTROS;app.budget_fixed=1;
    budget_name_text[0]=budget_amount_text[0]=0;
    if(idx>=0 && idx<app.budget.count){
        const BudgetItem *it=&app.budget.items[idx];
        snprintf(budget_name_text,sizeof(budget_name_text),"%s",it->name);
        number(it->amount,budget_amount_text,sizeof(budget_amount_text));
        app.budget_category=it->category;app.budget_fixed=it->fixed;
        app.budget_filter=it->kind;
    }
    app.modal=MODAL_BUDGET_EDIT;app.focus=F_NONE;status("",0);
}
static int update_budget(void){
    BudgetStore candidate=app.budget;
    BudgetItem item={0};BudgetTotals check;
    size_t len=strlen(budget_name_text);double val;
    if(len==0 || !utf8_valid(budget_name_text) || len>=sizeof(item.name) || strpbrk(budget_name_text,"\n\r\t")){
        status("Informe uma descricao de ate 63 caracteres.",1);return -1;
    }
    if(parse_number(budget_amount_text,&val)<0 || val<0 || val>1e12){
        status("Informe um valor entre 0 e 1 trilhao.",1);return -1;
    }
    snprintf(item.name,sizeof(item.name),"%s",budget_name_text);
    item.amount=round(val*100.0)/100.0;
    item.kind=app.budget_filter;
    item.category=app.budget_filter==BUDGET_INCOME?CAT_OUTROS:app.budget_category;
    item.fixed=app.budget_filter==BUDGET_INCOME?1:app.budget_fixed;
    if(!budget_valid_item(&item)){status("Lançamento inválido.",1);return -1;}
    if(app.budget_edit_index>=0){
        if(app.budget_edit_index>=candidate.count)return -1;
        candidate.items[app.budget_edit_index]=item;
    }else{
        if(candidate.count>=BUDGET_MAX_ITEMS){status("Limite de 128 lancamentos.",1);return -1;}
        candidate.items[candidate.count++]=item;
    }
    if(budget_totals(&candidate,&check)<0){status("Orcamento invalido.",1);return -1;}
    app.budget=candidate;budget_modified();app.modal=MODAL_NONE;app.budget_scroll=0;
    status("Lançamento atualizado.",0);return 0;
}
static void launch_investment_editor(void){
    number(app.budget.monthly_investment,investment_amount_text,sizeof(investment_amount_text));
    app.modal=MODAL_INVESTMENT_EDIT;app.focus=F_NONE;status("",0);
}
static int update_investment(void){
    BudgetStore candidate=app.budget;
    BudgetTotals check;double amount;
    if(parse_number(investment_amount_text,&amount)<0 || amount<0 || amount>1e12){
        status("Informe um investimento mensal válido (0 a 1 trilhão).",1);return -1;
    }
    candidate.monthly_investment=round(amount*100.0)/100.0;
    if(budget_totals(&candidate,&check)<0){status("Valor invalido.",1);return -1;}
    if(candidate.monthly_investment!=app.budget.monthly_investment){
        app.budget=candidate;budget_modified();
    }
    app.modal=MODAL_NONE;status("Valor reservado para investir atualizado.",0);return 0;
}
static void remove_budget(void){
    int i=app.budget_edit_index;
    if(i<0||i>=app.budget.count)return;
    for(;i<app.budget.count-1;i++)app.budget.items[i]=app.budget.items[i+1];
    app.budget.count--;app.budget_scroll=0;budget_modified();
    status("Lançamento removido.",0);
}
static void begin_focus(Field f){
    char buf[MAX_EDIT]={0};
    if(app.focus==f)return;
    if(end_focus()<0)return;
    app.focus=f;
    if(f==F_EVENT_MONTH)snprintf(buf,sizeof(buf),"%s",event_month_text);
    else if(f==F_EVENT_VALUE)snprintf(buf,sizeof(buf),"%s",event_value_text);
    else if(f==F_EVENT_DURATION)snprintf(buf,sizeof(buf),"%s",event_duration_text);
    else if(f==F_BUDGET_NAME)snprintf(buf,sizeof(buf),"%s",budget_name_text);
    else if(f==F_BUDGET_AMOUNT)snprintf(buf,sizeof(buf),"%s",budget_amount_text);
    else if(f==F_INVESTMENT_AMOUNT)snprintf(buf,sizeof(buf),"%s",investment_amount_text);
    else get_field_text(f,buf,sizeof(buf));
    snprintf(app.input,sizeof(app.input),"%s",buf);
    app.caret=(int)strlen(app.input);app.sel_all=1;
}
static int end_focus(void){
    Field f=app.focus;
    if(app.dead_pending)reset_dead_composition();
    if(f==F_NONE)return 0;
    /* Nome vazio nunca deve bloquear a navegacao nem alterar o cenario.
     * Descarta somente a edicao incompleta, preservando todos os dados. */
    if(f==F_NAME){
        const unsigned char *p=(const unsigned char*)app.input;
        while(*p==' '||*p=='\t'||*p=='\r'||*p=='\n')p++;
        if(!*p || !utf8_valid(app.input) || strlen(app.input)>=sizeof(current()->name) ||
           strpbrk(app.input,"\r\n\t")){
            app.focus=F_NONE;app.sel_all=0;app.caret=0;
            snprintf(app.input,sizeof(app.input),"%s",current()->name);
            status("Nome inválido descartado. Dados do cenário preservados.",1);
            return 1;
        }
    }
    if(f==F_EVENT_MONTH)snprintf(event_month_text,sizeof(event_month_text),"%s",app.input);
    else if(f==F_EVENT_VALUE)snprintf(event_value_text,sizeof(event_value_text),"%s",app.input);
    else if(f==F_EVENT_DURATION)snprintf(event_duration_text,sizeof(event_duration_text),"%s",app.input);
    else if(f==F_BUDGET_NAME)snprintf(budget_name_text,sizeof(budget_name_text),"%s",app.input);
    else if(f==F_BUDGET_AMOUNT)snprintf(budget_amount_text,sizeof(budget_amount_text),"%s",app.input);
    else if(f==F_INVESTMENT_AMOUNT)snprintf(investment_amount_text,sizeof(investment_amount_text),"%s",app.input);
    else if(apply_field(f,app.input)<0)return -1;
    app.focus=F_NONE;app.sel_all=0;return 0;
}
static void scenario_select(int n){
    if(n<0||n>=app.store.count)return;
    int rc=end_focus();
    if(rc<0)return;
    app.store.selected=n;
    modified();
    if(rc==0)status("",0);
}
static int list_visible(void);
static void scenario_new(int duplicate){
    Scenario s;int num=1,i,used;
    if(end_focus()<0)return;
    if(app.store.count==INVEST_MAX_SCENARIOS){status("Limite de 32 cenários.",1);return;}
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
    if(app.store.count<=1){status("Pelo menos um cenário precisa permanecer.",1);return;}
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
    if(selected){
        rounded(r,7,SIDE_ACTIVE);
        rounded((Rect){r.x+9,r.y+r.h-4,r.w-18,3},2,BLUE);
    }
    text_center(r,r.y+r.h/2+4,name,selected?INK:SIDE_TEXT,app.f_bold);
    add_hit(r,action,0);
}
static void draw_header(void){
    Rect savebtn={app.w-159,18,132,38};
    fill((Rect){0,0,app.w,74},SIDE_BG);
    txt(22,46,"invest.",INK,app.f_title);
    txt(114,42,"v0.3.11",SIDE_TEXT,app.f_small);
    header_tab((Rect){251,17,114,39},"SIMULADOR",app.view==SIMULATE,A_SCREEN_SIM);
    header_tab((Rect){374,17,96,39},"EVENTOS",app.view==EVENTS,A_SCREEN_EVENTS);
    header_tab((Rect){479,17,112,39},"COMPARAR",app.view==COMPARE,A_SCREEN_COMPARE);
    header_tab((Rect){600,17,115,39},"FINANÇAS",app.view==FINANCES,A_SCREEN_FINANCES);
    button(savebtn,app.dirty||app.budget_dirty?"Salvar agora":"Salvar",1,A_SAVE,0);
}
static void draw_side(void){
    int y,i,visible;
    if(app.view==FINANCES){
        BudgetTotals t;char text[80];
        budget_totals(&app.budget,&t);
        fill((Rect){0,74,SIDE,app.h-74},SIDE_BG);
        line(SIDE-1,74,SIDE-1,app.h,SIDE_ACTIVE);
        txt(23,116,"MEU ORÇAMENTO",SIDE_TEXT,app.f_bold);
        {char when[70];snprintf(when,sizeof(when),"%02d/%04d  |  %s",app.budget_month,app.budget_year,
         app.budget_mode?"Realizado":"Planejado");
         txt(23,148,when,SIDE_TEXT,app.f_small);}
        line(22,130,SIDE-22,130,SIDE_ACTIVE);
        txt(23,177,"Receitas mensais",SIDE_TEXT,app.f_small);
        money(t.income,text,sizeof(text));txt(23,207,text,INK,app.f_bold);
        txt(23,253,"Despesas mensais",SIDE_TEXT,app.f_small);
        money(t.expenses,text,sizeof(text));txt(23,283,text,INK,app.f_bold);
        txt(23,329,"Reservado para investir",SIDE_TEXT,app.f_small);
        money(t.monthly_investment,text,sizeof(text));txt(23,358,text,INK,app.f_bold);
        line(22,382,SIDE-22,382,SIDE_ACTIVE);
        txt(23,414,"SALDO DISPONIVEL",SIDE_TEXT,app.f_small);
        money(t.balance,text,sizeof(text));
        txt(23,444,text,t.balance>=0?INK:RED,app.f_bold);
        txt(23,512,"Orçamento mensal",SIDE_TEXT,app.f_small);
        txt(23,533,"sem conexão bancária.",SIDE_TEXT,app.f_small);
        txt(23,567,"Reserva sem vinculação",SIDE_TEXT,app.f_small);
        txt(23,588,"a ativos do simulador.",SIDE_TEXT,app.f_small);
        txt(22,app.h-18,"Dados locais  |  Linux",SIDE_TEXT,app.f_small);
        return;
    }
    fill((Rect){0,74,SIDE,app.h-74},SIDE_BG);
    line(SIDE-1,74,SIDE-1,app.h,SIDE_ACTIVE);
    txt(23,116,"CENÁRIOS",SIDE_TEXT,app.f_bold);
    line(22,128,SIDE-22,128,SIDE_ACTIVE);
    visible=list_visible();clamp_list();
    for(i=0;i<visible&&i+app.sidebar_scroll<app.store.count;i++){
        int n=i+app.sidebar_scroll;Rect r={13,142+i*62,SIDE-26,54};
        Scenario*s=&app.store.items[n];char title[80],sub[100];
        if(n==app.store.selected){
            rounded(r,8,SIDE_ACTIVE);
            rounded((Rect){r.x,r.y+8,3,r.h-16},2,BLUE);
        }
        cut_text(title,sizeof(title),s->name,r.w-28,app.f_bold);
        txt(r.x+13,r.y+23,title,INK,app.f_bold);
        snprintf(sub,sizeof(sub),"%d meses  |  %.2f%% ao mês",s->months,s->rate_percent);
        txt(r.x+13,r.y+43,sub,SIDE_TEXT,app.f_small);
        add_hit(r,A_CHOOSE,n);
    }
    y=app.h-87;
    if(app.store.count>visible){
        button((Rect){23,y-41,87,29},"Anterior",0,A_LIST_PREV,0);
        button((Rect){132,y-41,87,29},"Proximo",0,A_LIST_NEXT,0);
    }
    button((Rect){18,y,SIDE-36,39},"+ Novo cenário",1,A_NEW,0);
    txt(22,app.h-18,"Dados locais  |  Linux",SIDE_TEXT,app.f_small);
}
static void draw_footer(void){
    fill((Rect){SIDE,app.h-36,app.w-SIDE,36},BG);
    line(SIDE,app.h-36,app.w,app.h-36,BORDER);
    char msg[240];
    cut_text(msg,sizeof(msg),app.status[0]?app.status:"Dados locais  |  Ctrl+S para salvar",app.w-SIDE-224,app.f_small);
    txt(SIDE+28,app.h-13,msg,app.error?RED:MUTED,app.f_small);
    text_right(app.w-27,app.h-13,(app.dirty||app.budget_dirty)?"Alteracoes pendentes":"Tudo salvo",(app.dirty||app.budget_dirty)?BLUE:GREEN,app.f_small);
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
    /* Draw each series as one antialiased path; no pixel-by-pixel stair steps. */
    cairo_save(app.cr);
    cairo_rectangle(app.cr,plotx,ploty,plotw,ploth);
    cairo_clip(app.cr);
    if(!b){
        source_color(PURPLE);
        cairo_set_line_width(app.cr,2.0);
        cairo_set_line_join(app.cr,CAIRO_LINE_JOIN_ROUND);
        cairo_set_line_cap(app.cr,CAIRO_LINE_CAP_ROUND);
        for(i=0;i<=a->months;i++){
            double px=plotx+(double)plotw*i/end;
            double py=ploty+ploth-(capitalA[i]/ymax)*ploth;
            if(i==0)cairo_move_to(app.cr,px,py);else cairo_line_to(app.cr,px,py);
        }
        cairo_stroke(app.cr);
    }
    if(!b){
        source_color(BLUE);
        cairo_set_line_width(app.cr,1.0);
        cairo_move_to(app.cr,plotx,ploty+ploth);
        for(i=0;i<=a->months;i++){
            double px=plotx+(double)plotw*i/end;
            double py=ploty+ploth-(balancesA[i]/ymax)*ploth;
            cairo_line_to(app.cr,px,py);
        }
        cairo_line_to(app.cr,plotx+plotw,ploty+ploth);
        cairo_close_path(app.cr);
        cairo_set_source_rgba(app.cr,palette[BLUE][0],palette[BLUE][1],palette[BLUE][2],0.11);
        cairo_fill(app.cr);
    }
    source_color(BLUE);
    cairo_set_line_width(app.cr,2.5);
    cairo_set_line_join(app.cr,CAIRO_LINE_JOIN_ROUND);
    cairo_set_line_cap(app.cr,CAIRO_LINE_CAP_ROUND);
    for(i=0;i<=a->months;i++){
        double px=plotx+(double)plotw*i/end;
        double py=ploty+ploth-(balancesA[i]/ymax)*ploth;
        if(i==0)cairo_move_to(app.cr,px,py);else cairo_line_to(app.cr,px,py);
    }
    cairo_stroke(app.cr);
    if(b){
        source_color(PURPLE);
        cairo_set_line_width(app.cr,2.7);
        for(i=0;i<=b->months;i++){
            double px=plotx+(double)plotw*i/end;
            double py=ploty+ploth-(balancesB[i]/ymax)*ploth;
            if(i==0)cairo_move_to(app.cr,px,py);else cairo_line_to(app.cr,px,py);
        }
        cairo_stroke(app.cr);
    }
    cairo_restore(app.cr);
    txt(r.x+25,r.y+27,b?"Evolução dos dois cenários":"Evolução do patrimônio",INK,app.f_bold);
    rounded((Rect){r.x+r.w-271,r.y+15,11,11},5,BLUE);
    txt(r.x+r.w-252,r.y+25,b?"Cenário A":"Patrimônio",MUTED,app.f_small);
    rounded((Rect){r.x+r.w-131,r.y+15,11,11},5,PURPLE);
    txt(r.x+r.w-112,r.y+25,b?"Cenário B":"Investido",MUTED,app.f_small);
}
static void draw_sim(void){
    int x=SIDE+30,w=app.w-SIDE-60,half=(w-51)/2;
    Rect r;char b1[80],b2[80],b3[80],b4[80];
    Scenario*s=current();Projection p;
    if(project_scenario(s,&p,NULL,NULL)<0){
        memset(&p,0,sizeof(p));status("Projecao fora da escala numerica.",1);
    }
    display_field((Rect){x,99,w-268,43},"Nome do cenário",s->name,F_NAME,NULL);
    button((Rect){x+w-236,100,112,39},"Duplicar",0,A_DUPLICATE,0);
    button((Rect){x+w-112,100,112,39},"Excluir",3,A_DELETE,0);
    txt(x,163,"Preencha os campos e acompanhe o patrimônio projetado.",MUTED,app.f_normal);
    r=(Rect){x,179,w,199};card(r);
    txt(x+22,206,"Dados do investimento",INK,app.f_bold);
    button((Rect){x+w-212,188,189,32},"Eventos e aportes  >",2,A_OPEN_EVENTS,0);
    number(s->initial,b1,sizeof(b1));number(s->monthly,b2,sizeof(b2));
    number(s->rate_percent,b3,sizeof(b3));snprintf(b4,sizeof(b4),"%d",s->months);
    display_field((Rect){x+24,234,half-6,41},"Valor inicial",b1,F_INITIAL,"R$");
    display_field((Rect){x+31+half,234,half-6,41},"Aporte mensal",b2,F_MONTHLY,"R$");
    display_field((Rect){x+24,299,half-6,41},"Juros por mês",b3,F_RATE,"%");
    display_field((Rect){x+31+half,299,half-6,41},"Prazo",b4,F_MONTHS,"meses");
    /* Equivalencia composta: (1 + taxa mensal/100)^12 - 1. Taxa BASE,
       mesmo quando o cenario inclui mudancas futuras na aba Eventos. */
    {
        double annual=annual_equivalent_rate(s->rate_percent);
        char percent[40];
        number(annual,percent,sizeof(percent));
        txt(x+24,362,s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",MUTED,app.f_small);
        txt(x+24+tw(s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",app.f_small)+8,
            362,percent,GREEN,app.f_bold);
        txt(x+24+tw(s->rate_count?"Taxa base anual equivalente:":"Taxa anual equivalente:",app.f_small)+
            tw(percent,app.f_bold)+12,362,"% a.a.",GREEN,app.f_small);
        if(s->rate_count||s->monthly_count||s->extra_count){
            char changes[96];
            snprintf(changes,sizeof(changes),"%d juros | %d mensais | %d extras",s->rate_count,s->monthly_count,s->extra_count);
            txt(x+31+half,362,changes,MUTED,app.f_small);
        }
    }
    r=(Rect){x,389,w,117};card(r);
    txt(x+24,413,"Resultado projetado",INK,app.f_bold);
    money(p.final_balance,b1,sizeof(b1));
    txt(x+24,452,b1,INK,app.f_number);
    txt(x+24,488,"Montante final estimado",MUTED,app.f_small);
    line(x+w/2,410,x+w/2,496,BORDER);
    money(p.contributions,b2,sizeof(b2));money(p.earnings,b3,sizeof(b3));
    txt(x+w/2+20,419,"Total investido",MUTED,app.f_small);
    txt(x+w/2+20,442,b2,INK,app.f_bold);
    txt(x+w/2+20,465,"Juros acumulados",MUTED,app.f_small);
    txt(x+w/2+20,491,b3,p.earnings>=0?GREEN:RED,app.f_bold);
    r=(Rect){x,518,w,app.h-571};card(r);
    draw_graph(r,s,NULL);
}
static void draw_compare(void){
    int x=SIDE+30,w=app.w-SIDE-60,top=123;Rect r;
    const Scenario *a=&app.store.items[app.cmp_a];
    const Scenario *b=&app.store.items[app.cmp_b];
    Projection pa,pb;char v[96],text[120];int split=(w-14)/2;
    if(project_scenario(a,&pa,NULL,NULL)<0)memset(&pa,0,sizeof(pa));
    if(project_scenario(b,&pb,NULL,NULL)<0)memset(&pb,0,sizeof(pb));
    txt(x,top,"Comparar cenários",INK,app.f_title);
    txt(x,top+25,app.store.count>1?"Veja os resultados e as curvas lado a lado.":
        "Crie mais um cenário para comparar resultados diferentes.",MUTED,app.f_normal);
    button((Rect){x,181,split,42},a->name,2,A_CMP_A,0);
    button((Rect){x+split+14,181,split,42},b->name,0,A_CMP_B,0);
    for(int i=0;i<2;i++){
        const Projection*p=i?&pb:&pa;const Scenario*s=i?b:a;
        int colx=x+i*(split+14);
        r=(Rect){colx,240,split,153};card(r);
        txt(colx+20,269,i?"CENÁRIO B":"CENÁRIO A",MUTED,app.f_small);
        money(p->final_balance,v,sizeof(v));txt(colx+20,309,v,INK,app.f_number);
        money(p->earnings,v,sizeof(v));
        snprintf(text,sizeof(text),"Juros: %s",v);txt(colx+20,343,text,p->earnings>=0?GREEN:RED,app.f_bold);
        snprintf(text,sizeof(text),"%d meses | %.2f%% ao mês",s->months,s->rate_percent);
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
    r=(Rect){x,512,w,app.h-588};card(r);
    draw_graph(r,a,b);
    txt(x,app.h-51,"Comparacao nominal, sem impostos ou inflacao.",MUTED,app.f_small);
}
/* Monthly personal finance: each calendar month has independent planned/actual. */
static const char *const month_labels[12]={
    "Janeiro","Fevereiro","Março","Abril","Maio","Junho",
    "Julho","Agosto","Setembro","Outubro","Novembro","Dezembro"
};
static Rect budget_list_box(void){
    return (Rect){SIDE+30,478,app.w-SIDE-60,app.h-531};
}
static int budget_visible(void){
    Rect r=budget_list_box();return imax(1,(r.h-123)/57);
}
static void budget_amount_text_draw(int x,int y,int width,double value,Color col){
    char buf[80],clipped[80];
    money(value,buf,sizeof(buf));
    if(tw(buf,app.f_number)<=width)txt(x,y,buf,col,app.f_number);
    else{cut_text(clipped,sizeof(clipped),buf,width,app.f_bold);txt(x,y,clipped,col,app.f_bold);}
}
static double monthly_metric(const BudgetTotals *t,int metric){
    switch(metric){
    case 1:return t->income;
    case 2:return t->expenses;
    case 3:return t->monthly_investment;
    default:return t->balance;
    }
}
/* Cache anual: a v0.3.9 consultava ate 12 arquivos em CADA repintura.
 * Agora a leitura so ocorre ao entrar no ano ou alterar seus registros. */
static void ensure_annual_cache(void){
    if(app.annual_cache.valid && app.annual_cache.year==app.budget_year)return;
    memset(&app.annual_cache,0,sizeof(app.annual_cache));
    app.annual_cache.year=app.budget_year;
    for(int m=1;m<=12;m++){
        MonthlyBudget data={0};char err[180];int rc;
        if(m==app.budget_month){data=app.monthly;rc=0;}
        else rc=monthly_load(app.budget_year,m,&data,err,sizeof(err));
        if(rc<0){app.annual_cache.present[m-1]=-1;continue;}
        if(rc==1)continue;
        app.annual_cache.present[m-1]=1;
        for(int mode=0;mode<2;mode++){
            const BudgetStore *b=mode?&data.actual:&data.planned;
            BudgetTotals t;
            if(budget_totals(b,&t)<0)continue;
            app.annual_cache.have[mode][m-1]=(b->count>0||b->monthly_investment>0);
            for(int metric=0;metric<4;metric++)
                app.annual_cache.values[mode][m-1][metric]=monthly_metric(&t,metric);
        }
    }
    app.annual_cache.valid=1;
}
static void draw_year_chart(Rect r){
    static const char *const labels[]={"Jan","Fev","Mar","Abr","Mai","Jun","Jul","Ago","Set","Out","Nov","Dez"};
    static const char *const metrics[]={"Saldo disponível","Receitas","Despesas","Investimentos"};
    double mn=0,mx=0;
    int x=r.x+25,y=r.y+83,w=r.w-50,h=r.h-145;
    char buf[100],title[140];
    ensure_annual_cache();
    card(r);
    snprintf(title,sizeof(title),"Evolução de %s em %d",metrics[app.chart_metric],app.budget_year);
    txt(r.x+19,r.y+32,title,INK,app.f_bold);
    button((Rect){r.x+r.w-160,r.y+13,139,33},"Trocar indicador",0,A_MONTH_CHART,0);
    rounded((Rect){r.x+21,r.y+47,12,12},3,BLUE);
    txt(r.x+40,r.y+58,"Planejado",MUTED,app.f_small);
    rounded((Rect){r.x+142,r.y+47,12,12},3,PURPLE);
    txt(r.x+161,r.y+58,"Realizado",MUTED,app.f_small);
    for(int m=0;m<12;m++)for(int mode=0;mode<2;mode++){
        if(!app.annual_cache.have[mode][m])continue;
        double v=app.annual_cache.values[mode][m][app.chart_metric];
        mn=fmin(mn,v);mx=fmax(mx,v);
    }
    if(h<60){txt(x,r.y+104,"Aumente a janela para ver o gráfico anual.",MUTED,app.f_small);return;}
    if(mx-mn<1.0){mx+=1;mn-=1;}
    double range=mx-mn;
    for(int tick=0;tick<=3;tick++){
        int yy=y+h-tick*h/3;double v=mn+tick*range/3;
        line(x,yy,x+w,yy,GRID);
        money(v,buf,sizeof(buf));
        /* Labels compact to keep the graph readable on narrow windows. */
        if(tick==0||tick==3){
            snprintf(buf,sizeof(buf),"%.0f",v);
            txt(x+3,yy-5,buf,MUTED,app.f_small);
        }
    }
    {double base=(0-mn)/range;int zero=y+h-(int)round(h*base);
     if(zero>=y&&zero<=y+h)line(x,zero,x+w,zero,BORDER);}
    for(int m=0;m<12;m++){
        int center=x+(m*2+1)*w/24,bw=imax(5,imin(13,w/42));
        if(app.annual_cache.present[m]==1){
            for(int mode=0;mode<2;mode++){
                if(!app.annual_cache.have[mode][m])continue;
                double v=app.annual_cache.values[mode][m][app.chart_metric];
                int yy=y+h-(int)round((v-mn)/range*h);
                int zy=y+h-(int)round((0-mn)/range*h);
                int top=imin(yy,zy),hh=abs(yy-zy);
                rounded((Rect){center+(mode?2:-bw-2),top,bw,imax(2,hh)},2,mode?PURPLE:BLUE);
            }
        }else if(app.annual_cache.present[m]<0){
            txt(center-4,y+h/2,"!",RED,app.f_bold);
        }
        txt(center-11,y+h+21,labels[m],m+1==app.budget_month?BLUE:MUTED,app.f_small);
    }
    txt(r.x+20,r.y+r.h-14,"Meses sem registros ficam vazios. ! indica arquivo inválido.",MUTED,app.f_small);
}
static void draw_budget(void){
    int x=SIDE+30,w=app.w-SIDE-60,i,j,cell=(w-24)/3;
    BudgetTotals t,planned,actual;Rect r;char buf[120],amount[80],monthtext[120];double used;
    int count,n;
    if(budget_totals(&app.budget,&t)<0||
       budget_totals(&app.monthly.planned,&planned)<0||
       budget_totals(&app.monthly.actual,&actual)<0){status("Orçamento inválido.",1);return;}
    txt(x,113,"Organização financeira",INK,app.f_title);
    button((Rect){x,132,37,34},"<",0,A_MONTH_PREV,0);
    snprintf(monthtext,sizeof(monthtext),"%s de %d",month_labels[app.budget_month-1],app.budget_year);
    txt(x+45,155,monthtext,INK,app.f_bold);
    button((Rect){x+210,132,37,34},">",0,A_MONTH_NEXT,0);
    button((Rect){x+w-206,132,98,34},"Planejado",app.budget_mode==0?1:0,A_MONTH_PLAN,0);
    button((Rect){x+w-102,132,102,34},"Realizado",app.budget_mode==1?1:0,A_MONTH_ACTUAL,0);
    txt(x,183,app.budget_mode?"Registre o que realmente entrou, saiu e foi investido.":
          "Planeje receitas, gastos e quanto pretende investir neste mês.",MUTED,app.f_small);
    for(i=0;i<3;i++){
        int col=x+i*(cell+12);double v=i==0?t.income:i==1?t.expenses:t.balance;
        r=(Rect){col,199,cell,100};card(r);
        txt(col+17,224,i==0?"Receitas":i==1?"Despesas":"Saldo disponível",MUTED,app.f_small);
        budget_amount_text_draw(col+17,265,cell-34,v,i==2?(v>=0?GREEN:RED):INK);
        txt(col+17,285,i==0?"Total recebido/esperado":i==1?"Total de gastos":"Após gastos e investimentos",MUTED,app.f_small);
    }
    r=(Rect){x,309,w,94};card(r);
    txt(x+18,335,app.budget_mode?"Investimentos realizados":"Investimentos planejados",INK,app.f_bold);
    money(t.monthly_investment,amount,sizeof(amount));
    text_right(x+w-181,335,amount,BLUE,app.f_bold);
    button((Rect){x+w-162,315,143,34},"Definir valor",1,A_INVESTMENT_EDIT,0);
    used=t.income>0?(t.expenses+t.monthly_investment)/t.income:0;
    if(t.income>0)snprintf(buf,sizeof(buf),"%.0f%% da renda para gastos e investimentos",used*100.0);
    else snprintf(buf,sizeof(buf),"Cadastre receitas para calcular o percentual.");
    txt(x+19,361,buf,MUTED,app.f_small);
    rounded((Rect){x+19,371,w-38,8},4,LIGHT);
    if(t.income>0){
        double ex=fmin(1.0,t.expenses/t.income),inv=fmin(1.0-ex,t.monthly_investment/t.income);
        int ew=(int)round((w-38)*ex),iw=(int)round((w-38)*inv);
        if(ew>0)rounded((Rect){x+19,371,ew,8},3,GREEN);
        if(iw>0)rounded((Rect){x+19+ew,371,iw,8},3,PURPLE);
    }
    money(planned.balance,buf,sizeof(buf));
    {char temp[165];snprintf(temp,sizeof(temp),"Saldo previsto: %s",buf);
     txt(x+6,428,temp,MUTED,app.f_small);}
    if(app.monthly.actual.count==0&&app.monthly.actual.monthly_investment==0.0)
        txt(x+6,448,"Saldo realizado: sem lançamentos",MUTED,app.f_small);
    else{
        money(actual.balance,buf,sizeof(buf));
        char temp[165];snprintf(temp,sizeof(temp),"Saldo realizado: %s",buf);
        txt(x+6,448,temp,MUTED,app.f_small);
    }
    button((Rect){x+w-307,414,146,37},"Copiar anterior",0,A_MONTH_COPY,0);
    button((Rect){x+w-154,414,154,37},app.year_view?"Lançamentos":"Visão anual",1,A_MONTH_YEAR,0);
    r=budget_list_box();
    if(app.year_view){draw_year_chart(r);return;}
    card(r);
    button((Rect){x+17,r.y+14,110,34},"Receitas",app.budget_filter==BUDGET_INCOME?1:0,A_BUDGET_INCOME,0);
    button((Rect){x+136,r.y+14,115,34},"Despesas",app.budget_filter==BUDGET_EXPENSE?1:0,A_BUDGET_EXPENSE,0);
    button((Rect){x+w-166,r.y+14,146,34},"+ Adicionar",1,A_BUDGET_NEW,0);
    line(x+17,r.y+56,x+w-17,r.y+56,BORDER);
    count=budget_count_kind(app.budget_filter);
    n=budget_visible();
    app.budget_scroll=imin(app.budget_scroll,imax(0,count-n));
    if(!count){
        txt(x+25,r.y+91,app.budget_filter==BUDGET_INCOME?
           "Nenhuma receita registrada neste mês.":"Nenhuma despesa registrada neste mês.",INK,app.f_bold);
        txt(x+25,r.y+114,"Clique em + Adicionar para começar.",MUTED,app.f_small);
    }
    for(i=0;i<n && i+app.budget_scroll<count;i++){
        j=budget_index(i+app.budget_scroll);
        if(j<0)break;
        const BudgetItem *it=&app.budget.items[j];
        Rect row={x+17,r.y+63+i*57,w-34,51};char label[100],val[80],name[90];
        rounded(row,7,LIGHT);
        cut_text(name,sizeof(name),it->name,imax(65,row.w-408),app.f_bold);
        txt(row.x+14,row.y+21,name,INK,app.f_bold);
        if(it->kind==BUDGET_EXPENSE)
            snprintf(label,sizeof(label),"%s | %s",budget_categories[it->category],it->fixed?"Fixa":"Variável");
        else snprintf(label,sizeof(label),"Receita");
        txt(row.x+14,row.y+40,label,MUTED,app.f_small);
        money(it->amount,val,sizeof(val));
        text_right(row.x+row.w-199,row.y+31,val,it->kind==BUDGET_INCOME?GREEN:INK,app.f_bold);
        button((Rect){row.x+row.w-190,row.y+9,85,33},"Editar",0,A_BUDGET_EDIT,j);
        button((Rect){row.x+row.w-98,row.y+9,88,33},"Excluir",3,A_BUDGET_DELETE,j);
    }
    if(count>n){
        button((Rect){x+19,r.y+r.h-41,94,29},"Anterior",0,A_BUDGET_PREV,0);
        button((Rect){x+121,r.y+r.h-41,94,29},"Próximo",0,A_BUDGET_NEXT,0);
        snprintf(buf,sizeof(buf),"%d-%d de %d",app.budget_scroll+1,imin(count,app.budget_scroll+n),count);
        text_right(x+w-24,r.y+r.h-22,buf,MUTED,app.f_small);
    }else{
        txt(x+20,r.y+r.h-18,"Valores deste mês; planejado e realizado são independentes.",MUTED,app.f_small);
    }
}
static Rect modal_box(void){
    int w=imin(740,app.w-110);int h=imin(606,app.h-95);
    return (Rect){(app.w-w)/2,(app.h-h)/2,w,h};
}
static void draw_modal_bg(void){
    cairo_set_source_rgba(app.cr,0.06,0.10,0.11,0.56);
    cairo_rectangle(app.cr,0,0,app.w,app.h);
    cairo_fill(app.cr);
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
    txt(x,119,"Eventos do cenário",INK,app.f_title);
    txt(x,146,"Programe taxas e aportes temporarios ou permanentes.",MUTED,app.f_normal);
    card(r);
    button((Rect){x+20,r.y+21,165,40},"Juros",app.event_type==INVEST_RATES?1:0,A_EVENT_RATE_TAB,0);
    button((Rect){x+194,r.y+21,205,40},"Aportes mensais",app.event_type==INVEST_MONTHLY?1:0,A_EVENT_MONTHLY_TAB,0);
    button((Rect){x+408,r.y+21,175,40},"Aportes extras",app.event_type==INVEST_EXTRAS?1:0,A_EVENT_EXTRA_TAB,0);
    line(x+23,r.y+78,x+w-23,r.y+78,BORDER);
    txt(x+25,r.y+115,title,INK,app.f_bold);
    button((Rect){x+w-199,r.y+92,175,41},"+ Adicionar",1,A_EVENT_NEW,0);
    txt(x+25,r.y+147,app.event_type==INVEST_RATES?
        "Defina uma taxa por alguns meses ou até o fim.":
        app.event_type==INVEST_MONTHLY?
        "Defina um novo aporte mensal por alguns meses ou até o fim.":
        "Acrescente um investimento uma única vez.",MUTED,app.f_small);
    line(x+23,r.y+161,x+w-23,r.y+161,BORDER);
    n=imax(1,(r.h-266)/65);
    if(app.event_scroll>imax(0,count-n))app.event_scroll=imax(0,count-n);
    if(count==0){
        txt(x+29,r.y+218,"Nenhuma alteração cadastrada.",INK,app.f_bold);
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
            snprintf(heading,sizeof(heading),"No mês %d",arr[idx].month);
        else if(arr[idx].end_month==0)
            snprintf(heading,sizeof(heading),"A partir do mês %d",arr[idx].month);
        else
            snprintf(heading,sizeof(heading),"Do mês %d ao %d",arr[idx].month,arr[idx].end_month);
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
    txt(d.x+25,d.y+38,app.event_edit_index<0?"Nova alteração":"Editar alteração",INK,app.f_title);
    txt(d.x+25,d.y+68,app.event_type==INVEST_RATES?
       "Escolha quando os novos juros comecam.":
       app.event_type==INVEST_MONTHLY?
       "Escolha quando o novo aporte mensal comeca.":
       "Defina um aporte extra para um único mês.",MUTED,app.f_normal);
    snprintf(text,sizeof(text),"%s",event_month_text);
    display_field((Rect){d.x+25,d.y+114,d.w-50,43},
                  app.event_type==INVEST_EXTRAS?"Mês do aporte":"Mês inicial",text,F_EVENT_MONTH,"mes");
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
            display_field((Rect){d.x+25,d.y+362,225,43},"Duração",text,F_EVENT_DURATION,"meses");
            txt(d.x+272,d.y+389,"Depois, retoma a regra",MUTED,app.f_small);
            txt(d.x+272,d.y+405,"anterior ainda vigente.",MUTED,app.f_small);
        }else{
            txt(d.x+25,d.y+365,"A nova regra vale ate o fim, exceto quando",MUTED,app.f_small);
            txt(d.x+25,d.y+387,"outra alteração estiver ativa.",MUTED,app.f_small);
        }
    }else{
        txt(d.x+25,d.y+293,"Aporte unico: nao altera o aporte mensal regular.",MUTED,app.f_small);
    }
    button((Rect){d.x+25,d.y+432,122,42},"Cancelar",0,A_EVENT_CANCEL,0);
    button((Rect){d.x+d.w-165,d.y+432,140,42},"Salvar",1,A_EVENT_SAVE,0);
}
static void draw_budget_editor(void){
    Rect r=modal_box(),d={r.x+63,r.y+52,r.w-126,492};char label[96];
    card(r);card(d);
    txt(d.x+25,d.y+38,app.budget_edit_index>=0?"Editar lancamento":"Novo lancamento",INK,app.f_title);
    txt(d.x+25,d.y+68,app.budget_filter==BUDGET_INCOME?
        "Registre a receita deste mês.":
        "Registre uma despesa deste mês.",MUTED,app.f_normal);
    display_field((Rect){d.x+25,d.y+114,d.w-50,43},"Descrição",budget_name_text,F_BUDGET_NAME,NULL);
    display_field((Rect){d.x+25,d.y+198,d.w-50,43},"Valor neste mês",budget_amount_text,F_BUDGET_AMOUNT,"R$");
    if(app.budget_filter==BUDGET_EXPENSE){
        txt(d.x+25,d.y+276,"Categoria",MUTED,app.f_small);
        snprintf(label,sizeof(label),"%s  >",budget_categories[app.budget_category]);
        button((Rect){d.x+25,d.y+288,235,38},label,0,A_BUDGET_CATEGORY,0);
        txt(d.x+25,d.y+357,"Tipo de despesa",MUTED,app.f_small);
        button((Rect){d.x+25,d.y+368,180,39},"Fixa",app.budget_fixed?1:0,A_BUDGET_FIXED,0);
        button((Rect){d.x+214,d.y+368,180,39},"Variável",app.budget_fixed?0:1,A_BUDGET_VARIABLE,0);
    }else{
        txt(d.x+25,d.y+299,"Cada mês tem seus próprios lançamentos.",MUTED,app.f_small);
        txt(d.x+25,d.y+330,"Planejado e realizado são independentes.",MUTED,app.f_small);
    }
    button((Rect){d.x+25,d.y+432,125,42},"Cancelar",0,A_BUDGET_CANCEL,0);
    button((Rect){d.x+d.w-165,d.y+432,140,42},"Salvar",1,A_BUDGET_SAVE,0);
}
static void draw_investment_editor(void){
    Rect outer=modal_box();Rect d={outer.x+73,outer.y+105,outer.w-146,348};
    card(outer);card(d);
    txt(d.x+25,d.y+45,"Investimento mensal",INK,app.f_title);
    txt(d.x+25,d.y+84,"Informe quanto pretende investir ou já investiu no mês.",MUTED,app.f_normal);
    display_field((Rect){d.x+25,d.y+144,d.w-50,46},"Valor deste mês",investment_amount_text,F_INVESTMENT_AMOUNT,"R$");
    txt(d.x+25,d.y+221,"O valor será descontado do saldo disponível.",MUTED,app.f_small);
    txt(d.x+25,d.y+246,"Nao altera os aportes de suas simulacoes.",MUTED,app.f_small);
    button((Rect){d.x+25,d.y+288,128,42},"Cancelar",0,A_INVESTMENT_CANCEL,0);
    button((Rect){d.x+d.w-166,d.y+288,141,42},"Confirmar",1,A_INVESTMENT_SAVE,0);
}
static void draw_confirm(void){
    Rect outer=modal_box();Rect r={outer.x+110,outer.y+115,outer.w-220,286};
    card(outer);card(r);
    txt(r.x+24,r.y+51,"Confirmar exclusao",INK,app.f_title);
    txt(r.x+24,r.y+105,app.confirm==CONFIRM_EVENT?
      "Remover este evento do cenário?":app.confirm==CONFIRM_BUDGET?
      "Excluir este lançamento do orçamento?":"Excluir este cenário e seus eventos?",INK,app.f_normal);
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
    else if(app.view==COMPARE)draw_compare();
    else draw_budget();
    draw_footer();
    if(app.modal!=MODAL_NONE){
        /* Modal bloqueia completamente cliques na tela principal. */
        app.hit_n=0;draw_modal_bg();
        if(app.modal==MODAL_EVENT_EDIT)draw_event_editor();
        if(app.modal==MODAL_BUDGET_EDIT)draw_budget_editor();
        if(app.modal==MODAL_INVESTMENT_EDIT)draw_investment_editor();
        if(app.modal==MODAL_CONFIRM)draw_confirm();
        if(app.status[0]){
            Rect r=modal_box();
            txt(r.x+30,r.y+r.h-62,app.status,app.error?RED:GREEN,app.f_small);
        }
    }
    cairo_surface_flush(app.surface);
    app.frame_ready=1;
    XCopyArea(app.dpy,app.back,app.win,app.gc,0,0,(unsigned)app.w,(unsigned)app.h,0,0);
    XFlush(app.dpy);
}
/* Expose so copia pixels prontos. Nao recalcula graficos nem consulta disco. */
static void present_exposed(const XExposeEvent *ev){
    if(!app.frame_ready){paint();return;}
    int x=imax(0,ev->x),y=imax(0,ev->y);
    int w=imin(ev->width,app.w-x),h=imin(ev->height,app.h-y);
    if(w>0&&h>0)XCopyArea(app.dpy,app.back,app.win,app.gc,x,y,(unsigned)w,(unsigned)h,x,y);
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
    case A_SCREEN_FINANCES:app.view=FINANCES;break;
    case A_MONTH_PREV:month_shift(-1);break;
    case A_MONTH_NEXT:month_shift(1);break;
    case A_MONTH_PLAN:month_mode_set(0);break;
    case A_MONTH_ACTUAL:month_mode_set(1);break;
    case A_MONTH_COPY:month_copy_previous();break;
    case A_MONTH_YEAR:app.year_view=!app.year_view;break;
    case A_MONTH_CHART:app.chart_metric=(app.chart_metric+1)%4;break;
    case A_NEW:scenario_new(0);break;
    case A_DUPLICATE:scenario_new(1);break;
    case A_DELETE:if(app.store.count>1){app.confirm=CONFIRM_SCENARIO;app.modal=MODAL_CONFIRM;}else status("Mantenha pelo menos um cenário.",1);break;
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
        else if(app.confirm==CONFIRM_BUDGET){remove_budget();app.modal=MODAL_NONE;}
        else{remove_event();app.modal=MODAL_NONE;}
        break;
    case A_CONFIRM_CANCEL:app.modal=MODAL_NONE;status("",0);break;
    case A_CMP_A:app.cmp_a=(app.cmp_a+1)%app.store.count;break;
    case A_CMP_B:app.cmp_b=(app.cmp_b+1)%app.store.count;break;
    case A_LIST_PREV:app.sidebar_scroll=imax(0,app.sidebar_scroll-2);break;
    case A_LIST_NEXT:app.sidebar_scroll=imin(imax(0,app.store.count-list_visible()),app.sidebar_scroll+2);break;
    case A_EVENTS_PREV:app.event_scroll=imax(0,app.event_scroll-2);break;
    case A_EVENTS_NEXT:app.event_scroll+=2;break;
    case A_BUDGET_INCOME:app.budget_filter=BUDGET_INCOME;app.budget_scroll=0;break;
    case A_BUDGET_EXPENSE:app.budget_filter=BUDGET_EXPENSE;app.budget_scroll=0;break;
    case A_BUDGET_NEW:launch_budget_editor(-1);break;
    case A_BUDGET_EDIT:launch_budget_editor(arg);break;
    case A_BUDGET_DELETE:
        app.budget_edit_index=arg;app.confirm=CONFIRM_BUDGET;app.modal=MODAL_CONFIRM;break;
    case A_BUDGET_CATEGORY:app.budget_category=(app.budget_category+1)%BUDGET_CATEGORY_COUNT;break;
    case A_BUDGET_FIXED:app.budget_fixed=1;break;
    case A_BUDGET_VARIABLE:app.budget_fixed=0;break;
    case A_BUDGET_SAVE:(void)update_budget();break;
    case A_BUDGET_CANCEL:app.modal=MODAL_NONE;status("",0);break;
    case A_INVESTMENT_EDIT:launch_investment_editor();break;
    case A_INVESTMENT_SAVE:(void)update_investment();break;
    case A_INVESTMENT_CANCEL:app.modal=MODAL_NONE;status("",0);break;
    case A_BUDGET_PREV:app.budget_scroll=imax(0,app.budget_scroll-2);break;
    case A_BUDGET_NEXT:app.budget_scroll+=2;break;
    default:break;
    }
}
static void mouse_press(int x,int y,unsigned button){
    if(button==4 || button==5){
        if(app.modal==MODAL_NONE&&app.view==EVENTS&&inside(x,y,events_area())){
            app.event_scroll=imax(0,app.event_scroll+(button==4?-1:1));
        }else if(app.modal==MODAL_NONE&&app.view==FINANCES && inside(x,y,budget_list_box())){
            app.budget_scroll=imax(0,app.budget_scroll+(button==4?-1:1));
        }else if(app.modal==MODAL_NONE&&app.view!=FINANCES&&inside(x,y,(Rect){0,130,SIDE,app.h-130})){
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
/* O XIM nem sempre esta presente (comum em sessoes Linux minimalistas).
 * Usa os keysyms dead_* para ativar um segundo caminho de composicao. */
static char key_accent(KeySym sym){
    switch(sym){
    case XK_dead_tilde: return '~';
    case XK_dead_acute: return '\'';
    case XK_dead_grave: return '`';
    case XK_dead_circumflex: return '^';
    case XK_dead_diaeresis: return '"';
    case XK_dead_cedilla: return ',';
    case XK_dead_abovering: return 'o';
    default:return 0;
    }
}
static int is_dead_key(KeySym sym){return key_accent(sym)!=0;}
static int focus_is_text(void){
    return app.focus==F_NAME || app.focus==F_BUDGET_NAME;
}
static void reset_dead_composition(void){
    if(app.ic){char *unused=Xutf8ResetIC(app.ic);if(unused)XFree(unused);}
    app.dead_pending=0;
}
/* Recebe texto UTF-8 completo do XIM. Sem XIM, converte o texto do locale
 * com mbrtowc/wcrtomb; como ultimo recurso, aceita Latin-1 valido. */
static int lookup_text(XKeyEvent *ev,KeySym *sym,char *out,size_t size){
    char raw[128];int len=0;
    *sym=NoSymbol;out[0]=0;
    if(app.ic){
        Status st;
        len=Xutf8LookupString(app.ic,ev,raw,(int)sizeof(raw)-1,sym,&st);
        if(st==XBufferOverflow||st==XLookupNone||st==XLookupKeySym)return 0;
    }else{
        len=XLookupString(ev,raw,(int)sizeof(raw)-1,sym,NULL);
    }
    if(len<=0||len>=(int)sizeof(raw))return 0;
    raw[len]=0;
    if(utf8_valid(raw)){
        if(strlen(raw)>=size)return 0;
        memcpy(out,raw,(size_t)len+1);return len;
    }
    /* Fallback para XLookupString em teclados que produzem Latin-1. */
    {
        size_t j=0;
        for(int i=0;i<len;i++){
            unsigned char c=(unsigned char)raw[i];
            size_t need=c<0x80?1:2;
            if(j+need>=size)return 0;
            if(need==1)out[j++]=(char)c;
            else{out[j++]=(char)(0xC0|(c>>6));out[j++]=(char)(0x80|(c&63));}
        }
        out[j]=0;return (int)j;
    }
}
static void handle_key(XKeyEvent *ev){
    KeySym sym;char input[256];
    int len=lookup_text(ev,&sym,input,sizeof(input));
    int ctrl=(ev->state & ControlMask)!=0;
    if(is_dead_key(sym)){
        if(focus_is_text())app.dead_pending=key_accent(sym);
        return; /* tecla morta: aguarda a proxima letra */
    }
    if(sym==XK_BackSpace&&app.dead_pending){
        /* Backspace cancela um acento ainda pendente; nao apaga o nome. */
        reset_dead_composition();return;
    }
    /* Deixar o XIM concluir a composicao quando ele entregar um caractere
     * acentuado. Se ele falhar, compose_feed compoe a partir da letra ASCII. */
    if(ctrl && (sym==XK_s||sym==XK_S)){if(end_focus()==0)(void)save();return;}
    if(sym==XK_Escape){
        reset_dead_composition();
        if(app.focus!=F_NONE){app.focus=F_NONE;status("Edição cancelada.",0);}
        else if(app.modal==MODAL_EVENT_EDIT||app.modal==MODAL_BUDGET_EDIT||app.modal==MODAL_INVESTMENT_EDIT||app.modal==MODAL_CONFIRM){app.modal=MODAL_NONE;}
        return;
    }
    if(app.focus==F_NONE){
        if(sym==XK_F1 && app.modal==MODAL_NONE)app.view=SIMULATE;
        if(sym==XK_F2 && app.modal==MODAL_NONE)app.view=EVENTS;
        if(sym==XK_F3 && app.modal==MODAL_NONE)app.view=COMPARE;
        if(sym==XK_F4 && app.modal==MODAL_NONE)app.view=FINANCES;
        return;
    }
    if(sym==XK_Return||sym==XK_KP_Enter){end_focus();return;}
    if(sym==XK_Tab){
        Field next=app.focus;
        if(app.modal==MODAL_EVENT_EDIT)
            next=app.focus==F_EVENT_MONTH?F_EVENT_VALUE:
                 app.focus==F_EVENT_VALUE&&app.event_limited&&app.event_type!=INVEST_EXTRAS?
                 F_EVENT_DURATION:F_EVENT_MONTH;
        else if(app.modal==MODAL_INVESTMENT_EDIT)next=F_INVESTMENT_AMOUNT;
        else if(app.modal==MODAL_BUDGET_EDIT)
            next=app.focus==F_BUDGET_NAME?F_BUDGET_AMOUNT:F_BUDGET_NAME;
        else next=app.focus==F_INITIAL?F_MONTHLY:app.focus==F_MONTHLY?F_RATE:app.focus==F_RATE?F_MONTHS:F_INITIAL;
        begin_focus(next);return;
    }
    if(ctrl && (sym==XK_a||sym==XK_A)){app.sel_all=1;app.caret=(int)strlen(app.input);return;}
    if(sym==XK_Home){app.caret=0;app.sel_all=0;return;}
    if(sym==XK_End){app.caret=(int)strlen(app.input);app.sel_all=0;return;}
    if(sym==XK_Left){app.caret=(int)utf8_prev_grapheme(app.input,(size_t)app.caret);app.sel_all=0;return;}
    if(sym==XK_Right){app.caret=(int)utf8_next_grapheme(app.input,(size_t)app.caret);app.sel_all=0;return;}
    if(sym==XK_BackSpace||sym==XK_Delete){
        size_t pos=(size_t)app.caret;
        (void)utf8_erase(app.input,&pos,&app.sel_all,sym==XK_BackSpace);
        app.caret=(int)pos;
        return;
    }
    if(ctrl||len<=0)return;
    if(focus_is_text()){
        char composed[256];
        int rc=compose_feed(&app.dead_pending,input,composed,sizeof(composed));
        if(rc<=0)return;
        snprintf(input,sizeof(input),"%s",composed);
        len=(int)strlen(input);
    }
    if(!focus_is_text()){
        /* Campos financeiros permanecem estritamente numericos. */
        for(int i=0;i<len;i++){
            unsigned char c=(unsigned char)input[i];
            if(!(c>='0'&&c<='9')&&c!='.'&&c!=','&&c!='-'&&c!=' ')return;
        }
    }else{
        for(int i=0;i<len;i++){
            unsigned char c=(unsigned char)input[i];
            if(c<32||c==127)return;
        }
    }
    {
        size_t pos=(size_t)app.caret;
        if(utf8_insert(app.input,sizeof(app.input),&pos,&app.sel_all,input)==0)
            app.caret=(int)pos;
    }
}
static AppFont font_small={12.0,0};
static AppFont font_normal={14.0,0};
static AppFont font_bold={14.0,1};
static AppFont font_title={23.0,1};
static AppFont font_number={26.0,1};
/* Recreate Cairo's target when the X11 window is resized. */
static int make_surface(void){
    app.frame_ready=0;app.last_font=NULL;
    if(app.cr){cairo_destroy(app.cr);app.cr=NULL;}
    if(app.surface){cairo_surface_destroy(app.surface);app.surface=NULL;}
    if(app.back) XFreePixmap(app.dpy,app.back);
    app.back=XCreatePixmap(app.dpy,app.win,(unsigned)app.w,(unsigned)app.h,
                            (unsigned)DefaultDepth(app.dpy,app.screen));
    if(!app.back) return -1;
    app.surface=cairo_xlib_surface_create(app.dpy,app.back,DefaultVisual(app.dpy,app.screen),app.w,app.h);
    if(cairo_surface_status(app.surface)!=CAIRO_STATUS_SUCCESS)return -1;
    app.cr=cairo_create(app.surface);
    if(cairo_status(app.cr)!=CAIRO_STATUS_SUCCESS)return -1;
    /* GOOD e bem mais leve que BEST e nao muda a aparencia perceptivelmente. */
    cairo_set_antialias(app.cr,CAIRO_ANTIALIAS_GOOD);
    return 0;
}
static void shutdown(void){
    if(app.ic){XDestroyIC(app.ic);app.ic=NULL;}
    if(app.im){XCloseIM(app.im);app.im=NULL;}
    if(app.cr)cairo_destroy(app.cr);
    if(app.surface)cairo_surface_destroy(app.surface);
    if(app.back)XFreePixmap(app.dpy,app.back);
    if(app.gc)XFreeGC(app.dpy,app.gc);
    if(app.win)XDestroyWindow(app.dpy,app.win);
    if(app.dpy)XCloseDisplay(app.dpy);
}
static int setup(void){
    XSizeHints hints={0};
    app.dpy=XOpenDisplay(NULL);
    if(!app.dpy){fprintf(stderr,"invest: nao foi possivel abrir o X11 (DISPLAY)\n");return -1;}
    app.screen=DefaultScreen(app.dpy);
    /* O ThinkPad X200 costuma ter painel de 1280x800. O tamanho inicial
     * deixa espaco para a decoracao da janela e o painel do ambiente. */
    app.w=imin(1120,DisplayWidth(app.dpy,app.screen)-80);
    app.h=imin(780,DisplayHeight(app.dpy,app.screen)-60);
    app.w=imax(MIN_W,app.w);app.h=imax(MIN_H,app.h);
    int start_x=imax(0,(DisplayWidth(app.dpy,app.screen)-app.w)/2);
    int start_y=imax(10,(DisplayHeight(app.dpy,app.screen)-app.h)/2-12);
    app.win=XCreateSimpleWindow(app.dpy,RootWindow(app.dpy,app.screen),start_x,start_y,
                                (unsigned)app.w,(unsigned)app.h,0,0,WhitePixel(app.dpy,app.screen));
    XStoreName(app.dpy,app.win,APP_TITLE);
    hints.flags=PMinSize;hints.min_width=MIN_W;hints.min_height=MIN_H;
    XSetWMNormalHints(app.dpy,app.win,&hints);
    XSelectInput(app.dpy,app.win,ExposureMask|KeyPressMask|ButtonPressMask|StructureNotifyMask|FocusChangeMask);
    app.im=XOpenIM(app.dpy,NULL,NULL,NULL);
    if(app.im){
        app.ic=XCreateIC(app.im,XNInputStyle,XIMPreeditNothing|XIMStatusNothing,
                         XNClientWindow,app.win,XNFocusWindow,app.win,NULL);
        if(!app.ic){XCloseIM(app.im);app.im=NULL;}
    }
    app.wm_delete=XInternAtom(app.dpy,"WM_DELETE_WINDOW",False);
    XSetWMProtocols(app.dpy,app.win,&app.wm_delete,1);
    app.gc=XCreateGC(app.dpy,app.win,0,NULL);
    app.f_small=&font_small;
    app.f_normal=&font_normal;
    app.f_bold=&font_bold;
    app.f_title=&font_title;
    app.f_number=&font_number;
    if(make_surface()<0){fprintf(stderr,"invest: erro ao criar superficie Cairo\n");return -1;}
    XMapWindow(app.dpy,app.win);
    if(app.ic)XSetICFocus(app.ic);
    return 0;
}
int main(void){
    XEvent ev;char err[180]={0};
    (void)setlocale(LC_CTYPE,"");
    if(XSupportsLocale())XSetLocaleModifiers("");
    memset(&app,0,sizeof(app));app.focus=F_NONE;palette_init();
    if(invest_lock_acquire(err,sizeof(err))<0){fprintf(stderr,"invest: %s\n",err);return EXIT_FAILURE;}
    if(storage_load(&app.store,err,sizeof(err))<0){
        fprintf(stderr,"invest: %s\nNenhum arquivo existente foi alterado. Execute invest-doctor check.\n",err);
        return EXIT_FAILURE;
    }
    {
        BudgetStore legacy={0};int rc;time_t now=time(NULL);
        struct tm *today=localtime(&now);
        if(!today){fprintf(stderr,"invest: relógio do sistema indisponível\n");return EXIT_FAILURE;}
        app.budget_year=today->tm_year+1900;
        app.budget_month=today->tm_mon+1;
        rc=monthly_load(app.budget_year,app.budget_month,&app.monthly,err,sizeof(err));
        if(rc<0){fprintf(stderr,"invest: %s\nDados existentes preservados. Execute invest-doctor check.\n",err);return EXIT_FAILURE;}
        if(rc==1){
            /* Legacy is an initial template, never overwritten by the new model. */
            if(budget_load(&legacy,err,sizeof(err))<0){
                fprintf(stderr,"invest: %s\nNenhum arquivo existente foi alterado. Execute invest-doctor check.\n",err);
                return EXIT_FAILURE;
            }
            if(legacy.count||legacy.monthly_investment>0){
                app.monthly.planned=legacy;app.budget_dirty=1;
                status("Orçamento antigo importado como planejamento deste mês.",0);
            }
        }
        app.budget=app.monthly.planned;
    }
    if(!app.store.count){scenario_default(&app.store.items[0]);app.store.count=1;app.store.selected=0;app.dirty=1;}
    app.cmp_a=0;app.cmp_b=app.store.count>1?1:0;
    app.running=1;
    if(setup()<0){if(app.dpy)shutdown();return EXIT_FAILURE;}
    while(app.running){
        XNextEvent(app.dpy,&ev);
        if(ev.type==KeyPress){
            /* Teclas mortas podem ser filtradas pelo XIM antes de handle_key.
             * Ler o nivel Shift correto para o layout brasileiro. */
            int level=(ev.xkey.state&ShiftMask)?1:0;
            KeySym physical=XLookupKeysym(&ev.xkey,level);
            if(focus_is_text() && is_dead_key(physical))
                app.dead_pending=key_accent(physical);
            if(app.dead_pending && physical==XK_BackSpace){
                reset_dead_composition();
                paint();
                continue; /* cancela o acento, nao o nome do cenario */
            }
        }
        if(XFilterEvent(&ev,app.win))continue;
        switch(ev.type){
        case Expose:present_exposed(&ev.xexpose);break;
        case ButtonPress:mouse_press(ev.xbutton.x,ev.xbutton.y,ev.xbutton.button);paint();break;
        case KeyPress:handle_key(&ev.xkey);paint();break;
        case FocusIn:if(app.ic)XSetICFocus(app.ic);break;
        case FocusOut:if(app.ic)XUnsetICFocus(app.ic);break;
        case ConfigureNotify:
            {
                XEvent newer;
                /* Descarte dimensoes intermediarias ao arrastar bordas:
                 * uma alocacao de pixmap para a ultima medida apenas. */
                while(XCheckTypedWindowEvent(app.dpy,app.win,ConfigureNotify,&newer))
                    ev=newer;
            }
            if(app.w!=ev.xconfigure.width||app.h!=ev.xconfigure.height){
                app.w=ev.xconfigure.width;app.h=ev.xconfigure.height;
                if(make_surface()<0){fprintf(stderr,"invest: erro no redimensionamento\n");app.running=0;break;}
                paint();
            }break;
        case ClientMessage:
            if((Atom)ev.xclient.data.l[0]==app.wm_delete){
                if(end_focus()<0)break;
                if((app.dirty||app.budget_dirty)&&save()<0)break;
                app.running=0;
            }break;
        default:break;
        }
    }
    shutdown();invest_lock_release();return EXIT_SUCCESS;
}

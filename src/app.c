/* Native Win32 UI. All slow file work runs on a worker thread.
   Only the UI thread owns controls and the page displayed by the virtual list. */
#include "core.h"
#define PAGE_SIZE 100
#define ID_OPEN 101
#define ID_DEMO 102
#define ID_EXPORT 103
#define ID_RAW 104
#define ID_REINDEX 105
#define ID_ENCODING 106
#define ID_CACHE 107
#define ID_HELP 108
#define ID_TREE 110
#define ID_TREESEARCH 111
#define ID_TABS 112
#define ID_GRID 113
#define ID_COLUMN 114
#define ID_OPERATOR 115
#define ID_QUERY 116
#define ID_APPLY 117
#define ID_RESET 118
#define ID_FIRST 120
#define ID_PREV 121
#define ID_PAGE 122
#define ID_GO 123
#define ID_NEXT 124
#define ID_LAST 125
#define ID_CANCEL 126
#define ID_DETAIL 130
#define ID_SQL 131
#define ID_PAGERLABEL 132
#define ID_BREADCRUMB 133
#define ID_FILTERLABEL 134
#define ID_INFOHEADER 135
#define MSG_DONE (WM_APP+1)
#define MSG_OPEN_PATH (WM_APP+2)
#define WINCLASS L"BitLineDumpBrowser.Main"
#define TEXTCLASS L"BitLineDumpBrowser.Text"
typedef struct {uint64_t start;uint32_t rows,cols;WCHAR ***cells;BYTE **kinds;BLRef refs[PAGE_SIZE];} Page;
typedef struct Job {
    int type,ok,table,encoding,safe,force;volatile int cancelled;
    volatile uint64_t done,total,found;uint64_t count,start;const char *volatile phase;
    BLDatabase *db,*newdb;BLFile lock;char *source,*target,*index,*query,*text;BLRef ref;BLFilter filter;Page *page;char error[1024];
} Job;
enum {JOB_OPEN=1,JOB_PAGE,JOB_FILTER,JOB_EXPORT,JOB_DETAIL,JOB_RAW};
typedef struct {
    HINSTANCE instance;HWND window,tree,grid,tabs,tree_search,column,op,query,detail,sql,progress,pager,page_edit,breadcrumb,filterlabel,infoheader;
    HWND buttons[32],encoding;int nb,dpi,busy,selected,mode,grid_cols,rebuilding,closing;uint64_t start,count;
    BLDatabase *db;BLFile cache_lock;Page *page;Job *job;char *cache_root,*exe_dir,*filter_index;char status[2048];
    HFONT font,bold,title,small,mono;HBRUSH white,background,line,blue,pale;HICON icon;HBITMAP logo;HIMAGELIST images;WCHAR scratch[16384];
} App;
static App a;
static void refresh_view(void);static void open_path(const char *path,int force);static void load_page(uint64_t start);static void select_table(int table);static void build_tree(void);static void start_filter(void);static void do_command(int id);static void layout(void);static void update_buttons(void);
static int px(int v){return (v*a.dpi+48)/96;}
static size_t wlen(const WCHAR *s){size_t n=0;while(s&&s[n])n++;return n;}
static void wcopy(WCHAR *out,size_t cap,const WCHAR *in){if(!cap)return;size_t n=wlen(in);if(n>=cap)n=cap-1;memcpy(out,in,n*2);out[n]=0;}
static void text(HWND h,const char *s){WCHAR *w=bl_wide(s?s:"");if(w){SetWindowTextW(h,w);free(w);}}
static char *read_text(HWND h){int n=GetWindowTextLengthW(h);WCHAR *w=(WCHAR*)calloc((size_t)n+1,2);if(!w)return NULL;GetWindowTextW(h,w,n+1);char *s=bl_utf8(w);free(w);return s;}
static void info(const char *message,UINT flags){WCHAR *w=bl_wide(message);if(w){MessageBoxW(a.window,w,L"Bit-Line Dump Browser",flags);free(w);}}
static void status(const char *s){bl_format(a.status,sizeof(a.status),"%s",s);if(a.window)InvalidateRect(a.window,NULL,0);}
static void number(char *out,size_t cap,uint64_t value){char s[32];bl_format(s,sizeof(s),"%llu",(unsigned long long)value);size_t n=strlen(s),k=0;for(size_t i=0;i<n&&k+1<cap;i++){if(i&&((n-i)%3)==0&&k+1<cap)out[k++]='.';if(k+1<cap)out[k++]=s[i];}out[k]=0;}
static void bytes_text(char *out,size_t cap,uint64_t n){const char *unit[] = {"B","KiB","MiB","GiB","TiB"};uint64_t divisor=1;int i=0;while(i<4&&n/divisor>=1024){divisor*=1024;i++;}if(!i)bl_format(out,cap,"%llu B",(unsigned long long)n);else bl_format(out,cap,"%llu,%llu %s",(unsigned long long)(n/divisor),(unsigned long long)((n%divisor)*10/divisor),unit[i]);}
static const char *basename8(const char *p){const char *b=p;for(;*p;p++)if(*p=='/'||*p=='\\')b=p+1;return b;}
static char *join(const char *dir,const char *file){size_t n=strlen(dir)+strlen(file)+2;char *s=(char*)malloc(n);if(s)bl_format(s,n,"%s/%s",dir,file);return s;}
static void copy_clipboard(const WCHAR *s){if(!OpenClipboard(a.window))return;size_t n=(wlen(s)+1)*2;HGLOBAL h=GlobalAlloc(GMEM_MOVEABLE,n);if(h){void *p=GlobalLock(h);if(p){memcpy(p,s,n);GlobalUnlock(h);EmptyClipboard();if(!SetClipboardData(CF_UNICODETEXT,h))GlobalFree(h);}else GlobalFree(h);}CloseClipboard();}
static void page_free(Page *p){if(!p)return;for(uint32_t i=0;i<p->rows;i++){if(p->cells&&p->cells[i]){for(uint32_t j=0;j<p->cols;j++)free(p->cells[i][j]);free(p->cells[i]);}if(p->kinds)free(p->kinds[i]);}free(p->cells);free(p->kinds);free(p);}
static char *preview(const BLValue *v,int encoding,size_t max){
    if(v->kind==BL_NULL)return bl_dup("NULL");if(v->kind==BL_MISSING)return bl_dup("— nicht im INSERT —");size_t take=v->len>max?max:v->len,n=0;
    char *u=bl_to_utf8(v->data,take,encoding,&n);if(!u)return NULL;char *s=(char*)malloc(n*2+40);if(!s){free(u);return NULL;}size_t k=0;
    if(v->kind==BL_EXPR){memcpy(s,"SQL: ",5);k=5;}
    for(size_t i=0;i<n;i++){unsigned char c=(unsigned char)u[i];if(c==0||c=='\n'||c=='\r'||c=='\t'){s[k++]='\\';s[k++]=c==0?'0':(c=='\n'?'n':(c=='\r'?'r':'t'));}else if(c<32){s[k++]='?';}else s[k++]=(char)c;}
    if(take<v->len){memcpy(s+k," …",4);k+=4;}s[k]=0;free(u);return s;
}
static int progress_cb(void *user,uint64_t done,uint64_t total,uint64_t found,const char *phase){Job *j=(Job*)user;__atomic_store_n(&j->done,done,__ATOMIC_RELAXED);__atomic_store_n(&j->total,total,__ATOMIC_RELAXED);__atomic_store_n(&j->found,found,__ATOMIC_RELAXED);__atomic_store_n(&j->phase,phase,__ATOMIC_RELAXED);return !__atomic_load_n(&j->cancelled,__ATOMIC_RELAXED);}
static Page *make_page(Job *j){
    BLTable *t=&j->db->tables[j->table];Page *p=(Page*)calloc(1,sizeof(Page));if(!p)return NULL;p->start=j->start;p->cols=t->ncolumns+1;
    p->rows=(uint32_t)(j->count-j->start>PAGE_SIZE?PAGE_SIZE:j->count-j->start);
    p->cells=(WCHAR***)calloc(p->rows?p->rows:1,sizeof(WCHAR**));p->kinds=(BYTE**)calloc(p->rows?p->rows:1,sizeof(BYTE*));if(!p->cells||!p->kinds){page_free(p);return NULL;}
    BLFile index=bl_open(j->index,0);if(p->rows&&(index==BL_BAD_FILE||!bl_seek(index,j->start*sizeof(BLRef))||bl_read(index,p->refs,p->rows*sizeof(BLRef))!=p->rows*sizeof(BLRef))){bl_close(index);page_free(p);bl_format(j->error,sizeof(j->error),"Die angeforderte Indexseite kann nicht gelesen werden.");return NULL;}bl_close(index);
    for(uint32_t i=0;i<p->rows;i++){
        if(!progress_cb(j,i,p->rows,i,"Seite laden")){page_free(p);return NULL;}
        p->cells[i]=(WCHAR**)calloc(p->cols,sizeof(WCHAR*));p->kinds[i]=(BYTE*)calloc(p->cols,1);if(!p->cells[i]||!p->kinds[i]){page_free(p);return NULL;}
        char n[40];number(n,sizeof(n),j->start+i+1);p->cells[i][0]=bl_wide(n);if(!p->cells[i][0]){page_free(p);return NULL;}
        if(p->refs[i].length>BL_ROW_LIMIT){for(uint32_t c=1;c<p->cols;c++)p->cells[i][c]=bl_wide(c==1?"[Datensatz >64 MiB: SQL-Tupel speichern]":"—");continue;}
        BLRow row;if(!bl_get_row(j->db,(uint32_t)j->table,&p->refs[i],&row)){page_free(p);bl_format(j->error,sizeof(j->error),"%s",j->db->error);return NULL;}
        size_t cap=t->ncolumns?16384/t->ncolumns:256;if(cap>256)cap=256;if(cap<24)cap=24;
        int ok=1;for(uint32_t c=0;c<row.count;c++){char *s=preview(&row.values[c],j->encoding,cap);p->cells[i][c+1]=s?bl_wide(s):NULL;p->kinds[i][c+1]=(BYTE)row.values[c].kind;free(s);if(!p->cells[i][c+1]){ok=0;break;}}
        bl_row_free(&row);if(!ok){page_free(p);return NULL;}
    }return p;
}
typedef struct {char *s;size_t n,cap;int clipped;} Text;
static void add(Text *b,const char *s){size_t n=strlen(s);if(b->n+n+1>b->cap){size_t take=b->cap>b->n+1?b->cap-b->n-1:0;if(take){memcpy(b->s+b->n,s,take);b->n+=take;b->s[b->n]=0;}b->clipped=1;return;}memcpy(b->s+b->n,s,n+1);b->n+=n;}
static char *detail_text(Job *j){
    BLRow row;if(!bl_get_row(j->db,j->table,&j->ref,&row)){bl_format(j->error,sizeof(j->error),"%s",j->db->error);return NULL;}Text b={0};b.cap=2*1024*1024;b.s=(char*)calloc(b.cap+128,1);if(!b.s){bl_row_free(&row);return NULL;}
    BLTable *t=&j->db->tables[j->table];char head[1024];bl_format(head,sizeof(head),"BIT-LINE DUMP BROWSER  |  Datensatzdetails\r\n\r\nByteposition: %llu\r\nLänge des SQL-Tupels: %u Bytes\r\n\r\n",(unsigned long long)j->ref.offset,j->ref.length);add(&b,head);
    for(uint32_t c=0;c<row.count;c++){size_t len=0;char *name=bl_to_utf8(t->columns[c].name,strlen(t->columns[c].name),j->encoding,&len);add(&b,name?name:"?");free(name);
        const char *kind=row.values[c].kind==BL_NULL?"NULL":row.values[c].kind==BL_NUMBER?"Zahl / exaktes SQL-Literal":row.values[c].kind==BL_BINARY?"Binärliteral":row.values[c].kind==BL_EXPR?"SQL-Ausdruck / nicht ausgewertet":row.values[c].kind==BL_MISSING?"Nicht im INSERT enthalten":"Text";
        add(&b,"  [");add(&b,kind);add(&b,"]\r\n");char *value=preview(&row.values[c],j->encoding,65536);if(value){add(&b,value);free(value);}add(&b,"\r\n\r\n");if(b.clipped)break;
    }
    if(b.clipped){const char *tail="\r\n[Vorschau auf 2 MiB begrenzt. Für bytegenaue Daten das SQL-Rohtupel speichern.]";memcpy(b.s+b.n,tail,strlen(tail)+1);}bl_row_free(&row);return b.s;
}
static DWORD WINAPI worker(void *param){
    Job *j=(Job*)param;BLOptions options={progress_cb,j,0};
    if(j->type==JOB_OPEN){
        j->newdb=(BLDatabase*)malloc(sizeof(BLDatabase));if(j->newdb){bl_db_init(j->newdb);j->ok=bl_db_prepare(j->newdb,j->source,a.cache_root);
            if(j->ok){char *lp=join(j->newdb->cache,"session.lock");WCHAR *w=lp?bl_winpath(lp):NULL;free(lp);j->lock=w?CreateFileW(w,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL):BL_BAD_FILE;free(w);
                if(j->lock==BL_BAD_FILE){j->ok=0;bl_format(j->newdb->error,sizeof(j->newdb->error),"Dieser Dump-Cache ist bereits in einer anderen Instanz geöffnet, oder der Cache-Ordner ist nicht beschreibbar.");}
            }
            if(j->ok){j->ok=!j->force&&bl_db_load_cache(j->newdb);if(!j->ok)j->ok=bl_db_index(j->newdb,&options);}
            if(!j->ok)bl_format(j->error,sizeof(j->error),"%s",j->newdb->error);
        }
    }else if(j->type==JOB_PAGE){j->page=make_page(j);j->ok=j->page!=NULL;}
    else if(j->type==JOB_FILTER){j->filter.text=j->query;j->ok=bl_filter(j->db,j->table,&j->filter,j->target,&j->count,&options);}
    else if(j->type==JOB_EXPORT)j->ok=bl_export_csv(j->db,j->table,j->index,j->count,j->target,j->encoding,1,&options);
    else if(j->type==JOB_DETAIL){j->text=detail_text(j);j->ok=j->text!=NULL;}
    else if(j->type==JOB_RAW)j->ok=bl_export_raw(j->db,&j->ref,j->target,&options);
    if(!j->ok&&!j->error[0]&&j->db)bl_format(j->error,sizeof(j->error),"%s",j->db->error);
    if(!j->ok&&!j->error[0])bl_format(j->error,sizeof(j->error),"Nicht genügend Speicher oder Vorgang abgebrochen.");
    PostMessageW(a.window,MSG_DONE,0,(LPARAM)j);return 0;
}
static void job_free(Job *j){if(!j)return;free(j->source);free(j->target);free(j->index);free(j->query);free(j->text);page_free(j->page);if(j->newdb){bl_db_free(j->newdb);free(j->newdb);}if(j->lock!=BL_BAD_FILE)bl_close(j->lock);free(j);}
static Job *new_job(int type){Job *j=(Job*)calloc(1,sizeof(Job));if(j){j->type=type;j->lock=BL_BAD_FILE;j->db=a.db;j->table=a.selected;j->encoding=(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0);}return j;}
static int begin_job(Job *j){if(!j){info("Nicht genügend Arbeitsspeicher.",MB_ICONERROR);return 0;}if(a.busy){job_free(j);return 0;}
    a.busy=1;a.job=j;update_buttons();ShowWindow(a.progress,SW_SHOW);SendMessageW(a.progress,PBM_SETPOS,0,0);status("Vorgang läuft …");
    HANDLE thread=CreateThread(NULL,0,worker,j,0,NULL);if(!thread){a.busy=0;a.job=NULL;job_free(j);update_buttons();info("Der Hintergrund-Thread konnte nicht gestartet werden.",MB_ICONERROR);return 0;}CloseHandle(thread);return 1;
}
static HWND control(LPCWSTR cls,LPCWSTR caption,DWORD style,int id){HWND h=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,0,0,10,10,a.window,(HMENU)(uintptr_t)id,a.instance,NULL);SendMessageW(h,WM_SETFONT,(WPARAM)a.font,1);return h;}
static HWND button(LPCWSTR caption,int id){HWND h=control(L"BUTTON",caption,WS_TABSTOP|BS_OWNERDRAW,id);if(a.nb<32)a.buttons[a.nb++]=h;SetWindowLongPtrW(h,GWLP_USERDATA,id);return h;}
static HWND by_id(int id){for(int i=0;i<a.nb;i++)if(GetWindowLongPtrW(a.buttons[i],GWLP_USERDATA)==id)return a.buttons[i];return NULL;}
static void move(HWND h,int x,int y,int w,int ht){if(h)MoveWindow(h,px(x),px(y),px(w>0?w:1),px(ht>0?ht:1),1);}
static void fonts(void){
    HFONT old[]={a.font,a.bold,a.title,a.small,a.mono};a.font=CreateFontW(-px(14),0,0,0,400,0,0,0,1,0,0,5,0,L"Segoe UI");a.bold=CreateFontW(-px(14),0,0,0,600,0,0,0,1,0,0,5,0,L"Segoe UI");
    a.title=CreateFontW(-px(24),0,0,0,600,0,0,0,1,0,0,5,0,L"Segoe UI");a.small=CreateFontW(-px(12),0,0,0,400,0,0,0,1,0,0,5,0,L"Segoe UI");a.mono=CreateFontW(-px(13),0,0,0,400,0,0,0,1,0,0,5,0,L"Consolas");
    if(a.window){HWND cs[]={a.tree,a.grid,a.tabs,a.tree_search,a.column,a.op,a.query,a.detail,a.sql,a.pager,a.page_edit,a.breadcrumb,a.filterlabel,a.infoheader,a.encoding};for(size_t i=0;i<sizeof(cs)/sizeof(cs[0]);i++)if(cs[i])SendMessageW(cs[i],WM_SETFONT,(WPARAM)((cs[i]==a.sql)?a.mono:a.font),1);for(int i=0;i<a.nb;i++)SendMessageW(a.buttons[i],WM_SETFONT,(WPARAM)a.font,1);}
    for(int i=0;i<5;i++)if(old[i])DeleteObject(old[i]);
}
static void draw_text(HDC dc,LPCWSTR s,int x,int y,int w,int h,HFONT font,COLORREF color,UINT flags){RECT r={px(x),px(y),px(x+w),px(y+h)};SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,s,-1,&r,flags|DT_NOPREFIX);}
static void fill(HDC dc,HBRUSH brush,int x,int y,int w,int h){RECT r={px(x),px(y),px(x+w),px(y+h)};FillRect(dc,&r,brush);}
static void paint(HWND hwnd){PAINTSTRUCT ps;HDC dc=BeginPaint(hwnd,&ps);RECT rc;GetClientRect(hwnd,&rc);int w=rc.right*96/a.dpi,h=rc.bottom*96/a.dpi;
    fill(dc,a.background,0,0,w,h);fill(dc,a.white,0,0,w,86);fill(dc,a.line,0,86,w,1);fill(dc,a.white,0,h-38,w,38);fill(dc,a.line,0,h-39,w,1);
    DrawIconEx(dc,px(18),px(18),a.icon,px(48),px(48),0,NULL,DI_NORMAL);
    if(a.logo){HDC mem=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(mem,a.logo);BITMAP bm;GetObjectW(a.logo,sizeof(bm),&bm);SetStretchBltMode(dc,HALFTONE);StretchBlt(dc,px(76),px(16),px(161),px(37),mem,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);SelectObject(mem,old);DeleteDC(mem);}
    draw_text(dc,L"D U M P   B R O W S E R",80,57,190,18,a.small,RGB(70,109,143),DT_SINGLELINE);
    draw_text(dc,L"Dein Dump. Ohne Server.",277,20,520,30,a.title,RGB(30,58,83),DT_SINGLELINE);
    draw_text(dc,L"SQL lesen. Daten verstehen. Alles lokal.",279,54,500,20,a.small,RGB(102,126,146),DT_SINGLELINE);
    draw_text(dc,L"READ ONLY  ·  NATIVE x64",w-260,33,240,26,a.small,RGB(48,144,198),DT_SINGLELINE|DT_RIGHT);
    fill(dc,a.white,16,146,216,h-200);draw_text(dc,L"DATENBANKEN",29,153,190,22,a.bold,RGB(45,75,103),DT_SINGLELINE);
    int right=w>=1180?250:0,mainw=w-248-right-16;
    fill(dc,a.white,248,146,mainw,h-200);
    if(right){fill(dc,a.white,w-250,146,234,h-200);fill(dc,a.line,w-259,156,1,h-223);}
    if(!a.db){DrawIconEx(dc,px(248+mainw/2-36),px(280),a.icon,px(72),px(72),0,NULL,DI_NORMAL);
        draw_text(dc,L"MySQL-Dumps lokal erkunden",268,374,mainw-40,40,a.title,RGB(37,73,101),DT_CENTER|DT_SINGLELINE);
        draw_text(dc,L"Öffne eine .sql-Datei oder lade die Demo.\nDatenbanken, Tabellen und Einträge – ohne MySQL-Instanz.",278,431,mainw-60,70,a.font,RGB(107,129,148),DT_CENTER|DT_WORDBREAK);
        draw_text(dc,L"64-Bit-Positionsindex  ·  Originaldatei bleibt unverändert",268,528,mainw-40,34,a.small,RGB(47,142,195),DT_CENTER|DT_SINGLELINE);
    }
    WCHAR *st=bl_wide(a.status);if(st){draw_text(dc,st,18,h-29,w-(a.busy?350:36),24,a.small,RGB(76,104,126),DT_SINGLELINE|DT_END_ELLIPSIS);free(st);}EndPaint(hwnd,&ps);
}
static void draw_button(DRAWITEMSTRUCT *d){
    int id=(int)d->CtlID,accent=id==ID_OPEN||id==ID_APPLY;int disabled=(d->itemState&ODS_DISABLED)!=0,pressed=(d->itemState&ODS_SELECTED)!=0;
    COLORREF bg=disabled?RGB(241,244,247):(accent?(pressed?RGB(29,123,180):RGB(49,148,203)):RGB(255,255,255));COLORREF fg=disabled?RGB(159,173,184):(accent?RGB(255,255,255):RGB(46,78,104));
    HBRUSH brush=CreateSolidBrush(bg);HGDIOBJ pen=CreatePen(PS_SOLID,px(1),accent&&!disabled?bg:RGB(212,226,236));HGDIOBJ ob=SelectObject(d->hDC,brush),op=SelectObject(d->hDC,pen);
    RoundRect(d->hDC,d->rcItem.left,d->rcItem.top,d->rcItem.right,d->rcItem.bottom,px(7),px(7));SelectObject(d->hDC,ob);SelectObject(d->hDC,op);DeleteObject(brush);DeleteObject(pen);
    WCHAR caption[128];GetWindowTextW(d->hwndItem,caption,128);SelectObject(d->hDC,a.font);SetBkMode(d->hDC,TRANSPARENT);SetTextColor(d->hDC,fg);RECT r=d->rcItem;r.left+=px(5);r.right-=px(5);DrawTextW(d->hDC,caption,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
    if(d->itemState&ODS_FOCUS){r.left+=px(3);r.right-=px(3);r.top+=px(3);r.bottom-=px(3);DrawFocusRect(d->hDC,&r);}
}
static void layout(void){if(!a.window)return;RECT rc;GetClientRect(a.window,&rc);int w=rc.right*96/a.dpi,h=rc.bottom*96/a.dpi,right=w>=1180?250:0,mw=w-248-right-16;
    int ids[]={ID_OPEN,ID_DEMO,ID_EXPORT,ID_RAW,ID_REINDEX,ID_ENCODING,ID_CACHE,ID_HELP};int widths[]={142,w<1000?76:96,132,126,120,136,76,64};int x=16;
    for(int i=0;i<8;i++){move(ids[i]==ID_ENCODING?a.encoding:by_id(ids[i]),x,99,widths[i],ids[i]==ID_ENCODING?240:33);x+=widths[i]+8;}
    move(a.tree_search,27,183,194,30);move(a.tree,25,224,198,h-294);
    move(a.breadcrumb,260,153,mw-24,24);move(a.tabs,258,183,mw-20,31);
    int colw=139,opw=119,qw=mw-colw-opw-171;if(qw<80)qw=80;
    move(a.column,260,226,colw,280);move(a.op,260+colw+6,226,opw,180);move(a.query,260+colw+opw+12,226,qw,29);
    move(by_id(ID_APPLY),260+colw+opw+qw+18,226,70,29);move(by_id(ID_RESET),260+colw+opw+qw+94,226,57,29);
    move(a.filterlabel,260,262,mw-24,22);move(a.grid,260,292,mw-24,h-411);move(a.sql,260,229,mw-24,h-302);
    int py=h-102;move(by_id(ID_FIRST),260,py,34,30);move(by_id(ID_PREV),299,py,34,30);move(a.page_edit,339,py,62,30);move(by_id(ID_GO),407,py,42,30);move(by_id(ID_NEXT),455,py,34,30);move(by_id(ID_LAST),495,py,34,30);move(a.pager,541,py+5,mw-305,25);
    move(a.infoheader,w-234,155,202,38);move(a.detail,w-234,203,202,h-278);ShowWindow(a.detail,right?SW_SHOW:SW_HIDE);ShowWindow(a.infoheader,right?SW_SHOW:SW_HIDE);
    move(a.progress,w-330,h-25,226,10);move(by_id(ID_CANCEL),w-93,h-32,77,25);InvalidateRect(a.window,NULL,0);
}
static void update_buttons(void){
    int data=a.db&&a.selected>=0&&a.db->tables[a.selected].kind==0,selected=data&&a.mode==0&&a.page&&SendMessageW(a.grid,LVM_GETNEXTITEM,(WPARAM)-1,LVNI_SELECTED)>=0;
    for(int i=0;i<a.nb;i++){int id=(int)GetWindowLongPtrW(a.buttons[i],GWLP_USERDATA),ok=!a.busy;
        if(id==ID_EXPORT||id==ID_APPLY||id==ID_RESET)ok=ok&&data;
        if(id==ID_RAW)ok=ok&&selected;
        if(id==ID_REINDEX)ok=ok&&a.db;
        if(id==ID_FIRST||id==ID_PREV)ok=ok&&data&&a.mode==0&&a.start>0;
        if(id==ID_NEXT||id==ID_LAST)ok=ok&&data&&a.mode==0&&a.start+PAGE_SIZE<a.count;
        if(id==ID_GO)ok=ok&&data&&a.mode==0&&a.count>0;
        if(id==ID_CANCEL){ShowWindow(a.buttons[i],a.busy?SW_SHOW:SW_HIDE);ok=a.busy&&a.job&&!__atomic_load_n(&a.job->cancelled,__ATOMIC_RELAXED);}
        EnableWindow(a.buttons[i],ok);
    }
    HWND cs[]={a.tree,a.tree_search,a.tabs,a.encoding,a.column,a.op,a.query,a.grid,a.page_edit};for(size_t i=0;i<sizeof(cs)/sizeof(cs[0]);i++)EnableWindow(cs[i],!a.busy);
    EnableWindow(a.column,!a.busy&&data);EnableWindow(a.op,!a.busy&&data);EnableWindow(a.query,!a.busy&&data);EnableWindow(a.page_edit,!a.busy&&data&&a.mode==0);
    ShowWindow(a.progress,a.busy?SW_SHOW:SW_HIDE);
}
static void clear_grid(void){SendMessageW(a.grid,LVM_SETITEMCOUNT,0,0);while(a.grid_cols>0){SendMessageW(a.grid,LVM_DELETECOLUMN,0,0);a.grid_cols--;}}
static void grid_column(const char *name,int width,int encoding){size_t n;char *u=bl_to_utf8(name,strlen(name),encoding,&n);WCHAR *w=u?bl_wide(u):NULL;free(u);LVCOLUMNW col={0};col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM;col.pszText=w;col.cx=px(width);col.iSubItem=a.grid_cols;SendMessageW(a.grid,LVM_INSERTCOLUMNW,a.grid_cols,(LPARAM)&col);a.grid_cols++;free(w);}
static void table_info(int row){
    if(!a.db||a.selected<0){text(a.infoheader,"DEINE DATEN.\nIN DEINER HAND.");text(a.detail,"Lokal und schreibgeschützt.\r\n\r\n.sql-Datei öffnen oder per Drag-and-drop ins Fenster ziehen.\r\n\r\nDer Cache enthält Positionsdaten und Schema – keine zweite Datenbankkopie.");return;}
    BLTable *t=&a.db->tables[a.selected];int enc=(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0);size_t size;char *name=bl_to_utf8(t->name,strlen(t->name),enc,&size);text(a.infoheader,name?name:t->name);free(name);
    Text b={0};b.cap=65536;b.s=(char*)calloc(b.cap+1,1);if(!b.s)return;char count[64],data[64],line[512];number(count,sizeof(count),t->rows);bytes_text(data,sizeof(data),t->bytes);
    add(&b,"Datenbank\r\n");name=bl_to_utf8(t->database,strlen(t->database),enc,&size);add(&b,name?name:t->database);free(name);
    bl_format(line,sizeof(line),"\r\n\r\n%s Einträge\r\n%u Spalten\r\n%s SQL-Tupel\r\n\r\n",count,t->ncolumns,data);add(&b,line);
    if(row>=0&&a.page&&(uint32_t)row<a.page->rows){bl_format(line,sizeof(line),"AUSGEWÄHLTER EINTRAG\r\nByte %llu\r\n%u Bytes im Dump\r\n\r\n",(unsigned long long)a.page->refs[row].offset,a.page->refs[row].length);add(&b,line);
        for(uint32_t c=0;c<t->ncolumns;c++){name=bl_to_utf8(t->columns[c].name,strlen(t->columns[c].name),enc,&size);add(&b,name?name:t->columns[c].name);free(name);add(&b,"\r\n");char *value=bl_utf8(a.page->cells[row][c+1]);if(value){add(&b,value);free(value);}add(&b,"\r\n\r\n");if(b.clipped)break;}
        add(&b,"Doppelklick: erweiterte Details.\r\nSQL-Tupel: Originalbytes speichern.");
    }else{add(&b,"SPALTEN\r\n\r\n");for(uint32_t c=0;c<t->ncolumns;c++){name=bl_to_utf8(t->columns[c].name,strlen(t->columns[c].name),enc,&size);add(&b,name?name:t->columns[c].name);free(name);add(&b,"\r\n");BLValue v={t->columns[c].definition,strlen(t->columns[c].definition),BL_TEXT};char *s=preview(&v,enc,180);if(s){add(&b,s);free(s);}add(&b,"\r\n\r\n");if(b.clipped)break;}}
    text(a.detail,b.s);free(b.s);
}
static void refresh_view(void){
    int has=a.db&&a.selected>=0,table=has&&a.db->tables[a.selected].kind==0,data=table&&a.mode==0,structure=has&&a.mode==1;
    ShowWindow(a.tabs,a.db?SW_SHOW:SW_HIDE);ShowWindow(a.breadcrumb,a.db?SW_SHOW:SW_HIDE);ShowWindow(a.grid,(data||structure)?SW_SHOW:SW_HIDE);ShowWindow(a.sql,has&&a.mode>=2?SW_SHOW:SW_HIDE);
    HWND filtercs[]={a.column,a.op,a.query,by_id(ID_APPLY),by_id(ID_RESET),a.filterlabel};for(size_t i=0;i<sizeof(filtercs)/sizeof(filtercs[0]);i++)ShowWindow(filtercs[i],data||structure?SW_SHOW:SW_HIDE);
    if(structure){for(size_t i=0;i<5;i++)ShowWindow(filtercs[i],SW_HIDE);}
    HWND pagers[]={by_id(ID_FIRST),by_id(ID_PREV),a.page_edit,by_id(ID_GO),by_id(ID_NEXT),by_id(ID_LAST),a.pager};for(size_t i=0;i<sizeof(pagers)/sizeof(pagers[0]);i++)ShowWindow(pagers[i],data?SW_SHOW:SW_HIDE);
    if(has){BLTable *t=&a.db->tables[a.selected];int enc=(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0);char *db=bl_to_utf8(t->database,strlen(t->database),enc,NULL),*tn=bl_to_utf8(t->name,strlen(t->name),enc,NULL);
        size_t cap=strlen(db?db:"")+strlen(tn?tn:"")+96;char *bread=(char*)malloc(cap);if(bread){bl_format(bread,cap,"%s  ›  %s  ›  %s",db?db:"",t->kind==1?"Views":t->kind==2?"Routinen":"Tabellen",tn?tn:"");text(a.breadcrumb,bread);free(bread);}free(db);free(tn);
        SendMessageW(a.grid,0x000b,0,0);clear_grid();
        if(data){grid_column("#",76,0);for(uint32_t c=0;c<t->ncolumns;c++)grid_column(t->columns[c].name,c==0?120:185,enc);SendMessageW(a.grid,LVM_SETITEMCOUNT,a.page?a.page->rows:0,0);
            char first[48],last[48],total[48],s[256],p[48];number(first,sizeof(first),a.count?a.start+1:0);number(last,sizeof(last),a.start+PAGE_SIZE<a.count?a.start+PAGE_SIZE:a.count);number(total,sizeof(total),a.count);
            bl_format(s,sizeof(s),"%s–%s von %s",first,last,total);text(a.pager,s);bl_format(p,sizeof(p),"%llu",(unsigned long long)(a.start/PAGE_SIZE+1));text(a.page_edit,p);
            if(a.filter_index)bl_format(s,sizeof(s),"%s Treffer · Filter aktiv · Groß-/Kleinschreibung beachten",total);else bl_format(s,sizeof(s),"%s Einträge · Originalreihenfolge · 100 pro Seite",total);text(a.filterlabel,s);
        }else if(structure){grid_column("#",60,0);grid_column("Spalte",185,0);grid_column("MySQL-Definition (Original)",560,0);SendMessageW(a.grid,LVM_SETITEMCOUNT,t->ncolumns,0);text(a.filterlabel,"Spalten, Datentypen und Constraints aus dem CREATE TABLE – ohne Typkonvertierung.");}
        else if(a.mode==2){char *u=bl_to_utf8(t->ddl,strlen(t->ddl),enc,NULL);text(a.sql,u?u:"Kein CREATE-Text vorhanden.");free(u);}
        else {Text b={0};b.cap=131072;b.s=(char*)calloc(b.cap,1);if(b.s){add(&b,"BIT-LINE DUMP BROWSER – IMPORT-HINWEISE\r\n\r\n");add(&b,a.db->notes?a.db->notes:"Keine besonderen Import-Hinweise.\r\n");add(&b,"\r\nDer Browser führt kein SQL aus. Er zeigt INSERT-/REPLACE-VALUES in Dump-Reihenfolge. Defaults, UPDATE/DELETE, Constraints, Views und Routinen werden nicht ausgewertet. MySQL-Code in Versionskommentaren wird außer SQL_MODE nicht interpretiert.\r\n\r\nSQL-Ausdrücke werden als 'SQL:' gekennzeichnet. Fehlende INSERT-Spalten sind nicht dasselbe wie NULL.\r\n\r\nDer Cache benötigt die unveränderte Originaldatei. Cache-Erkennung: Pfad, Größe, Änderungszeit sowie Stichproben am Dateianfang/-ende; keine kryptografische Vollprüfung.\r\n\r\nAnwendung: 1.0.1 / native Windows x64\r\n");text(a.sql,b.s);free(b.s);}}
        SendMessageW(a.grid,0x000b,1,0);InvalidateRect(a.grid,NULL,1);
    }else clear_grid();table_info(-1);update_buttons();layout();
}
static HTREEITEM tree_item(HTREEITEM parent,const char *caption,int image,LPARAM value){WCHAR *w=bl_wide(caption);TVINSERTSTRUCTW ins={0};ins.hParent=parent;ins.hInsertAfter=TVI_LAST;ins.item.mask=TVIF_TEXT|TVIF_IMAGE|TVIF_SELECTEDIMAGE|TVIF_PARAM;ins.item.pszText=w;ins.item.iImage=ins.item.iSelectedImage=image;ins.item.lParam=value;HTREEITEM h=(HTREEITEM)SendMessageW(a.tree,TVM_INSERTITEMW,0,(LPARAM)&ins);free(w);return h;}
static int find_text_ascii(const char *s,const char *q){if(!*q)return 1;for(;*s;s++){size_t i=0;while(q[i]&&s[i]){unsigned c=(unsigned char)s[i],d=(unsigned char)q[i];if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;if(c!=d)break;i++;}if(!q[i])return 1;}return 0;}
static void build_tree(void){
    a.rebuilding=1;SendMessageW(a.tree,0x000b,0,0);SendMessageW(a.tree,TVM_DELETEITEM,0,(LPARAM)TVI_ROOT);
    if(!a.db){SendMessageW(a.tree,0x000b,1,0);a.rebuilding=0;return;}
    typedef struct {const char *name;HTREEITEM db,groups[3];} Group;
    Group *groups=(Group*)calloc(a.db->ntables?a.db->ntables:1,sizeof(Group));if(!groups){a.rebuilding=0;SendMessageW(a.tree,0x000b,1,0);return;}uint32_t ng=0;char *q=read_text(a.tree_search);int enc=(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0);
    HTREEITEM root=tree_item(TVI_ROOT,basename8(a.db->source),0,-1),first=NULL,selected=NULL;
    for(uint32_t i=0;i<a.db->ntables;i++){BLTable *t=&a.db->tables[i];char *name=bl_to_utf8(t->name,strlen(t->name),enc,NULL),*dbname=bl_to_utf8(t->database,strlen(t->database),enc,NULL);
        if(q&&*q&&!find_text_ascii(name?name:"",q)&&!find_text_ascii(dbname?dbname:"",q)){free(name);free(dbname);continue;}
        uint32_t g;for(g=0;g<ng;g++)if(!strcmp(groups[g].name,t->database))break;
        if(g==ng){groups[g].name=t->database;groups[g].db=tree_item(root,dbname?dbname:t->database,1,-2);ng++;}
        if(t->kind<3){if(!groups[g].groups[t->kind])groups[g].groups[t->kind]=tree_item(groups[g].db,t->kind==0?"Tabellen":t->kind==1?"Views":"Routinen / Trigger",t->kind==0?2:3,-2);
            char count[64];number(count,sizeof(count),t->rows);size_t n=strlen(name?name:"")+96;char *caption=(char*)malloc(n);if(caption){if(t->kind==0)bl_format(caption,n,"%s  (%s)",name?name:t->name,count);else bl_format(caption,n,"%s",name?name:t->name);
                HTREEITEM item=tree_item(groups[g].groups[t->kind],caption,t->kind==0?2:3,(LPARAM)(i+1));if(!first&&t->kind==0)first=item;if((int)i==a.selected)selected=item;free(caption);}}
        free(name);free(dbname);
    }
    for(uint32_t g=0;g<ng;g++){SendMessageW(a.tree,TVM_EXPAND,TVE_EXPAND,(LPARAM)groups[g].db);for(int k=0;k<3;k++)if(groups[g].groups[k])SendMessageW(a.tree,TVM_EXPAND,TVE_EXPAND,(LPARAM)groups[g].groups[k]);}
    SendMessageW(a.tree,TVM_EXPAND,TVE_EXPAND,(LPARAM)root);free(groups);free(q);SendMessageW(a.tree,0x000b,1,0);InvalidateRect(a.tree,NULL,1);a.rebuilding=0;
    if(selected)SendMessageW(a.tree,TVM_SELECTITEM,TVGN_CARET,(LPARAM)selected);else if(first&&a.selected<0)SendMessageW(a.tree,TVM_SELECTITEM,TVGN_CARET,(LPARAM)first);
    else if(a.selected<0){for(uint32_t i=0;i<a.db->ntables;i++)if(a.db->tables[i].kind<3){select_table(i);break;}}
}
static void clear_filter(void){if(a.filter_index){bl_remove(a.filter_index);free(a.filter_index);a.filter_index=NULL;}}
static void select_table(int table){if(a.busy||!a.db||table<0||(uint32_t)table>=a.db->ntables)return;if(a.selected==table)return;
    a.selected=table;a.start=0;clear_filter();page_free(a.page);a.page=NULL;BLTable *t=&a.db->tables[table];a.count=t->rows;a.mode=t->kind==0?0:2;SendMessageW(a.tabs,TCM_SETCURSEL,a.mode,0);
    SendMessageW(a.column,CB_RESETCONTENT,0,0);SendMessageW(a.column,CB_ADDSTRING,0,(LPARAM)L"Alle Spalten");int enc=(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0);
    for(uint32_t c=0;c<t->ncolumns;c++){char *s=bl_to_utf8(t->columns[c].name,strlen(t->columns[c].name),enc,NULL);WCHAR *w=s?bl_wide(s):NULL;SendMessageW(a.column,CB_ADDSTRING,0,(LPARAM)(w?w:L"?"));free(w);free(s);}SendMessageW(a.column,CB_SETCURSEL,0,0);SendMessageW(a.op,CB_SETCURSEL,0,0);text(a.query,"");refresh_view();if(a.mode==0)load_page(0);
}
static void load_page(uint64_t start){if(a.busy||!a.db||a.selected<0)return;if(start>=a.count&&a.count)start=((a.count-1)/PAGE_SIZE)*PAGE_SIZE;
    Job *j=new_job(JOB_PAGE);if(!j)return;j->start=start;j->count=a.count;j->index=a.filter_index?bl_dup(a.filter_index):bl_index_path(a.db,a.selected);if(!j->index){job_free(j);return;}begin_job(j);
}
static void close_database(void){clear_grid();page_free(a.page);a.page=NULL;clear_filter();if(a.db){bl_db_free(a.db);free(a.db);a.db=NULL;}if(a.cache_lock!=BL_BAD_FILE)bl_close(a.cache_lock);a.cache_lock=BL_BAD_FILE;a.selected=-1;a.count=a.start=0;}
static void open_path(const char *path,int force){if(a.busy)return;Job *j=new_job(JOB_OPEN);if(!j)return;j->source=bl_dup(path);j->force=force;j->db=NULL;if(!j->source){job_free(j);return;}close_database();text(a.tree_search,"");a.mode=0;build_tree();refresh_view();begin_job(j);}
static char *file_dialog(int save,LPCWSTR title,LPCWSTR filter,const char *suggested,LPCWSTR ext){
    WCHAR *filename=(WCHAR*)calloc(32768,2);if(!filename)return NULL;if(suggested){WCHAR *w=bl_wide(suggested);if(w){wcopy(filename,32768,w);free(w);}}
    OPENFILENAMEW ofn={0};ofn.lStructSize=sizeof(ofn);ofn.hwndOwner=a.window;ofn.lpstrFilter=filter;ofn.lpstrFile=filename;ofn.nMaxFile=32768;ofn.lpstrTitle=title;ofn.lpstrDefExt=ext;ofn.nFilterIndex=1;ofn.Flags=OFN_EXPLORER|OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|OFN_HIDEREADONLY|(save?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
    BOOL ok=save?GetSaveFileNameW(&ofn):GetOpenFileNameW(&ofn);char *out=ok?bl_utf8(filename):NULL;free(filename);return out;
}
static void start_filter(void){if(a.busy||!a.db||a.selected<0||a.db->tables[a.selected].kind!=0)return;Job *j=new_job(JOB_FILTER);if(!j)return;
    j->query=read_text(a.query);j->filter.column=(int)SendMessageW(a.column,CB_GETCURSEL,0,0)-1;j->filter.op=(int)SendMessageW(a.op,CB_GETCURSEL,0,0);j->filter.encoding=j->encoding;j->target=join(a.db->cache,"query-new.bli");if(!j->query||!j->target){job_free(j);return;}begin_job(j);
}
static int selected_row(void){return (int)SendMessageW(a.grid,LVM_GETNEXTITEM,(WPARAM)-1,LVNI_SELECTED);}
static void open_detail(void){int row=selected_row();if(a.busy||a.mode!=0||!a.page||row<0||(uint32_t)row>=a.page->rows)return;Job *j=new_job(JOB_DETAIL);if(j){j->ref=a.page->refs[row];begin_job(j);}}
static HWND modal;
static LRESULT CALLBACK text_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    typedef struct {HWND edit,copy,close;} Controls;Controls *c=(Controls*)GetWindowLongPtrW(hwnd,GWLP_USERDATA);
    if(msg==WM_CREATE){CREATESTRUCTW *cs=(CREATESTRUCTW*)lp;c=(Controls*)calloc(1,sizeof(Controls));if(!c)return -1;SetWindowLongPtrW(hwnd,GWLP_USERDATA,(LONG_PTR)c);
        c->edit=CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_AUTOHSCROLL|ES_READONLY,0,0,1,1,hwnd,(HMENU)3,a.instance,NULL);SendMessageW(c->edit,WM_SETFONT,(WPARAM)a.mono,1);SendMessageW(c->edit,EM_SETLIMITTEXT,4*1024*1024,0);text(c->edit,(const char*)cs->lpCreateParams);
        c->copy=CreateWindowExW(0,L"BUTTON",L"Alles kopieren",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,1,1,hwnd,(HMENU)1,a.instance,NULL);c->close=CreateWindowExW(0,L"BUTTON",L"Schließen",WS_CHILD|WS_VISIBLE|WS_TABSTOP,0,0,1,1,hwnd,(HMENU)2,a.instance,NULL);SendMessageW(c->copy,WM_SETFONT,(WPARAM)a.font,1);SendMessageW(c->close,WM_SETFONT,(WPARAM)a.font,1);return 0;
    }
    if(msg==WM_SIZE&&c){RECT r;GetClientRect(hwnd,&r);MoveWindow(c->edit,px(14),px(14),r.right-px(28),r.bottom-px(70),1);MoveWindow(c->copy,px(14),r.bottom-px(44),px(135),px(30),1);MoveWindow(c->close,r.right-px(124),r.bottom-px(44),px(110),px(30),1);return 0;}
    if(msg==WM_COMMAND&&c){if(LOWORD(wp)==2)DestroyWindow(hwnd);else if(LOWORD(wp)==1){int n=GetWindowTextLengthW(c->edit);WCHAR *w=(WCHAR*)calloc((size_t)n+1,2);if(w){GetWindowTextW(c->edit,w,n+1);copy_clipboard(w);free(w);}}return 0;}
    if(msg==WM_DESTROY){free(c);modal=NULL;EnableWindow(a.window,1);SetForegroundWindow(a.window);return 0;}return DefWindowProcW(hwnd,msg,wp,lp);
}
static void text_window(const char *title,const char *contents){if(modal)return;WCHAR *w=bl_wide(title);RECT r;GetWindowRect(a.window,&r);modal=CreateWindowExW(WS_EX_CONTROLPARENT,TEXTCLASS,w,WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,r.left+px(90),r.top+px(50),px(900),px(640),a.window,NULL,a.instance,(void*)contents);free(w);if(modal){EnableWindow(a.window,0);ShowWindow(modal,SW_SHOW);}}
static const char *help_text=
"BIT-LINE DUMP BROWSER 1.0.1\r\nNative Windows-x64-App · schreibgeschützt · ohne Server\r\n\r\n"
"START\r\nÖffnen: normale .sql-/Text-Dumps. .gz und .zip vorher entpacken. Die Demo liegt im Unterordner samples.\r\n"
"Struktur: Dump → Datenbanken → Tabellen / Views / Routinen → Einträge. Doppelklick auf eine Zeile öffnet die Details.\r\n"
"Die Spalte # ist die Anzeige-Reihenfolge, kein MySQL-Primärschlüssel.\r\n\r\n"
"GROSSE DATEIEN\r\nDer Import liest blockweise und speichert 16 Bytes pro Datensatz plus Schemametadaten. Die Oberfläche lädt 100 Zeilen pro Seite. Ein Seitensprung liest direkt den Positionsindex, ohne alle vorherigen Einträge zu durchlaufen.\r\n"
"Filter durchsuchen die komplette Tabelle im Hintergrund; Treffer werden auf der Platte abgelegt. Ein Vollscan kann bei großen Dumps dauern. Es gibt keinen Volltextindex und keine globale Spaltensortierung.\r\n"
"Die Originaldatei wird weiter benötigt und während der Sitzung gegen Schreibzugriffe gesperrt.\r\n\r\n"
"FILTER & EXPORT\r\nFilter: enthält, entspricht (exakter Text, kein Zahlenvergleich), IS NULL oder IS NOT NULL; pro Spalte oder über alle Spalten. Textfilter beachten Groß-/Kleinschreibung.\r\n"
"CSV exportiert alle Einträge bzw. alle aktuellen Filtertreffer, nicht nur die Seite. UTF-8 mit BOM, Semikolon, CRLF. NULL = \\N; fehlende INSERT-Spalte = [nicht im INSERT]. Ein gleichlautender Text ist in CSV nicht mehr eindeutig von diesem Marker unterscheidbar.\r\n"
"Binärwerte bleiben Hex-/Bitliterale. Text-NUL wird als \\0 geschrieben. Formelverdächtige Textfelder und Überschriften erhalten ein führendes Apostroph. CSV ist daher ein geschützter Anzeigeexport, kein bytegenaues Backup.\r\n"
"SQL-Tupel speichert die Originalbytes eines ausgewählten Tupels – ohne INSERT-Header, keine ausführbare Wiederherstellungsdatei.\r\n\r\n"
"ZEICHENSATZ\r\nUTF-8 ist voreingestellt. Für ältere Dumps Windows-1252 oder Latin-1 wählen. Die Auswahl steuert Anzeige, Filter und CSV. SQL-SET-NAMES wird nicht automatisch zur Zeichensatzerkennung benutzt. Andere Codierungen wie Shift-JIS/GBK und UTF-16 werden nicht unterstützt.\r\n\r\n"
"SQL-GRENZEN\r\nUnterstützt: gewöhnliche CREATE TABLE, USE, INSERT/REPLACE VALUES, Mehrfach-INSERT, explizite Spaltenlisten, MySQL-Escapes, einfache SQL_MODE-Umschaltungen und DELIMITER-Routinen. Einfache Views/Routinen werden nur als SQL-Text angezeigt.\r\n"
"Kein SQL wird ausgeführt. UPDATE/DELETE, REPLACE-Eindeutigkeitsregeln, Defaults, generierte Spalten, Constraints, ON DUPLICATE KEY, Views und Routinen werden nicht berechnet. SQL-Ausdrücke bleiben Literale. Code in Versionskommentaren wird außer SQL_MODE nicht interpretiert.\r\n"
"Maximal 4.096 Spalten; Vorschau/Filter/CSV bis 64 MiB je einzelnem SQL-Tupel. Größere Tupel bis knapp 4 GiB werden indexiert und lassen sich roh speichern. Vorschauen sind begrenzt und mit … markiert.\r\n\r\n"
"CACHE & DATENSCHUTZ\r\nCache: %LOCALAPPDATA%\\Bit-Line\\DumpBrowser\\Cache. 'Cache' öffnet den Ordner. Zum Löschen zuerst die Anwendung schließen. 'Neu indexieren' baut den Cache erneut auf. Keine Telemetrie, keine Netzwerkverbindung und keine automatischen Updates im Programmcode. Cache, Quellpfad und Schema sind lokal unverschlüsselt.\r\n"
"Der Cache-Fingerprint verwendet Pfad, Größe, Änderungszeit und Anfang/Ende der Datei, keine kryptografische Vollprüfung. Bei bewusst manipulierten Dateizeiten neu indexieren.\r\n\r\n"
"TASTATUR\r\nCtrl+O: öffnen · Ctrl+F: Filter · Enter: Filter/Seitensprung · Alt+←/→: Seite wechseln\r\nCtrl+C im Datengitter: sichtbare Zeilenvorschau als TSV · Esc: laufende Aktion abbrechen\r\n\r\n"
"LIEFERUNG\r\nAlle anwendungsspezifischen Bestandteile sind eingebaut. Genutzt werden nur Windows-Systembibliotheken. Keine MySQL-, Python-, .NET- oder Java-Installation nötig. Unterstützungsziel: Windows 11 x64, Windows 10 ab 1703 x64 mit den verwendeten Win32-APIs; die Windows-Oberfläche wurde in dieser Build-Umgebung nicht ausgeführt.\r\n";
static void do_command(int id){
    if(id==ID_CANCEL&&a.job){__atomic_store_n(&a.job->cancelled,1,__ATOMIC_RELAXED);update_buttons();return;}
    if(a.busy)return;
    if(id==ID_OPEN){char *p=file_dialog(0,L"MySQL-Dump öffnen",L"SQL-Dumps (*.sql;*.dump)\0*.sql;*.dump\0Alle Dateien\0*.*\0\0",NULL,NULL);if(p){open_path(p,0);free(p);}}
    else if(id==ID_DEMO){char *p=join(a.exe_dir,"samples/bitline-demo.sql");if(p){open_path(p,0);free(p);}}
    else if(id==ID_HELP)text_window("Bit-Line · Hilfe & technische Grenzen",help_text);
    else if(id==ID_CACHE){WCHAR *w=bl_wide(a.cache_root);if(w){ShellExecuteW(a.window,L"open",w,NULL,NULL,SW_SHOW);free(w);}}
    else if(id==ID_REINDEX&&a.db){if(MessageBoxW(a.window,L"Den Positionsindex dieses Dumps neu aufbauen?\nDie Originaldatei bleibt unverändert.",L"Bit-Line · Cache neu aufbauen",MB_YESNO|MB_ICONINFORMATION)==IDYES)open_path(a.db->source,1);}
    else if(id==ID_APPLY)start_filter();
    else if(id==ID_RESET&&a.db&&a.selected>=0){clear_filter();a.count=a.db->tables[a.selected].rows;text(a.query,"");load_page(0);}
    else if(id==ID_FIRST)load_page(0);
    else if(id==ID_PREV)load_page(a.start>=PAGE_SIZE?a.start-PAGE_SIZE:0);
    else if(id==ID_NEXT&&a.start+PAGE_SIZE<a.count)load_page(a.start+PAGE_SIZE);
    else if(id==ID_LAST)load_page(a.count?((a.count-1)/PAGE_SIZE)*PAGE_SIZE:0);
    else if(id==ID_GO){char *s=read_text(a.page_edit);if(s){uint64_t page=0;int valid=1;for(size_t i=0;s[i];i++){if(s[i]<'0'||s[i]>'9'||page>1844674407370955161ull){valid=0;break;}uint64_t next=page*10+(unsigned)(s[i]-'0');if(next<page){valid=0;break;}page=next;}free(s);uint64_t pages=a.count?(a.count-1)/PAGE_SIZE+1:1;if(!valid||!page||page>pages)info("Diese Seitennummer existiert nicht.",MB_ICONINFORMATION);else load_page((page-1)*PAGE_SIZE);}}
    else if(id==ID_EXPORT&&a.db&&a.selected>=0){char *p=file_dialog(1,a.filter_index?L"Alle Filtertreffer als CSV exportieren":L"Gesamte Tabelle als CSV exportieren",L"CSV-Dateien\0*.csv\0\0","bitline-export.csv",L"csv");if(p){Job *j=new_job(JOB_EXPORT);if(j){j->target=p;j->count=a.count;j->index=a.filter_index?bl_dup(a.filter_index):bl_index_path(a.db,a.selected);if(j->index)begin_job(j);else job_free(j);}else free(p);}}
    else if(id==ID_RAW){int row=selected_row();if(a.page&&row>=0&&(uint32_t)row<a.page->rows){char name[96];bl_format(name,sizeof(name),"tuple-byte-%llu.sql",(unsigned long long)a.page->refs[row].offset);char *p=file_dialog(1,L"Originales SQL-Rohtupel speichern (ohne INSERT-Header)",L"SQL-Tupel\0*.sql\0Alle Dateien\0*.*\0\0",name,L"sql");if(p){Job *j=new_job(JOB_RAW);if(j){j->target=p;j->ref=a.page->refs[row];begin_job(j);}else free(p);}}}
}
static void completed(Job *j){
    a.busy=0;a.job=NULL;if(a.closing){job_free(j);DestroyWindow(a.window);return;}
    int cancelled=__atomic_load_n(&j->cancelled,__ATOMIC_RELAXED);
    if(!j->ok){status(cancelled?"Vorgang abgebrochen. Originaldatei unverändert.":"Vorgang fehlgeschlagen. Details beachten.");update_buttons();if(!cancelled)info(j->error,MB_ICONERROR);}
    else if(j->type==JOB_OPEN){a.db=j->newdb;j->newdb=NULL;a.cache_lock=j->lock;j->lock=BL_BAD_FILE;char count[64],size[64],msg[512];number(count,sizeof(count),a.db->total_rows);bytes_text(size,sizeof(size),a.db->total_rows*16);
        bl_format(msg,sizeof(msg),"%s · %s Einträge · %s Positionsindex · %u Hinweise",a.db->reused?"Cache geladen":"Index erstellt",count,size,a.db->warnings);status(msg);
        size_t cap=strlen(basename8(a.db->source))+64;char *title=(char*)malloc(cap);if(title){bl_format(title,cap,"%s — Bit-Line Dump Browser",basename8(a.db->source));text(a.window,title);free(title);}refresh_view();build_tree();
    }else if(j->type==JOB_PAGE){page_free(a.page);a.page=j->page;j->page=NULL;a.start=j->start;refresh_view();if(a.page->rows){LVITEMW state={0};state.state=LVIS_SELECTED|LVIS_FOCUSED;state.stateMask=state.state;SendMessageW(a.grid,LVM_SETITEMSTATE,0,(LPARAM)&state);SendMessageW(a.grid,LVM_ENSUREVISIBLE,0,0);}char msg[1024],n[64],size[64];number(n,sizeof(n),a.db->total_rows);bytes_text(size,sizeof(size),a.db->source_size);bl_format(msg,sizeof(msg),"%s  ·  %s  ·  %s Einträge im Dump  ·  %u Hinweise  ·  Schreibgeschützt",basename8(a.db->source),size,n,a.db->warnings);status(msg);}
    else if(j->type==JOB_FILTER){char *dest=join(a.db->cache,"query.bli");if(dest&&bl_move(j->target,dest)){free(a.filter_index);a.filter_index=dest;a.count=j->count;load_page(0);}else{free(dest);info("Der Filterindex konnte nicht übernommen werden.",MB_ICONERROR);}}
    else if(j->type==JOB_DETAIL){status("Datensatzdetails geladen. Vorschauen können begrenzt sein.");text_window("Bit-Line · Datensatzdetails",j->text);}
    else if(j->type==JOB_EXPORT||j->type==JOB_RAW){size_t cap=strlen(j->target)+64;char *s=(char*)malloc(cap);if(s){bl_format(s,cap,"Export abgeschlossen: %s",j->target);status(s);free(s);}}
    update_buttons();job_free(j);
}
static void clipboard_row(void){if(a.mode!=0||!a.page)return;int row=selected_row();if(row<0||(uint32_t)row>=a.page->rows)return;size_t n=1;for(uint32_t c=1;c<a.page->cols;c++)n+=wlen(a.page->cells[row][c])+1;WCHAR *s=(WCHAR*)calloc(n,2);if(!s)return;size_t k=0;for(uint32_t c=1;c<a.page->cols;c++){if(c>1)s[k++]='\t';size_t len=wlen(a.page->cells[row][c]);memcpy(s+k,a.page->cells[row][c],len*2);k+=len;}copy_clipboard(s);free(s);status("Sichtbare Zeilenvorschau als TSV kopiert. Für vollständige Werte Details oder Export verwenden.");}
static void display_info(NMLVDISPINFOW *n){
    int row=n->item.iItem,col=n->item.iSubItem;if(!(n->item.mask&LVIF_TEXT))return;
    n->item.pszText=L"";
    if(a.mode==0&&a.page&&row>=0&&col>=0&&(uint32_t)row<a.page->rows&&(uint32_t)col<a.page->cols){n->item.pszText=a.page->cells[row]?a.page->cells[row][col]:L"";return;}
    if(a.mode==1&&a.db&&a.selected>=0&&row>=0&&(uint32_t)row<a.db->tables[a.selected].ncolumns){BLColumn *c=&a.db->tables[a.selected].columns[row];char count[40];char *s=NULL;
        if(col==0){number(count,sizeof(count),(uint64_t)row+1);s=bl_dup(count);}else{const char *raw=col==1?c->name:c->definition;BLValue v={(char*)raw,strlen(raw),BL_TEXT};s=preview(&v,(int)SendMessageW(a.encoding,CB_GETCURSEL,0,0),4096);}
        WCHAR *w=s?bl_wide(s):NULL;if(w){wcopy(a.scratch,sizeof(a.scratch)/2,w);free(w);n->item.pszText=a.scratch;}free(s);
    }
}
static void make_controls(void){
    fonts();button(L"Dump öffnen …",ID_OPEN);button(L"Demo laden",ID_DEMO);button(L"CSV exportieren",ID_EXPORT);button(L"SQL-Tupel …",ID_RAW);button(L"Neu indexieren",ID_REINDEX);button(L"Cache",ID_CACHE);button(L"Hilfe",ID_HELP);
    a.encoding=control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS,ID_ENCODING);SendMessageW(a.encoding,CB_ADDSTRING,0,(LPARAM)L"UTF-8");SendMessageW(a.encoding,CB_ADDSTRING,0,(LPARAM)L"Windows-1252");SendMessageW(a.encoding,CB_ADDSTRING,0,(LPARAM)L"Latin-1");SendMessageW(a.encoding,CB_SETCURSEL,0,0);
    a.tree_search=control(L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,ID_TREESEARCH);SendMessageW(a.tree_search,EM_SETCUEBANNER,1,(LPARAM)L"Tabellen / Datenbanken filtern");SendMessageW(a.tree_search,EM_SETLIMITTEXT,1024,0);
    a.tree=control(L"SysTreeView32",L"",WS_TABSTOP|TVS_HASBUTTONS|TVS_HASLINES|TVS_LINESATROOT|TVS_SHOWSELALWAYS,ID_TREE);SendMessageW(a.tree,TVM_SETBKCOLOR,0,RGB(255,255,255));SendMessageW(a.tree,TVM_SETTEXTCOLOR,0,RGB(39,72,101));SendMessageW(a.tree,TVM_SETITEMHEIGHT,px(27),0);SetWindowTheme(a.tree,L"Explorer",NULL);
    a.images=ImageList_Create(px(17),px(17),ILC_COLOR32|ILC_MASK,4,1);int ids[]={101,103,104,105};for(int i=0;i<4;i++){HICON icon=(HICON)LoadImageW(a.instance,MAKEINTRESOURCEW(ids[i]),IMAGE_ICON,px(17),px(17),0);if(icon){ImageList_AddIcon(a.images,icon);DestroyIcon(icon);}}SendMessageW(a.tree,TVM_SETIMAGELIST,0,(LPARAM)a.images);
    a.breadcrumb=control(L"STATIC",L"",0,ID_BREADCRUMB);a.tabs=control(L"SysTabControl32",L"",WS_TABSTOP,ID_TABS);LPCWSTR tabtitles[]={L"Daten",L"Struktur",L"Original-SQL",L"Import-Hinweise"};for(int i=0;i<4;i++){TCITEMW item={0};item.mask=TCIF_TEXT;item.pszText=(LPWSTR)tabtitles[i];SendMessageW(a.tabs,TCM_INSERTITEMW,i,(LPARAM)&item);}
    a.column=control(L"COMBOBOX",L"",WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWNLIST|CBS_HASSTRINGS,ID_COLUMN);SendMessageW(a.column,CB_SETDROPPEDWIDTH,px(300),0);
    a.op=control(L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|CBS_HASSTRINGS,ID_OPERATOR);LPCWSTR ops[]={L"enthält",L"entspricht",L"IS NULL",L"IS NOT NULL"};for(int i=0;i<4;i++)SendMessageW(a.op,CB_ADDSTRING,0,(LPARAM)ops[i]);SendMessageW(a.op,CB_SETCURSEL,0,0);
    a.query=control(L"EDIT",L"",WS_TABSTOP|WS_BORDER|ES_AUTOHSCROLL,ID_QUERY);SendMessageW(a.query,EM_SETCUEBANNER,1,(LPARAM)L"Filterwert …  (Ctrl+F)");SendMessageW(a.query,EM_SETLIMITTEXT,4096,0);button(L"Filtern",ID_APPLY);button(L"Reset",ID_RESET);
    a.filterlabel=control(L"STATIC",L"",0,ID_FILTERLABEL);a.grid=control(L"SysListView32",L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_OWNERDATA|LVS_SINGLESEL|LVS_SHOWSELALWAYS,ID_GRID);
    SendMessageW(a.grid,LVM_SETEXTENDEDLISTVIEWSTYLE,0,LVS_EX_GRIDLINES|LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER);SendMessageW(a.grid,LVM_SETBKCOLOR,0,RGB(255,255,255));SendMessageW(a.grid,LVM_SETTEXTBKCOLOR,0,RGB(255,255,255));SendMessageW(a.grid,LVM_SETTEXTCOLOR,0,RGB(38,63,85));SetWindowTheme(a.grid,L"Explorer",NULL);
    a.sql=control(L"EDIT",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,ID_SQL);SendMessageW(a.sql,EM_SETLIMITTEXT,8*1024*1024,0);SendMessageW(a.sql,WM_SETFONT,(WPARAM)a.mono,1);
    button(L"«",ID_FIRST);button(L"‹",ID_PREV);a.page_edit=control(L"EDIT",L"1",WS_TABSTOP|WS_BORDER|ES_NUMBER|ES_AUTOHSCROLL,ID_PAGE);SendMessageW(a.page_edit,EM_SETLIMITTEXT,20,0);button(L"Los",ID_GO);button(L"›",ID_NEXT);button(L"»",ID_LAST);a.pager=control(L"STATIC",L"",0,ID_PAGERLABEL);
    a.infoheader=control(L"STATIC",L"",0,ID_INFOHEADER);SendMessageW(a.infoheader,WM_SETFONT,(WPARAM)a.bold,1);a.detail=control(L"EDIT",L"",WS_VSCROLL|ES_MULTILINE|ES_AUTOVSCROLL|ES_READONLY,ID_DETAIL);SendMessageW(a.detail,EM_SETLIMITTEXT,131072,0);
    a.progress=control(L"msctls_progress32",L"",0,140);SendMessageW(a.progress,PBM_SETRANGE32,0,1000);button(L"Abbrechen",ID_CANCEL);DragAcceptFiles(a.window,1);SetTimer(a.window,1,150,NULL);refresh_view();
}
static LRESULT CALLBACK main_proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    switch(msg){
    case WM_CREATE:a.window=hwnd;a.dpi=(int)GetDpiForWindow(hwnd);if(a.dpi<96)a.dpi=96;make_controls();return 0;
    case WM_SIZE:layout();return 0;
    case WM_GETMINMAXINFO:{MINMAXINFO *mm=(MINMAXINFO*)lp;mm->ptMinTrackSize.x=px(960);mm->ptMinTrackSize.y=px(580);int sw=GetSystemMetrics(0),sh=GetSystemMetrics(1);if(mm->ptMinTrackSize.x>sw-16)mm->ptMinTrackSize.x=sw-16;if(mm->ptMinTrackSize.y>sh-55)mm->ptMinTrackSize.y=sh-55;return 0;}
    case WM_DPICHANGED:{a.dpi=LOWORD(wp);RECT *r=(RECT*)lp;fonts();SetWindowPos(hwnd,NULL,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);SendMessageW(a.tree,TVM_SETITEMHEIGHT,px(27),0);refresh_view();return 0;}
    case WM_PAINT:paint(hwnd);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_DRAWITEM:draw_button((DRAWITEMSTRUCT*)lp);return 1;
    case WM_CTLCOLOREDIT:case WM_CTLCOLORSTATIC:case WM_CTLCOLORLISTBOX:SetTextColor((HDC)wp,RGB(39,70,97));SetBkMode((HDC)wp,TRANSPARENT);return (LRESULT)a.white;
    case WM_COMMAND:{int id=LOWORD(wp),event=HIWORD(wp);
        if(id==ID_TREESEARCH&&event==EN_CHANGE&&!a.busy&&!a.rebuilding)build_tree();
        else if(id==ID_ENCODING&&event==CBN_SELCHANGE&&!a.busy){int was=a.selected;clear_filter();if(a.db&&was>=0){a.count=a.db->tables[was].rows;page_free(a.page);a.page=NULL;a.selected=-1;select_table(was);build_tree();}else refresh_view();}
        else if(event==0)do_command(id);return 0;}
    case WM_NOTIFY:{NMHDR *hdr=(NMHDR*)lp;
        if(hdr->hwndFrom==a.tree&&hdr->code==TVN_SELCHANGEDW&&!a.rebuilding){NMTREEVIEWW *tv=(NMTREEVIEWW*)lp;if(tv->itemNew.lParam>0)select_table((int)tv->itemNew.lParam-1);return 0;}
        if(hdr->hwndFrom==a.tabs&&hdr->code==TCN_SELCHANGE&&!a.busy){a.mode=(int)SendMessageW(a.tabs,TCM_GETCURSEL,0,0);refresh_view();if(a.mode==0&&!a.page&&a.db&&a.selected>=0&&a.db->tables[a.selected].kind==0)load_page(0);return 0;}
        if(hdr->hwndFrom==a.grid){if(hdr->code==LVN_GETDISPINFOW){display_info((NMLVDISPINFOW*)lp);return 0;}
            if(hdr->code==LVN_ITEMCHANGED){NMLISTVIEW *n=(NMLISTVIEW*)lp;if(a.mode==0&&(n->uNewState&LVIS_SELECTED)){table_info(n->iItem);update_buttons();}return 0;}
            if(hdr->code==NM_DBLCLK){open_detail();return 0;}
            if(hdr->code==NM_CUSTOMDRAW){NMLVCUSTOMDRAW *c=(NMLVCUSTOMDRAW*)lp;if(c->nmcd.dwDrawStage==CDDS_PREPAINT)return CDRF_NOTIFYITEMDRAW;if(c->nmcd.dwDrawStage==CDDS_ITEMPREPAINT)return 0x20;
                if(c->nmcd.dwDrawStage==(CDDS_ITEMPREPAINT|0x20000)){if(!(c->nmcd.uItemState&1)){c->clrTextBk=c->nmcd.dwItemSpec%2?RGB(246,250,253):RGB(255,255,255);c->clrText=RGB(41,65,86);
                    if(a.mode==0&&a.page&&c->nmcd.dwItemSpec<a.page->rows&&c->iSubItem>0&&(uint32_t)c->iSubItem<a.page->cols){int kind=a.page->kinds[c->nmcd.dwItemSpec][c->iSubItem];if(kind==BL_NULL||kind==BL_MISSING)c->clrText=RGB(136,152,166);else if(kind==BL_BINARY)c->clrText=RGB(41,129,175);else if(kind==BL_EXPR)c->clrText=RGB(147,102,50);}}
                    return CDRF_NEWFONT;}}
        }break;}
    case WM_TIMER:if(a.job){Job *j=a.job;uint64_t done=__atomic_load_n(&j->done,__ATOMIC_RELAXED),total=__atomic_load_n(&j->total,__ATOMIC_RELAXED),found=__atomic_load_n(&j->found,__ATOMIC_RELAXED);const char *phase=__atomic_load_n(&j->phase,__ATOMIC_RELAXED);char n[64],s[512];number(n,sizeof(n),found);uint64_t percent=total?done/(total/1000+1):0;if(percent>1000)percent=1000;SendMessageW(a.progress,PBM_SETPOS,(WPARAM)percent,0);
            if(__atomic_load_n(&j->cancelled,__ATOMIC_RELAXED))status("Abbruch angefordert – laufende Dateioperation wird beendet …");else{bl_format(s,sizeof(s),"%s …  %llu %%  ·  %s Einträge",phase?phase:"Datei vorbereiten",(unsigned long long)(percent/10),n);status(s);}}return 0;
    case WM_DROPFILES:{HANDLE drop=(HANDLE)wp;if(!a.busy){UINT n=DragQueryFileW(drop,0,NULL,0);WCHAR *w=(WCHAR*)calloc((size_t)n+1,2);if(w){DragQueryFileW(drop,0,w,n+1);char *p=bl_utf8(w);free(w);if(p){open_path(p,0);free(p);}}}DragFinish(drop);return 0;}
    case MSG_OPEN_PATH:{char *p=(char*)lp;if(p){open_path(p,0);free(p);}return 0;}
    case MSG_DONE:completed((Job*)lp);return 0;
    case WM_CLOSE:if(a.busy){if(MessageBoxW(hwnd,L"Laufenden Vorgang abbrechen und das Programm schließen?",L"Bit-Line Dump Browser",MB_YESNO|MB_ICONWARNING)==IDYES){a.closing=1;__atomic_store_n(&a.job->cancelled,1,__ATOMIC_RELAXED);update_buttons();}return 0;}DestroyWindow(hwnd);return 0;
    case WM_DESTROY:KillTimer(hwnd,1);close_database();if(a.images)ImageList_Destroy(a.images);PostQuitMessage(0);return 0;
    }return DefWindowProcW(hwnd,msg,wp,lp);
}
static int init_paths(void){
    WCHAR *w=(WCHAR*)calloc(32768,2);if(!w)return 0;GetModuleFileNameW(NULL,w,32768);char *exe=bl_utf8(w);if(!exe){free(w);return 0;}size_t n=strlen(exe);while(n&&exe[n-1]!='/'&&exe[n-1]!='\\')n--;if(n)exe[n-1]=0;a.exe_dir=exe;
    if(!GetEnvironmentVariableW(L"LOCALAPPDATA",w,32768))GetTempPathW(32768,w);char *base=bl_utf8(w);free(w);if(!base)return 0;char *brand=join(base,"Bit-Line"),*app=brand?join(brand,"DumpBrowser"):NULL;a.cache_root=app?join(app,"Cache"):NULL;int ok=brand&&app&&a.cache_root&&bl_mkdir(brand)&&bl_mkdir(app)&&bl_mkdir(a.cache_root);free(base);free(brand);free(app);return ok;
}
void WINAPI WinMainCRTStartup(void){
    memset(&a,0,sizeof(a));a.instance=GetModuleHandleW(NULL);a.selected=-1;a.dpi=96;a.cache_lock=BL_BAD_FILE;
    SetProcessDpiAwarenessContext((HANDLE)(intptr_t)-4);
    INITCOMMONCONTROLSEX ic={sizeof(ic),0x000040ff};if(!InitCommonControlsEx(&ic)){MessageBoxW(NULL,L"Windows Common Controls konnten nicht initialisiert werden.",L"Bit-Line Dump Browser",MB_ICONERROR);ExitProcess(1);}
    if(!init_paths()){MessageBoxW(NULL,L"Der Cache-Ordner kann nicht angelegt werden. Schreibrechte in LOCALAPPDATA prüfen.",L"Bit-Line Dump Browser",MB_ICONERROR);ExitProcess(1);}
    a.white=CreateSolidBrush(RGB(255,255,255));a.background=CreateSolidBrush(RGB(244,248,251));a.line=CreateSolidBrush(RGB(219,231,240));a.blue=CreateSolidBrush(RGB(49,148,203));a.pale=CreateSolidBrush(RGB(230,244,253));
    a.icon=(HICON)LoadImageW(a.instance,MAKEINTRESOURCEW(101),IMAGE_ICON,64,64,0);a.logo=(HBITMAP)LoadImageW(a.instance,MAKEINTRESOURCEW(102),IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION);
    WNDCLASSEXW wc={0};wc.cbSize=sizeof(wc);wc.style=3;wc.lpfnWndProc=main_proc;wc.hInstance=a.instance;wc.hIcon=a.icon;wc.hIconSm=a.icon;wc.hCursor=LoadCursorW(NULL,MAKEINTRESOURCEW(IDC_ARROW));wc.hbrBackground=a.background;wc.lpszClassName=WINCLASS;
    if(!RegisterClassExW(&wc))ExitProcess(2);wc.lpfnWndProc=text_proc;wc.lpszClassName=TEXTCLASS;RegisterClassExW(&wc);
    int sw=GetSystemMetrics(0),sh=GetSystemMetrics(1),ww=1360,hh=860;if(ww>sw-40)ww=sw-40;if(hh>sh-70)hh=sh-70;
    bl_format(a.status,sizeof(a.status),"Bereit · .sql-Datei öffnen oder ins Fenster ziehen · Keine MySQL-Installation nötig");
    HWND hwnd=CreateWindowExW(WS_EX_CONTROLPARENT,WINCLASS,L"Bit-Line Dump Browser",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN,(sw-ww)/2,(sh-hh)/2,ww,hh,NULL,NULL,a.instance,NULL);
    if(!hwnd)ExitProcess(3);ShowWindow(hwnd,SW_SHOW);UpdateWindow(hwnd);
    int argc=0;LPWSTR *argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argv&&argc>1){char *path=bl_utf8(argv[1]);if(path)PostMessageW(hwnd,MSG_OPEN_PATH,0,(LPARAM)path);}if(argv)LocalFree(argv);
    MSG msg;BOOL result;
    while((result=GetMessageW(&msg,NULL,0,0))>0){
        if(modal){if(msg.message==WM_KEYDOWN&&msg.wParam==VK_ESCAPE){DestroyWindow(modal);continue;}if(IsDialogMessageW(modal,&msg))continue;}
        else if(msg.message==WM_KEYDOWN||msg.message==WM_SYSKEYDOWN){int ctrl=GetKeyState(VK_CONTROL)<0,alt=GetKeyState(VK_MENU)<0;
            if(msg.wParam==VK_ESCAPE&&a.busy){do_command(ID_CANCEL);continue;}
            if(!a.busy){if(ctrl&&msg.wParam=='O'){do_command(ID_OPEN);continue;}if(ctrl&&msg.wParam=='F'&&a.db&&a.selected>=0){a.mode=0;SendMessageW(a.tabs,TCM_SETCURSEL,0,0);refresh_view();SetFocus(a.query);SendMessageW(a.query,EM_SETSEL,0,-1);continue;}
                if(ctrl&&msg.wParam=='C'&&GetFocus()==a.grid){clipboard_row();continue;}
                if(msg.wParam==VK_RETURN&&GetFocus()==a.query){start_filter();continue;}if(msg.wParam==VK_RETURN&&GetFocus()==a.page_edit){do_command(ID_GO);continue;}
                if(alt&&msg.wParam==VK_LEFT){do_command(ID_PREV);continue;}if(alt&&msg.wParam==VK_RIGHT){do_command(ID_NEXT);continue;}}
        }
        if(!modal&&IsDialogMessageW(hwnd,&msg))continue;TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    free(a.cache_root);free(a.exe_dir);ExitProcess(result<0?1:0);
}

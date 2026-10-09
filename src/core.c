/* Bit-Line Dump Browser: read-only, streaming MySQL dump index.
   No SQL is executed. The cache stores positions, layouts and schema, not a database copy. */
#include "core.h"
#define BUFFER_SIZE (1024u*1024u)
#define DDL_LIMIT (2u*1024u*1024u)
#define NOTE_LIMIT 65536u
#define CACHE_MAGIC "BLDUMPIDX04\0"
_Static_assert(sizeof(BLRef)==16,"RowRef layout must be exactly 16 bytes");
static int fail(BLDatabase *d,const char *s){bl_format(d->error,sizeof(d->error),"%s",s);return 0;}
static int whitespace(int c){return c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f';}
static int lower(int c){return c>='A'&&c<='Z'?c+32:c;}
static int eq(const char *a,const char *b){while(*a&&*b){if(lower((unsigned char)*a++)!=lower((unsigned char)*b++))return 0;}return *a==*b;}
static int word(int c){return c>=128||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='$'||c=='@';}
static char *trimdup(const char *s,size_t n){while(n&&whitespace((unsigned char)*s)){s++;n--;}while(n&&whitespace((unsigned char)s[n-1]))n--;return bl_ndup(s,n);}
static void note(BLDatabase *d,const char *text){
    d->warnings++;size_t old=d->notes?strlen(d->notes):0,n=strlen(text);
    if(old+n+4>NOTE_LIMIT)return;char *p=(char*)realloc(d->notes,old+n+4);if(!p)return;
    d->notes=p;memcpy(p+old,text,n);p[old+n]='\r';p[old+n+1]='\n';p[old+n+2]=0;
}
uint64_t bl_hash(const void *p,size_t n,uint64_t h){const unsigned char *s=(const unsigned char*)p;if(!h)h=14695981039346656037ull;while(n--)h=(h^*s++)*1099511628211ull;return h;}
void bl_db_init(BLDatabase *d){memset(d,0,sizeof(*d));d->source_file=BL_BAD_FILE;}
static void tables_free(BLDatabase *d){
    for(uint32_t i=0;i<d->ntables;i++){BLTable *t=&d->tables[i];free(t->database);free(t->name);free(t->ddl);
        for(uint32_t j=0;j<t->ncolumns;j++){free(t->columns[j].name);free(t->columns[j].definition);}free(t->columns);
        for(uint32_t j=0;j<t->nlayouts;j++)free(t->layouts[j].columns);free(t->layouts);
    }free(d->tables);d->tables=NULL;d->ntables=0;d->total_rows=0;
}
void bl_db_free(BLDatabase *d){tables_free(d);free(d->source);free(d->cache);free(d->notes);free(d->read_buffer);bl_close(d->source_file);bl_db_init(d);}
char *bl_index_path(const BLDatabase *d,uint32_t table){size_t n=strlen(d->cache)+40;char *p=(char*)malloc(n);if(p)bl_format(p,n,"%s/t%06u.bli",d->cache,table);return p;}
static char *cache_path(const BLDatabase *d,const char *name){size_t n=strlen(d->cache)+strlen(name)+2;char *p=(char*)malloc(n);if(p)bl_format(p,n,"%s/%s",d->cache,name);return p;}
int bl_db_prepare(BLDatabase *d,const char *source,const char *root){
    d->source=bl_dup(source);d->source_file=bl_open(source,0);
    if(d->source_file==BL_BAD_FILE)return fail(d,"Die Dump-Datei kann nicht schreibgeschützt geöffnet werden.");
    d->source_size=bl_size(d->source_file);d->source_mtime=bl_mtime(d->source_file);
    unsigned char *buf=(unsigned char*)malloc(65536);if(!buf)return fail(d,"Nicht genügend Arbeitsspeicher.");
    size_t n=bl_read(d->source_file,buf,65536);
    if(n>=2&&((buf[0]==0x1f&&buf[1]==0x8b)||(buf[0]=='P'&&buf[1]=='K'))){free(buf);return fail(d,"Komprimierter Dump: Bitte .gz / .zip zunächst entpacken und die .sql-Datei öffnen.");}
    if(n>=2&&((buf[0]==0xff&&buf[1]==0xfe)||(buf[0]==0xfe&&buf[1]==0xff))){free(buf);return fail(d,"UTF-16/UTF-32 wird nicht unterstützt. Bitte den Dump zunächst als UTF-8 speichern.");}
    d->source_hash=bl_hash(buf,n,0);
    if(d->source_size>65536){bl_seek(d->source_file,d->source_size-65536);n=bl_read(d->source_file,buf,65536);d->source_hash=bl_hash(buf,n,d->source_hash);}free(buf);
    uint64_t h=bl_hash(source,strlen(source),d->source_hash);h=bl_hash(&d->source_size,8,h);h=bl_hash(&d->source_mtime,8,h);
    size_t cap=strlen(root)+32;d->cache=(char*)malloc(cap);if(!d->cache)return fail(d,"Nicht genügend Arbeitsspeicher.");
    bl_format(d->cache,cap,"%s/%016llx",root,(unsigned long long)h);
    if(!bl_mkdir(root)||!bl_mkdir(d->cache))return fail(d,"Der lokale Cache-Ordner kann nicht angelegt werden. Schreibrechte und freien Platz prüfen.");
    return 1;
}
static int table_find(BLDatabase *d,const char *database,const char *name,int kind){for(uint32_t i=0;i<d->ntables;i++)if(d->tables[i].kind==kind&&!strcmp(d->tables[i].database,database)&&!strcmp(d->tables[i].name,name))return (int)i;return -1;}
static int table_add(BLDatabase *d,const char *database,const char *name,int kind){
    int found=table_find(d,database,name,kind);if(found>=0)return found;
    if(d->ntables>=65536){fail(d,"Mehr als 65.536 Schemaobjekte werden nicht unterstützt.");return -1;}
    BLTable *p=(BLTable*)realloc(d->tables,(d->ntables+1)*sizeof(BLTable));if(!p){fail(d,"Nicht genügend Arbeitsspeicher für das Schema.");return -1;}
    d->tables=p;BLTable *t=&p[d->ntables];memset(t,0,sizeof(*t));t->database=bl_dup(database);t->name=bl_dup(name);t->ddl=bl_dup("");t->kind=kind;
    if(!t->database||!t->name||!t->ddl){free(t->database);free(t->name);free(t->ddl);fail(d,"Nicht genügend Arbeitsspeicher.");return -1;}return (int)d->ntables++;
}
static int column_add(BLDatabase *d,BLTable *t,const char *name,const char *definition){
    for(uint32_t i=0;i<t->ncolumns;i++)if(eq(t->columns[i].name,name))return (int)i;
    if(t->ncolumns>=BL_MAX_COLUMNS){fail(d,"Die Tabelle überschreitet das Limit von 4.096 Spalten.");return -1;}
    BLColumn *p=(BLColumn*)realloc(t->columns,(t->ncolumns+1)*sizeof(BLColumn));if(!p){fail(d,"Nicht genügend Arbeitsspeicher.");return -1;}t->columns=p;
    p[t->ncolumns].name=bl_dup(name);p[t->ncolumns].definition=bl_dup(definition);
    if(!p[t->ncolumns].name||!p[t->ncolumns].definition){free(p[t->ncolumns].name);free(p[t->ncolumns].definition);fail(d,"Nicht genügend Arbeitsspeicher.");return -1;}return (int)t->ncolumns++;
}
/* Reader position always refers to original bytes, including comments, escapes and line breaks. */
typedef struct {
    BLDatabase *db;const BLOptions *opt;unsigned char *buf;uint64_t pos,base;size_t len;
    int failed,noesc,saved_noesc;char delimiter[20];uint64_t last_progress;
} Reader;
static int progress(Reader *r,const char *phase){
    uint64_t now=bl_millis();if(now-r->last_progress<100&&r->pos<r->db->source_size)return !r->failed;r->last_progress=now;
    if(r->opt&&r->opt->progress&&!r->opt->progress(r->opt->user,r->pos,r->db->source_size,r->db->total_rows,phase)){r->failed=1;fail(r->db,"Vorgang abgebrochen.");}
    return !r->failed;
}
static int rp(Reader *r,unsigned k){
    if(r->failed||r->pos+k>=r->db->source_size)return -1;
    if(r->pos<r->base||r->pos+k>=r->base+r->len){
        if(!progress(r,"Dump indexieren"))return -1;
        r->base=r->pos;if(!bl_seek(r->db->source_file,r->base)){r->failed=1;fail(r->db,"Leseposition im Dump konnte nicht gesetzt werden.");return -1;}
        r->len=bl_read(r->db->source_file,r->buf,BUFFER_SIZE);
        if(r->pos+k>=r->base+r->len){r->failed=1;fail(r->db,"Lesefehler im Dump. Datei oder Datenträger prüfen.");return -1;}
    }return r->buf[(size_t)(r->pos-r->base)+k];
}
static int rg(Reader *r){int c=rp(r,0);if(c>=0)r->pos++;return c;}
static int rmatch(Reader *r,const char *s){for(unsigned i=0;s[i];i++)if(rp(r,i)!=(unsigned char)s[i])return 0;return 1;}
static void apply_mode(Reader *r,const char *s){
    size_t n=strlen(s);
    if(strstr(s,"@OLD_SQL_MODE=@@SQL_MODE")||strstr(s,"@old_sql_mode=@@sql_mode"))r->saved_noesc=r->noesc;
    for(size_t i=0;i+8<=n;i++){
        if(i&&word((unsigned char)s[i-1]))continue;
        const char *p=s+i;const char *key="sql_mode";int same=1;for(int j=0;j<8;j++)if(lower((unsigned char)p[j])!=key[j])same=0;if(!same)continue;
        p+=8;while(whitespace((unsigned char)*p))p++;if(*p!='=')continue;p++;while(whitespace((unsigned char)*p))p++;
        if(*p=='\''||*p=='"'){char q=*p++;char mode[512];size_t k=0;while(*p&&*p!=q&&k+1<sizeof(mode))mode[k++]=(char)lower((unsigned char)*p++);mode[k]=0;r->noesc=strstr(mode,"no_backslash_escapes")!=NULL;}
        else if(*p=='@')r->noesc=r->saved_noesc;
        else if((p[0]=='D'||p[0]=='d')&&(p[1]=='E'||p[1]=='e'))r->noesc=0;
        else {r->failed=1;fail(r->db,"Dynamischer SQL_MODE-Ausdruck wird nicht unterstützt. Für einen zuverlässigen Import einen normalen mysqldump verwenden.");}
    }
}
static void rcomment(Reader *r){
    int c=rg(r);if(c=='#'||c=='-'){if(c=='-')rg(r);while((c=rg(r))>=0&&c!='\n'){}return;}
    rg(r);int version=rp(r,0)=='!'||(rp(r,0)=='M'&&rp(r,1)=='!');
    char *buf=version?(char*)malloc(65537):NULL;size_t n=0;int done=0;
    while((c=rg(r))>=0){if(c=='*'&&rp(r,0)=='/'){rg(r);done=1;break;}if(buf&&n<65536)buf[n++]=(char)c;}
    if(!done){r->failed=1;fail(r->db,"Nicht abgeschlossener Blockkommentar im Dump.");}
    if(buf){buf[n]=0;apply_mode(r,buf);
        if(strstr(buf,"CREATE")&&(strstr(buf,"VIEW")||strstr(buf,"PROCEDURE")||strstr(buf,"FUNCTION")||strstr(buf,"TRIGGER")||strstr(buf,"EVENT")))note(r->db,"View-/Routinen-Code in einem Versionskommentar wurde nicht als Schemaobjekt übernommen. Originaldatei enthält den vollständigen SQL-Text.");
        free(buf);}
}
static void rskip(Reader *r){for(;;){int c=rp(r,0);if(whitespace(c)){rg(r);continue;}if(c=='#'||(c=='-'&&rp(r,1)=='-'&&(whitespace(rp(r,2))||rp(r,2)<0))||(c=='/'&&rp(r,1)=='*')){rcomment(r);continue;}break;}}
typedef struct {char text[8192];int kind;uint64_t start,end;} Token;
static int rt(Reader *r,Token *t){
    rskip(r);t->start=r->pos;t->text[0]=0;int c=rg(r);if(c<0){t->kind=0;return 0;}size_t n=0;t->kind=1;
    if(c=='`'||c=='\''||c=='"'){
        int q=c;t->kind=c=='`'?2:3;int closed=0;
        while((c=rg(r))>=0){if(c==q){if(rp(r,0)==q){rg(r);}else{closed=1;break;}}
            else if(c=='\\'&&q!='`'&&!r->noesc){c=rg(r);if(c<0)break;}
            if(n+1>=sizeof(t->text)){r->failed=1;fail(r->db,"Ein SQL-Bezeichner oder Header-Token ist zu lang.");return 0;}t->text[n++]=(char)c;
        }if(!closed){r->failed=1;fail(r->db,"Nicht abgeschlossene Zeichenkette oder Bezeichner im SQL-Header.");return 0;}
    }else if(word(c)){t->text[n++]=(char)c;while(word(rp(r,0))){if(n+1>=sizeof(t->text)){r->failed=1;fail(r->db,"Zu langes Token im SQL-Header.");return 0;}t->text[n++]=(char)rg(r);}}
    else {t->kind=4;t->text[n++]=(char)c;}
    t->text[n]=0;t->end=r->pos;return 1;
}
static int rquoted(Reader *r,int q){int c;while((c=rg(r))>=0){if(c==q){if(rp(r,0)==q){rg(r);continue;}return 1;}if(c=='\\'&&q!='`'&&!r->noesc){if(rg(r)<0)break;}}r->failed=1;return fail(r->db,"Nicht abgeschlossene Zeichenkette im Dump.");}
/* Consume through the active delimiter, never buffering the complete statement. */
static int rend(Reader *r){
    while(rp(r,0)>=0){if(rmatch(r,r->delimiter)){r->pos+=strlen(r->delimiter);return 1;}
        int c=rg(r);if(c=='\''||c=='"'||c=='`'){if(!rquoted(r,c))return 0;}
        else if(c=='#'||(c=='-'&&rp(r,0)=='-'&&whitespace(rp(r,1)))||(c=='/'&&rp(r,0)=='*')){r->pos--;rcomment(r);}
    }return !r->failed; /* Some schema-only dumps omit the final semicolon. */
}
static char *slice(BLDatabase *d,uint64_t off,uint64_t len){
    if(len>DDL_LIMIT){len=DDL_LIMIT;note(d,"Ein Schema-/SQL-Text wurde in der Vorschau auf 2 MiB begrenzt.");}
    char *s=(char*)malloc((size_t)len+1);if(!s)return NULL;
    if(!bl_seek(d->source_file,off)||bl_read(d->source_file,s,(size_t)len)!=(size_t)len){free(s);return NULL;}s[len]=0;return s;
}
/* A bounded in-memory lexer is used only for CREATE metadata, never for INSERT payloads. */
typedef struct {const char *s;size_t n,p;int noesc;} MemLex;
static void mskip(MemLex *m){for(;;){while(m->p<m->n&&whitespace((unsigned char)m->s[m->p]))m->p++;
    if(m->p+1<m->n&&m->s[m->p]=='/'&&m->s[m->p+1]=='*'){m->p+=2;while(m->p+1<m->n&&(m->s[m->p]!='*'||m->s[m->p+1]!='/'))m->p++;m->p=m->p+1<m->n?m->p+2:m->n;continue;}
    if(m->p<m->n&&(m->s[m->p]=='#'||(m->p+2<m->n&&m->s[m->p]=='-'&&m->s[m->p+1]=='-'&&whitespace(m->s[m->p+2])))){while(m->p<m->n&&m->s[m->p]!='\n')m->p++;continue;}break;}}
static int mt(MemLex *m,Token *t){mskip(m);t->start=m->p;size_t k=0;if(m->p>=m->n){t->kind=0;t->text[0]=0;return 0;}int c=(unsigned char)m->s[m->p++];t->kind=1;
    if(c=='`'||c=='\''||c=='"'){int q=c;t->kind=c=='`'?2:3;while(m->p<m->n){c=(unsigned char)m->s[m->p++];if(c==q){if(m->p<m->n&&m->s[m->p]==q)m->p++;else break;}else if(c=='\\'&&q!='`'&&!m->noesc&&m->p<m->n)c=(unsigned char)m->s[m->p++];if(k+1<sizeof(t->text))t->text[k++]=(char)c;}}
    else if(word(c)){t->text[k++]=(char)c;while(m->p<m->n&&word((unsigned char)m->s[m->p])){c=(unsigned char)m->s[m->p++];if(k+1<sizeof(t->text))t->text[k++]=(char)c;}}
    else {t->kind=4;t->text[k++]=(char)c;}t->text[k]=0;t->end=m->p;return 1;
}
/* Split comma-separated fields at depth zero, honoring SQL strings and comments. */
static size_t field_end(const char *s,size_t n,size_t p,int noesc,char endchar){
    int depth=0,q=0;for(;p<n;p++){int c=(unsigned char)s[p];if(q){if(c=='\\'&&q!='`'&&!noesc&&p+1<n)p++;else if(c==q){if(p+1<n&&s[p+1]==q)p++;else q=0;}continue;}
        if(c=='\''||c=='"'||c=='`'){q=c;continue;}
        if(c=='/'&&p+1<n&&s[p+1]=='*'){p+=2;while(p+1<n&&(s[p]!='*'||s[p+1]!='/'))p++;if(p+1<n)p++;continue;}
        if(c=='#'||(c=='-'&&p+2<n&&s[p+1]=='-'&&whitespace(s[p+2]))){while(p<n&&s[p]!='\n')p++;continue;}
        if(c=='(')depth++;else if(c==')'){if(depth==0&&endchar==')')return p;depth--;}
        else if(c==','&&depth==0)return p;
    }return p;
}
static int parse_create(BLDatabase *d,const char *sql,const char *current,int noesc){
    MemLex m={sql,strlen(sql),0,noesc};Token t;mt(&m,&t);int kind=-1;
    while(mt(&m,&t)){if(eq(t.text,"TABLE")){kind=0;break;}if(eq(t.text,"DATABASE")||eq(t.text,"SCHEMA")){kind=3;break;}if(eq(t.text,"VIEW")){kind=1;break;}if(eq(t.text,"PROCEDURE")||eq(t.text,"FUNCTION")||eq(t.text,"TRIGGER")||eq(t.text,"EVENT")){kind=2;break;}}
    if(kind<0)return 1;
    if(!mt(&m,&t))return fail(d,"Unvollständiges CREATE-Statement.");
    if(eq(t.text,"IF")){mt(&m,&t);mt(&m,&t);mt(&m,&t);}
    char *db=bl_dup(current),*name=bl_dup(t.text);if(!db||!name){free(db);free(name);return fail(d,"Nicht genügend Arbeitsspeicher.");}
    mt(&m,&t);if(!strcmp(t.text,".")){free(db);db=name;if(!mt(&m,&t)){free(db);return fail(d,"Unvollständiger qualifizierter Tabellenname.");}name=bl_dup(t.text);mt(&m,&t);}
    if(kind==3){free(db);db=name;name=bl_dup("");}
    int id=table_add(d,db,name,kind);free(db);free(name);if(id<0)return 0;BLTable *tb=&d->tables[id];
    if(tb->rows&&kind==0){note(d,"CREATE TABLE nach INSERT: Daten werden als Dump-Literale angezeigt, nicht als ausgeführter SQL-Endzustand.");}
    free(tb->ddl);tb->ddl=bl_dup(sql);if(!tb->ddl)return fail(d,"Nicht genügend Arbeitsspeicher.");
    if(kind!=0||strcmp(t.text,"("))return 1;
    size_t p=m.p;
    while(p<m.n){size_t e=field_end(sql,m.n,p,noesc,')');if(e<=p)break;MemLex field={sql+p,e-p,0,noesc};Token col;
        if(mt(&field,&col)){
            int constraint=col.kind!=2&&(eq(col.text,"PRIMARY")||eq(col.text,"KEY")||eq(col.text,"UNIQUE")||eq(col.text,"CONSTRAINT")||eq(col.text,"FOREIGN")||eq(col.text,"INDEX")||eq(col.text,"FULLTEXT")||eq(col.text,"SPATIAL")||eq(col.text,"CHECK"));
            if(!constraint){char *def=trimdup(sql+p+field.p,e-p-field.p);if(!def)return fail(d,"Nicht genügend Arbeitsspeicher.");int cid=column_add(d,tb,col.text,def);
                if(cid>=0){free(tb->columns[cid].definition);tb->columns[cid].definition=def;}else{free(def);return 0;}}
        }if(e>=m.n||sql[e]==')')break;p=e+1;
    }return 1;
}
typedef struct {BLFile file;uint32_t table;BLRef *buf;size_t used;} Sink;
static int sink_flush(BLDatabase *d,Sink *s){if(s->used&&!bl_write(s->file,s->buf,s->used*sizeof(BLRef)))return fail(d,"Cache kann nicht geschrieben werden. Freien Speicherplatz prüfen.");s->used=0;return 1;}
static int sink_put(BLDatabase *d,Sink *s,uint32_t table,BLRef ref){
    if(s->file==BL_BAD_FILE||s->table!=table){if(!sink_flush(d,s))return 0;bl_close(s->file);s->file=BL_BAD_FILE;char *path=bl_index_path(d,table);if(!path)return fail(d,"Nicht genügend Arbeitsspeicher.");s->file=bl_open(path,d->tables[table].rows?2:1);free(path);s->table=table;if(s->file==BL_BAD_FILE)return fail(d,"Indexdatei kann nicht angelegt werden.");}
    s->buf[s->used++]=ref;if(s->used==4096)return sink_flush(d,s);return 1;
}
static int layout_get(BLDatabase *d,BLTable *t,const uint32_t *map,uint32_t n){
    for(uint32_t i=0;i<t->nlayouts;i++)if(t->layouts[i].n==n&&(!n||!memcmp(t->layouts[i].columns,map,n*4)))return (int)i;
    if(t->nlayouts>=65536){fail(d,"Zu viele unterschiedliche INSERT-Spaltenlayouts.");return -1;}
    BLLayout *p=(BLLayout*)realloc(t->layouts,(t->nlayouts+1)*sizeof(BLLayout));if(!p){fail(d,"Nicht genügend Arbeitsspeicher.");return -1;}t->layouts=p;
    p[t->nlayouts].n=n;p[t->nlayouts].columns=n?(uint32_t*)malloc(n*4):NULL;if(n&&!p[t->nlayouts].columns){fail(d,"Nicht genügend Arbeitsspeicher.");return -1;}
    if(n)memcpy(p[t->nlayouts].columns,map,n*4);return (int)t->nlayouts++;
}
static int tuple(Reader *r,BLRef *ref,uint32_t *cols){
    rskip(r);ref->offset=r->pos;if(rg(r)!='('){r->failed=1;return fail(r->db,"INSERT VALUES: Ein Datentupel mit '(' wurde erwartet.");}
    int depth=1,c;uint32_t n=1;rskip(r);if(rp(r,0)==')')n=0;
    while((c=rg(r))>=0){if(c=='\''||c=='"'||c=='`'){if(!rquoted(r,c))return 0;}
        else if(c=='(')depth++;
        else if(c==')'){if(--depth==0){uint64_t len=r->pos-ref->offset;if(len>0xffffffffu)return fail(r->db,"Ein einzelner Datensatz ist größer als 4 GiB und kann nicht indexiert werden.");ref->length=(uint32_t)len;*cols=n;return 1;}}
        else if(c==','&&depth==1){if(++n>BL_MAX_COLUMNS)return fail(r->db,"Mehr als 4.096 Werte in einem Datensatz.");}
        else if(c=='#'||(c=='/'&&rp(r,0)=='*')||(c=='-'&&rp(r,0)=='-'&&whitespace(rp(r,1)))){r->pos--;rcomment(r);}
        if(depth>4096)return fail(r->db,"SQL-Ausdruck ist zu tief verschachtelt.");
    }r->failed=1;return fail(r->db,"Unvollständiges INSERT: Datentupel oder Zeichenkette endet vor der schließenden Klammer.");
}
static int insert(Reader *r,const char *current,Sink *sink,int replace){
    BLDatabase *d=r->db;Token t;if(!rt(r,&t))return fail(d,"Unvollständiger INSERT-Header.");
    while(eq(t.text,"LOW_PRIORITY")||eq(t.text,"HIGH_PRIORITY")||eq(t.text,"DELAYED")||eq(t.text,"IGNORE")){if(!rt(r,&t))return fail(d,"Unvollständiger INSERT-Header.");}
    if(eq(t.text,"INTO"))if(!rt(r,&t))return fail(d,"Tabellenname nach INTO fehlt.");
    char *database=bl_dup(current),*name=bl_dup(t.text);if(!database||!name){free(database);free(name);return fail(d,"Nicht genügend Arbeitsspeicher.");}
    if(!rt(r,&t)){free(database);free(name);return fail(d,"INSERT-Header endet vor VALUES.");}
    if(!strcmp(t.text,".")){free(database);database=name;if(!rt(r,&t)){free(database);return fail(d,"Tabellenname nach Datenbank fehlt.");}name=bl_dup(t.text);rt(r,&t);}
    int id=table_add(d,database,name,0);free(database);free(name);if(id<0)return 0;BLTable *tb=&d->tables[id];
    uint32_t *map=(uint32_t*)malloc(BL_MAX_COLUMNS*4);if(!map)return fail(d,"Nicht genügend Arbeitsspeicher.");int explicit_cols=0;uint32_t nmap=0;
    if(!strcmp(t.text,"(")){explicit_cols=1;if(!rt(r,&t)){free(map);return fail(d,"Unvollständige INSERT-Spaltenliste.");}
        while(strcmp(t.text,")")){
            if(nmap>=BL_MAX_COLUMNS){free(map);return fail(d,"Zu viele INSERT-Spalten.");}
            int col=column_add(d,tb,t.text,"(Definition nicht im Dump)");if(col<0){free(map);return 0;}
            for(uint32_t k=0;k<nmap;k++)if(map[k]==(uint32_t)col){free(map);return fail(d,"Doppelte Spalte in einer INSERT-Spaltenliste.");}
            map[nmap++]=(uint32_t)col;if(!rt(r,&t)){free(map);return fail(d,"Unvollständige INSERT-Spaltenliste.");}if(!strcmp(t.text,")"))break;
            if(strcmp(t.text,",")||!rt(r,&t)){free(map);return fail(d,"Ungültige INSERT-Spaltenliste.");}
        }if(!rt(r,&t)){free(map);return fail(d,"VALUES nach INSERT-Spaltenliste fehlt.");}
    }
    if(!eq(t.text,"VALUES")&&!eq(t.text,"VALUE")){free(map);return fail(d,"Nur INSERT/REPLACE ... VALUES wird gelesen. INSERT SELECT, INSERT SET und PARTITION-Header werden nicht ausgewertet.");}
    if(replace)note(d,"REPLACE wird als Folge von Dump-Datensätzen angezeigt. Ersetzungs-/Eindeutigkeitsregeln werden nicht ausgeführt.");
    int layout=-1;for(;;){BLRef ref={0};uint32_t n=0;int noesc=r->noesc;if(!tuple(r,&ref,&n)){free(map);return 0;}
        if(layout<0){
            if(!explicit_cols){nmap=n;if(!tb->ncolumns&&n){note(d,"Tabelle ohne CREATE TABLE/Spaltendefinition: generische Spaltennamen wurden vergeben.");for(uint32_t j=0;j<n;j++){char s[48];bl_format(s,sizeof(s),"Spalte_%u",j+1);if(column_add(d,tb,s,"(Definition nicht im Dump)")<0){free(map);return 0;}}}
                if(n!=tb->ncolumns){free(map);return fail(d,"INSERT ohne Spaltenliste passt nicht zum CREATE TABLE. Der Import wurde gestoppt, damit keine Werte falschen Spalten zugeordnet werden.");}
                for(uint32_t j=0;j<n;j++)map[j]=j;
            }
            if(n!=nmap){free(map);return fail(d,"Anzahl der INSERT-Werte stimmt nicht mit der Spaltenliste überein.");}
            layout=layout_get(d,tb,map,nmap);if(layout<0){free(map);return 0;}
        }else if(n!=nmap){free(map);return fail(d,"Unterschiedliche Anzahl von Werten innerhalb eines INSERT-Statements.");}
        ref.layout=(uint32_t)layout|(noesc?BL_NOESC:0);
        if(!sink_put(d,sink,(uint32_t)id,ref)){free(map);return 0;}tb->rows++;tb->bytes+=ref.length;d->total_rows++;
        rskip(r);if(rp(r,0)==','){rg(r);continue;}break;
    }free(map);
    if(rmatch(r,r->delimiter)){r->pos+=strlen(r->delimiter);return 1;}
    if(rp(r,0)<0)return !r->failed;
    if(!rt(r,&t))return !r->failed;
    if(eq(t.text,"ON")){note(d,"ON DUPLICATE KEY UPDATE wurde nicht ausgeführt. Angezeigt werden die ursprünglichen VALUES-Tupel.");return rend(r);}
    return fail(d,"Unerwartete Zeichen nach INSERT VALUES. Der Dump wurde nicht vollständig übernommen.");
}
/* Cache metadata is written only after every index file has closed successfully. */
static int write_u32(BLFile f,uint32_t n){return bl_write(f,&n,4);}
static int write_u64(BLFile f,uint64_t n){return bl_write(f,&n,8);}
static int write_string(BLFile f,const char *s){uint32_t n=(uint32_t)strlen(s?s:"");return write_u32(f,n)&&(!n||bl_write(f,s,n));}
static int save_cache(BLDatabase *d){
    char *tmp=cache_path(d,"manifest.part"),*dest=cache_path(d,"manifest.bld");if(!tmp||!dest){free(tmp);free(dest);return fail(d,"Nicht genügend Arbeitsspeicher.");}
    BLFile f=bl_open(tmp,1);int ok=f!=BL_BAD_FILE;
    if(ok)ok=bl_write(f,CACHE_MAGIC,12)&&write_u64(f,d->source_size)&&write_u64(f,d->source_mtime)&&write_u64(f,d->source_hash)&&write_u64(f,d->total_rows)&&write_u32(f,d->ntables)&&write_u32(f,d->warnings)&&write_string(f,d->notes);
    for(uint32_t i=0;ok&&i<d->ntables;i++){BLTable *t=&d->tables[i];ok=write_string(f,t->database)&&write_string(f,t->name)&&write_string(f,t->ddl)&&write_u32(f,(uint32_t)t->kind)&&write_u64(f,t->rows)&&write_u64(f,t->bytes)&&write_u32(f,t->ncolumns);
        for(uint32_t j=0;ok&&j<t->ncolumns;j++)ok=write_string(f,t->columns[j].name)&&write_string(f,t->columns[j].definition);
        ok=ok&&write_u32(f,t->nlayouts);for(uint32_t j=0;ok&&j<t->nlayouts;j++)ok=write_u32(f,t->layouts[j].n)&&bl_write(f,t->layouts[j].columns,t->layouts[j].n*4);
    }
    if(ok)ok=bl_flush(f);bl_close(f);if(ok)ok=bl_move(tmp,dest);if(!ok)bl_remove(tmp);free(tmp);free(dest);return ok?1:fail(d,"Cache-Metadaten konnten nicht sicher gespeichert werden.");
}
int bl_db_index(BLDatabase *d,const BLOptions *opt){
    tables_free(d);free(d->notes);d->notes=NULL;d->warnings=0;d->error[0]=0;d->reused=0;d->read_length=0;
    char *manifest=cache_path(d,"manifest.bld");if(manifest){bl_remove(manifest);free(manifest);}
    Reader r={0};r.db=d;r.opt=opt;r.base=(uint64_t)-1;r.noesc=opt?opt->no_backslash:0;r.saved_noesc=r.noesc;r.delimiter[0]=';';r.buf=(unsigned char*)malloc(BUFFER_SIZE);
    Sink sink={0};sink.file=BL_BAD_FILE;sink.buf=(BLRef*)malloc(4096*sizeof(BLRef));
    if(!r.buf||!sink.buf){free(r.buf);free(sink.buf);return fail(d,"Nicht genügend Arbeitsspeicher.");}
    char *current=bl_dup("dump");int ok=current!=NULL;Token t;
    if(rp(&r,0)==0xef&&rp(&r,1)==0xbb&&rp(&r,2)==0xbf)r.pos=3;
    while(ok&&rt(&r,&t)){
        if(!strcmp(t.text,";"))continue;uint64_t start=t.start;
        if(eq(t.text,"DELIMITER")){size_t n=0;while(rp(&r,0)==' '||rp(&r,0)=='\t')rg(&r);while(rp(&r,0)>=0&&rp(&r,0)!='\r'&&rp(&r,0)!='\n'){int c=rg(&r);if(n+1<sizeof(r.delimiter))r.delimiter[n++]=(char)c;else{ok=fail(d,"DELIMITER ist zu lang (maximal 19 Zeichen).");break;}}
            while(n&&whitespace((unsigned char)r.delimiter[n-1]))n--;r.delimiter[n]=0;if(!n)ok=fail(d,"Leerer SQL-DELIMITER.");continue;}
        if(eq(t.text,"INSERT")||eq(t.text,"REPLACE")){ok=insert(&r,current,&sink,eq(t.text,"REPLACE"));continue;}
        if(eq(t.text,"USE")){if(!rt(&r,&t)){ok=fail(d,"Datenbankname nach USE fehlt.");break;}free(current);current=bl_dup(t.text);if(!current){ok=fail(d,"Nicht genügend Arbeitsspeicher.");break;}ok=rend(&r);continue;}
        int create=eq(t.text,"CREATE"),set=eq(t.text,"SET"),mutates=eq(t.text,"UPDATE")||eq(t.text,"DELETE")||eq(t.text,"TRUNCATE");
        if(!rend(&r)){ok=0;break;}
        if(create||set){char *sql=slice(d,start,r.pos-start);if(!sql){ok=fail(d,"SQL-Schema konnte nicht gelesen werden.");break;}
            if(create)ok=parse_create(d,sql,current,r.noesc);else apply_mode(&r,sql);free(sql);
        }else if(mutates)note(d,"SQL-Änderungsstatement wurde nicht ausgeführt. Der Browser zeigt VALUES-Datensätze, keinen berechneten MySQL-Endzustand.");
    }
    if(r.failed)ok=0;
    if(ok)ok=sink_flush(d,&sink);
    if(ok&&sink.file!=BL_BAD_FILE)ok=bl_flush(sink.file);
    bl_close(sink.file);free(sink.buf);free(r.buf);free(current);
    if(ok&&(!d->ntables||!d->total_rows))note(d,"Keine VALUES-Datensätze gefunden. Schemaobjekte können trotzdem angezeigt werden. Ausführbarer Code in Versionskommentaren wird außer SQL_MODE nicht interpretiert.");
    if(ok)ok=save_cache(d);
    if(ok&&opt&&opt->progress)opt->progress(opt->user,d->source_size,d->source_size,d->total_rows,"Bereit");
    if(!ok&&d->error[0]){size_t n=strlen(d->error);if(n+60<sizeof(d->error))bl_format(d->error+n,sizeof(d->error)-n," (Byte %llu)",(unsigned long long)r.pos);}
    return ok;
}
/* Reject corrupt/oversized cache records rather than trusting file-supplied allocation sizes. */
typedef struct {BLFile f;uint64_t left;int ok;} Meta;
static int mr(Meta *m,void *p,size_t n){if(!m->ok||n>m->left||bl_read(m->f,p,n)!=n){m->ok=0;return 0;}m->left-=n;return 1;}
static uint32_t mu32(Meta *m){uint32_t x=0;mr(m,&x,4);return x;}
static uint64_t mu64(Meta *m){uint64_t x=0;mr(m,&x,8);return x;}
static char *mstr(Meta *m,uint32_t max){uint32_t n=mu32(m);if(n>max||n>m->left){m->ok=0;return NULL;}char *s=(char*)malloc((size_t)n+1);if(!s){m->ok=0;return NULL;}if(!mr(m,s,n)){free(s);return NULL;}s[n]=0;if(strlen(s)!=n){free(s);m->ok=0;return NULL;}return s;}
int bl_db_load_cache(BLDatabase *d){
    char *path=cache_path(d,"manifest.bld");if(!path)return 0;BLFile f=bl_open(path,0);free(path);if(f==BL_BAD_FILE)return 0;
    Meta m={f,bl_size(f),1};if(m.left>256u*1024u*1024u||m.left<52){bl_close(f);return 0;}
    char magic[12];mr(&m,magic,12);uint64_t size=mu64(&m),mtime=mu64(&m),hash=mu64(&m),total=mu64(&m);uint32_t nt=mu32(&m),nw=mu32(&m);
    if(memcmp(magic,CACHE_MAGIC,12)||size!=d->source_size||mtime!=d->source_mtime||hash!=d->source_hash||nt>65536){bl_close(f);return 0;}
    tables_free(d);free(d->notes);d->notes=mstr(&m,NOTE_LIMIT);d->tables=nt?(BLTable*)calloc(nt,sizeof(BLTable)):NULL;if(nt&&!d->tables)m.ok=0;d->ntables=d->tables?nt:0;d->warnings=nw;uint64_t rows=0;
    for(uint32_t i=0;m.ok&&i<nt;i++){BLTable *t=&d->tables[i];t->database=mstr(&m,8192);t->name=mstr(&m,8192);t->ddl=mstr(&m,DDL_LIMIT);t->kind=(int)mu32(&m);t->rows=mu64(&m);t->bytes=mu64(&m);uint32_t nc=mu32(&m);
        if(t->kind<0||t->kind>3||nc>BL_MAX_COLUMNS||t->rows>d->source_size||t->bytes>d->source_size||(!t->database||!t->name||!t->ddl)){m.ok=0;break;}
        t->columns=nc?(BLColumn*)calloc(nc,sizeof(BLColumn)):NULL;if(nc&&!t->columns){m.ok=0;break;}t->ncolumns=nc;
        for(uint32_t j=0;m.ok&&j<nc;j++){t->columns[j].name=mstr(&m,8192);t->columns[j].definition=mstr(&m,DDL_LIMIT);}
        uint32_t nl=mu32(&m);if(nl>65536){m.ok=0;break;}t->layouts=nl?(BLLayout*)calloc(nl,sizeof(BLLayout)):NULL;if(nl&&!t->layouts){m.ok=0;break;}t->nlayouts=nl;
        for(uint32_t j=0;m.ok&&j<nl;j++){uint32_t n=mu32(&m);if(n>nc){m.ok=0;break;}t->layouts[j].columns=n?(uint32_t*)malloc(n*4):NULL;if(n&&!t->layouts[j].columns){m.ok=0;break;}t->layouts[j].n=n;mr(&m,t->layouts[j].columns,n*4);
            unsigned char seen[BL_MAX_COLUMNS/8]={0};for(uint32_t k=0;m.ok&&k<n;k++){uint32_t col=t->layouts[j].columns[k];if(col>=nc||(seen[col/8]&(1u<<(col%8))))m.ok=0;else seen[col/8]|=(unsigned char)(1u<<(col%8));}
        }
        if(t->rows){if(!nl||t->kind!=0)m.ok=0;char *ix=bl_index_path(d,i);BLFile fi=ix?bl_open(ix,0):BL_BAD_FILE;free(ix);if(fi==BL_BAD_FILE||t->rows>0x0fffffffffffffffull||bl_size(fi)!=t->rows*sizeof(BLRef))m.ok=0;bl_close(fi);}rows+=t->rows;
    }
    bl_close(f);if(!m.ok||m.left||rows!=total){tables_free(d);free(d->notes);d->notes=NULL;d->warnings=0;return 0;}d->total_rows=total;d->reused=1;return 1;
}
int bl_read_ref(BLFile f,uint64_t row,BLRef *ref){if(row>0x0fffffffffffffffull)return 0;return bl_seek(f,row*sizeof(BLRef))&&bl_read(f,ref,sizeof(*ref))==sizeof(*ref);}
void bl_row_free(BLRow *r){for(uint32_t i=0;i<r->count;i++)free(r->values[i].data);free(r->values);free(r->raw);memset(r,0,sizeof(*r));}
static int numeric(const char *s,size_t n){size_t p=0,digits=0;if(p<n&&(s[p]=='+'||s[p]=='-'))p++;while(p<n&&s[p]>='0'&&s[p]<='9'){p++;digits++;}if(p<n&&s[p]=='.'){p++;while(p<n&&s[p]>='0'&&s[p]<='9'){p++;digits++;}}if(!digits)return 0;if(p<n&&(s[p]=='e'||s[p]=='E')){p++;if(p<n&&(s[p]=='+'||s[p]=='-'))p++;size_t e=p;while(p<n&&s[p]>='0'&&s[p]<='9')p++;if(p==e)return 0;}return p==n;}
static int hexdigit(int c){return (c>='0'&&c<='9')||(lower(c)>='a'&&lower(c)<='f');}
static char *as_hex(const char *s,size_t n,size_t *out){if(n>((size_t)-1-3)/2)return NULL;char *p=(char*)malloc(n*2+3);if(!p)return NULL;const char *h="0123456789ABCDEF";p[0]='0';p[1]='x';for(size_t i=0;i<n;i++){p[2+i*2]=h[(unsigned char)s[i]>>4];p[3+i*2]=h[(unsigned char)s[i]&15];}p[2+n*2]=0;*out=2+n*2;return p;}
static int decode_clean(const char *s,size_t n,int noesc,BLValue *v){
    MemLex m={s,n,0,noesc};mskip(&m);s+=m.p;n-=m.p;while(n&&whitespace((unsigned char)s[n-1]))n--;
    v->kind=BL_EXPR;v->data=NULL;v->len=0;
    if(n==4&&lower(s[0])=='n'&&lower(s[1])=='u'&&lower(s[2])=='l'&&lower(s[3])=='l'){v->kind=BL_NULL;v->data=bl_dup("");return v->data!=NULL;}
    if(numeric(s,n)){v->kind=BL_NUMBER;v->len=n;v->data=bl_ndup(s,n);return v->data!=NULL;}
    if(n>=2&&s[0]=='0'&&(s[1]=='x'||s[1]=='X')){int ok=1;for(size_t i=2;i<n;i++)if(!hexdigit((unsigned char)s[i]))ok=0;if(ok){v->kind=BL_BINARY;v->data=bl_ndup(s,n);v->len=n;return v->data!=NULL;}}
    if(n>=3&&(s[0]=='x'||s[0]=='X')&&s[1]=='\''&&s[n-1]=='\''){int ok=(n-3)%2==0;for(size_t i=2;i+1<n;i++)if(!hexdigit((unsigned char)s[i]))ok=0;if(ok){v->kind=BL_BINARY;v->len=n-1;v->data=(char*)malloc(n);if(!v->data)return 0;v->data[0]='0';v->data[1]='x';memcpy(v->data+2,s+2,n-3);v->data[n-1]=0;return 1;}}
    if(n>=3&&(s[0]=='b'||s[0]=='B')&&s[1]=='\''&&s[n-1]=='\''){int ok=1;for(size_t i=2;i+1<n;i++)if(s[i]!='0'&&s[i]!='1')ok=0;if(ok){v->kind=BL_BINARY;v->data=bl_ndup(s,n);v->len=n;return v->data!=NULL;}}
    size_t p=0;int binary=0;
    if(n>1&&(s[0]=='n'||s[0]=='N')&&(s[1]=='\''||s[1]=='"'))p=1;
    else if(n&&s[0]=='_'){while(p<n&&word((unsigned char)s[p]))p++;char *prefix=bl_ndup(s,p);if(!prefix)return 0;binary=eq(prefix,"_binary");free(prefix);while(p<n&&whitespace((unsigned char)s[p]))p++;}
    if(p<n&&(s[p]=='\''||s[p]=='"')){
        char *out=(char*)malloc(n+1);if(!out)return 0;size_t k=0;int valid=1;
        while(p<n){if(s[p]!='\''&&s[p]!='"'){valid=0;break;}int q=s[p++],closed=0;
            while(p<n){int c=(unsigned char)s[p++];if(c==q){if(p<n&&s[p]==q){p++;out[k++]=(char)c;continue;}closed=1;break;}
                if(c=='\\'&&!noesc&&p<n){c=(unsigned char)s[p++];switch(c){case '0':c=0;break;case 'b':c=8;break;case 'n':c=10;break;case 'r':c=13;break;case 't':c=9;break;case 'Z':c=26;break;case '%':case '_':out[k++]='\\';break;default:break;}}out[k++]=(char)c;
            }if(!closed){valid=0;break;}MemLex rest={s,n,p,noesc};mskip(&rest);p=rest.p;
        }
        if(valid){out[k]=0;v->kind=binary?BL_BINARY:BL_TEXT;if(binary){v->data=as_hex(out,k,&v->len);free(out);}else{v->data=out;v->len=k;}return v->data!=NULL;}free(out);
    }
    v->data=bl_ndup(s,n);v->len=n;return v->data!=NULL;
}
static int decode(const char *s,size_t n,int noesc,BLValue *v){
    int possible=0;for(size_t i=0;i<n;i++)if(s[i]=='#'||(i+1<n&&((s[i]=='/'&&s[i+1]=='*')||(s[i]=='-'&&s[i+1]=='-')))){possible=1;break;}
    if(!possible)return decode_clean(s,n,noesc,v);
    char *clean=(char*)malloc(n+1);if(!clean)return 0;size_t k=0;int q=0;
    for(size_t i=0;i<n;i++){int c=(unsigned char)s[i];
        if(q){clean[k++]=(char)c;if(c=='\\'&&q!='`'&&!noesc&&i+1<n)clean[k++]=s[++i];else if(c==q){if(i+1<n&&s[i+1]==q)clean[k++]=s[++i];else q=0;}continue;}
        if(c=='\''||c=='"'||c=='`'){q=c;clean[k++]=(char)c;continue;}
        if(c=='/'&&i+1<n&&s[i+1]=='*'){i+=2;while(i+1<n&&(s[i]!='*'||s[i+1]!='/'))i++;if(i+1<n)i++;clean[k++]=' ';continue;}
        if(c=='#'||(c=='-'&&i+2<n&&s[i+1]=='-'&&whitespace(s[i+2]))){while(i<n&&s[i]!='\n')i++;clean[k++]=' ';continue;}
        clean[k++]=(char)c;
    }clean[k]=0;int ok=decode_clean(clean,k,noesc,v);free(clean);return ok;
}
static int binary_column(const char *def){char b[32];size_t n=0;while(def[n]&&!whitespace((unsigned char)def[n])&&def[n]!='('&&n+1<sizeof(b)){b[n]=(char)lower((unsigned char)def[n]);n++;}b[n]=0;return eq(b,"binary")||eq(b,"varbinary")||eq(b,"blob")||eq(b,"tinyblob")||eq(b,"mediumblob")||eq(b,"longblob");}
int bl_get_row(BLDatabase *d,uint32_t table,const BLRef *ref,BLRow *row){
    memset(row,0,sizeof(*row));if(table>=d->ntables)return fail(d,"Ungültige Tabelle.");BLTable *t=&d->tables[table];uint32_t li=ref->layout&~BL_NOESC;
    if(li>=t->nlayouts||ref->offset>d->source_size||ref->length>d->source_size-ref->offset||ref->length<2)return fail(d,"Ungültiger oder veralteter Positionsindex. Cache neu aufbauen.");
    if(ref->length>BL_ROW_LIMIT)return fail(d,"Dieser einzelne Datensatz ist größer als 64 MiB. Er ist indexiert, kann aber nur als SQL-Rohtupel gespeichert werden; Vorschau, Filter und CSV stoppen hier.");
    row->raw=(char*)malloc((size_t)ref->length+1);if(!row->raw)return fail(d,"Nicht genügend Arbeitsspeicher für den Datensatz.");row->raw_len=ref->length;
    int got=0;
    if(ref->length<=BUFFER_SIZE){
        if(!d->read_buffer)d->read_buffer=(unsigned char*)malloc(BUFFER_SIZE);
        if(d->read_buffer){
            if(!d->read_length||ref->offset<d->read_base||ref->offset+ref->length>d->read_base+d->read_length){d->read_base=ref->offset;d->read_length=bl_seek(d->source_file,ref->offset)?bl_read(d->source_file,d->read_buffer,BUFFER_SIZE):0;}
            if(ref->offset>=d->read_base&&ref->offset+ref->length<=d->read_base+d->read_length){memcpy(row->raw,d->read_buffer+(size_t)(ref->offset-d->read_base),ref->length);got=1;}
        }
    }else got=bl_seek(d->source_file,ref->offset)&&bl_read(d->source_file,row->raw,ref->length)==ref->length;
    if(!got){bl_row_free(row);return fail(d,"Datensatz konnte nicht aus der Originaldatei gelesen werden.");}row->raw[ref->length]=0;
    if(row->raw[0]!='('||row->raw[ref->length-1]!=')'){bl_row_free(row);return fail(d,"Der Index passt nicht mehr zum Dump. Cache neu aufbauen.");}
    row->values=t->ncolumns?(BLValue*)calloc(t->ncolumns,sizeof(BLValue)):NULL;if(t->ncolumns&&!row->values){bl_row_free(row);return fail(d,"Nicht genügend Arbeitsspeicher.");}row->count=t->ncolumns;
    for(uint32_t i=0;i<row->count;i++)row->values[i].kind=BL_MISSING;
    BLLayout *layout=&t->layouts[li];size_t p=1;int noesc=(ref->layout&BL_NOESC)!=0;
    for(uint32_t i=0;i<layout->n;i++){size_t e=field_end(row->raw,row->raw_len,p,noesc,')');uint32_t col=layout->columns[i];
        if(e>=row->raw_len||col>=row->count||!decode(row->raw+p,e-p,noesc,&row->values[col])){bl_row_free(row);return fail(d,"Ein Feld konnte nicht sicher dekodiert werden.");}
        BLValue *v=&row->values[col];if(v->kind==BL_TEXT&&binary_column(t->columns[col].definition)){size_t n=0;char *hex=as_hex(v->data,v->len,&n);if(!hex){bl_row_free(row);return fail(d,"Nicht genügend Arbeitsspeicher.");}free(v->data);v->data=hex;v->len=n;v->kind=BL_BINARY;}
        if(i+1<layout->n&&row->raw[e]!=','){bl_row_free(row);return fail(d,"Cache-Spaltenlayout passt nicht zum Datensatz.");}
        if(i+1==layout->n&&row->raw[e]!=')'){bl_row_free(row);return fail(d,"Cache-Spaltenlayout passt nicht zum Datensatz.");}p=e+1;
    }return 1;
}
/* UTF-8 conversion is explicit. No lossy numeric conversion is performed. */
char *bl_to_utf8(const char *s,size_t n,int encoding,size_t *out_n){
    static const uint16_t cp1252[32]={0x20ac,0x81,0x201a,0x192,0x201e,0x2026,0x2020,0x2021,0x2c6,0x2030,0x160,0x2039,0x152,0x8d,0x17d,0x8f,0x90,0x2018,0x2019,0x201c,0x201d,0x2022,0x2013,0x2014,0x2dc,0x2122,0x161,0x203a,0x153,0x9d,0x17e,0x178};
    if(n>((size_t)-1-1)/3)return NULL;char *o=(char*)malloc(n*3+1);if(!o)return NULL;size_t k=0;
    for(size_t i=0;i<n;i++){uint32_t c=(unsigned char)s[i];
        if(encoding==0&&c>=128){unsigned need=c>=0xc2&&c<=0xdf?1:(c>=0xe0&&c<=0xef?2:(c>=0xf0&&c<=0xf4?3:0));uint32_t cp=need?(c&((1u<<(6-need))-1)):0;int valid=need&&i+need<n;
            for(unsigned j=1;valid&&j<=need;j++){unsigned b=(unsigned char)s[i+j];if((b&0xc0)!=0x80)valid=0;else cp=(cp<<6)|(b&63);}
            if(valid&&(cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)||(need==1&&cp<0x80)||(need==2&&cp<0x800)||(need==3&&cp<0x10000)))valid=0;
            if(valid){for(unsigned j=0;j<=need;j++)o[k++]=s[i+j];i+=need;continue;}c=0xfffd;
        }else if(encoding==1&&c>=0x80&&c<=0x9f)c=cp1252[c-0x80];
        if(c<0x80)o[k++]=(char)c;else if(c<0x800){o[k++]=(char)(0xc0|(c>>6));o[k++]=(char)(0x80|(c&63));}else{o[k++]=(char)(0xe0|(c>>12));o[k++]=(char)(0x80|((c>>6)&63));o[k++]=(char)(0x80|(c&63));}
    }o[k]=0;if(out_n)*out_n=k;return o;
}
static int contains(const char *s,size_t n,const char *q,size_t nq,const uint32_t *prefix){
    if(nq>n)return 0;if(!nq)return 1;size_t k=0;
    for(size_t i=0;i<n;i++){while(k&&s[i]!=q[k])k=prefix[k-1];if(s[i]==q[k])k++;if(k==nq)return 1;}return 0;
}
static int matches_row(const BLRow *row,const BLFilter *filter,const uint32_t *prefix,int *oom){
    uint32_t first=filter->column<0?0:(uint32_t)filter->column,last=filter->column<0?row->count:first+1;if(last>row->count)return 0;size_t nq=strlen(filter->text?filter->text:"");
    for(uint32_t i=first;i<last;i++){const BLValue *v=&row->values[i];if(filter->op==2){if(v->kind==BL_NULL)return 1;continue;}if(filter->op==3){if(v->kind!=BL_NULL&&v->kind!=BL_MISSING)return 1;continue;}if(v->kind==BL_NULL||v->kind==BL_MISSING)continue;
        size_t n=0;char *s=bl_to_utf8(v->data,v->len,filter->encoding,&n);if(!s){*oom=1;return 0;}int hit=filter->op==1?(n==nq&&!memcmp(s,filter->text,n)):contains(s,n,filter->text,nq,prefix);free(s);if(hit)return 1;
    }return 0;
}
static int operation_progress(BLDatabase *d,const BLOptions *o,uint64_t done,uint64_t count,uint64_t rows,const char *phase,uint64_t *last){
    uint64_t now=bl_millis();if(now-*last<100&&done<count)return 1;*last=now;
    if(o&&o->progress&&!o->progress(o->user,done,count,rows,phase))return fail(d,"Vorgang abgebrochen.");return 1;
}
int bl_filter(BLDatabase *d,uint32_t table,const BLFilter *filter,const char *target,uint64_t *matches,const BLOptions *options){
    *matches=0;d->error[0]=0;if(table>=d->ntables)return fail(d,"Ungültige Tabelle.");BLTable *t=&d->tables[table];
    size_t qlen=strlen(filter->text?filter->text:"");if(qlen>1024u*1024u)return fail(d,"Der Filtertext ist zu lang.");
    uint32_t *prefix=qlen?(uint32_t*)calloc(qlen,4):NULL;if(qlen&&!prefix)return fail(d,"Nicht genügend Arbeitsspeicher.");
    for(size_t i=1,k=0;i<qlen;i++){while(k&&filter->text[i]!=filter->text[k])k=prefix[k-1];if(filter->text[i]==filter->text[k])k++;prefix[i]=(uint32_t)k;}
    char *path=bl_index_path(d,table);if(!path){free(prefix);return fail(d,"Nicht genügend Arbeitsspeicher.");}BLFile in=bl_open(path,0);free(path);BLFile out=bl_open(target,1);
    BLRef *ib=(BLRef*)malloc(4096*sizeof(BLRef)),*ob=(BLRef*)malloc(4096*sizeof(BLRef));size_t used=0;int ok=(in!=BL_BAD_FILE||t->rows==0)&&out!=BL_BAD_FILE&&ib&&ob;uint64_t done=0,last=0;
    if(!ok)fail(d,"Filterdatei kann nicht angelegt oder gelesen werden.");
    while(ok&&done<t->rows){size_t n=t->rows-done>4096?4096:(size_t)(t->rows-done);if(bl_read(in,ib,n*sizeof(BLRef))!=n*sizeof(BLRef)){ok=fail(d,"Positionsindex ist unvollständig.");break;}
        for(size_t j=0;ok&&j<n;j++,done++){BLRow row;if(!bl_get_row(d,table,&ib[j],&row)){ok=0;break;}int oom=0,hit=matches_row(&row,filter,prefix,&oom);bl_row_free(&row);if(oom){ok=fail(d,"Nicht genügend Arbeitsspeicher beim Filtern.");break;}
            if(hit){ob[used++]=ib[j];(*matches)++;if(used==4096){if(!bl_write(out,ob,used*sizeof(BLRef)))ok=fail(d,"Filterindex kann nicht geschrieben werden. Freien Speicherplatz prüfen.");used=0;}}
            ok=ok&&operation_progress(d,options,done,t->rows,*matches,"Tabelle filtern",&last);
        }
    }
    if(ok&&used)ok=bl_write(out,ob,used*sizeof(BLRef));if(ok)ok=bl_flush(out);bl_close(in);bl_close(out);free(ib);free(ob);free(prefix);
    if(!ok){bl_remove(target);if(!d->error[0])fail(d,"Filterdatei konnte nicht gespeichert werden.");}return ok;
}
typedef struct {BLFile f;char *buf;size_t used;int ok;} Writer;
static void wflush(Writer *w){if(w->ok&&w->used)w->ok=bl_write(w->f,w->buf,w->used);w->used=0;}
static void wbytes(Writer *w,const char *s,size_t n){while(w->ok&&n){size_t take=65536-w->used;if(take>n)take=n;memcpy(w->buf+w->used,s,take);w->used+=take;s+=take;n-=take;if(w->used==65536)wflush(w);}}
static void csv_cell(Writer *w,const char *s,size_t n,int safe){
    wbytes(w,"\"",1);size_t p=0;while(p<n&&(s[p]==' '||s[p]=='\t'||s[p]=='\r'||s[p]=='\n'))p++;
    if(safe&&n&&((p<n&&(s[p]=='='||s[p]=='+'||s[p]=='-'||s[p]=='@'))||s[0]=='\t'||s[0]=='\r'||s[0]=='\n'))wbytes(w,"'",1);
    size_t start=0;for(size_t i=0;i<n;i++){if(s[i]=='"'||s[i]==0){wbytes(w,s+start,i-start);wbytes(w,s[i]=='"'?"\"\"":"\\0",2);start=i+1;}}wbytes(w,s+start,n-start);wbytes(w,"\"",1);
}
int bl_export_csv(BLDatabase *d,uint32_t table,const char *index_path,uint64_t count,const char *target,int encoding,int safe,const BLOptions *options){
    d->error[0]=0;if(table>=d->ntables)return fail(d,"Ungültige Tabelle.");if(!strcmp(target,d->source)||!strcmp(target,index_path))return fail(d,"Exportziel darf keine geöffnete Quelldatei sein.");
    size_t cap=strlen(target)+64;char *tmp=(char*)malloc(cap);if(!tmp)return fail(d,"Nicht genügend Arbeitsspeicher.");bl_format(tmp,cap,"%s.bitline-part-%u",target,bl_pid());
    BLFile in=bl_open(index_path,0);Writer w={0};w.f=bl_open(tmp,1);w.buf=(char*)malloc(65536);w.ok=w.f!=BL_BAD_FILE&&w.buf&&(in!=BL_BAD_FILE||count==0);
    BLRef *refs=(BLRef*)malloc(4096*sizeof(BLRef));if(!refs)w.ok=0;BLTable *t=&d->tables[table];uint64_t done=0,last=0;int ok=w.ok;
    if(!ok)fail(d,"Exportdatei kann nicht angelegt oder Index nicht gelesen werden.");
    if(ok){wbytes(&w,"\xef\xbb\xbf",3);for(uint32_t c=0;c<t->ncolumns;c++){if(c)wbytes(&w,";",1);size_t n=0;char *s=bl_to_utf8(t->columns[c].name,strlen(t->columns[c].name),encoding,&n);if(!s){ok=0;break;}csv_cell(&w,s,n,safe);free(s);}wbytes(&w,"\r\n",2);}
    while(ok&&w.ok&&done<count){size_t n=count-done>4096?4096:(size_t)(count-done);if(bl_read(in,refs,n*sizeof(BLRef))!=n*sizeof(BLRef)){ok=fail(d,"Positionsindex ist unvollständig.");break;}
        for(size_t j=0;ok&&w.ok&&j<n;j++,done++){BLRow row;if(!bl_get_row(d,table,&refs[j],&row)){ok=0;break;}
            for(uint32_t c=0;c<row.count;c++){if(c)wbytes(&w,";",1);BLValue *v=&row.values[c];if(v->kind==BL_NULL)csv_cell(&w,"\\N",2,0);else if(v->kind==BL_MISSING)csv_cell(&w,"[nicht im INSERT]",17,0);else{
                    size_t len=0;char *s=bl_to_utf8(v->data,v->len,encoding,&len);if(!s){ok=fail(d,"Nicht genügend Arbeitsspeicher beim Export.");break;}
                    csv_cell(&w,s,len,safe&&v->kind!=BL_NUMBER&&v->kind!=BL_BINARY);free(s);
                }}bl_row_free(&row);wbytes(&w,"\r\n",2);
            ok=ok&&operation_progress(d,options,done,count,done,"CSV exportieren",&last);
        }
    }wflush(&w);ok=ok&&w.ok;if(ok)ok=bl_flush(w.f);bl_close(w.f);bl_close(in);free(w.buf);free(refs);
    if(ok)ok=bl_move(tmp,target);if(!ok){bl_remove(tmp);if(!d->error[0])fail(d,"CSV-Export konnte nicht abgeschlossen werden. Freien Speicherplatz und Schreibrechte prüfen.");}free(tmp);return ok;
}
int bl_export_raw(BLDatabase *d,const BLRef *ref,const char *target,const BLOptions *options){
    d->error[0]=0;
    if(!strcmp(target,d->source))return fail(d,"Die Originaldatei darf nicht überschrieben werden.");if(ref->offset>d->source_size||ref->length>d->source_size-ref->offset)return fail(d,"Ungültige Datensatzposition.");
    size_t cap=strlen(target)+64;char *tmp=(char*)malloc(cap);if(!tmp)return fail(d,"Nicht genügend Arbeitsspeicher.");bl_format(tmp,cap,"%s.bitline-part-%u",target,bl_pid());
    BLFile out=bl_open(tmp,1);if(out==BL_BAD_FILE){free(tmp);return fail(d,"Zieldatei kann nicht geöffnet werden.");}char *buf=(char*)malloc(BUFFER_SIZE);int ok=buf!=NULL&&bl_seek(d->source_file,ref->offset);uint64_t left=ref->length,last=0;
    while(ok&&left){if(!operation_progress(d,options,ref->length-left,ref->length,0,"SQL-Rohtupel speichern",&last)){ok=0;break;}size_t n=left>BUFFER_SIZE?BUFFER_SIZE:(size_t)left;ok=bl_read(d->source_file,buf,n)==n&&bl_write(out,buf,n);left-=n;}
    if(ok)ok=bl_flush(out);bl_close(out);free(buf);if(ok)ok=bl_move(tmp,target);if(!ok){bl_remove(tmp);if(!d->error[0])fail(d,"SQL-Rohtupel konnte nicht gespeichert werden.");}free(tmp);return ok;
}

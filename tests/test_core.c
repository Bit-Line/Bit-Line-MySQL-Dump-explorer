#include "../src/core.h"
#include <assert.h>
static int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void write_sql(const char *path,const char *s){BLFile f=bl_open(path,1);CHECK(f!=BL_BAD_FILE);CHECK(bl_write(f,s,strlen(s)));bl_close(f);}
static BLRow get(BLDatabase *d,int t,int n){char *p=bl_index_path(d,t);BLFile f=bl_open(p,0);free(p);BLRef ref;CHECK(bl_read_ref(f,(uint64_t)n,&ref));bl_close(f);BLRow r;int ok=bl_get_row(d,(uint32_t)t,&ref,&r);if(!ok)fprintf(stderr,"%s\n",d->error);CHECK(ok);return r;}
static int find_table(BLDatabase *d,const char *name){for(uint32_t i=0;i<d->ntables;i++)if(!strcmp(d->tables[i].name,name))return (int)i;return -1;}
static int cancel(void *u,uint64_t a,uint64_t b,uint64_t c,const char *s){(void)u;(void)a;(void)b;(void)c;(void)s;return 0;}
int main(void){
    bl_mkdir("build/test-data");bl_mkdir("build/test-data/cache");
    const char *path="build/test-data/test.sql";
    write_sql(path,"\xef\xbb\xbf-- demo\nCREATE DATABASE `one`; USE `one`;\n"
      "CREATE TABLE `items` (`id` BIGINT UNSIGNED, `text` TEXT, `other` TEXT, `blob` BLOB, PRIMARY KEY (`id`));\n"
      "INSERT INTO `items` VALUES (18446744073709551615,'a; b,(x) -- ok','it''s',0x00FF),(2,'line\\nnext',NULL,_binary'\\0a');\n"
      "INSERT INTO items (`text`,`id`,`blob`) VALUES (_utf8mb4'Grüße 🐺',3,X'ABCD');\n"
      "/*!40101 SET SQL_MODE='NO_BACKSLASH_ESCAPES' */;\n"
      "INSERT INTO items VALUES (4,'C:\\tmp\\','back\\slash',b'1010');\n"
      "/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;\n"
      "INSERT INTO items VALUES (5,'hello\\\'world','\\%\\_',NULL);\n"
      "CREATE TABLE `empty` (`x` int);\n"
      "DELIMITER $$\nCREATE PROCEDURE `p`() BEGIN INSERT INTO items VALUES (999,'bad','bad',NULL); END$$\nDELIMITER ;\n"
      "CREATE VIEW `v` AS SELECT * FROM items;\nUSE `two`;\n"
      "INSERT INTO `odd``name` (`a`,`b`) VALUES (-1.2300e+12,CONCAT('not', 'executed'));\n");
    BLDatabase d;bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(bl_db_index(&d,NULL));CHECK(d.total_rows==6);
    int it=find_table(&d,"items");CHECK(it>=0);CHECK(d.tables[it].rows==5);CHECK(d.tables[it].ncolumns==4);
    BLRow r=get(&d,it,0);CHECK(!strcmp(r.values[0].data,"18446744073709551615"));CHECK(r.values[0].kind==BL_NUMBER);CHECK(!strcmp(r.values[1].data,"a; b,(x) -- ok"));CHECK(!strcmp(r.values[2].data,"it's"));CHECK(r.values[3].kind==BL_BINARY);CHECK(!strcmp(r.values[3].data,"0x00FF"));bl_row_free(&r);
    r=get(&d,it,1);CHECK(!strcmp(r.values[1].data,"line\nnext"));CHECK(r.values[2].kind==BL_NULL);CHECK(!strcmp(r.values[3].data,"0x0061"));bl_row_free(&r);
    r=get(&d,it,2);CHECK(!strcmp(r.values[0].data,"3"));CHECK(!strcmp(r.values[1].data,"Grüße 🐺"));CHECK(r.values[2].kind==BL_MISSING);CHECK(!strcmp(r.values[3].data,"0xABCD"));bl_row_free(&r);
    r=get(&d,it,3);CHECK(!strcmp(r.values[1].data,"C:\\tmp\\"));CHECK(!strcmp(r.values[2].data,"back\\slash"));bl_row_free(&r);
    r=get(&d,it,4);CHECK(!strcmp(r.values[1].data,"hello'world"));CHECK(!strcmp(r.values[2].data,"\\%\\_"));bl_row_free(&r);
    CHECK(find_table(&d,"p")>=0);CHECK(d.tables[find_table(&d,"p")].kind==2);CHECK(d.tables[find_table(&d,"v")].kind==1);CHECK(d.tables[find_table(&d,"empty")].rows==0);
    r=get(&d,find_table(&d,"odd`name"),0);CHECK(!strcmp(r.values[0].data,"-1.2300e+12"));CHECK(r.values[1].kind==BL_EXPR);bl_row_free(&r);
    BLFilter filter={1,0,"Grüße",0};uint64_t count=0;CHECK(bl_filter(&d,it,&filter,"build/test-data/filter.bli",&count,NULL));CHECK(count==1);
    filter.column=2;filter.op=2;CHECK(bl_filter(&d,it,&filter,"build/test-data/filter.bli",&count,NULL));CHECK(count==1);
    char *ix=bl_index_path(&d,it);CHECK(bl_export_csv(&d,it,ix,5,"build/test-data/export.csv",0,1,NULL));free(ix);
    bl_db_free(&d);bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(bl_db_load_cache(&d));CHECK(d.reused);CHECK(d.total_rows==6);it=find_table(&d,"items");r=get(&d,it,2);CHECK(r.values[2].kind==BL_MISSING);bl_row_free(&r);
    char *cache=bl_dup(d.cache);bl_db_free(&d);
    char mp[4096];bl_format(mp,sizeof(mp),"%s/manifest.bld",cache);write_sql(mp,"corrupt");free(cache);
    bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(!bl_db_load_cache(&d));CHECK(bl_db_index(&d,NULL));bl_db_free(&d);
    const char *bad[]={"INSERT INTO x VALUES (1,'unterminated", "CREATE TABLE x(a int,b int); INSERT INTO x VALUES (1);", "INSERT INTO x VALUES (1),(2,3);", "INSERT INTO x VALUES (1),", "INSERT INTO x SELECT 1;", "/* comment", "SET SQL_MODE=concat(@@sql_mode,',x');"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++){write_sql(path,bad[i]);bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(!bl_db_index(&d,NULL));CHECK(d.error[0]);CHECK(!bl_db_load_cache(&d));bl_db_free(&d);}
    write_sql(path,"CREATE TABLE t (n text); INSERT INTO t VALUES ('=HYPERLINK(\"evil\")'),('a\\0b'),(''),(NULL); ");
    bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(bl_db_index(&d,NULL));ix=bl_index_path(&d,0);CHECK(bl_export_csv(&d,0,ix,4,"build/test-data/safe.csv",0,1,NULL));free(ix);r=get(&d,0,1);CHECK(r.values[0].len==3&&r.values[0].data[1]==0);bl_row_free(&r);
    BLOptions o={cancel,NULL,0};CHECK(!bl_filter(&d,0,&filter,"build/test-data/cancel.bli",&count,&o));bl_db_free(&d);
    bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(!bl_db_index(&d,&o));bl_db_free(&d);
    size_t len;char *u=bl_to_utf8("\x80\xe4",2,1,&len);CHECK(!strcmp(u,"€ä"));free(u);u=bl_to_utf8("\xf0\x9f\x90\xba",4,0,&len);CHECK(len==4);free(u);u=bl_to_utf8("\xc0\xaf",2,0,&len);CHECK(len==6);free(u);

    write_sql(path,"CREATE TABLE t (a int,b text,c text); INSERT INTO t VALUES (7 /* number */, NULL /* null */, 'literal /* text */ # -- remains');");
    bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(bl_db_index(&d,NULL));
    r=get(&d,0,0);CHECK(r.values[0].kind==BL_NUMBER);CHECK(!strcmp(r.values[0].data,"7"));CHECK(r.values[1].kind==BL_NULL);CHECK(!strcmp(r.values[2].data,"literal /* text */ # -- remains"));
    ix=bl_index_path(&d,0);BLFile f=bl_open(ix,0);BLRef ref;CHECK(bl_read_ref(f,0,&ref));bl_close(f);free(ix);
    CHECK(bl_export_raw(&d,&ref,"build/test-data/raw.txt",NULL));
    f=bl_open("build/test-data/raw.txt",0);char buf[512]={0};CHECK(bl_read(f,buf,sizeof(buf)-1)==r.raw_len);CHECK(!strcmp(buf,r.raw));bl_close(f);bl_row_free(&r);
    write_sql("build/test-data/raw.txt","preserved");CHECK(!bl_export_raw(&d,&ref,"build/test-data/raw.txt",&o));
    f=bl_open("build/test-data/raw.txt",0);memset(buf,0,sizeof(buf));CHECK(bl_read(f,buf,sizeof(buf)-1)==9);CHECK(!strcmp(buf,"preserved"));bl_close(f);
    CHECK(!bl_export_raw(&d,&ref,path,NULL));bl_db_free(&d);
    write_sql(path,"SET SQL_MODE='NO_BACKSLASH_ESCAPES'; /*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='' */; CREATE TABLE t(a text); INSERT INTO t VALUES ('a\\nb'); /*!40101 SET SQL_MODE=@OLD_SQL_MODE */; INSERT INTO t VALUES ('a\\nb');");
    bl_db_init(&d);CHECK(bl_db_prepare(&d,path,"build/test-data/cache"));CHECK(bl_db_index(&d,NULL));
    r=get(&d,0,0);CHECK(!strcmp(r.values[0].data,"a\nb"));bl_row_free(&r);r=get(&d,0,1);CHECK(!strcmp(r.values[0].data,"a\\nb"));bl_row_free(&r);bl_db_free(&d);
    printf("PASS: %d checks\n",checks);return 0;
}

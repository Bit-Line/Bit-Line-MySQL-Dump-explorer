/* Regression for 64-bit file/index addressing. Sparse files, not a full >4 GiB import benchmark. */
#include "../src/core.h"
static int checks=0;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
    bl_mkdir("build/test-data");
    const char *path="build/test-data/sparse.sql",*index="build/test-data/sparse.bli";
    const char *tuple="(18446744073709551615,'beyond 4 GiB')";
    const uint64_t offset=(1ull<<32)+1234,row_number=300000000ull;
    BLFile f=bl_open(path,1);CHECK(f!=BL_BAD_FILE);CHECK(bl_seek(f,offset));CHECK(bl_write(f,tuple,strlen(tuple)));bl_close(f);
    BLRef ref={offset,(uint32_t)strlen(tuple),0},back={0};f=bl_open(index,1);CHECK(f!=BL_BAD_FILE);CHECK(bl_seek(f,row_number*16));CHECK(bl_write(f,&ref,sizeof(ref)));bl_close(f);
    f=bl_open(index,0);CHECK(bl_read_ref(f,row_number,&back));CHECK(back.offset==offset);CHECK(back.length==strlen(tuple));CHECK(!bl_read_ref(f,UINT64_MAX,&back));bl_close(f);
    BLDatabase d;bl_db_init(&d);d.source=bl_dup(path);d.source_file=bl_open(path,0);d.source_size=bl_size(d.source_file);d.ntables=1;d.tables=(BLTable*)calloc(1,sizeof(BLTable));CHECK(d.tables!=NULL);
    BLTable *t=d.tables;t->ncolumns=2;t->columns=(BLColumn*)calloc(2,sizeof(BLColumn));t->nlayouts=1;t->layouts=(BLLayout*)calloc(1,sizeof(BLLayout));CHECK(t->columns&&t->layouts);
    t->columns[0].definition=bl_dup("BIGINT UNSIGNED");t->columns[1].definition=bl_dup("TEXT");t->layouts[0].n=2;t->layouts[0].columns=(uint32_t*)calloc(2,4);CHECK(t->layouts[0].columns!=NULL);t->layouts[0].columns[1]=1;
    BLRow row;CHECK(bl_get_row(&d,0,&ref,&row));CHECK(!strcmp(row.values[0].data,"18446744073709551615"));CHECK(!strcmp(row.values[1].data,"beyond 4 GiB"));bl_row_free(&row);
    CHECK(bl_export_raw(&d,&ref,"build/test-data/sparse-tuple.sql",NULL));f=bl_open("build/test-data/sparse-tuple.sql",0);char buf[128]={0};CHECK(bl_read(f,buf,sizeof(buf)-1)==strlen(tuple));CHECK(!strcmp(buf,tuple));bl_close(f);
    bl_db_free(&d);CHECK(bl_remove(path));CHECK(bl_remove(index));bl_remove("build/test-data/sparse-tuple.sql");
    printf("PASS: %d checks (source offset %llu; index byte offset %llu)\n",checks,(unsigned long long)offset,(unsigned long long)(row_number*16));return 0;
}

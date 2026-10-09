/* Headless sample validation; not shipped as a separate end-user executable. */
#include "../src/core.h"
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"Usage: inspect dump.sql cache-root\n");return 2;}
    BLDatabase d;bl_db_init(&d);if(!bl_db_prepare(&d,argv[1],argv[2])||!bl_db_index(&d,NULL)){fprintf(stderr,"%s\n",d.error);bl_db_free(&d);return 1;}
    uint64_t decoded=0;
    for(uint32_t t=0;t<d.ntables;t++){
        printf("%s / %s: %llu rows, %u columns, kind %d\n",d.tables[t].database,d.tables[t].name,(unsigned long long)d.tables[t].rows,d.tables[t].ncolumns,d.tables[t].kind);
        if(!d.tables[t].rows)continue;char *p=bl_index_path(&d,t);BLFile f=bl_open(p,0);free(p);
        for(uint64_t i=0;i<d.tables[t].rows;i++){BLRef ref;BLRow row;if(!bl_read_ref(f,i,&ref)||!bl_get_row(&d,t,&ref,&row)){fprintf(stderr,"%s\n",d.error);bl_close(f);bl_db_free(&d);return 1;}bl_row_free(&row);decoded++;}bl_close(f);
    }
    printf("PASS: %llu rows decoded; %u schema objects; %u notes\n",(unsigned long long)decoded,d.ntables,d.warnings);if(d.notes)printf("%s",d.notes);bl_db_free(&d);return 0;
}

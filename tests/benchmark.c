#include "../src/core.h"
int main(int argc,char **argv){
    if(argc!=3){fprintf(stderr,"Usage: benchmark dump.sql cache-root\n");return 2;}
    BLDatabase d;bl_db_init(&d);uint64_t start=bl_millis();
    if(!bl_db_prepare(&d,argv[1],argv[2])||!bl_db_index(&d,NULL)){fprintf(stderr,"%s\n",d.error);return 1;}
    uint64_t index_ms=bl_millis()-start,rows=d.total_rows,size=d.source_size;int table=-1;for(uint32_t i=0;i<d.ntables;i++)if(d.tables[i].kind==0&&d.tables[i].rows){table=(int)i;break;}
    if(table<0)return 3;
    char *index=bl_index_path(&d,table);BLFile f=bl_open(index,0);start=bl_millis();
    uint64_t page_start=rows>900000?900000:(rows>100?rows-100:0),n=rows-page_start;if(n>100)n=100;
    for(uint64_t i=0;i<n;i++){BLRef ref;BLRow row;if(!bl_read_ref(f,page_start+i,&ref)||!bl_get_row(&d,table,&ref,&row)){fprintf(stderr,"%s\n",d.error);return 4;}bl_row_free(&row);}
    uint64_t page_ms=bl_millis()-start;bl_close(f);
    BLFilter query={1,1,"category_07",0};uint64_t matched=0;size_t cap=strlen(d.cache)+32;char *filter=(char*)malloc(cap);bl_format(filter,cap,"%s/bench-filter.bli",d.cache);
    start=bl_millis();if(!bl_filter(&d,table,&query,filter,&matched,NULL)){fprintf(stderr,"%s\n",d.error);return 5;}uint64_t filter_ms=bl_millis()-start;
    if(matched!=rows/100+(rows%100>7?1:0))return 6;
    char *csv=(char*)malloc(cap);bl_format(csv,cap,"%s/bench-export.csv",d.cache);start=bl_millis();if(!bl_export_csv(&d,table,filter,matched,csv,0,1,NULL)){fprintf(stderr,"%s\n",d.error);return 7;}uint64_t csv_ms=bl_millis()-start;
    free(filter);free(csv);free(index);bl_db_free(&d);bl_db_init(&d);start=bl_millis();if(!bl_db_prepare(&d,argv[1],argv[2])||!bl_db_load_cache(&d))return 8;uint64_t cache_ms=bl_millis()-start;
    printf("{\"rows\":%llu,\"dump_bytes\":%llu,\"position_index_bytes\":%llu,\"index_ms\":%llu,\"page_start\":%llu,\"page_rows\":%llu,\"page_ms\":%llu,\"filter_ms\":%llu,\"matches\":%llu,\"filtered_csv_ms\":%llu,\"reopen_cache_ms\":%llu}\n",(unsigned long long)rows,(unsigned long long)size,(unsigned long long)(rows*16),(unsigned long long)index_ms,(unsigned long long)page_start,(unsigned long long)n,(unsigned long long)page_ms,(unsigned long long)filter_ms,(unsigned long long)matched,(unsigned long long)csv_ms,(unsigned long long)cache_ms);
    bl_db_free(&d);return 0;
}

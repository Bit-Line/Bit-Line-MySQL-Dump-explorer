#ifndef BL_CORE_H
#define BL_CORE_H
#include "platform.h"
#define BL_VERSION "1.0.1"
#define BL_MAX_COLUMNS 4096
#define BL_ROW_LIMIT (64u*1024u*1024u)
#define BL_NOESC 0x80000000u
/* On-disk format is explicitly little-endian, versioned. RowRef is exactly 16 bytes. */
typedef struct {uint64_t offset;uint32_t length,layout;} BLRef;
typedef struct {char *name,*definition;} BLColumn;
typedef struct {uint32_t n;uint32_t *columns;} BLLayout;
typedef struct {
    char *database,*name,*ddl;BLColumn *columns;uint32_t ncolumns;
    BLLayout *layouts;uint32_t nlayouts;uint64_t rows,bytes;
    int kind; /* 0 table, 1 view, 2 routine, 3 database marker */
} BLTable;
typedef struct {
    char *source,*cache,*notes;uint64_t source_size,source_mtime,source_hash,total_rows;
    BLTable *tables;uint32_t ntables;uint32_t warnings;BLFile source_file;
    char error[1024];int reused;
    unsigned char *read_buffer;uint64_t read_base;size_t read_length;
} BLDatabase;
typedef int (*BLProgress)(void *user,uint64_t bytes,uint64_t total,uint64_t rows,const char *phase);
typedef struct {BLProgress progress;void *user;int no_backslash;} BLOptions;
typedef enum {BL_NULL=0,BL_TEXT=1,BL_NUMBER=2,BL_BINARY=3,BL_EXPR=4,BL_MISSING=5} BLKind;
typedef struct {char *data;size_t len;BLKind kind;} BLValue;
typedef struct {BLValue *values;uint32_t count;char *raw;size_t raw_len;} BLRow;
typedef struct {int column;int op;const char *text;int encoding;} BLFilter; /* op 0 contains,1 equals,2 NULL,3 not NULL; column -1 all */
/* encoding: 0 UTF-8, 1 Windows-1252, 2 ISO-8859-1. Values stay byte exact until conversion. */
void bl_db_init(BLDatabase *db);
void bl_db_free(BLDatabase *db);
int bl_db_prepare(BLDatabase *db,const char *source,const char *cache_root);
int bl_db_load_cache(BLDatabase *db);
int bl_db_index(BLDatabase *db,const BLOptions *options);
char *bl_index_path(const BLDatabase *db,uint32_t table);
int bl_read_ref(BLFile index,uint64_t row,BLRef *ref);
int bl_get_row(BLDatabase *db,uint32_t table,const BLRef *ref,BLRow *row);
void bl_row_free(BLRow *row);
int bl_filter(BLDatabase *db,uint32_t table,const BLFilter *filter,const char *target,uint64_t *matches,const BLOptions *options);
int bl_export_csv(BLDatabase *db,uint32_t table,const char *index_path,uint64_t count,const char *target,int encoding,int safe,const BLOptions *options);
int bl_export_raw(BLDatabase *db,const BLRef *ref,const char *target,const BLOptions *options);
char *bl_to_utf8(const char *data,size_t n,int encoding,size_t *out_n);
uint64_t bl_hash(const void *data,size_t n,uint64_t seed);
#endif

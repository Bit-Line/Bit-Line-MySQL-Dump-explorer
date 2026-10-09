#include "platform.h"
int bl_format(char *out,size_t cap,const char *fmt,...) {
    va_list ap; va_start(ap,fmt);
#ifdef _WIN32
    /* Use the long-established MSVCRT I64 length modifier for 64-bit formatting. */
    char native[2048];size_t k=0;int in_spec=0;
    for(size_t i=0;fmt[i]&&k+4<sizeof(native);i++){
        char c=fmt[i];
        if(in_spec&&c=='l'&&fmt[i+1]=='l'){native[k++]='I';native[k++]='6';native[k++]='4';i++;continue;}
        native[k++]=c;
        if(c=='%')in_spec=!in_spec;
        else if(in_spec&&strchr("diouxXfFeEgGaAcCsSpn",c))in_spec=0;
    }native[k]=0;
    int n=vsnprintf(out,cap,native,ap);
#else
    int n=vsnprintf(out,cap,fmt,ap);
#endif
    va_end(ap);
    if(cap) out[cap-1]=0; return n;
}
char *bl_ndup(const char *s,size_t n) { char *p=(char*)malloc(n+1); if(p){memcpy(p,s,n);p[n]=0;}return p; }
char *bl_dup(const char *s){return bl_ndup(s,strlen(s));}
#ifdef _WIN32
WCHAR *bl_wide(const char *s){int n=MultiByteToWideChar(65001,0,s,-1,NULL,0);WCHAR *p=(WCHAR*)calloc((size_t)n+1,2);if(p)MultiByteToWideChar(65001,0,s,-1,p,n);return p;}
char *bl_utf8(const WCHAR *s){int n=WideCharToMultiByte(65001,0,s,-1,NULL,0,NULL,NULL);char *p=(char*)calloc((size_t)n+1,1);if(p)WideCharToMultiByte(65001,0,s,-1,p,n,NULL,NULL);return p;}
WCHAR *bl_winpath(const char *s){
    WCHAR *w=bl_wide(s);if(!w)return NULL;
    if(w[0]=='\\'&&w[1]=='\\'&&w[2]=='?'&&w[3]=='\\')return w;
    DWORD n=GetFullPathNameW(w,0,NULL,NULL);WCHAR *abs=(WCHAR*)calloc((size_t)n+8,2);
    if(!abs){free(w);return NULL;}
    GetFullPathNameW(w,n+1,abs,NULL);free(w);
    size_t len=0;while(abs[len])len++;
    WCHAR *out=(WCHAR*)calloc(len+9,2);if(!out){free(abs);return NULL;}
    out[0]='\\';out[1]='\\';out[2]='?';out[3]='\\';
    if(abs[0]=='\\'&&abs[1]=='\\'){out[4]='U';out[5]='N';out[6]='C';out[7]='\\';memcpy(out+8,abs+2,(len-1)*2);}else memcpy(out+4,abs,(len+1)*2);
    free(abs);return out;
}
BLFile bl_open(const char *p,int write){WCHAR *w=bl_winpath(p);if(!w)return BL_BAD_FILE;
    HANDLE h=CreateFileW(w,write?GENERIC_WRITE|GENERIC_READ:GENERIC_READ,FILE_SHARE_READ,NULL,write==1?CREATE_ALWAYS:(write==2?OPEN_ALWAYS:OPEN_EXISTING),FILE_ATTRIBUTE_NORMAL,NULL);free(w);
    if(write==2&&h!=BL_BAD_FILE){LARGE_INTEGER z;z.QuadPart=0;SetFilePointerEx(h,z,NULL,FILE_END);}return h;}
void bl_close(BLFile f){if(f!=BL_BAD_FILE)CloseHandle(f);}
size_t bl_read(BLFile f,void *p,size_t n){DWORD got=0;if(n>0x7fffffff)n=0x7fffffff;return ReadFile(f,p,(DWORD)n,&got,NULL)?got:0;}
int bl_write(BLFile f,const void *p,size_t n){const char *q=(const char*)p;while(n){DWORD done=0,take=n>0x7fffffff?0x7fffffff:(DWORD)n;if(!WriteFile(f,q,take,&done,NULL)||done!=take)return 0;n-=done;q+=done;}return 1;}
int bl_seek(BLFile f,uint64_t off){LARGE_INTEGER v;v.QuadPart=(int64_t)off;return SetFilePointerEx(f,v,NULL,FILE_BEGIN);}
uint64_t bl_size(BLFile f){LARGE_INTEGER v;return GetFileSizeEx(f,&v)?(uint64_t)v.QuadPart:0;}
uint64_t bl_mtime(BLFile f){FILETIME t;if(!GetFileTime(f,NULL,NULL,&t))return 0;return ((uint64_t)t.dwHighDateTime<<32)|t.dwLowDateTime;}
int bl_mkdir(const char *p){WCHAR *w=bl_winpath(p);if(!w)return 0;int ok=CreateDirectoryW(w,NULL)||GetLastError()==183;free(w);return ok;}
int bl_remove(const char *p){WCHAR *w=bl_winpath(p);if(!w)return 0;int r=DeleteFileW(w);free(w);return r;}
int bl_move(const char *a,const char *b){WCHAR *wa=bl_winpath(a),*wb=bl_winpath(b);int r=wa&&wb&&MoveFileExW(wa,wb,9);free(wa);free(wb);return r;}
uint64_t bl_millis(void){return GetTickCount64();}
uint32_t bl_pid(void){return GetCurrentProcessId();}
int bl_flush(BLFile f){return FlushFileBuffers(f);}
#else
BLFile bl_open(const char *p,int write){return fopen(p,write==1?"w+b":(write==2?"a+b":"rb"));}
void bl_close(BLFile f){if(f)fclose(f);}
size_t bl_read(BLFile f,void *p,size_t n){return fread(p,1,n,f);}
int bl_write(BLFile f,const void *p,size_t n){return fwrite(p,1,n,f)==n;}
int bl_seek(BLFile f,uint64_t off){return !fseeko(f,(off_t)off,SEEK_SET);}
uint64_t bl_size(BLFile f){struct stat s;return !fstat(fileno(f),&s)?(uint64_t)s.st_size:0;}
uint64_t bl_mtime(BLFile f){struct stat s;return !fstat(fileno(f),&s)?(uint64_t)s.st_mtim.tv_sec*1000000000ull+s.st_mtim.tv_nsec:0;}
int bl_mkdir(const char *p){struct stat s;return !mkdir(p,0700)||(!stat(p,&s)&&S_ISDIR(s.st_mode));}
int bl_remove(const char *p){return !remove(p);}
int bl_move(const char *a,const char *b){return !rename(a,b);}
uint64_t bl_millis(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
uint32_t bl_pid(void){return (uint32_t)getpid();}
int bl_flush(BLFile f){return !fflush(f)&&!fsync(fileno(f));}
#endif

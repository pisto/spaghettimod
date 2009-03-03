// implementation of generic tools

#include "cube.h"

///////////////////////// file system ///////////////////////

#ifndef WIN32
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif

string homedir = "";
vector<char *> packagedirs;

char *makerelpath(const char *dir, const char *file, const char *prefix, const char *cmd)
{
    static string tmp;
    if(prefix) s_strcpy(tmp, prefix);
    else tmp[0] = '\0';
    if(file[0]=='<')
    {
        const char *end = strrchr(file, '>');
        if(end)
        {
            size_t len = strlen(tmp);
            s_strncpy(&tmp[len], file, min(sizeof(tmp)-len, size_t(end+2-file)));
            file = end+1;
        }
    }
    if(cmd) s_strcat(tmp, cmd);
    s_sprintfd(pname)("%s/%s", dir, file);
    s_strcat(tmp, pname);
    return tmp;
}

char *path(char *s)
{
    for(char *curpart = s;;)
    {
        char *endpart = strchr(curpart, '&');
        if(endpart) *endpart = '\0';
        if(curpart[0]=='<')
        {
            char *file = strrchr(curpart, '>');
            if(!file) return s;
            curpart = file+1;
        }
        for(char *t = curpart; (t = strpbrk(t, "/\\")); *t++ = PATHDIV);
        for(char *prevdir = NULL, *curdir = s;;)
        {
            prevdir = curdir[0]==PATHDIV ? curdir+1 : curdir;
            curdir = strchr(prevdir, PATHDIV);
            if(!curdir) break;
            if(prevdir+1==curdir && prevdir[0]=='.')
            {
                memmove(prevdir, curdir+1, strlen(curdir+1)+1);
                curdir = prevdir;
            }
            else if(curdir[1]=='.' && curdir[2]=='.' && curdir[3]==PATHDIV) 
            {
                if(prevdir+2==curdir && prevdir[0]=='.' && prevdir[1]=='.') continue;
                memmove(prevdir, curdir+4, strlen(curdir+4)+1); 
                curdir = prevdir;
            }
        }
        if(endpart)
        {
            *endpart = '&';
            curpart = endpart+1;
        }
        else break;
    }
    return s;
}

char *path(const char *s, bool copy)
{
    static string tmp;
    s_strcpy(tmp, s);
    path(tmp);
    return tmp;
}

const char *parentdir(const char *directory)
{
    const char *p = directory + strlen(directory);
    while(p > directory && *p != '/' && *p != '\\') p--;
    static string parent;
    size_t len = p-directory+1;
    s_strncpy(parent, directory, len);
    return parent;
}

bool fileexists(const char *path, const char *mode)
{
    bool exists = true;
    if(mode[0]=='w' || mode[0]=='a') path = parentdir(path);
#ifdef WIN32
    if(GetFileAttributes(path) == INVALID_FILE_ATTRIBUTES) exists = false;
#else
    if(access(path, R_OK | (mode[0]=='w' || mode[0]=='a' ? W_OK : 0)) == -1) exists = false;
#endif
    return exists;
}

bool createdir(const char *path)
{
    size_t len = strlen(path);
    if(path[len-1]==PATHDIV)
    {
        static string strip;
        path = s_strncpy(strip, path, len);
    }
#ifdef WIN32
    return CreateDirectory(path, NULL)!=0;
#else
    return mkdir(path, 0777)==0;
#endif
}

static void fixdir(char *dir)
{
    path(dir);
    size_t len = strlen(dir);
    if(dir[len-1]!=PATHDIV)
    {
        dir[len] = PATHDIV;
        dir[len+1] = '\0';
    }
}

void sethomedir(const char *dir)
{
    fixdir(s_strcpy(homedir, dir));
}

void addpackagedir(const char *dir)
{
    fixdir(packagedirs.add(newstringbuf(dir)));
}
 
const char *findfile(const char *filename, const char *mode)
{
    static string s;
    if(homedir[0])
    {
        s_sprintf(s)("%s%s", homedir, filename);
        if(fileexists(s, mode)) return s;
        if(mode[0]=='w' || mode[0]=='a')
        {
            string dirs;
            s_strcpy(dirs, s);
            char *dir = strchr(dirs[0]==PATHDIV ? dirs+1 : dirs, PATHDIV);
            while(dir)
            {
                *dir = '\0';
                if(!fileexists(dirs, "r") && !createdir(dirs)) return s;
                *dir = PATHDIV;
                dir = strchr(dir+1, PATHDIV);
            }
            return s;
        } 
    }
    if(mode[0]=='w' || mode[0]=='a') return filename;
    loopv(packagedirs)
    {
        s_sprintf(s)("%s%s", packagedirs[i], filename);
        if(fileexists(s, mode)) return s;
    }
    return filename;
}

FILE *openfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found) return NULL;
    return fopen(found, mode);
}

gzFile opengzfile(const char *filename, const char *mode)
{
    const char *found = findfile(filename, mode);
    if(!found) return NULL;
    return gzopen(found, mode);
}

char *loadfile(const char *fn, int *size)
{
    FILE *f = openfile(fn, "rb");
    if(!f) return NULL;
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    if(len<=0) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *buf = new char[len+1];
    if(!buf) { fclose(f); return NULL; }
    buf[len] = 0;
    size_t rlen = fread(buf, 1, len, f);
    fclose(f);
    if(size_t(len)!=rlen)
    {
        delete[] buf;
        return NULL;
    }
    if(size!=NULL) *size = len;
    return buf;
}

bool listdir(const char *dir, const char *ext, vector<char *> &files)
{
    int extsize = ext ? (int)strlen(ext)+1 : 0;
    #if defined(WIN32)
    s_sprintfd(pathname)("%s\\*.%s", dir, ext ? ext : "*");
    WIN32_FIND_DATA FindFileData;
    HANDLE Find = FindFirstFile(path(pathname), &FindFileData);
    if(Find != INVALID_HANDLE_VALUE)
    {
        do {
            files.add(newstring(FindFileData.cFileName, (int)strlen(FindFileData.cFileName) - extsize));
        } while(FindNextFile(Find, &FindFileData));
        return true;
    }
    #else
    string pathname;
    s_strcpy(pathname, dir);
    DIR *d = opendir(path(pathname));
    if(d)
    {
        struct dirent *de;
        while((de = readdir(d)) != NULL)
        {
            if(!ext) files.add(newstring(de->d_name));
            else
            {
                int namelength = (int)strlen(de->d_name) - extsize;
                if(namelength > 0 && de->d_name[namelength] == '.' && strncmp(de->d_name+namelength+1, ext, extsize-1)==0)
                    files.add(newstring(de->d_name, namelength));
            }
        }
        closedir(d);
        return true;
    }
    #endif
    else return false;
}

int listfiles(const char *dir, const char *ext, vector<char *> &files)
{
    int dirs = 0;
    if(listdir(dir, ext, files)) dirs++;
    string s;
    if(homedir[0])
    {
        s_sprintf(s)("%s%s", homedir, dir);
        if(listdir(s, ext, files)) dirs++;
    }
    loopv(packagedirs)
    {
        s_sprintf(s)("%s%s", packagedirs[i], dir);
        if(listdir(s, ext, files)) dirs++;
    }
    return dirs;
}

///////////////////////// misc tools ///////////////////////

void endianswap(void *memory, int stride, int length)   // little endian as storage format
{
    static const int littleendian = 1;
    if(!*(const char *)&littleendian) loop(w, length) loop(i, stride/2)
    {
        uchar *p = (uchar *)memory+w*stride;
        uchar t = p[i];
        p[i] = p[stride-i-1];
        p[stride-i-1] = t;
    }
}


////////////////////////// rnd numbers ////////////////////////////////////////

#define N              (624)             
#define M              (397)                
#define K              (0x9908B0DFU)       
#define hiBit(u)       ((u) & 0x80000000U)  
#define loBit(u)       ((u) & 0x00000001U)  
#define loBits(u)      ((u) & 0x7FFFFFFFU)  
#define mixBits(u, v)  (hiBit(u)|loBits(v)) 

static uint state[N+1];     
static uint *next;          
static int left = -1;     

void seedMT(uint seed)
{
    register uint x = (seed | 1U) & 0xFFFFFFFFU, *s = state;
    register int j;
    for(left=0, *s++=x, j=N; --j; *s++ = (x*=69069U) & 0xFFFFFFFFU);
}

uint reloadMT(void)
{
    register uint *p0=state, *p2=state+2, *pM=state+M, s0, s1;
    register int j;
    if(left < -1) seedMT(time(NULL));
    left=N-1, next=state+1;
    for(s0=state[0], s1=state[1], j=N-M+1; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    for(pM=state, j=M; --j; s0=s1, s1=*p2++) *p0++ = *pM++ ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1=state[0], *p0 = *pM ^ (mixBits(s0, s1) >> 1) ^ (loBit(s1) ? K : 0U);
    s1 ^= (s1 >> 11);
    s1 ^= (s1 <<  7) & 0x9D2C5680U;
    s1 ^= (s1 << 15) & 0xEFC60000U;
    return(s1 ^ (s1 >> 18));
}

uint randomMT(void)
{
    uint y;
    if(--left < 0) return(reloadMT());
    y  = *next++;
    y ^= (y >> 11);
    y ^= (y <<  7) & 0x9D2C5680U;
    y ^= (y << 15) & 0xEFC60000U;
    return(y ^ (y >> 18));
}

// GL_ARB_vertex_program, GL_ARB_fragment_program
extern PFNGLGENPROGRAMSARBPROC              glGenPrograms_;
extern PFNGLDELETEPROGRAMSARBPROC           glDeletePrograms_;
extern PFNGLBINDPROGRAMARBPROC              glBindProgram_;
extern PFNGLPROGRAMSTRINGARBPROC            glProgramString_;
extern PFNGLGETPROGRAMIVARBPROC             glGetProgramiv_;
extern PFNGLPROGRAMENVPARAMETER4FARBPROC    glProgramEnvParameter4f_;
extern PFNGLPROGRAMENVPARAMETER4FVARBPROC   glProgramEnvParameter4fv_;
extern PFNGLENABLEVERTEXATTRIBARRAYARBPROC  glEnableVertexAttribArray_;
extern PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArray_;
extern PFNGLVERTEXATTRIBPOINTERARBPROC      glVertexAttribPointer_;

// GL_EXT_gpu_program_parameters
#ifndef GL_EXT_gpu_program_parameters
#define GL_EXT_gpu_program_parameters 1
typedef void (APIENTRYP PFNGLPROGRAMENVPARAMETERS4FVEXTPROC) (GLenum target, GLuint index, GLsizei count, const GLfloat *params);
typedef void (APIENTRYP PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC) (GLenum target, GLuint index, GLsizei count, const GLfloat *params);
#endif

extern PFNGLPROGRAMENVPARAMETERS4FVEXTPROC   glProgramEnvParameters4fv_;
extern PFNGLPROGRAMLOCALPARAMETERS4FVEXTPROC glProgramLocalParameters4fv_;

// GL_ARB_shading_language_100, GL_ARB_shader_objects, GL_ARB_fragment_shader, GL_ARB_vertex_shader
extern PFNGLCREATEPROGRAMOBJECTARBPROC  glCreateProgramObject_;
extern PFNGLDELETEOBJECTARBPROC         glDeleteObject_;
extern PFNGLUSEPROGRAMOBJECTARBPROC     glUseProgramObject_;
extern PFNGLCREATESHADEROBJECTARBPROC   glCreateShaderObject_;
extern PFNGLSHADERSOURCEARBPROC         glShaderSource_;
extern PFNGLCOMPILESHADERARBPROC        glCompileShader_;
extern PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameteriv_;
extern PFNGLATTACHOBJECTARBPROC         glAttachObject_;
extern PFNGLGETINFOLOGARBPROC           glGetInfoLog_;
extern PFNGLLINKPROGRAMARBPROC          glLinkProgram_;
extern PFNGLGETUNIFORMLOCATIONARBPROC   glGetUniformLocation_;
extern PFNGLUNIFORM4FVARBPROC           glUniform4fv_;
extern PFNGLUNIFORM1IARBPROC            glUniform1i_;

extern int renderpath;

enum { R_FIXEDFUNCTION = 0, R_ASMSHADER, R_GLSLANG };

enum { SHPARAM_VERTEX = 0, SHPARAM_PIXEL, SHPARAM_UNIFORM };

#define RESERVEDSHADERPARAMS 16
#define MAXSHADERPARAMS 8

struct ShaderParam
{
    const char *name;
    int type, index, loc;
    float val[4];
};

struct LocalShaderParamState : ShaderParam
{
    float curval[4];

    LocalShaderParamState() 
    { 
        memset(curval, 0, sizeof(curval)); 
    }
    LocalShaderParamState(const ShaderParam &p) : ShaderParam(p)
    {
        memset(curval, 0, sizeof(curval));
    }
};

struct ShaderParamState
{
    enum
    {
        CLEAN = 0,
        INVALID,
        DIRTY
    };
    
    const char *name;
    float val[4];
    bool local;
    int dirty;

    ShaderParamState()
        : name(NULL), local(false), dirty(INVALID)
    {
        memset(val, 0, sizeof(val));
    }
};

enum 
{ 
    SHADER_DEFAULT    = 0, 
    SHADER_NORMALSLMS = 1<<0, 
    SHADER_ENVMAP     = 1<<1,
    SHADER_GLSLANG    = 1<<2,
    SHADER_OPTION     = 1<<3,
    SHADER_INVALID    = 1<<4,
    SHADER_DEFERRED   = 1<<5
};

#define MAXSHADERDETAIL 3
#define MAXVARIANTROWS 5

extern int shaderdetail;

struct Slot;

struct Shader
{
    static Shader *lastshader;

    char *name, *vsstr, *psstr, *defer;
    int type;
    GLuint vs, ps;
    GLhandleARB program, vsobj, psobj;
    vector<LocalShaderParamState> defaultparams;
    Shader *detailshader, *variantshader, *altshader, *fastshader[MAXSHADERDETAIL];
    vector<Shader *> variants[MAXVARIANTROWS];
    bool standard, forced, used, native;
    Shader *reusevs, *reuseps;
    int numextparams;
    LocalShaderParamState *extparams;
    uchar *extvertparams, *extpixparams;


    Shader() : name(NULL), vsstr(NULL), psstr(NULL), defer(NULL), type(SHADER_DEFAULT), vs(0), ps(0), program(0), vsobj(0), psobj(0), detailshader(NULL), variantshader(NULL), altshader(NULL), standard(false), forced(false), used(false), native(true), reusevs(NULL), reuseps(NULL), numextparams(0), extparams(NULL), extvertparams(NULL), extpixparams(NULL)
    {
        loopi(MAXSHADERDETAIL) fastshader[i] = this;
    }

    ~Shader()
    {
        DELETEA(name);
        DELETEA(vsstr);
        DELETEA(psstr);
        DELETEA(defer);
        DELETEA(extparams);
        DELETEA(extvertparams);
        extpixparams = NULL;
    }

    void fixdetailshader(bool force = true, bool recurse = true);
    void allocenvparams(Slot *slot = NULL);
    void flushenvparams(Slot *slot = NULL);
    void setslotparams(Slot &slot);
    void bindprograms();

    bool hasoption(int row)
    {
        if(!detailshader || detailshader->variants[row].empty()) return false;
        return (detailshader->variants[row][0]->type&SHADER_OPTION)!=0;
    }

    void setvariant(int col, int row, Slot *slot, Shader *fallbackshader)
    {
        if(!this || !detailshader || renderpath==R_FIXEDFUNCTION) return;
        int len = detailshader->variants[row].length();
        if(col >= len) col = len-1;
        Shader *s = fallbackshader;
        while(col >= 0) if(!(detailshader->variants[row][col]->type&SHADER_INVALID)) { s = detailshader->variants[row][col]; break; }
        if(lastshader!=s) s->bindprograms();
        lastshader->flushenvparams(slot);
        if(slot) lastshader->setslotparams(*slot);
    }

    void setvariant(int col, int row = 0, Slot *slot = NULL)
    {
        setvariant(col, row, slot, detailshader);
    }

    void set(Slot *slot = NULL)
    {
        if(!this || !detailshader || renderpath==R_FIXEDFUNCTION) return;
        if(lastshader!=detailshader) detailshader->bindprograms();
        lastshader->flushenvparams(slot);
        if(slot) lastshader->setslotparams(*slot);
    }

    bool compile();
    void cleanup(bool invalid = false);
};

#define SETSHADER(name) \
    do { \
        static Shader *name##shader = NULL; \
        if(!name##shader) name##shader = lookupshaderbyname(#name); \
        name##shader->set(); \
    } while(0)

// management of texture slots
// each texture slot can have multiple texture frames, of which currently only the first is used
// additional frames can be used for various shaders

struct Texture
{
    enum
    {
        STUB,
        TRANSIENT,
        IMAGE,
        CUBEMAP
    };

    char *name;
    int type, w, h, xs, ys, bpp, clamp;
    bool mipmap, canreduce;
    GLuint id;
    uchar *alphamask;

    Texture() : alphamask(NULL) {}
};

enum
{
    TEX_DIFFUSE = 0,
    TEX_UNKNOWN,
    TEX_DECAL,
    TEX_NORMAL,
    TEX_GLOW,
    TEX_SPEC,
    TEX_DEPTH,
    TEX_ENVMAP
};
    
struct Slot
{
    struct Tex
    {
        int type;
        Texture *t;
        string name;
        int combined;
    };

    vector<Tex> sts;
    Shader *shader;
    vector<ShaderParam> params;
    float scale;
    int rotation, xoffset, yoffset;
    float scrollS, scrollT;
    int layer;
    vec glowcolor, pulseglowcolor;
    float pulseglowspeed;
    bool mtglowed, loaded;
    uint texmask;
    char *autograss;
    Texture *grasstex, *thumbnail;
    char *layermaskname;
    int layermaskmode;
    float layermaskscale;
    SDL_Surface *layermask;

    Slot() : autograss(NULL), layermaskname(NULL), layermask(NULL) { reset(); }
    
    void reset()
    {
        sts.setsize(0);
        shader = NULL;
        params.setsize(0);
        scale = 1;
        rotation = xoffset = yoffset = 0;
        scrollS = scrollT = 0;
        layer = 0;
        glowcolor = vec(1, 1, 1);
        pulseglowcolor = vec(0, 0, 0);
        pulseglowspeed = 0;
        loaded = false;
        texmask = 0;
        DELETEA(autograss);
        grasstex = NULL;
        thumbnail = NULL;
        DELETEA(layermaskname);
        layermaskmode = 0;
        layermaskscale = 1;
        if(layermask) { SDL_FreeSurface(layermask); layermask = NULL; }
    }

    void cleanup()
    {
        loaded = false;
        grasstex = NULL;
        thumbnail = NULL;
        loopv(sts) 
        {
            Tex &t = sts[i];
            t.t = NULL;
            t.combined = -1;
        }
    }
};

struct cubemapside
{
    GLenum target;
    const char *name;
    bool flipx, flipy, swapxy;
};

extern cubemapside cubemapsides[6];
extern Texture *notexture;
extern Shader *defaultshader, *rectshader, *notextureshader, *nocolorshader, *foggedshader, *foggednotextureshader, *stdworldshader;
extern int reservevpparams, maxvpenvparams, maxvplocalparams, maxfpenvparams, maxfplocalparams;

extern Shader *lookupshaderbyname(const char *name);
extern Shader *useshaderbyname(const char *name);
extern Texture *loadthumbnail(Slot &slot);
extern void resetslotshader();
extern void setslotshader(Slot &s);
extern void linkslotshader(Slot &s, bool load = true);
extern void linkslotshaders();
extern void setenvparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setenvparamfv(const char *name, int type, int index, const float *v);
extern void flushenvparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void flushenvparamfv(const char *name, int type, int index, const float *v);
extern void setlocalparamf(const char *name, int type, int index, float x = 0, float y = 0, float z = 0, float w = 0);
extern void setlocalparamfv(const char *name, int type, int index, const float *v);
extern void invalidateenvparams(int type, int start, int count);
extern ShaderParam *findshaderparam(Slot &s, const char *name, int type, int index);

extern int maxtmus, nolights, nowater, nomasks;

extern void inittmus();
extern void resettmu(int n);
extern void scaletmu(int n, int rgbscale, int alphascale = 0);
extern void colortmu(int n, float r = 0, float g = 0, float b = 0, float a = 0);
extern void setuptmu(int n, const char *rgbfunc = NULL, const char *alphafunc = NULL);

#define MAXDYNLIGHTS 5
#define DYNLIGHTBITS 6
#define DYNLIGHTMASK ((1<<DYNLIGHTBITS)-1)

#define MAXBLURRADIUS 7

extern void setupblurkernel(int radius, float sigma, float *weights, float *offsets);
extern void setblurshader(int pass, int size, int radius, float *weights, float *offsets, GLenum target = GL_TEXTURE_2D);

extern SDL_Surface *createsurface(int width, int height, int bpp);
extern SDL_Surface *wrapsurface(void *data, int width, int height, int bpp);
extern SDL_Surface *fixsurfaceformat(SDL_Surface *s);
extern void savepng(const char *filename, SDL_Surface *image, bool flip = false);
extern void savetga(const char *filename, SDL_Surface *image, bool flip = false);


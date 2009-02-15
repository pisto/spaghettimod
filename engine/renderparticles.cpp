// renderparticles.cpp

#include "pch.h"
#include "engine.h"
#include "rendertarget.h"

Shader *particleshader = NULL, *particlenotextureshader = NULL;

VARP(particlesize, 20, 100, 500);
    
// Check emit_particles() to limit the rate that paricles can be emitted for models/sparklies
// Automatically stops particles being emitted when paused or in reflective drawing
VARP(emitmillis, 1, 17, 1000);
static int lastemitframe = 0;
static bool emit = false;

static bool emit_particles()
{
    if(reflecting || refracting) return false;
    return emit;
}

enum
{
    PT_PART = 0,
    PT_TAPE,
    PT_TRAIL,
    PT_TEXT,
    PT_TEXTUP,
    PT_METER,
    PT_METERVS,
    PT_FIREBALL,
    PT_LIGHTNING,
    PT_FLARE,

    PT_MOD   = 1<<8,
    PT_RND4  = 1<<9,
    PT_LERP  = 1<<10, // use very sparingly - order of blending issues
    PT_TRACK = 1<<11,
    PT_GLARE = 1<<12,
    PT_SOFT  = 1<<13
};

const char *partnames[] = { "part", "tape", "trail", "text", "textup", "meter", "metervs", "fireball", "lightning", "flare" };

struct particle
{
    vec o, d;
    int fade, millis;
    bvec color;
    uchar flags;
    float size;
    union
    {
        const char *text;         // will call delete[] on this only if it starts with an @
        float val;
        physent *owner;
        struct
        {
            uchar color2[3];
            uchar progress;
        };
    }; 
};

struct partvert
{
    vec pos;
    float u, v;
    bvec color;
    uchar alpha;
};

#define COLLIDERADIUS 8.0f
#define COLLIDEERROR 1.0f

struct partrenderer
{
    Texture *tex;
    const char *texname;
    uint type;
    int grav, collide;
    
    partrenderer(const char *texname, int type, int grav, int collide) 
        : tex(NULL), texname(texname), type(type), grav(grav), collide(collide)
    {
    }
    virtual ~partrenderer()
    {
    }

    virtual void init(int n) { }
    virtual void reset() = NULL;
    virtual void resettracked(physent *owner) { }   
    virtual particle *addpart(const vec &o, const vec &d, int fade, int color, float size) = NULL;    
    virtual int adddepthfx(vec &bbmin, vec &bbmax) { return 0; }
    virtual void update() { }
    virtual void render() = NULL;
    virtual bool haswork() = NULL;
    virtual int count() = NULL; //for debug
    virtual bool usesvertexarray() { return false; } 
    virtual void cleanup() {}

    //blend = 0 => remove it
    void calc(particle *p, int &blend, int &ts, vec &o, vec &d, bool lastpass = true)
    {
        o = p->o;
        d = p->d;
        if(type&PT_TRACK && p->owner) cl->particletrack(p->owner, o, d);
        if(p->fade <= 5) 
        {
            ts = 1;
            blend = 255;
        }
        else
        {
            ts = lastmillis-p->millis;
            blend = max(255 - (ts<<8)/p->fade, 0);
            if(grav)
            {
                if(ts > p->fade) ts = p->fade;
                float t = (float)(ts);
                vec v = d;
                v.mul(t/5000.0f);
                o.add(v);
                o.z -= t*t/(2.0f * 5000.0f * grav);
            }
            if(collide && o.z < p->val && lastpass)
            {
                vec surface;
                float floorz = rayfloor(vec(o.x, o.y, p->val), surface, RAY_CLIPMAT, COLLIDERADIUS);
                float collidez = floorz<0 ? o.z-COLLIDERADIUS : p->val - rayfloor(vec(o.x, o.y, p->val), surface, RAY_CLIPMAT, COLLIDERADIUS);
                if(o.z >= collidez+COLLIDEERROR) 
                    p->val = collidez+COLLIDEERROR;
                else 
                {
                    adddecal(collide, vec(o.x, o.y, collidez), vec(p->o).sub(o).normalize(), 2*p->size, p->color, type&PT_RND4 ? detrnd((size_t)p, 4) : 0);
                    blend = 0;
                }
            }
        }
    }
};

struct listparticle : particle
{   
    listparticle *next;
};

static listparticle *parempty = NULL;

VARP(outlinemeters, 0, 0, 1);

struct listrenderer : partrenderer
{
    listparticle *list;

    listrenderer(const char *texname, int type, int grav, int collide) 
        : partrenderer(texname, type, grav, collide), list(NULL)
    {
    }

    virtual ~listrenderer()
    {
    }

    virtual void cleanup(listparticle *p)
    {
    }

    void reset()  
    {
        if(!list) return;
        listparticle *p = list;
        for(;;)
        {
            cleanup(p);
            if(p->next) p = p->next;
            else break;
        }
        p->next = parempty;
        parempty = list;
        list = NULL;
    }
    
    void resettracked(physent *owner) 
    {
        if(!(type&PT_TRACK)) return;
        for(listparticle **prev = &list, *cur = list; cur; cur = *prev)
        {
            if(!owner || cur->owner==owner) 
            {
                *prev = cur->next;
                cur->next = parempty;
                parempty = cur;
            }
            else prev = &cur->next;
        }
    }
    
    particle *addpart(const vec &o, const vec &d, int fade, int color, float size) 
    {
        if(!parempty)
        {
            listparticle *ps = new listparticle[256];
            loopi(255) ps[i].next = &ps[i+1];
            ps[255].next = parempty;
            parempty = ps;
        }
        listparticle *p = parempty;
        parempty = p->next;
        p->next = list;
        list = p;
        p->o = o;
        p->d = d;
        p->fade = fade;
        p->millis = lastmillis;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->size = size;
        p->owner = NULL;
        return p;
    }
    
    int count() 
    {
        int num = 0;
        listparticle *lp;
        for(lp = list; lp; lp = lp->next) num++;
        return num;
    }
    
    bool haswork() 
    {
        return (list != NULL);
    }
    
    virtual void startrender() = 0;
    virtual void endrender() = 0;
    virtual void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color) = 0;

    void render() 
    {
        startrender();
        if(texname)
        {
            if(!tex) tex = textureload(texname);
            glBindTexture(GL_TEXTURE_2D, tex->id);
        }
        
        bool lastpass = !reflecting && !refracting;
        
        for(listparticle **prev = &list, *p = list; p; p = *prev)
        {   
            vec o, d;
            int blend, ts;
            calc(p, blend, ts, o, d, lastpass);
            if(blend > 0) 
            {
                renderpart(p, o, d, blend, ts, p->color.v);

                if(p->fade > 5 || !lastpass) 
                {
                    prev = &p->next;
                    continue;
                }
            }
            //remove
            *prev = p->next;
            p->next = parempty;
            cleanup(p);
            parempty = p;
        }
       
        endrender();
    }
};

struct meterrenderer : listrenderer
{
    meterrenderer(int type)
        : listrenderer(NULL, type, 0, 0)
    {}

    void startrender()
    {
         glDisable(GL_BLEND);
         glDisable(GL_TEXTURE_2D);
         particlenotextureshader->set();
    }

    void endrender()
    {
         glEnable(GL_BLEND);
         glEnable(GL_TEXTURE_2D);
         if(fogging && renderpath!=R_FIXEDFUNCTION) setfogplane(1, reflectz);
         particleshader->set();
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color)
    {
        int basetype = type&0xFF;

        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        if(fogging && renderpath!=R_FIXEDFUNCTION) setfogplane(0, reflectz - o.z, true);
        glRotatef(camera1->yaw-180, 0, 0, 1);
        glRotatef(camera1->pitch-90, 1, 0, 0);

        float scale = p->size/80.0f;
        glScalef(-scale, scale, -scale);

        float right = 8*FONTH, left = p->progress/100.0f*right;
        glTranslatef(-right/2.0f, 0, 0);

        if(outlinemeters)
        {
            glColor3f(0, 0.8f, 0);
            glBegin(GL_TRIANGLE_STRIP);
            loopk(10)
            {
                float c = (0.5f + 0.1f)*sinf(k/9.0f*M_PI), s = 0.5f - (0.5f + 0.1f)*cosf(k/9.0f*M_PI);
                glVertex2f(-c*FONTH, s*FONTH);
                glVertex2f(right + c*FONTH, s*FONTH);
            }
            glEnd();
        }

        if(basetype==PT_METERVS) glColor3ubv(p->color2);
        else glColor3f(0, 0, 0);
        glBegin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            float c = 0.5f*sinf(k/9.0f*M_PI), s = 0.5f - 0.5f*cosf(k/9.0f*M_PI);
            glVertex2f(left + c*FONTH, s*FONTH);
            glVertex2f(right + c*FONTH, s*FONTH);
        }
        glEnd();

        if(outlinemeters)
        {
            glColor3f(0, 0.8f, 0);
            glBegin(GL_TRIANGLE_FAN);
            loopk(10)
            {
                float c = (0.5f + 0.1f)*sinf(k/9.0f*M_PI), s = 0.5f - (0.5f + 0.1f)*cosf(k/9.0f*M_PI);
                glVertex2f(left + c*FONTH, s*FONTH);
            }
            glEnd();
        }

        glColor3ubv(color);
        glBegin(GL_TRIANGLE_STRIP);
        loopk(10)
        {
            float c = 0.5f*sinf(k/9.0f*M_PI), s = 0.5f - 0.5f*cosf(k/9.0f*M_PI);
            glVertex2f(-c*FONTH, s*FONTH);
            glVertex2f(left + c*FONTH, s*FONTH);
        }
        glEnd();


        glPopMatrix();
    }
};
static meterrenderer meters(PT_METER|PT_LERP), metervs(PT_METERVS|PT_LERP);

struct textrenderer : listrenderer
{
    textrenderer(int type, int grav = 0)
        : listrenderer(NULL, type, grav, 0)
    {}

    void startrender()
    {
    }

    void endrender()
    {
        if(fogging && renderpath!=R_FIXEDFUNCTION) setfogplane(1, reflectz);
    }

    void cleanup(listparticle *p)
    {
        if(p->text && p->text[0]=='@') delete[] p->text;
    }

    void renderpart(listparticle *p, const vec &o, const vec &d, int blend, int ts, uchar *color)
    {
        glPushMatrix();
        glTranslatef(o.x, o.y, o.z);
        if(fogging)
        {
            if(renderpath!=R_FIXEDFUNCTION) setfogplane(0, reflectz - o.z, true);
            else blend = (uchar)(blend * max(0.0f, min(1.0f, 1.0f - (reflectz - o.z)/waterfog)));
        }

        glRotatef(camera1->yaw-180, 0, 0, 1);
        glRotatef(camera1->pitch-90, 1, 0, 0);

        float scale = p->size/80.0f;
        glScalef(-scale, scale, -scale);

        const char *text = p->text+(p->text[0]=='@' ? 1 : 0);
        float xoff = -text_width(text)/2;
        float yoff = 0;
        if((type&0xFF)==PT_TEXTUP) { xoff += detrnd((size_t)p, 100)-50; yoff -= detrnd((size_t)p, 101); } //@TODO instead in worldspace beforehand?
        glTranslatef(xoff, yoff, 50);

        draw_text(text, 0, 0, color[0], color[1], color[2], blend);

        glPopMatrix();
    } 
};
static textrenderer texts(PT_TEXT|PT_LERP), textups(PT_TEXTUP|PT_LERP, -8);

template<int T>
static inline void modifyblend(const vec &o, int &blend)
{
    blend = min(blend<<2, 255);
    if(renderpath==R_FIXEDFUNCTION && fogging) blend = (uchar)(blend * max(0.0f, min(1.0f, 1.0f - (reflectz - o.z)/waterfog)));
}

template<>
inline void modifyblend<PT_TAPE>(const vec &o, int &blend)
{
}

template<int T>
static inline void genpos(const vec &o, const vec &d, float size, int grav, int ts, partvert *vs)
{
    vec udir = vec(camup).sub(camright).mul(size);
    vec vdir = vec(camup).add(camright).mul(size);
    vs[0].pos = vec(o.x + udir.x, o.y + udir.y, o.z + udir.z);
    vs[1].pos = vec(o.x + vdir.x, o.y + vdir.y, o.z + vdir.z);
    vs[2].pos = vec(o.x - udir.x, o.y - udir.y, o.z - udir.z);
    vs[3].pos = vec(o.x - vdir.x, o.y - vdir.y, o.z - vdir.z);
}

template<>
inline void genpos<PT_TAPE>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec dir1 = d, dir2 = d, c;
    dir1.sub(o);
    dir2.sub(camera1->o);
    c.cross(dir2, dir1).normalize().mul(size);
    vs[0].pos = vec(d.x-c.x, d.y-c.y, d.z-c.z);
    vs[1].pos = vec(o.x-c.x, o.y-c.y, o.z-c.z);
    vs[2].pos = vec(o.x+c.x, o.y+c.y, o.z+c.z);
    vs[3].pos = vec(d.x+c.x, d.y+c.y, d.z+c.z);
}

template<>
inline void genpos<PT_TRAIL>(const vec &o, const vec &d, float size, int ts, int grav, partvert *vs)
{
    vec e = d;
    if(grav) e.z -= float(ts)/grav;
    e.div(-75.0f);
    e.add(o);
    genpos<PT_TAPE>(o, e, size, ts, grav, vs);
}

template<int T>
struct varenderer : partrenderer
{
    partvert *verts;
    particle *parts;
    int maxparts, numparts, lastupdate;

    varenderer(const char *texname, int type, int grav, int collide) 
        : partrenderer(texname, type, grav, collide),
          verts(NULL), parts(NULL), maxparts(0), numparts(0), lastupdate(-1)
    {
    }
    
    void init(int n)
    {
        DELETEA(parts);
        DELETEA(verts);
        parts = new particle[n];
        verts = new partvert[n*4];
        maxparts = n;
        numparts = 0;
        lastupdate = -1;
    }
        
    void reset() 
    {
        numparts = 0;
        lastupdate = -1;
    }
    
    void resettracked(physent *owner) 
    {
        if(!(type&PT_TRACK)) return;
        loopi(numparts)
        {
            particle *p = parts+i;
            if(!owner || (p->owner == owner)) p->fade = -1;
        }
        lastupdate = -1;
    }
    
    int count() 
    {
        return numparts;
    }
    
    bool haswork() 
    {
        return (numparts > 0);
    }

    bool usesvertexarray() { return true; }

    particle *addpart(const vec &o, const vec &d, int fade, int color, float size) 
    {
        particle *p = parts + (numparts < maxparts ? numparts++ : rnd(maxparts)); //next free slot, or kill a random kitten
        p->o = o;
        p->d = d;
        p->fade = fade;
        p->millis = lastmillis;
        p->color = bvec(color>>16, (color>>8)&0xFF, color&0xFF);
        p->size = size;
        p->owner = NULL;
        p->flags = 0x80;
        int offset = p-parts;
        if(type&PT_RND4) p->flags |= detrnd(offset, 4)<<2;
        if((type&0xFF)==PT_PART) p->flags |= detrnd(offset*offset+37, 4);
        lastupdate = -1;
        return p;
    }
  
    void genverts(particle *p, partvert *vs, bool regen)
    {
        vec o, d;
        int blend, ts;

        calc(p, blend, ts, o, d);
        if(blend <= 1 || p->fade <= 5) p->fade = -1; //mark to remove on next pass (i.e. after render)

        modifyblend<T>(o, blend);

        if(regen)
        {
            p->flags &= ~0x80;

            int orient = p->flags&3;
            #define SETTEXCOORDS(u1, u2, v1, v2) \
            do { \
                vs[orient].u       = u1; \
                vs[orient].v       = v2; \
                vs[(orient+1)&3].u = u2; \
                vs[(orient+1)&3].v = v2; \
                vs[(orient+2)&3].u = u2; \
                vs[(orient+2)&3].v = v1; \
                vs[(orient+3)&3].u = u1; \
                vs[(orient+3)&3].v = v1; \
            } while(0)    
            if(type&PT_RND4)
            {
                float tx = 0.5f*((p->flags>>2)&1), ty = 0.5f*((p->flags>>3)&1);
                SETTEXCOORDS(tx, tx + 0.5f, ty, ty + 0.5f);
            } 
            else SETTEXCOORDS(0, 1, 0, 1);

            #define SETCOLOR(r, g, b, a) \
            do { \
                uchar col[4] = { r, g, b, a }; \
                loopi(4) memcpy(vs[i].color.v, col, sizeof(col)); \
            } while(0) 
            #define SETMODCOLOR SETCOLOR((p->color[0]*blend)>>8, (p->color[1]*blend)>>8, (p->color[2]*blend)>>8, 255)
            if(type&PT_MOD) SETMODCOLOR;
            else SETCOLOR(p->color[0], p->color[1], p->color[2], blend);
        }
        else if(type&PT_MOD) SETMODCOLOR;
        else loopi(4) vs[i].alpha = blend;

        genpos<T>(o, d, p->size, ts, grav, vs); 
    }

    void update()
    {
        if(lastmillis == lastupdate) return;
        lastupdate = lastmillis;
      
        loopi(numparts)
        {
            particle *p = &parts[i];
            partvert *vs = &verts[i*4];
            if(p->fade < 0)
            {
                do 
                {
                    --numparts; 
                    if(numparts <= i) return;
                }
                while(parts[numparts].fade < 0);
                *p = parts[numparts];
                genverts(p, vs, true);
            }
            else genverts(p, vs, (p->flags&0x80)!=0);
        }
    }
    
    void render()
    {   
        if(!tex) tex = textureload(texname);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glVertexPointer(3, GL_FLOAT, sizeof(partvert), &verts->pos);
        glTexCoordPointer(2, GL_FLOAT, sizeof(partvert), &verts->u);
        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(partvert), &verts->color);
        glDrawArrays(GL_QUADS, 0, numparts*4);
    }
};
typedef varenderer<PT_PART> quadrenderer;
typedef varenderer<PT_TAPE> taperenderer;
typedef varenderer<PT_TRAIL> trailrenderer;

#include "depthfx.h"
#include "explosion.h"
#include "lensflare.h"
#include "lightning.h"

struct softquadrenderer : quadrenderer
{
    softquadrenderer(const char *texname, int type, int grav, int collide)
        : quadrenderer(texname, type|PT_SOFT, grav, collide)
    {
    }

    int adddepthfx(vec &bbmin, vec &bbmax)
    {
        if(!depthfxtex.highprecision() && !depthfxtex.emulatehighprecision()) return 0;
        int numsoft = 0;
        loopi(numparts)
        {
            particle &p = parts[i];
            float radius = p.size*SQRT2;
            if(depthfxscissor==2 ? depthfxtex.addscissorbox(p.o, radius) : isvisiblesphere(radius, p.o) < VFC_FOGGED) 
            {
                numsoft++;
                loopk(3)
                {
                    bbmin[k] = min(bbmin[k], p.o[k] - radius);
                    bbmax[k] = max(bbmax[k], p.o[k] + radius);
                }
            }
        }
        return numsoft;
    }
};

static partrenderer *parts[] = 
{
    new quadrenderer("packages/particles/blood.png", PT_PART|PT_MOD|PT_RND4, 2, 1), // blood spats (note: rgb is inverted) 
    new trailrenderer("packages/particles/base.png", PT_TRAIL|PT_LERP,   2, 0), // water, entity
    new quadrenderer("packages/particles/smoke.png", PT_PART,          -20, 0), // slowly rising smoke
    new quadrenderer("packages/particles/smoke.png", PT_PART,          -15, 0), // fast rising smoke          
    new quadrenderer("packages/particles/smoke.png", PT_PART,           20, 0), // slowly sinking smoke
    new quadrenderer("packages/particles/ball1.png", PT_PART|PT_GLARE,  20, 0), // fireball1
    new quadrenderer("packages/particles/ball2.png", PT_PART|PT_GLARE,  20, 0), // fireball2
    new quadrenderer("packages/particles/ball3.png", PT_PART|PT_GLARE,  20, 0), // fireball3
    new taperenderer("packages/particles/flare.jpg", PT_TAPE|PT_GLARE,  0, 0),  // streak
    &lightnings,                                                                // lightning
    &fireballs,                                                                 // explosion fireball
    &noglarefireballs,                                                          // explosion fireball no glare
    new quadrenderer("packages/particles/spark.png", PT_PART|PT_GLARE,   2, 0), // sparks
    new quadrenderer("packages/particles/base.png",  PT_PART|PT_GLARE,  20, 0), // edit mode entities
    &texts,                                                                     // TEXT, NON-MOVING
    &textups,                                                                   // TEXT, floats up
    &meters,                                                                    // METER, NON-MOVING
    &metervs,                                                                   // METER vs., NON-MOVING
    &flares // must be done last
};

void finddepthfxranges()
{
    depthfxmin = vec(1e16f, 1e16f, 1e16f);
    depthfxmax = vec(0, 0, 0);
    numdepthfxranges = fireballs.finddepthfxranges(depthfxowners, depthfxranges, MAXDFXRANGES, depthfxmin, depthfxmax);
    loopk(3)
    {
        depthfxmin[k] -= depthfxmargin;
        depthfxmax[k] += depthfxmargin;
    }
    if(depthfxparts)
    {
        loopi(sizeof(parts)/sizeof(parts[0]))
        {
            partrenderer *p = parts[i];
            if(p->type&PT_SOFT && p->adddepthfx(depthfxmin, depthfxmax))
            {
                if(!numdepthfxranges)
                {
                    numdepthfxranges = 1;
                    depthfxowners[0] = NULL;
                    depthfxranges[0] = 0;
                }
            }
        }
    }              
    if(depthfxscissor<2 && numdepthfxranges>0) depthfxtex.addscissorbox(depthfxmin, depthfxmax);
}
 
VARFP(maxparticles, 10, 4000, 40000, particleinit());

void particleinit() 
{
    if(!particleshader) particleshader = lookupshaderbyname("particle");
    if(!particlenotextureshader) particlenotextureshader = lookupshaderbyname("particlenotexture");
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->init(maxparticles);
}

void clearparticles()
{   
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->reset();
}   

void cleanupparticles()
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->cleanup();
}

void removetrackedparticles(physent *owner)
{
    loopi(sizeof(parts)/sizeof(parts[0])) parts[i]->resettracked(owner);
}

VARP(particleglare, 0, 4, 100);

VAR(debugparticles, 0, 0, 1);

void render_particles(int time)
{
    //want to debug BEFORE the lastpass render (that would delete particles)
    if(debugparticles && !glaring && !reflecting && !refracting) 
    {
        int n = sizeof(parts)/sizeof(parts[0]);
        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        glOrtho(0, FONTH*n*2, FONTH*n*2, 0, -1, 1); //squeeze into top-left corner        
        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        defaultshader->set();
        loopi(n) 
        {
            int type = parts[i]->type;
            const char *title = parts[i]->texname ? strrchr(parts[i]->texname, '/')+1 : NULL;
            string info = "";
            if(type&PT_GLARE) s_strcat(info, "g,");
            if(type&PT_LERP) s_strcat(info, "l,");
            if(type&PT_MOD) s_strcat(info, "m,");
            if(type&PT_RND4) s_strcat(info, "r,");
            if(type&PT_TRACK) s_strcat(info, "t,");
            if(parts[i]->collide) s_strcat(info, "c,");
            s_sprintfd(ds)("%d\t%s(%s%d) %s", parts[i]->count(), partnames[type&0xFF], info, parts[i]->grav, (title?title:""));
            draw_text(ds, FONTH, (i+n/2)*FONTH);
        }
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();
        glMatrixMode(GL_MODELVIEW);
        glPopMatrix();
    }

    if(glaring && !particleglare) return;
    
    loopi(sizeof(parts)/sizeof(parts[0])) 
    {
        if(glaring && !(parts[i]->type&PT_GLARE)) continue;
        parts[i]->update();
    }
    
    static float zerofog[4] = { 0, 0, 0, 1 };
    float oldfogc[4];
    bool rendered = false;
    uint lastflags = PT_LERP, flagmask = PT_LERP|PT_MOD;
   
    if(binddepthfxtex()) flagmask |= PT_SOFT;

    loopi(sizeof(parts)/sizeof(parts[0]))
    {
        partrenderer *p = parts[i];
        if(glaring && !(p->type&PT_GLARE)) continue;
        if(!p->haswork()) continue;
    
        if(!rendered)
        {
            rendered = true;
            glDepthMask(GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);             

            if(glaring) setenvparamf("colorscale", SHPARAM_VERTEX, 4, particleglare, particleglare, particleglare, 1);
            else setenvparamf("colorscale", SHPARAM_VERTEX, 4, 1, 1, 1, 1);

            particleshader->set();
            glGetFloatv(GL_FOG_COLOR, oldfogc);
        }
        
        uint flags = p->type & flagmask;
        if(p->usesvertexarray()) flags |= 0x01; //0x01 = VA marker
        uint changedbits = (flags ^ lastflags);
        if(changedbits != 0x0000)
        {
            if(changedbits&0x01)
            {
                if(flags&0x01)
                {
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glEnableClientState(GL_COLOR_ARRAY);
                } 
                else
                {
                    glDisableClientState(GL_VERTEX_ARRAY);
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    glDisableClientState(GL_COLOR_ARRAY);
                }
            }
            if(changedbits&PT_LERP) glFogfv(GL_FOG_COLOR, (flags&PT_LERP) ? oldfogc : zerofog);
            if(changedbits&(PT_LERP|PT_MOD))
            {
                if(flags&PT_LERP) glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                else if(flags&PT_MOD) glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
                else glBlendFunc(GL_SRC_ALPHA, GL_ONE);
            }
            if(changedbits&PT_SOFT)
            {
                if(flags&PT_SOFT)
                {
                    if(depthfxtex.target==GL_TEXTURE_RECTANGLE_ARB)
                    {
                        if(!depthfxtex.highprecision()) SETSHADER(particlesoft8rect);
                        else SETSHADER(particlesoftrect);
                    }
                    else
                    {
                        if(!depthfxtex.highprecision()) SETSHADER(particlesoft8);
                        else SETSHADER(particlesoft);
                    }

                    binddepthfxparams(depthfxpartblend);
                }
                else particleshader->set();
            }
            lastflags = flags;        
        }
        p->render();
    }

    if(rendered)
    {
        if(lastflags&(PT_LERP|PT_MOD)) glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        if(!(lastflags&PT_LERP)) glFogfv(GL_FOG_COLOR, oldfogc);
        if(lastflags&0x01)
        {
            glDisableClientState(GL_VERTEX_ARRAY);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glDisableClientState(GL_COLOR_ARRAY);
        }
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
    }
}

static inline particle *newparticle(const vec &o, const vec &d, int fade, int type, int color, float size)
{
    return parts[type]->addpart(o, d, fade, color, size);
}

VARP(maxparticledistance, 256, 1024, 4096);

static void splash(int type, int color, int radius, int num, int fade, const vec &p, float size)
{
    if(camera1->o.dist(p) > maxparticledistance) return;
    float collidez = parts[type]->collide ? p.z - raycube(p, vec(0, 0, -1), COLLIDERADIUS, RAY_CLIPMAT) + COLLIDEERROR : -1; 
    int fmin = 1;
    int fmax = fade*3;
    loopi(num)
    {
        int x, y, z;
        do
        {
            x = rnd(radius*2)-radius;
            y = rnd(radius*2)-radius;
            z = rnd(radius*2)-radius;
        }
        while(x*x+y*y+z*z>radius*radius);
    	vec tmp = vec((float)x, (float)y, (float)z);
        int f = (num < 10) ? (fmin + rnd(fmax)) : (fmax - (i*(fmax-fmin))/(num-1)); //help deallocater by using fade distribution rather than random
        newparticle(p, tmp, f, type, color, size)->val = collidez;
    }
}

static void regularsplash(int type, int color, int radius, int num, int fade, const vec &p, float size, int delay = 0) 
{
    if(!emit_particles() || (delay > 0 && rnd(delay) != 0)) return;
    splash(type, color, radius, num, fade, p, size);
}

void regular_particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius, int delay) 
{
    if(shadowmapping || renderedgame) return;
    regularsplash(type, color, radius, num, fade, p, size, delay);
}

void particle_splash(int type, int num, int fade, const vec &p, int color, float size, int radius) 
{
    if(shadowmapping || renderedgame) return;
    splash(type, color, radius, num, fade, p, size);
}

VARP(maxtrail, 1, 500, 10000);

void particle_trail(int type, int fade, const vec &s, const vec &e, int color, float size)
{
    if(shadowmapping || renderedgame) return;
    vec v;
    float d = e.dist(s, v);
    int steps = clamp(int(d*2), 1, maxtrail);
    v.div(steps);
    vec p = s;
    loopi(steps)
    {
        p.add(v);
        vec tmp = vec(float(rnd(11)-5), float(rnd(11)-5), float(rnd(11)-5));
        newparticle(p, tmp, rnd(fade)+fade, type, color, size);
    }
}

VARP(particletext, 0, 1, 1);
VARP(maxparticletextdistance, 0, 128, 10000);

void particle_text(const vec &s, const char *t, int type, int fade, int color, float size)
{
    if(shadowmapping || renderedgame) return;
    if(!particletext || camera1->o.dist(s) > maxparticletextdistance) return;
    if(t[0]=='@') t = newstring(t);
    newparticle(s, vec(0, 0, 1), fade, type, color, size)->text = t;
}

void particle_meter(const vec &s, float val, int type, int fade, int color, int color2, float size)
{
    if(shadowmapping || renderedgame) return;
    particle *p = newparticle(s, vec(0, 0, 1), fade, type, color, size);
    p->color2[0] = color2>>16;
    p->color2[1] = (color2>>8)&0xFF;
    p->color2[2] = color2&0xFF;
    p->progress = clamp(int(val*100), 0, 100);
}

void particle_flare(const vec &p, const vec &dest, int fade, int type, int color, float size, physent *owner)
{
    if(shadowmapping || renderedgame) return;
    newparticle(p, dest, fade, type, color, size)->owner = owner;
}

void particle_fireball(const vec &dest, float maxsize, int type, int fade, int color, float size)
{
    if(shadowmapping || renderedgame) return;
    float growth = maxsize - size;
    if(fade < 0) fade = int(growth*25);
    newparticle(dest, vec(0, 0, 1), fade, type, color, size)->val = growth;
}

//dir = 0..6 where 0=up
static inline vec offsetvec(vec o, int dir, int dist) 
{
    vec v = vec(o);    
    v[(2+dir)%3] += (dir>2)?(-dist):dist;
    return v;
}

//converts a 16bit color to 24bit
static inline int colorfromattr(int attr) 
{
    return (((attr&0xF)<<4) | ((attr&0xF0)<<8) | ((attr&0xF00)<<12)) + 0x0F0F0F;
}

/* Experiments in shapes...
 * dir: (where dir%3 is similar to offsetvec with 0=up)
 * 0..2 circle
 * 3.. 5 cylinder shell
 * 6..11 cone shell
 * 12..14 plane volume
 * 15..20 line volume, i.e. wall
 * 21 sphere
 * +32 to inverse direction
 */
void regularshape(int type, int radius, int color, int dir, int num, int fade, const vec &p, float size)
{
    if(!emit_particles()) return;
    
    int basetype = parts[type]->type&0xFF;
    bool flare = (basetype == PT_TAPE) || (basetype == PT_LIGHTNING),
         inv = (dir&0x20)!=0, taper = (dir&0x40)!=0;
    dir &= 0x1F;
    loopi(num)
    {
        vec to, from;
        if(dir < 12) 
        { 
            float a = PI2*float(rnd(1000))/1000.0;
            to[dir%3] = sinf(a)*radius;
            to[(dir+1)%3] = cosf(a)*radius;
            to[(dir+2)%3] = 0.0;
            to.add(p);
            if(dir < 3) //circle
                from = p;
            else if(dir < 6) //cylinder
            {
                from = to;
                to[(dir+2)%3] += radius;
                from[(dir+2)%3] -= radius;
            }
            else //cone
            {
                from = p;
                to[(dir+2)%3] += (dir < 9)?radius:(-radius);
            }
        }
        else if(dir < 15) //plane
        { 
            to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            to[(dir+2)%3] = radius;
            to.add(p);
            from = to;
            from[(dir+2)%3] -= 2*radius;
        }
        else if(dir < 21) //line
        {
            if(dir < 18) 
            {
                to[dir%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
                to[(dir+1)%3] = 0.0;
            } 
            else 
            {
                to[dir%3] = 0.0;
                to[(dir+1)%3] = float(rnd(radius<<4)-(radius<<3))/8.0;
            }
            to[(dir+2)%3] = 0.0;
            to.add(p);
            from = to;
            to[(dir+2)%3] += radius;  
        } 
        else //sphere
        {   
            to = vec(PI2*float(rnd(1000))/1000.0, PI*float(rnd(1000)-500)/1000.0).mul(radius); 
            to.add(p);
            from = p;
        }
       
        if(taper)
        {
            vec o = inv ? to : from;
            o.sub(camera1->o);
            float dist = clamp(sqrtf(o.x*o.x + o.y*o.y)/maxparticledistance, 0.0f, 1.0f);
            if(dist > 0.2f)
            {
                dist = 1 - (dist - 0.2f)/0.8f;
                if(rnd(0x10000) > dist*dist*0xFFFF) continue;
            }
        }
 
        if(flare)
            newparticle(inv?to:from, inv?from:to, rnd(fade*3)+1, type, color, size);
        else 
        {  
            vec d(to);
            d.sub(from);
            d.normalize().mul(inv ? -200.0f : 200.0f); //velocity
            newparticle(inv?to:from, d, rnd(fade*3)+1, type, color, size);
        }
    }
}

static void makeparticles(entity &e) 
{
    switch(e.attr1)
    {
        case 0: //fire
            regularsplash(PART_FIREBALL1, 0xFFC8C8, 150, 1, 40, e.o, 4.8);
            regularsplash(PART_SMOKE_RISE_SLOW, 0x897661, 50, 1, 200,  vec(e.o.x, e.o.y, e.o.z+3.0), 2.4, 3);
            break;
        case 1: //smoke vent - <dir>
            regularsplash(PART_SMOKE_RISE_SLOW, 0x897661, 50, 1, 200,  offsetvec(e.o, e.attr2, rnd(10)), 2.4);
            break;
        case 2: //water fountain - <dir>
        {
            uchar col[3];
            getwatercolour(col);
            int color = (col[0]<<16) | (col[1]<<8) | col[2];
            regularsplash(PART_WATER, color, 150, 4, 200, offsetvec(e.o, e.attr2, rnd(10)), 0.6);
            break;
        }
        case 3: //fire ball - <size> <rgb>
            newparticle(e.o, vec(0, 0, 1), 1, PART_EXPLOSION, colorfromattr(e.attr3), 4.0)->val = 1+e.attr2;
            break;
        case 4:  //tape - <dir> <length> <rgb>
        case 7:  //lightning 
        case 8:  //fire
        case 9:  //smoke
        case 10: //water
        {
            const int typemap[]   = { PART_STREAK, -1, -1, PART_LIGHTNING, PART_FIREBALL1, PART_SMOKE_RISE_SLOW, PART_WATER };
            const float sizemap[] = { 0.28, 0.0, 0.0, 0.28, 4.8, 2.4, 0.60 };
            int type = typemap[e.attr1-4];
            float size = sizemap[e.attr1-4];
            if(e.attr2 >= 256) regularshape(type, 1+e.attr3, colorfromattr(e.attr4), e.attr2-256, 5, 200, e.o, size);
            else newparticle(e.o, offsetvec(e.o, e.attr2, 1+e.attr3), 1, type, colorfromattr(e.attr4), size);
            break;
        }
        case 5: //meter, metervs - <percent> <rgb> <rgb2>
        case 6:
        {
            particle *p = newparticle(e.o, vec(0, 0, 1), 1, e.attr1==5 ? PART_METER : PART_METER_VS, colorfromattr(e.attr3), 2.0);
            int color2 = colorfromattr(e.attr4);
            p->color2[0] = color2>>16;
            p->color2[1] = (color2>>8)&0xFF;
            p->color2[2] = color2&0xFF;
            p->progress = clamp(int(e.attr2), 0, 100);
            break;
        }

        case 32: //lens flares - plain/sparkle/sun/sparklesun <red> <green> <blue>
        case 33:
        case 34:
        case 35:
            flares.addflare(e.o, e.attr2, e.attr3, e.attr4, (e.attr1&0x02)!=0, (e.attr1&0x01)!=0);
            break;
        default:
            s_sprintfd(ds)("@particles %d?", e.attr1);
            particle_text(e.o, ds, PART_TEXT, 1, 0x6496FF, 2.0f);
    }
}

void entity_particles()
{
    if(lastmillis - lastemitframe >= emitmillis)
    {
        emit = true;
        lastemitframe = lastmillis - (lastmillis%emitmillis);
    }
    else emit = false;
   
    flares.makelightflares();
 
    const vector<extentity *> &ents = et->getents();
    if(!editmode) 
    {
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type != ET_PARTICLES || e.o.dist(camera1->o) > maxparticledistance) continue;
            makeparticles(e);
        }
    }
    else // show sparkly thingies for map entities in edit mode
    {
        // note: order matters in this case as particles of the same type are drawn in the reverse order that they are added
        loopv(entgroup)
        {
            entity &e = *ents[entgroup[i]];
            particle_text(e.o, entname(e), PART_TEXT, 1, 0xFF4B19, 2.0f);
        }
        loopv(ents)
        {
            entity &e = *ents[i];
            if(e.type==ET_EMPTY) continue;
            particle_text(e.o, entname(e), PART_TEXT, 1, 0x1EC850, 2.0f);
            regular_particle_splash(PART_EDIT, 2, 40, e.o, 0x3232FF, 0.32f*particlesize/100.0f);
        }
    }
}

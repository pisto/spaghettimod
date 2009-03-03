// world.cpp: core map management stuff

#include "cube.h"
#include "engine.h"

header hdr;
int worldscale;

VAR(octaentsize, 0, 128, 1024);
VAR(entselradius, 0, 2, 10);

bool getentboundingbox(extentity &e, ivec &o, ivec &r)
{
    switch(e.type)
    {
        case ET_EMPTY:
            return false;
        case ET_MAPMODEL:
        {
            model *m = loadmodel(NULL, e.attr2);
            if(m)
            {
                vec center, radius;
                m->boundbox(0, center, radius);
                rotatebb(center, radius, e.attr1);

                o = e.o;
                o.add(center);
                r = radius;
                r.add(1);
                o.sub(r);
                r.mul(2);
                break;
            }
        }
        // invisible mapmodels use entselradius
        default:
            o = e.o;
            o.sub(entselradius);
            r.x = r.y = r.z = entselradius*2;
            break;
    }
    return true;
}

enum
{
    MODOE_ADD      = 1<<0,
    MODOE_UPDATEBB = 1<<1
};

void modifyoctaentity(int flags, int id, cube *c, const ivec &cor, int size, const ivec &bo, const ivec &br, int leafsize, vtxarray *lastva = NULL)
{
    loopoctabox(cor, size, bo, br)
    {
        ivec o(i, cor.x, cor.y, cor.z, size);
        vtxarray *va = c[i].ext && c[i].ext->va ? c[i].ext->va : lastva;
        if(c[i].children != NULL && size > leafsize)
            modifyoctaentity(flags, id, c[i].children, o, size>>1, bo, br, leafsize, va);
        else if(flags&MODOE_ADD)
        {
            if(!c[i].ext || !c[i].ext->ents) ext(c[i]).ents = new octaentities(o, size);
            octaentities &oe = *c[i].ext->ents;
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(oe.mapmodels.empty()) 
                            {
                                if(!va->mapmodels) va->mapmodels = new vector<octaentities *>;
                                va->mapmodels->add(&oe);
                            }
                        }
                        oe.mapmodels.add(id);
                        loopk(3)
                        {
                            oe.bbmin[k] = min(oe.bbmin[k], max(oe.o[k], bo[k]));
                            oe.bbmax[k] = max(oe.bbmax[k], min(oe.o[k]+size, bo[k]+br[k]));
                        }
                        break;
                    }
                    // invisible mapmodel
                default:
                    oe.other.add(id);
                    break;
            }

        }
        else if(c[i].ext && c[i].ext->ents)
        {
            octaentities &oe = *c[i].ext->ents;
            switch(et->getents()[id]->type)
            {
                case ET_MAPMODEL:
                    if(loadmodel(NULL, et->getents()[id]->attr2))
                    {
                        oe.mapmodels.removeobj(id);
                        if(va)
                        {
                            va->bbmin.x = -1;
                            if(va->mapmodels && oe.mapmodels.empty())
                            {
                                va->mapmodels->removeobj(&oe);
                                if(va->mapmodels->empty()) DELETEP(va->mapmodels);
                            }
                        }
                        oe.bbmin = oe.bbmax = oe.o;
                        oe.bbmin.add(oe.size);
                        loopvj(oe.mapmodels)
                        {
                            extentity &e = *et->getents()[oe.mapmodels[j]];
                            ivec eo, er;
                            if(getentboundingbox(e, eo, er)) loopk(3)
                            {
                                oe.bbmin[k] = min(oe.bbmin[k], eo[k]);
                                oe.bbmax[k] = max(oe.bbmax[k], eo[k]+er[k]);
                            }
                        }
                        loopk(3)
                        {
                            oe.bbmin[k] = max(oe.bbmin[k], oe.o[k]);
                            oe.bbmax[k] = min(oe.bbmax[k], oe.o[k]+size);
                        }
                        break;
                    }
                    // invisible mapmodel
                default:
                    oe.other.removeobj(id);
                    break;
            }
            if(oe.mapmodels.empty() && oe.other.empty()) 
                freeoctaentities(c[i]);
        }
        if(c[i].ext && c[i].ext->ents) c[i].ext->ents->query = NULL;
        if(va && va!=lastva)
        {
            if(lastva)
            {
                if(va->bbmin.x < 0) lastva->bbmin.x = -1;
            }
            else if(flags&MODOE_UPDATEBB) updatevabb(va);
        }
    }
}

static void modifyoctaent(int flags, int id)
{
    vector<extentity *> &ents = et->getents();
    if(!ents.inrange(id)) return;
    ivec o, r;
    extentity &e = *ents[id];
    if((e.inoctanode!=0)==flags || !getentboundingbox(e, o, r)) return;

    int leafsize = octaentsize, limit = max(r.x, max(r.y, r.z));
    while(leafsize < limit) leafsize *= 2;
    int diff = ~(leafsize-1) & ((o.x^(o.x+r.x))|(o.y^(o.y+r.y))|(o.z^(o.z+r.z)));
    if(diff && (limit > octaentsize/2 || diff < leafsize*2)) leafsize *= 2;

    e.inoctanode = flags&MODOE_ADD ? 1 : 0;
    modifyoctaentity(flags, id, worldroot, ivec(0, 0, 0), hdr.worldsize>>1, o, r, leafsize);
    if(e.type == ET_LIGHT) clearlightcache(id);
    else if(flags&MODOE_ADD) lightent(e);
}

static inline void addentity(int id)    { modifyoctaent(MODOE_ADD|MODOE_UPDATEBB, id); }
static inline void removeentity(int id) { modifyoctaent(MODOE_UPDATEBB, id); }

void freeoctaentities(cube &c)
{
    if(!c.ext) return;
    if(et->getents().length())
    {
        while(c.ext->ents && !c.ext->ents->mapmodels.empty()) removeentity(c.ext->ents->mapmodels.pop());
        while(c.ext->ents && !c.ext->ents->other.empty())     removeentity(c.ext->ents->other.pop());
    }
    if(c.ext->ents)
    {
        delete c.ext->ents;
        c.ext->ents = NULL;
    }
}

void entitiesinoctanodes()
{
    loopv(et->getents()) modifyoctaent(MODOE_ADD, i);
}

char *entname(entity &e)
{
    static string fullentname;
    s_strcpy(fullentname, "@");
    s_strcat(fullentname, et->entname(e.type));
    const char *einfo = et->entnameinfo(e);
    if(*einfo)
    {
        s_strcat(fullentname, ": ");
        s_strcat(fullentname, einfo);
    }
    return fullentname;
}

extern selinfo sel;
extern bool havesel, selectcorners;
int entlooplevel = 0;
int efocus = -1, enthover = -1, entorient = -1, oldhover = -1;
bool undonext = true;

VAR(entediting, 0, 0, 1);

bool noentedit()
{
    if(!editmode) { conoutf(CON_ERROR, "operation only allowed in edit mode"); return true; }
    return !entediting;
}

bool pointinsel(selinfo &sel, vec &o)
{
    return(o.x <= sel.o.x+sel.s.x*sel.grid
        && o.x >= sel.o.x
        && o.y <= sel.o.y+sel.s.y*sel.grid
        && o.y >= sel.o.y
        && o.z <= sel.o.z+sel.s.z*sel.grid
        && o.z >= sel.o.z);
}

vector<int> entgroup;

bool haveselent()
{
    return entgroup.length() > 0;
}

void entcancel()
{
    entgroup.setsize(0);
}

void entadd(int id)
{
    undonext = true;
    entgroup.add(id);
}

undoblock *newundoent()
{
    int numents = entgroup.length();
    if(numents <= 0) return NULL;
    undoblock *u = (undoblock *)new uchar[sizeof(undoblock) + numents*sizeof(undoent)];
    u->numents = numents;
    undoent *e = (undoent *)(u + 1);
    loopv(entgroup)
    {
        e->i = entgroup[i];
        e->e = *et->getents()[entgroup[i]];
        e++;
    }
    return u;
}

void makeundoent()
{
    if(!undonext) return;
    undonext = false;
    oldhover = enthover;
    undoblock *u = newundoent();
    if(u) addundo(u);
}

void detachentity(extentity &e)
{
    if(!e.attached) return;
    e.attached->attached = NULL;
    e.attached = NULL;
}

VAR(attachradius, 1, 100, 1000);

void attachentity(extentity &e)
{
    switch(e.type)
    {
        case ET_SPOTLIGHT:
            break;

        default:
            if(e.type<ET_GAMESPECIFIC || !et->mayattach(e)) return;
            break;
    }

    detachentity(e);

    vector<extentity *> &ents = et->getents();
    int closest = -1;
    float closedist = 1e10f;
    loopv(ents)
    {
        extentity *a = ents[i];
        if(a->attached) continue;
        switch(e.type)
        {
            case ET_SPOTLIGHT: 
                if(a->type!=ET_LIGHT) continue; 
                break;

            default:
                if(e.type<ET_GAMESPECIFIC || !et->attachent(e, *a)) continue;
                break;
        }
        float dist = e.o.dist(a->o);
        if(dist < closedist)
        {
            closest = i;
            closedist = dist;
        }
    }
    if(closedist>attachradius) return;
    e.attached = ents[closest];
    ents[closest]->attached = &e;
}

void attachentities()
{
    vector<extentity *> &ents = et->getents();
    loopv(ents) attachentity(*ents[i]);
}

// convenience macros implicitly define:
// e         entity, currently edited ent
// n         int,    index to currently edited ent
#define addimplicit(f)  { if(entgroup.empty() && enthover>=0) { entadd(enthover); undonext = (enthover != oldhover); f; entgroup.drop(); } else f; }
#define entfocus(i, f)  { int n = efocus = (i); if(n>=0) { extentity &e = *et->getents()[n]; f; } }
#define entedit(i, f) \
{ \
    entfocus(i, \
    int oldtype = e.type; \
    removeentity(n);  \
    f; \
    if(oldtype!=e.type) detachentity(e); \
    if(e.type!=ET_EMPTY) { addentity(n); if(oldtype!=e.type) attachentity(e); } \
    et->editent(n)); \
}
#define addgroup(exp)   { loopv(et->getents()) entfocus(i, if(exp) entadd(n)); }
#define setgroup(exp)   { entcancel(); addgroup(exp); }
#define groupeditloop(f){ entlooplevel++; int _ = efocus; loopv(entgroup) entedit(entgroup[i], f); efocus = _; entlooplevel--; }
#define groupeditpure(f){ if(entlooplevel>0) { entedit(efocus, f); } else groupeditloop(f); }
#define groupeditundo(f){ makeundoent(); groupeditpure(f); }
#define groupedit(f)    { addimplicit(groupeditundo(f)); }

undoblock *copyundoents(undoblock *u)
{
    entcancel();
    undoent *e = u->ents();
    loopi(u->numents)
        entadd(e[i].i);
    undoblock *c = newundoent();
   	loopi(u->numents) if(e[i].e.type==ET_EMPTY)
		entgroup.removeobj(e[i].i);
    return c;
}

void pasteundoents(undoblock *u)
{
    undoent *ue = u->ents();
    loopi(u->numents)
        entedit(ue[i].i, (entity &)e = ue[i].e);
}

void entflip()
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    float mid = sel.s[d]*sel.grid/2+sel.o[d];
    groupeditundo(e.o[d] -= (e.o[d]-mid)*2);
}

void entrotate(int *cw)
{
    if(noentedit()) return;
    int d = dimension(sel.orient);
    int dd = (*cw<0) == dimcoord(sel.orient) ? R[d] : C[d];
    float mid = sel.s[dd]*sel.grid/2+sel.o[dd];
    vec s(sel.o.v);
    groupeditundo(
        e.o[dd] -= (e.o[dd]-mid)*2;
        e.o.sub(s);
        swap(e.o[R[d]], e.o[C[d]]);
        e.o.add(s);
    );
}

void entselectionbox(const entity &e, vec &eo, vec &es) 
{
    model *m = NULL;
    if(e.type == ET_MAPMODEL && (m = loadmodel(NULL, e.attr2)))
    {
        m->collisionbox(0, eo, es);
        rotatebb(eo, es, e.attr1);
        if(m->collide)
            eo.z -= player->aboveeye; // wacky but true. see physics collide                    
        else
            es.div(2);  // cause the usual bb is too big...
        eo.add(e.o);
    }   
    else
    {
        es = vec(entselradius);
        eo = e.o;
    }    
    eo.sub(es);
    es.mul(2);
}

VAR(entselsnap, 0, 0, 1);
VAR(entmovingshadow, 0, 1, 1);

extern void boxs(int orient, vec o, const vec &s);
extern void boxs3D(const vec &o, vec s, int g);
extern void editmoveplane(const vec &o, const vec &ray, int d, float off, vec &handle, vec &dest, bool first);

bool initentdragging = true;

void entdrag(const vec &ray)
{
    if(noentedit() || !haveselent()) return;

    float r = 0, c = 0;
    static vec v, handle;
    vec eo, es;
    int d = dimension(entorient),
        dc= dimcoord(entorient);

    entfocus(entgroup.last(),        
        entselectionbox(e, eo, es);

        editmoveplane(e.o, ray, d, eo[d] + (dc ? es[d] : 0), handle, v, initentdragging);        

        ivec g(v);
        int z = g[d]&(~(sel.grid-1));
        g.add(sel.grid/2).mask(~(sel.grid-1));
        g[d] = z;
        
        r = (entselsnap ? g[R[d]] : v[R[d]]) - e.o[R[d]];
        c = (entselsnap ? g[C[d]] : v[C[d]]) - e.o[C[d]];       
    );

    if(initentdragging) makeundoent();
    groupeditpure(e.o[R[d]] += r; e.o[C[d]] += c);
    initentdragging = false;
}

VAR(showentradius, 0, 1, 1);

void renderentradius(extentity &e)
{
    if(!showentradius) return;
    float radius = 0.0f, angle = 0.0f, ring = 0.0f;
    vec dir(0, 0, 0);
    float color[3] = {0, 1, 1};
    switch(e.type)
    {
        case ET_LIGHT:
            radius = e.attr1;
            color[0] = e.attr2/255.0f;
            color[1] = e.attr3/255.0f;
            color[2] = e.attr4/255.0f;
            break;

        case ET_SPOTLIGHT:
            if(e.attached)
            {
                radius = e.attached->attr1;
                if(!radius) radius = 2*e.o.dist(e.attached->o);
                dir = vec(e.o).sub(e.attached->o).normalize();
                angle = max(1, min(90, int(e.attr1)));
            }
            break;

        case ET_SOUND:
            radius = e.attr2;
            break;

        case ET_ENVMAP:
        {
            extern int envmapradius;
            radius = e.attr1 ? max(0, min(10000, int(e.attr1))) : envmapradius;
            break;
        }

        case ET_MAPMODEL:
        case ET_PLAYERSTART:
            radius = 4;
            if(e.type==ET_MAPMODEL && e.attr3) ring = checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12;
            vecfromyawpitch(e.attr1, 0, 1, 0, dir);
            break;

        default:
            if(e.type>=ET_GAMESPECIFIC) et->entradius(e, radius, angle, dir);
            break;
    }
    if(radius<=0) return;
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    loopj(2)
    {
        if(!j)
        {
            glDepthFunc(GL_GREATER);
            glColor3f(0.25f, 0.25f, 0.25f);
        }
        else 
        {
            glDepthFunc(GL_LESS);
            glColor3fv(color);
        }
        if(e.attached)
        {
            glBegin(GL_LINES);
            glVertex3fv(e.o.v);
            glVertex3fv(e.attached->o.v);
            glEnd();
        }
        if(ring)
        {
            glBegin(GL_LINE_LOOP);
            loopi(16)
            {
                vec p(e.o);
                p.x += ring*cosf(2*M_PI*i/16.0f);
                p.y += ring*sinf(2*M_PI*i/16.0f);
                glVertex3fv(p.v);
            }
            glEnd();
        }
        if(dir.iszero()) loopk(3)
        {
            glBegin(GL_LINE_LOOP);
            loopi(16)
            {
                vec p(e.o);
                p[k>=2 ? 1 : 0] += radius*cosf(2*M_PI*i/16.0f);
                p[k>=1 ? 2 : 1] += radius*sinf(2*M_PI*i/16.0f);
                glVertex3fv(p.v);
            }
            glEnd();
        }
        else if(!angle)
        {
            float arrowsize = min(radius/8, 0.5f);
            vec target(vec(dir).mul(radius).add(e.o)), arrowbase(vec(dir).mul(radius - arrowsize).add(e.o)), spoke;
            spoke.orthogonal(dir);
            spoke.normalize();
            spoke.mul(arrowsize);
            glBegin(GL_LINES);
            glVertex3fv(e.o.v);
            glVertex3fv(target.v);
            glEnd();
            glBegin(GL_TRIANGLE_FAN);
            glVertex3fv(target.v);
            loopi(5)
            {
                vec p(spoke);
                p.rotate(2*M_PI*i/4.0f, dir);
                p.add(arrowbase);
                glVertex3fv(p.v);
            }
            glEnd();
        }
        else
        {
            vec spot(vec(dir).mul(radius*cosf(angle*RAD)).add(e.attached->o)), spoke;
            spoke.orthogonal(dir);
            spoke.normalize();
            spoke.mul(radius*sinf(angle*RAD));
            glBegin(GL_LINES);
            loopi(8)
            {
                vec p(spoke);
                p.rotate(2*M_PI*i/8.0f, dir);
                p.add(spot);
                glVertex3fv(e.attached->o.v);
                glVertex3fv(p.v);
            }
            glEnd();
            glBegin(GL_LINE_LOOP);
            loopi(8)
            {
                vec p(spoke);
                p.rotate(2*M_PI*i/8.0f, dir);
                p.add(spot);
                glVertex3fv(p.v);
            }
            glEnd();
        }
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
}

void renderentselection(const vec &o, const vec &ray, bool entmoving)
{   
    if(noentedit()) return;
    vec eo, es;

    glColor3ub(0, 40, 0);
    loopv(entgroup) entfocus(entgroup[i],     
        entselectionbox(e, eo, es);
        boxs3D(eo, es, 1);
    );

    if(enthover >= 0)
    {
        entfocus(enthover, entselectionbox(e, eo, es)); // also ensures enthover is back in focus
        boxs3D(eo, es, 1);
        if(entmoving && entmovingshadow==1)
        {
            vec a, b;
            glColor3ub(20, 20, 20);
            (a=eo).x=0; (b=es).x=hdr.worldsize; boxs3D(a, b, 1);  
            (a=eo).y=0; (b=es).y=hdr.worldsize; boxs3D(a, b, 1);  
            (a=eo).z=0; (b=es).z=hdr.worldsize; boxs3D(a, b, 1);
        }
        glColor3ub(150,0,0);
        glLineWidth(5);
        boxs(entorient, eo, es);
        glLineWidth(1);
    }

    loopv(entgroup) entfocus(entgroup[i], renderentradius(e));
    if(enthover>=0) entfocus(enthover, renderentradius(e));
}

bool enttoggle(int id)
{
    undonext = true;
    int i = entgroup.find(id);
    if(i < 0)
        entadd(id);
    else
        entgroup.remove(i);
    return i < 0;
}

bool hoveringonent(int ent, int orient)
{
    if(noentedit()) return false;
    entorient = orient;
    if((efocus = enthover = ent) >= 0)
        return true;
    efocus   = entgroup.empty() ? -1 : entgroup.last();
    enthover = -1;
    return false;
}

VAR(entitysurf, 0, 0, 1);
VARF(entmoving, 0, 0, 2,
    if(enthover < 0 || noentedit())
        entmoving = 0;
    else if(entmoving == 1)
        entmoving = enttoggle(enthover);
    else if(entmoving == 2 && entgroup.find(enthover) < 0)
        entadd(enthover);
    if(entmoving > 0)
        initentdragging = true;
);

void entpush(int *dir)
{
    if(noentedit()) return;
    int d = dimension(entorient);
    int s = dimcoord(entorient) ? -*dir : *dir;
    if(entmoving) 
    {
        groupeditpure(e.o[d] += float(s*sel.grid)); // editdrag supplies the undo
    }
    else 
        groupedit(e.o[d] += float(s*sel.grid));
    if(entitysurf==1)
        player->o[d] += float(s*sel.grid);
}

VAR(entautoviewdist, 0, 25, 100);
void entautoview(int *dir) 
{
    if(!haveselent()) return;
    static int s = 0;
    vec v(player->o);
    v.sub(worldpos);
    v.normalize();
    v.mul(entautoviewdist);
    int t = s + *dir;
    s = abs(t) % entgroup.length();
    if(t<0 && s>0) s = entgroup.length() - s;
    entfocus(entgroup[s],
        v.add(e.o);
        player->o = v;
        player->resetinterp();
    );
}

COMMAND(entautoview, "i");
COMMAND(entflip, "");
COMMAND(entrotate, "i");
COMMAND(entpush, "i");

void delent()
{
    if(noentedit()) return;
    groupedit(e.type = ET_EMPTY;);
    entcancel();
}

int findtype(char *what)
{
    for(int i = 0; *et->entname(i); i++) if(strcmp(what, et->entname(i))==0) return i;
    conoutf(CON_ERROR, "unknown entity type \"%s\"", what);
    return ET_EMPTY;
}

VAR(entdrop, 0, 2, 3);

bool dropentity(entity &e, int drop = -1)
{
    vec radius(4.0f, 4.0f, 4.0f);
    if(drop<0) drop = entdrop;
    if(e.type == ET_MAPMODEL)
    {
        model *m = loadmodel(NULL, e.attr2);
        if(m)
        {
            vec center;
            m->boundbox(0, center, radius);
            rotatebb(center, radius, e.attr1);
            radius.x += fabs(center.x);
            radius.y += fabs(center.y);
        }
        radius.z = 0.0f;
    }
    switch(drop)
    {
    case 1:
        if(e.type != ET_LIGHT && e.type != ET_SPOTLIGHT)
            dropenttofloor(&e);
        break;
    case 2:
    case 3:
        int cx = 0, cy = 0;
        if(sel.cxs == 1 && sel.cys == 1)
        {
            cx = (sel.cx ? 1 : -1) * sel.grid / 2;
            cy = (sel.cy ? 1 : -1) * sel.grid / 2;
        }
        e.o = sel.o.v;
        int d = dimension(sel.orient), dc = dimcoord(sel.orient);
        e.o[R[d]] += sel.grid / 2 + cx;
        e.o[C[d]] += sel.grid / 2 + cy;
        if(!dc)
            e.o[D[d]] -= radius[D[d]];
        else
            e.o[D[d]] += sel.grid + radius[D[d]];

        if(drop == 3)
            dropenttofloor(&e);
        break;
    }
    return true;
}

void dropent()
{
    if(noentedit()) return;
    groupedit(dropentity(e));
}

void attachent()
{
    if(noentedit()) return;
    groupedit(attachentity(e));
}

COMMAND(attachent, "");

extentity *newentity(bool local, const vec &o, int type, int v1, int v2, int v3, int v4, int v5)
{
    extentity &e = *et->newentity();
    e.o = o;
    e.attr1 = v1;
    e.attr2 = v2;
    e.attr3 = v3;
    e.attr4 = v4;
    e.attr5 = v5;
    e.type = type;
    e.reserved = 0;
    e.spawned = false;
    e.inoctanode = false;
    e.light.color = vec(1, 1, 1);
    e.light.dir = vec(0, 0, 1);
    if(local)
    {
        switch(type)
        {
                case ET_MAPMODEL:
                case ET_PLAYERSTART:
                    e.attr5 = e.attr4;
                    e.attr4 = e.attr3;
                    e.attr3 = e.attr2;
                    e.attr2 = e.attr1;
                    e.attr1 = (int)camera1->yaw;
                    break;
        }
        et->fixentity(e);
    }
    return &e;
}

void newentity(int type, int a1, int a2, int a3, int a4, int a5)
{
    if(et->getents().length() >= MAXENTS) { conoutf("too many entities"); return; }
    extentity *t = newentity(true, player->o, type, a1, a2, a3, a4, a5);
    dropentity(*t);
    et->getents().add(t);
    int i = et->getents().length()-1;
    t->type = ET_EMPTY;
    enttoggle(i);
    makeundoent();
    entedit(i, e.type = type);
}

void newent(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit()) return;
    int type = findtype(what);
    if(type != ET_EMPTY)
        newentity(type, *a1, *a2, *a3, *a4, *a5);
}

int entcopygrid;
vector<entity> entcopybuf;

void entcopy()
{
    if(noentedit()) return;
    entcopygrid = sel.grid;
    entcopybuf.setsize(0);
    loopv(entgroup) 
        entfocus(entgroup[i], entcopybuf.add(e).o.sub(sel.o.v));
}

void entpaste()
{
    if(noentedit()) return;
    if(entcopybuf.length()==0) return;
    entcancel();
    int last = et->getents().length()-1;
    float m = float(sel.grid)/float(entcopygrid);
    loopv(entcopybuf)
    {
        entity &c = entcopybuf[i];
        vec o(c.o);
        o.mul(m).add(sel.o.v);
        extentity *e = newentity(true, o, ET_EMPTY, c.attr1, c.attr2, c.attr3, c.attr4, c.attr5);
        et->getents().add(e);
        entadd(++last);
    }
    int j = 0;
    groupeditundo(e.type = entcopybuf[j++].type;);
}

COMMAND(newent, "siiiii");
COMMAND(delent, "");
COMMAND(dropent, "");
COMMAND(entcopy, "");
COMMAND(entpaste, "");

void entset(char *what, int *a1, int *a2, int *a3, int *a4, int *a5)
{
    if(noentedit()) return;
    int type = findtype(what);
    groupedit(e.type=type;
              e.attr1=*a1;
              e.attr2=*a2;
              e.attr3=*a3;
              e.attr4=*a4;
              e.attr5=*a5);
}

ICOMMAND(enthavesel,"",  (), addimplicit(intret(entgroup.length())));
ICOMMAND(entselect, "s", (char *body), if(!noentedit()) addgroup(e.type != ET_EMPTY && entgroup.find(n)<0 && execute(body)>0));
ICOMMAND(entloop,   "s", (char *body), if(!noentedit()) addimplicit(groupeditloop(((void)e, execute(body)))));
ICOMMAND(insel,     "",  (), entfocus(efocus, intret(pointinsel(sel, e.o))));
ICOMMAND(entget,    "",  (), entfocus(efocus, s_sprintfd(s)("%s %d %d %d %d %d", et->entname(e.type), e.attr1, e.attr2, e.attr3, e.attr4, e.attr5);  result(s)));
ICOMMAND(entindex,  "",  (), intret(efocus));
COMMAND(entset, "siiiii");

int findentity(int type, int index, int attr1, int attr2)
{
    const vector<extentity *> &ents = et->getents();
    for(int i = index; i<ents.length(); i++) 
    {
        extentity &e = *ents[i];
        if(e.type==type && (attr1<0 || e.attr1==attr1) && (attr2<0 || e.attr2==attr2))
            return i;
    }
    loopj(min(index, ents.length())) 
    {
        extentity &e = *ents[j];
        if(e.type==type && (attr1<0 || e.attr1==attr1) && (attr2<0 || e.attr2==attr2))
            return j;
    }
    return -1;
}

int spawncycle = -1, fixspawn = 4;

void findplayerspawn(dynent *d, int forceent, int tag)   // place at random spawn. also used by monsters!
{
    int pick = forceent;
    if(pick<0)
    {
        int r = fixspawn-->0 ? 7 : rnd(10)+1;
        loopi(r) spawncycle = findentity(ET_PLAYERSTART, spawncycle+1, -1, tag);
        pick = spawncycle;
    }
    if(pick!=-1)
    {
        d->pitch = 0;
        d->roll = 0;
        for(int attempt = pick;;)
        {
            d->o = et->getents()[attempt]->o;
            d->yaw = et->getents()[attempt]->attr1;
            if(entinmap(d, true)) break;
            attempt = findentity(ET_PLAYERSTART, attempt+1, -1, tag);
            if(attempt<0 || attempt==pick)
            {
                d->o = et->getents()[attempt]->o;
                d->yaw = et->getents()[attempt]->attr1;
                entinmap(d);
                break;
            }    
        }
    }
    else
    {
        d->o.x = d->o.y = d->o.z = 0.5f*getworldsize();
        d->o.z += 1;
        entinmap(d);
    }
}

void splitocta(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) c[i].children = newcubes(isempty(c[i]) ? F_EMPTY : F_SOLID);
        splitocta(c[i].children, size>>1);
    }
}

void resetmap()
{
    clearoverrides();
    clearmapsounds();
    cleanreflections();
    resetblendmap();
    resetlightmaps();
    clearpvs();
    clearparticles();
    cleardecals();
    clearsleep();
    cancelsel();
    pruneundos();

    setvar("gamespeed", 100);
    setvar("paused", 0);

    et->clearents();
}

void startmap(const char *name)
{
    cl->startmap(name);
}

bool emptymap(int scale, bool force, const char *mname, bool usecfg)    // main empty world creation routine
{
    if(!force && !editmode) 
    {
        conoutf(CON_ERROR, "newmap only allowed in edit mode");
        return false;
    }

    resetmap();

    strncpy(hdr.head, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(header);
    worldscale = scale<10 ? 10 : (scale>20 ? 20 : scale);
    hdr.worldsize = 1<<worldscale;
    
    s_strncpy(hdr.maptitle, "Untitled Map by Unknown", 128);
    memset(hdr.watercolour, 0, sizeof(hdr.watercolour));
    hdr.maple = 8;
    hdr.mapprec = 32;
    hdr.mapllod = 0;
    hdr.numpvs = 0;
    hdr.blendmap = 0;
    hdr.lightmaps = 0;
    memset(hdr.skylight, 0, sizeof(hdr.skylight));
    memset(hdr.reserved, 0, sizeof(hdr.reserved));
    texmru.setsize(0);
    freeocta(worldroot);
    worldroot = newcubes(F_EMPTY);
    loopi(4) solidfaces(worldroot[i]);

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    clearlights();
    allchanged();

    if(usecfg)
    {
        overrideidents = true;
        execfile("data/default_map_settings.cfg");
        overrideidents = false;
    }

    startmap(mname);

    return true;
}

bool enlargemap(bool force)
{
    if(!force && !editmode)
    {
        conoutf(CON_ERROR, "mapenlarge only allowed in edit mode");
        return false;
    }
    if(hdr.worldsize >= 1<<20) return false;

    worldscale++;
    hdr.worldsize *= 2;
    cube *c = newcubes(F_EMPTY);
    c[0].children = worldroot;
    loopi(3) solidfaces(c[i+1]);
    worldroot = c;

    if(hdr.worldsize > VVEC_INT_MASK+1) splitocta(worldroot, hdr.worldsize>>1);

    enlargeblendmap();

    allchanged();

    return true;
}

void newmap(int *i) { if(emptymap(*i, false)) cl->newmap(max(*i, 0)); }
void mapenlarge() { if(enlargemap(false)) cl->newmap(-1); }
COMMAND(newmap, "i");
COMMAND(mapenlarge, "");

void mapname()
{
    result(cl->getclientmap());
}

COMMAND(mapname, "");

void mapsize()
{
    int size = 0;
    while(1<<size < hdr.worldsize) size++;
    intret(size);
}

COMMAND(mapsize, "");

void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local)
{
    if(et->getents().length()<=i)
    {
        if(i >= MAXENTS) return;
        while(et->getents().length()<i) et->getents().add(et->newentity())->type = ET_EMPTY;
        extentity *e = newentity(local, o, type, attr1, attr2, attr3, attr4, attr5);
        et->getents().add(e);
        addentity(i);
        attachentity(*e);
    }
    else
    {
        extentity &e = *et->getents()[i];
        removeentity(i);
        int oldtype = e.type;
        if(oldtype!=type) detachentity(e);
        e.type = type;
        e.o = o;
        e.attr1 = attr1; e.attr2 = attr2; e.attr3 = attr3; e.attr4 = attr4; e.attr5 = attr5;
        addentity(i);
        if(oldtype!=type) attachentity(e);
    }
}

int getworldsize() { return hdr.worldsize; }
int getmapversion() { return hdr.version; }

int triggertypes[NUMTRIGGERTYPES] =
{
    0,
    TRIG_ONCE,
    TRIG_RUMBLE,
    TRIG_TOGGLE,
    TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_MANY,
    TRIG_MANY | TRIG_RUMBLE,
    TRIG_MANY | TRIG_TOGGLE,
    TRIG_MANY | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_AUTO_RESET | TRIG_RUMBLE,
    TRIG_COLLIDE | TRIG_TOGGLE | TRIG_LOCKED | TRIG_RUMBLE,
    TRIG_DISAPPEAR,
    TRIG_DISAPPEAR | TRIG_RUMBLE,
    TRIG_DISAPPEAR | TRIG_COLLIDE | TRIG_LOCKED,
    0 /* reserved */
};

void resettriggers()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        e.triggerstate = TRIGGER_RESET;
        e.lasttrigger = 0;
    }
}

void unlocktriggers(int tag, int oldstate = TRIGGER_RESET, int newstate = TRIGGERING)
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        if(e.attr4 == tag && e.triggerstate == oldstate && checktriggertype(e.attr3, TRIG_LOCKED))
        {
            if(newstate == TRIGGER_RESETTING && checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) continue;
            e.triggerstate = newstate;
            e.lasttrigger = lastmillis;
            if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
        }
    }
}

void trigger(int *tag, int *state)
{
    if(*state) unlocktriggers(*tag);
    else unlocktriggers(*tag, TRIGGERED, TRIGGER_RESETTING);
}

COMMAND(trigger, "ii");

VAR(triggerstate, -1, 0, 1);

void doleveltrigger(int trigger, int state)
{
    s_sprintfd(aliasname)("level_trigger_%d", trigger);
    if(identexists(aliasname))
    {
        triggerstate = state;
        execute(aliasname);
    }
}

void checktriggers()
{
    if(player->state != CS_ALIVE) return;
    vec o(player->o);
    o.z -= player->eyeheight;
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type != ET_MAPMODEL || !e.attr3) continue;
        switch(e.triggerstate)
        {
            case TRIGGERING:
            case TRIGGER_RESETTING:
                if(lastmillis-e.lasttrigger>=1000)
                {
                    if(e.attr4)
                    {
                        if(e.triggerstate == TRIGGERING) unlocktriggers(e.attr4);
                        else unlocktriggers(e.attr4, TRIGGERED, TRIGGER_RESETTING);
                    }
                    if(checktriggertype(e.attr3, TRIG_DISAPPEAR)) e.triggerstate = TRIGGER_DISAPPEARED;
                    else if(e.triggerstate==TRIGGERING && checktriggertype(e.attr3, TRIG_TOGGLE)) e.triggerstate = TRIGGERED;
                    else e.triggerstate = TRIGGER_RESET;
                }
                break;
            case TRIGGER_RESET:
                if(e.lasttrigger)
                {
                    if(checktriggertype(e.attr3, TRIG_AUTO_RESET|TRIG_MANY|TRIG_LOCKED) && e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                        e.lasttrigger = 0;
                    break;
                }
                else if(e.o.dist(o)-player->radius>=(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12)) break;
                else if(checktriggertype(e.attr3, TRIG_LOCKED))
                {
                    if(!e.attr4) break;
                    doleveltrigger(e.attr4, -1);
                    e.lasttrigger = lastmillis;
                    break;
                }
                e.triggerstate = TRIGGERING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 1);
                break;
            case TRIGGERED:
                if(e.o.dist(o)-player->radius<(checktriggertype(e.attr3, TRIG_COLLIDE) ? 20 : 12))
                {
                    if(e.lasttrigger) break;
                }
                else if(checktriggertype(e.attr3, TRIG_AUTO_RESET))
                {
                    if(lastmillis-e.lasttrigger<6000) break;
                }
                else if(checktriggertype(e.attr3, TRIG_MANY))
                {
                    e.lasttrigger = 0;
                    break;
                }
                else break;
                if(checktriggertype(e.attr3, TRIG_COLLIDE) && overlapsdynent(e.o, 20)) break;
                e.triggerstate = TRIGGER_RESETTING;
                e.lasttrigger = lastmillis;
                if(checktriggertype(e.attr3, TRIG_RUMBLE)) et->rumble(e);
                et->trigger(e);
                if(e.attr4) doleveltrigger(e.attr4, 0);
                break;
        }
    }
}


#include "pch.h"
#include "engine.h"

VARP(grass, 0, 0, 1);
VAR(dbggrass, 0, 0, 1);
VAR(grassdist, 0, 128, 10000);
FVAR(grasstaper, 0, 0.2, 1);
FVAR(grassstep, 0.5, 2, 8);
VAR(grassheight, 1, 4, 64);

#define NUMGRASSWEDGES 8

struct grasswedge
{
    vec dir, edge1, edge2;
    plane bound1, bound2;

    grasswedge(int i) :
      dir(2*M_PI*(i+0.5f)/float(NUMGRASSWEDGES), 0),
      edge1(vec(2*M_PI*i/float(NUMGRASSWEDGES), 0).div(cos(M_PI/NUMGRASSWEDGES))),
      edge2(vec(2*M_PI*(i+1)/float(NUMGRASSWEDGES), 0).div(cos(M_PI/NUMGRASSWEDGES))),
      bound1(vec(2*M_PI*(i/float(NUMGRASSWEDGES) - 0.5f), 0), 0),
      bound2(vec(2*M_PI*((i+1)/float(NUMGRASSWEDGES) + 0.5f), 0), 0)
    {}
} grasswedges[NUMGRASSWEDGES] = { 0, 1, 2, 3, 4, 5, 6, 7 };

struct grassvert
{
    vec pos;
    uchar color[4];
    float u, v, lmu, lmv;
};

vector<grassvert> grassverts;

struct grassgroup
{
    float mindist, maxdist;
    int tex, lmtex, offset, numquads;
};

vector<grassgroup> grassgroups;

#define NUMGRASSOFFSETS 256

static float grassoffsets[NUMGRASSOFFSETS] = { -1 };

static inline bool clipgrassquad(const grasstri &g, vec &p1, vec &p2)
{
    loopi(g.numv)
    {
        float dist1 = g.e[i].dist(p1), dist2 = g.e[i].dist(p2);
        if(dist1 <= 0)
        {
            if(dist2 <= 0) return false;
            p1.add(vec(p2).sub(p1).mul(dist1 / (dist1 - dist2)));
        }
        else if(dist2 <= 0)
            p2.add(vec(p1).sub(p2).mul(dist2 / (dist2 - dist1)));
    }
    return true;
}
 
static void gengrassquads(grassgroup *&group, const grasswedge &w, const grasstri &g, Texture *tex)
{
    float t = camera1->o.dot(w.dir);
    int tstep = int(ceil(t/grassstep));
    float tstart = tstep*grassstep, tfrac = tstart - t;

    float t1 = w.dir.dot(g.v[0]), t2 = w.dir.dot(g.v[1]), t3 = w.dir.dot(g.v[2]),
          tmin = min(t1, min(t2, t3)),
          tmax = max(t1, max(t2, t3));
    if(g.numv>3)
    {
        float t4 = w.dir.dot(g.v[3]);
        tmin = min(tmin, t4);
        tmax = max(tmax, t4);
    }
 
    if(tmax < tstart || tmin > t + grassdist) return;

    int minstep = max(int(ceil(tmin/grassstep)) - tstep, 1),
        maxstep = int(floor(min(tmax, t + grassdist)/grassstep)) - tstep,
        numsteps = maxstep - minstep + 1;

    vec tc;
    tc.cross(g.surface, w.dir).mul(tex->ys/float(tex->xs));

    int color = tstep + maxstep;
    if(color < 0) color = NUMGRASSOFFSETS - (-color)%NUMGRASSOFFSETS;
    color += numsteps + NUMGRASSOFFSETS - numsteps%NUMGRASSOFFSETS;

    float taperdist = grassdist*grasstaper,
          taperscale = 1.0f / (grassdist - taperdist);

    for(int i = maxstep; i >= minstep; i--, color--)
    {
        float dist = i*grassstep + tfrac;
        vec p1 = vec(w.edge1).mul(dist).add(camera1->o),
            p2 = vec(w.edge2).mul(dist).add(camera1->o);
        p1.z = g.surface.zintersect(p1);
        p2.z = g.surface.zintersect(p2);

        if(!clipgrassquad(g, p1, p2)) continue;

        if(!group)
        {
            group = &grassgroups.add();
            group->mindist = group->maxdist = dist;
            group->tex = tex->id;
            group->lmtex = lightmaptexs.inrange(g.lmid) ? lightmaptexs[g.lmid].id : notexture->id;
            group->offset = grassverts.length();
            group->numquads = 0;
        }
  
        group->mindist = min(group->mindist, dist);
        group->maxdist = max(group->maxdist, dist);
        group->numquads++;
 
        float offset = grassoffsets[color%NUMGRASSOFFSETS],
              tc1 = tc.dot(p1) + offset, tc2 = tc.dot(p2) + offset,
              lm1u = g.tcu.dot(p1), lm1v = g.tcv.dot(p1),
              lm2u = g.tcu.dot(p2), lm2v = g.tcv.dot(p2),
              fade = dist > taperdist ? (grassdist - dist)*taperscale : 1;

        float height = grassheight * fade;
        uchar color[4] = { 255, 255, 255, uchar(fade*255) };

        #define GRASSVERT(n, tcv, pz) { \
            grassvert &gv = grassverts.add(); \
            (gv.pos = p##n) pz; \
            memcpy(gv.color, color, sizeof(color)); \
            gv.u = tc##n; gv.v = tcv; \
            gv.lmu = lm##n##u; gv.lmv = lm##n##v; \
        }
    
        GRASSVERT(2, 0, .z += height);
        GRASSVERT(1, 0, .z += height);
        GRASSVERT(1, 1, );
        GRASSVERT(2, 1, );
    }
}             

void gengrassquads(vtxarray *va)
{
    loopv(*va->grasstris)
    {
        grasstri &g = (*va->grasstris)[i];
        if(isvisiblesphere(g.radius, g.center) >= VFC_FOGGED || g.center.dist(camera1->o) - g.radius > grassdist) continue;

            
        Slot &s = lookuptexture(g.texture, false);
        if(!s.grasstex) s.grasstex = textureload(s.autograss, 2);

        grassgroup *group = NULL;
        loopi(NUMGRASSWEDGES)
        {
            grasswedge &w = grasswedges[i];    
            if(w.bound1.dist(g.center) > g.radius || w.bound2.dist(g.center) > g.radius) continue;
            gengrassquads(group, w, g, s.grasstex);
        }
    }
}

static inline int comparegrassgroups(const grassgroup *x, const grassgroup *y)
{
    if(x->mindist + x->maxdist > y->mindist + y->maxdist) return -1;
    else if(x->mindist + x->maxdist < y->mindist + y->maxdist) return 1;
    else return 0;
}

void generategrass()
{
    if(!grass || !grassdist) return;

    grassgroups.setsizenodelete(0);
    grassverts.setsizenodelete(0);

    if(grassoffsets[0] < 0) loopi(NUMGRASSOFFSETS) grassoffsets[i] = rnd(0x1000000)/float(0x1000000);

    loopi(NUMGRASSWEDGES)
    {
        grasswedge &w = grasswedges[i];
        w.bound1.offset = -camera1->o.dot(w.bound1);
        w.bound2.offset = -camera1->o.dot(w.bound2);
    }

    extern vtxarray *visibleva;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->grasstris || va->occluded >= OCCLUDE_GEOM) continue;
        if(va->distance > grassdist) continue;
        if(reflecting || refracting>0 ? va->o.z+va->size<reflectz : va->o.z>=reflectz) continue;
        gengrassquads(va);
    }

    grassgroups.sort(comparegrassgroups);
}

void rendergrass()
{
    if(!grass || !grassdist || grassgroups.empty() || dbggrass) return;

    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    lookupshaderbyname("grass")->set();

    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, sizeof(grassvert), grassverts[0].pos.v);

    glEnableClientState(GL_COLOR_ARRAY);
    glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(grassvert), grassverts[0].color);

    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glTexCoordPointer(2, GL_FLOAT, sizeof(grassvert), &grassverts[0].u);

    if(renderpath!=R_FIXEDFUNCTION || maxtmus>=2)
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        glClientActiveTexture_(GL_TEXTURE1_ARB);
        glEnable(GL_TEXTURE_2D);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, sizeof(grassvert), &grassverts[0].lmu);
        if(renderpath==R_FIXEDFUNCTION) setuptmu(1, "P * T x 2"); 
        glClientActiveTexture_(GL_TEXTURE0_ARB);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }

    int texid = -1, lmtexid = -1;
    loopv(grassgroups)
    {
        grassgroup &group = grassgroups[i];

        if(texid != group.tex)
        {
            glBindTexture(GL_TEXTURE_2D, group.tex);
            texid = group.tex;
        }
        if(lmtexid != group.lmtex)
        {
            if(renderpath!=R_FIXEDFUNCTION || maxtmus>=2)
            {
                glActiveTexture_(GL_TEXTURE1_ARB);
                glBindTexture(GL_TEXTURE_2D, group.lmtex);
                glActiveTexture_(GL_TEXTURE0_ARB);
            }
            lmtexid = group.lmtex;
        }

        glDrawArrays(GL_QUADS, group.offset, 4*group.numquads);
        xtravertsva += 4*group.numquads;
    }

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);

    if(renderpath!=R_FIXEDFUNCTION || maxtmus>=2)
    {
        glActiveTexture_(GL_TEXTURE1_ARB);
        glClientActiveTexture_(GL_TEXTURE1_ARB);
        if(renderpath==R_FIXEDFUNCTION) resettmu(1);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        glDisable(GL_TEXTURE_2D);
        glClientActiveTexture_(GL_TEXTURE0_ARB);
        glActiveTexture_(GL_TEXTURE0_ARB);
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}


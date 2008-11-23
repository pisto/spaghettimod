#include "pch.h"
#include "engine.h"

VARP(grassanimdist, 0, 500, 10000);
VARP(grassdist, 0, 500, 10000);
VARP(grassfalloff, 0, 100, 1000);

VAR(grasswidth, 1, 6, 64);
VAR(grassheight, 1, 8, 64);

void resetgrasssamples()
{
    extern vector<vtxarray *> valist;
    loopv(valist)
    {
        vtxarray *va = valist[i];
        DELETEP(va->grasssamples);
    }
}

VARF(grassgrid, 1, 6, 32, resetgrasssamples());

void gengrasssample(vtxarray *va, const vec &o, float tu, float tv, LightMap *lm)
{
    grasssample &g = va->grasssamples->add();

    g.x = ushort(o.x-va->o.x) | GRASS_SAMPLE;
    g.y = ushort(o.y-va->o.y);
    g.z = ushort(4*(o.z-va->o.z));

    if(lm)
    {
        tu = min(tu, LM_PACKW-0.01f);
        tv = min(tv, LM_PACKH-0.01f);
        memcpy(g.color, &lm->data[lm->bpp*(int(tv)*LM_PACKW + int(tu))], 3);
    }
    else loopk(3) g.color[k] = hdr.ambient;
}

bool gengrassheader(vtxarray *va, const vec *v)
{
    vec center = v[0];
    center.add(v[1]);
    center.add(v[2]);
    center.div(3);

    float r1 = center.dist(v[0]),
          r2 = center.dist(v[1]),
          r3 = center.dist(v[2]),
          radius = min(r1, min(r2, r3));
    if(radius < grassgrid*2) return false;

    grassbounds &g = *(grassbounds *)&va->grasssamples->add();
    g.x = ushort(center.x-va->o.x) | GRASS_BOUNDS;
    g.y = ushort(center.y-va->o.y);
    g.z = ushort(4*(center.z-va->o.z));
    g.radius = ushort(radius + grasswidth);
    g.numsamples = 0;
    return true;
}

void gengrasssamples(vtxarray *va, const vec *v, float *tc, LightMap *lm)
{
    int u, l, r;
    if(v[1].y < v[0].y) u = v[1].y < v[2].y ? 1 : 2;
    else u = v[0].y < v[2].y ? 0 : 2;
    l = (u+2)%3;
    r = (u+1)%3;
    if(v[l].x > v[r].x) swap(l, r);
    if(v[u].y == v[l].y)
    {
        if(v[l].x <= v[u].x) swap(u, l);
        swap(l, r);
    }
    vec o1 = v[u], dl = v[l];
    dl.sub(o1);
    if(dl.x==0 && dl.y==0) return;
    float endl = v[l].y,
          ls = tc[2*u], lt = tc[2*u+1],
          lds = tc[2*l] - ls, ldt = tc[2*l+1] - lt;

    vec o2, dr;
    float endr, rs, rt, rds, rdt;
    if(v[u].y==v[r].y)
    {
        if(v[u].x==v[r].x) return;
        o2 = v[r];
        dr = v[l];
        dr.sub(o2);
        endr = v[l].y;

        rs = tc[2*r];
        rt = tc[2*r+1];
        rds = tc[2*l] - rs;
        rdt = tc[2*l+1] - rt;
    }
    else
    {
        o2 = v[u];
        dr = v[r];
        dr.sub(o2);
        endr = v[r].y;
        rs = ls;
        rt = lt;
        rds = tc[2*r] - rs;
        rdt = tc[2*r+1] - rt;
    }
    if(dr.y==0 && (dr.x==0 || dl.y==0)) return;
    if(dr.x==0 && dl.x==0) return;
    
    bool header = false;
    int numsamples = 0;
    float dy = grassgrid - fmodf(o1.y, grassgrid);
    for(;;)
    {
        if(endl > o1.y) dy = min(dy, endl - o1.y);
        if(endr > o2.y) dy = min(dy, endr - o2.y);

        o1.y += dy;
        o1.x += dl.x * dy/dl.y;
        o1.z += dl.z * dy/dl.y;
        ls += lds * dy/dl.y;
        lt += ldt * dy/dl.y;

        o2.y += dy;
        o2.x += dr.x * dy/dr.y;
        o2.z += dr.z * dy/dr.y;
        rs += rds * dy/dr.y;
        rt += rdt * dy/dr.y;

        if(o1.y <= endl && o2.y <= endr && fmod(o1.y, grassgrid) < 0.01f)
        {
            vec p = o1, dp = o2;
            dp.sub(o1);
            float s = ls, t = lt,
                  ds = rs - ls, dt = rt - lt;
            float dx = grassgrid - fmodf(o1.x, grassgrid);
            if(o1.x==o2.x && dx==grassgrid)
            {
                if(!numsamples++) header = gengrassheader(va, v);
                gengrasssample(va, p, s, t, lm);
            }
            else while(!header || numsamples<USHRT_MAX)
            {
                p.x += dx;
                p.y += dp.y * dx/dp.x;
                p.z += dp.z * dx/dp.x;
                s += ds * dx/dp.x;
                t += dt * dx/dp.x;

                if(p.x > o2.x) break;

                if(!numsamples++) header = gengrassheader(va, v);
                gengrasssample(va, p, s, t, lm);

                dx = grassgrid;
            }
            if(header && numsamples>=USHRT_MAX) break;
        }

        if(o1.y >= endl)
        {
            if(v[r].y <= endl) break;
            dl = v[r];
            dl.sub(v[l]);
            endl = v[r].y;
            lds = tc[2*r] - tc[2*l];
            ldt = tc[2*r+1] - tc[2*l+1];

            dy = grassgrid - fmod(o1.y, grassgrid);
            continue;
        }

        if(o2.y >= endr)
        {
            if(v[l].y <= endr) break;
            dr = v[l];
            dr.sub(v[r]);
            endr = v[l].y;
            rds = tc[2*l] - tc[2*r];
            rdt = tc[2*l+1] - tc[2*r+1];

            dy = grassgrid - fmod(o1.y, grassgrid);
            continue;
        }

        dy = grassgrid;
    }
    if(header)
    {
        grassbounds &g = *(grassbounds *)&(*va->grasssamples)[va->grasssamples->length() - numsamples - 1];
        g.numsamples = numsamples;
    } 
}

void gengrasssamples(vtxarray *va)
{
    if(va->grasssamples) return;
    va->grasssamples = new vector<grasssample>;
    int lasttex = -1;
    loopv(*va->grasstris)
    {
        grasstri &g = (*va->grasstris)[i];
        if(g.texture != lasttex)
        {
            grasstexture &t = *(grasstexture *)&va->grasssamples->add();
            t.x = GRASS_TEXTURE;
            t.texture = g.texture;
            lasttex = g.texture;
        }
        vec v[4];
        float tc[8];
        static int remap[4] = { 1, 2, 0, 3 };
        loopk(4)
        {
            int j = remap[k];
            v[k] = g.v[j].tovec(va->o);
            if(g.surface)
            {
                tc[2*k] = float(g.surface->x + (g.surface->texcoords[j*2] / 255.0f) * (g.surface->w - 1) + 0.5f);
                tc[2*k+1] = float(g.surface->y + (g.surface->texcoords[j*2 + 1] / 255.0f) * (g.surface->h - 1) + 0.5f);
            }
        }
        LightMap *lm = g.surface && g.surface->lmid >= LMID_RESERVED ? &lightmaps[g.surface->lmid-LMID_RESERVED] : NULL;
        gengrasssamples(va, v, tc, lm);
        gengrasssamples(va, &v[1], &tc[2], lm);
    }
}

VAR(grasstest, 0, 0, 3);

static Texture *grasstex = NULL;

VARP(grasslod, 0, 25, 1000);

VARP(grasslodz, 0, 150, 10000);

float loddist(const vec &o)
{
    float dx = o.x - camera1->o.x, dy = o.y - camera1->o.y, dz = camera1->o.z - o.z;
    float dist = sqrt(dx*dx + dy*dy);
    dist -= grasslodz/100.0f * max(dz, 0.0f);
    return max(dist, 0.0f);
}

VAR(grassrand, 0, 30, 90);

VARP(grasssamples, 0, 50, 10000);

VARP(grassbillboard, 0, 1, 100);
VARP(grassbbcorrect, 0, 1, 1);
VARP(grasstaper, 0, 200, 10000);

void rendergrasssample(const grasssample &g, const vec &o, float dist, int seed, float height, int numsamples)
{
    if(grasstest>2) return;

    if(seed >= 2*numsamples) return;

    vec up(0, 0, 1), right(seed%2, (seed+1)%2, 0);
    float width = grasswidth;
    if(numsamples<=grassbillboard)
    {
        if(seed%2) return;
        right = camright;
        if(grassrand) right.rotate_around_z((detrnd((size_t)&g * (seed + 1), 2*grassrand)-grassrand)*RAD);
        if(grassbbcorrect) 
        {
            if(fabs(right.x) > fabs(right.y)) width *= sqrt(right.y*right.y/(right.x*right.x) + 1);
            else width *= sqrt(right.x*right.x/(right.y*right.y) + 1);
        }
    }
    else if(grassrand) right.rotate_around_z((detrnd((size_t)&g * (seed + 1), 2*grassrand)-grassrand)*RAD);

    vec b1 = right;
    b1.mul(-0.5f*width);
    b1.add(o);
    b1[seed%2] += (seed/2 * grassgrid) / float(numsamples) - grassgrid/2.0f;

    vec b2 = right;
    b2.mul(width);
    b2.add(b1);

    vec t1 = b1, t2 = b2;
    t1.z += grassheight * height;
    t2.z += grassheight * height;

    float w1 = 0, w2 = 0;
    if(grasstest>0) t1 = t2 = b1;
    else if(dist < grassanimdist)
    {
        w1 = detrnd((size_t)&g * (seed + 1)*7, 360)*RAD + t1.x*0.4f + t1.y*0.5f;
        w1 += lastmillis*0.0015f;
        w1 = sinf(w1);
        vec d1(1.0f, 1.0f, 0.5f);
        d1.mul(grassheight/4.0f * w1);
        t1.add(d1);

        w2 = detrnd((size_t)&g * (seed + 1)*11, 360)*RAD + t2.x*0.55f + t2.y*0.45f;
        w2 += lastmillis*0.0015f;
        w2 = sinf(w2);
        vec d2(0.4f, 0.4f, 0.2f);
        d2.mul(grassheight/4.0f * w2);
        t2.add(d2);
    }

    if(grasstest>1) return;

    extern int fullbright;
    if(nolights || (fullbright && editmode)) glColor3ub(128, 128, 128);
    else glColor3ubv(g.color);
    float offset = detrnd((size_t)&g * (seed + 1)*13, grasstex->xs)/float(grasstex->xs);
    glTexCoord2f(offset, 1); glVertex3fv(b1.v);
    glTexCoord2f(offset, 0); glVertex3fv(t1.v);
    glTexCoord2f(offset + float(grasswidth)*64.0f/grasstex->xs, 0); glVertex3fv(t2.v);
    glTexCoord2f(offset + float(grasswidth)*64.0f/grasstex->xs, 1); glVertex3fv(b2.v);
    xtraverts += 4;
}

void rendergrasssamples(vtxarray *va, const vec &dir)
{
    if(!va->grasssamples) return;
    loopv(*va->grasssamples)
    {
        grasssample &g = (*va->grasssamples)[i];

        vec o((g.x&~GRASS_TYPE)+va->o.x, g.y+va->o.y, g.z/4.0f+va->o.z), tograss;
        switch(g.x&GRASS_TYPE)
        {
            case GRASS_BOUNDS:
            {
                grassbounds &b = *(grassbounds *)&g;
                if(reflecting || refracting>0 ? o.z+b.radius<reflectz : o.z-b.radius>=reflectz)
                {
                    i += b.numsamples;
                    continue;
                }
                float dist = o.dist(camera1->o, tograss);
                if(dist > grassdist + b.radius || (dir.dot(tograss)<0 && dist > b.radius + 2*(grassgrid + player->eyeheight)))
                    i += b.numsamples;
                break;
            }

            case GRASS_TEXTURE:
            {
                grasstexture &t = *(grasstexture *)&g;
                Slot &s = lookuptexture(t.texture, false);
                if(!s.grasstex || s.grasstex!=grasstex)
                {
                    glEnd();
                    if(!s.grasstex) s.grasstex = textureload(s.autograss, 2);
                    glBindTexture(GL_TEXTURE_2D, s.grasstex->id);
                    glBegin(GL_QUADS);
                    grasstex = s.grasstex;
                }
                break;
            }

            case GRASS_SAMPLE:
            {
                if(reflecting || refracting>0 ? o.z+grassheight<=reflectz : o.z>=reflectz) continue;
                float dist = o.dist(camera1->o, tograss);
                if(dist > grassdist || (dir.dot(tograss)<0 && dist > grasswidth/2 + 2*(grassgrid + player->eyeheight))) continue;

                float ld = loddist(o);
                int numsamples = int(grasssamples/100.0f*max(grassgrid - ld/grasslod, 100.0f/grasssamples));
                float height = 1 - (dist + grasstaper - grassdist) / (grasstaper ? grasstaper : 1);
                height = min(height, 1.0f);
                loopj(2*numsamples)
                {
                    rendergrasssample(g, o, dist, j, height, numsamples);
                }
                break;
            }
        }
    }
}

VAR(grassblend, 0, 0, 100);

void setupgrass()
{
    glDisable(GL_CULL_FACE);
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, grassblend ? grassblend/100.0f : 0.6f);
    if(grassblend)
    {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    static Shader *grassshader = NULL;
    if(!grassshader) grassshader = lookupshaderbyname("grass");
    grassshader->set();

    grasstex = NULL;

    setuptmu(0, "C * T x 2");

    glBegin(GL_QUADS);
}

void cleanupgrass()
{
    glEnd();

    resettmu(0);

    defaultshader->set();

    if(grassblend) glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_CULL_FACE);
}

VARP(grass, 0, 0, 1);

void rendergrass()
{
    if(!grass || !grasssamples || !grassdist) return;

    vec dir;
    vecfromyawpitch(camera1->yaw, 0, 1, 0, dir);

    int rendered = 0;
    extern vtxarray *visibleva;
    for(vtxarray *va = visibleva; va; va = va->next)
    {
        if(!va->grasstris || va->occluded >= OCCLUDE_GEOM) continue;
        if(va->distance > grassdist) continue;
        if(reflecting || refracting>0 ? va->o.z+va->size<reflectz : va->o.z>=reflectz) continue;
        if(!va->grasssamples) gengrasssamples(va);
        if(!rendered++) setupgrass();
        rendergrasssamples(va, dir);
    }

    if(rendered) cleanupgrass();
}


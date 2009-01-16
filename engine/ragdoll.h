struct ragdollskel
{
    struct vert
    {
        vec pos;
        float weight;
    };

    struct tri
    {
        int vert[3];
    };

    struct distlimit
    {
        int vert[2];
        float dist;
    }; 

    struct rotlimit
    {
        int tri[2];
        float maxangle;
        matrix3x3 middle;
    };

    struct joint
    {
        int bone, tri, vert[3];
        float weight;
        matrix3x4 orient;
    };

    struct reljoint
    {
        int bone, parent;
    };

    int eye;
    vector<vert> verts;
    vector<tri> tris;
    vector<distlimit> distlimits;
    vector<rotlimit> rotlimits;
    vector<joint> joints;
    vector<reljoint> reljoints;

    ragdollskel() : eye(-1) {}

    void setup()
    {
        loopv(verts) verts[i].weight = 0;
        loopv(joints)
        {
            joint &j = joints[i];
            j.weight = 0;
            vec pos(0, 0, 0);
            loopk(3) if(j.vert[k]>=0) 
            {
                pos.add(verts[j.vert[k]].pos);
                j.weight++;
                verts[j.vert[k]].weight++;
            }
            if(j.weight) j.weight = 1/j.weight;
            pos.mul(j.weight);

            tri &t = tris[j.tri];
            matrix3x3 m;
            const vec &v1 = verts[t.vert[0]].pos,
                      &v2 = verts[t.vert[1]].pos,
                      &v3 = verts[t.vert[2]].pos;
            m.a = vec(v2).sub(v1).normalize();
            m.c.cross(m.a, vec(v3).sub(v1)).normalize();
            m.b.cross(m.c, m.a);

            j.orient = matrix3x4(m, m.transform(pos).neg());        
        }
        loopv(verts) if(verts[i].weight) verts[i].weight = 1/verts[i].weight;
        loopv(distlimits)
        {
            distlimit &d = distlimits[i];
            if(d.dist <= 0) d.dist = verts[d.vert[0]].pos.dist(verts[d.vert[1]].pos);
        }
        reljoints.setsize(0);
    } 

    void addreljoint(int bone, int parent)
    {
        reljoint &r = reljoints.add();
        r.bone = bone;
        r.parent = parent;
    }
};

struct ragdolldata
{
    struct vert
    {
        vec oldpos, pos, newpos;
        float weight, life;
        bool collided;

        vert() : pos(0, 0, 0), life(1), collided(false) {}
    };

    ragdollskel *skel;
    int millis, collidemillis, lastmove;
    vec offset, center;
    float radius, timestep, scale;
    vert *verts;
    matrix3x3 *tris;
    matrix3x4 *reljoints;

    ragdolldata(ragdollskel *skel, float scale = 1)
        : skel(skel),
          millis(lastmillis),
          collidemillis(0),
          lastmove(lastmillis),
          timestep(0),
          scale(scale),
          verts(new vert[skel->verts.length()]), 
          tris(new matrix3x3[skel->tris.length()]),
          reljoints(skel->reljoints.empty() ? NULL : new matrix3x4[skel->reljoints.length()])
    {
    }

    ~ragdolldata()
    {
        delete[] verts;
        delete[] tris;
        if(reljoints) delete[] reljoints;
    }

    void calctris()
    {
        loopv(skel->tris)
        {
            ragdollskel::tri &t = skel->tris[i];
            matrix3x3 &m = tris[i];
            const vec &v1 = verts[t.vert[0]].pos,
                      &v2 = verts[t.vert[1]].pos,
                      &v3 = verts[t.vert[2]].pos;
            m.a = vec(v2).sub(v1).normalize();
            m.c.cross(m.a, vec(v3).sub(v1)).normalize();
            m.b.cross(m.c, m.a);
        }
    }

    void calcboundsphere()
    {
        center = vec(0, 0, 0);
        loopv(skel->verts) center.add(verts[i].pos);
        center.div(skel->verts.length());
        radius = 0;
        loopv(skel->verts) radius = max(radius, verts[i].pos.dist(center));
    }

    void init(dynent *d)
    {
        float ts = curtime/1000.0f;
        loopv(skel->verts) (verts[i].oldpos = verts[i].pos).sub(vec(d->vel).add(d->falling).mul(ts));
        timestep = ts;

        calctris();
        calcboundsphere();
        offset = d->o;
        offset.sub(skel->eye >= 0 ? verts[skel->eye].pos : center);
        offset.z += (d->eyeheight + d->aboveeye)/2;
    }

    void move(dynent *pl, float ts);
    void constrain();
    void constraindist();
    void constrainrot();
};

/*
    seed particle position = avg(modelview * base2anim * spherepos)  
    mapped transform = invert(curtri) * origtrig 
    parented transform = parent{invert(curtri) * origtrig} * (invert(parent{base2anim}) * base2anim)
*/

void ragdolldata::constraindist()
{
    loopv(skel->distlimits)
    {
        ragdollskel::distlimit &d = skel->distlimits[i];
        vert &v1 = verts[d.vert[0]], &v2 = verts[d.vert[1]];
        vec dir = vec(v2.pos).sub(v1.pos),
            center = vec(v1.pos).add(v2.pos).mul(0.5f);
        float dist = dir.magnitude();
        if(dist < 1e-4f) dir = vec(0, 0, d.dist*scale*0.5f);
        else dir.mul(d.dist*scale*0.5f/dist);
        v1.newpos.add(vec(center).sub(dir));
        v1.weight++;
        v2.newpos.add(vec(center).add(dir));
        v2.weight++;
    }
}
        
void ragdolldata::constrainrot()
{
    loopv(skel->rotlimits)
    {
        ragdollskel::rotlimit &r = skel->rotlimits[i];
        matrix3x3 rot;
        rot.transpose(tris[r.tri[0]]);
        rot.mul(r.middle);
        rot.mul(tris[r.tri[1]]);

        vec axis;
        float angle;
        rot.calcangleaxis(angle, axis);
        if(angle < 0)
        {
            if(-angle <= r.maxangle) continue;
            angle += r.maxangle;
        }
        else if(angle <= r.maxangle) continue;
        else angle = r.maxangle - angle;
        angle += 1e-3f;

        ragdollskel::tri &t1 = skel->tris[r.tri[0]], &t2 = skel->tris[r.tri[1]];
        vec v1[3], v2[3], c1(0, 0, 0), c2(0, 0, 0);
        loopk(3)
        {
            c1.add(v1[k] = verts[t1.vert[k]].pos);
            c2.add(v2[k] = verts[t2.vert[k]].pos);
        }
        c1.div(3);
        c2.div(3);
        loopk(3) { v1[k].sub(c1); v2[k].sub(c2); }
        matrix3x3 wrot, crot1, crot2;
        wrot.rotate(0.5f*RAD, axis);
        float w1 = 0, w2 = 0;
        loopk(3) 
        { 
            w1 += wrot.transform(v1[k]).dist(v1[k]); 
            w2 += wrot.transform(v2[k]).dist(v2[k]); 
        }
        crot1.rotate(angle*w2/(w1+w2), axis);
        crot2.rotate(-angle*w1/(w1+w2), axis);
        vec r1[3], r2[3], diff1(0, 0, 0), diff2(0, 0, 0);
        loopk(3) 
        { 
            r1[k] = crot1.transform(v1[k]);
            r2[k] = crot2.transform(v2[k]);
            diff1.add(r1[k]).sub(v1[k]);
            diff2.add(r2[k]).sub(v2[k]);
        }
        diff1.div(3).add(c1);
        diff2.div(3).add(c2);
        loopk(3)
        {
            verts[t1.vert[k]].newpos.add(r1[k]).add(diff1);
            verts[t2.vert[k]].newpos.add(r2[k]).add(diff2);
            verts[t1.vert[k]].weight++;
            verts[t2.vert[k]].weight++;
        }
    }
}

void ragdolldata::constrain()
{
    loopv(skel->verts)
    {
        vert &v = verts[i];
        v.newpos = vec(0, 0, 0);
        v.weight = 0;
    }
    constraindist();
    physent d;
    d.type = ENT_BOUNCE;
    d.radius = d.eyeheight = d.aboveeye = 1;
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.weight)
        {
            d.o = v.newpos.div(v.weight);
            if(collide(&d, vec(v.newpos).sub(v.pos), 0, false)) v.pos = v.newpos;
        }
        v.newpos = vec(0, 0, 0);
        v.weight = 0;
    }
    calctris();
    constrainrot();
    loopv(skel->verts)
    {
        vert &v = verts[i];
        if(v.weight) 
        {
            d.o = v.newpos.div(v.weight);        
            if(collide(&d, vec(v.newpos).sub(v.pos), 0, false)) v.pos = v.newpos;
        }
    }
}

FVAR(ragdollfricmin, 0, 0.5f, 1);
FVAR(ragdollfricmax, 0, 0.99f, 1);
FVAR(ragdollelasticitymin, 0, 0, 1);
FVAR(ragdollelasticitymax, 0, 1, 1);
VAR(ragdollconstrain, 1, 3, 100);
VAR(ragdollvertlife, 1, 100, 10000);
VAR(ragdollexpireoffset, 0, 5000, 30000);
VAR(ragdollexpiremillis, 1, 3000, 30000);

void ragdolldata::move(dynent *pl, float ts)
{
    extern const float GRAVITY;
    float expirefric = collidemillis && lastmillis > collidemillis ? max(1 - float(lastmillis - collidemillis)/ragdollexpiremillis, 0.0f) : 1;
    if(!expirefric) return;
    if(timestep) expirefric *= ts/timestep;

    int material = lookupmaterial(vec(center.x, center.y, center.z + radius/2));
    bool water = isliquid(material&MATF_VOLUME);
    if(!pl->inwater && water) cl->physicstrigger(pl, true, 0, -1, material&MATF_VOLUME);
    else if(pl->inwater && !water) cl->physicstrigger(pl, true, 0, 1, pl->inwater);
    pl->inwater = water ? material&MATF_VOLUME : MAT_AIR;
    
    physent d;
    d.type = ENT_BOUNCE;
    d.radius = d.eyeheight = d.aboveeye = 1;
    int collisions = 0;
    loopv(skel->verts)
    {
        vert &v = verts[i];
        v.collided = false;
        loopk(2)
        {
            vec curpos = v.pos, dpos = vec(v.pos).sub(v.oldpos);
            if(!k) 
            {
                dpos.mul((v.collided ? ragdollfricmin + v.life*(ragdollfricmax - ragdollfricmin) : ragdollfricmax)*expirefric);
                v.pos.z -= GRAVITY*ts*ts;
            }
            if(water) 
            {
                dpos.mul(0.75f);
                v.pos.z += 0.25f*sinf(detrnd(size_t(this)+i, 360)*RAD + lastmillis/10000.0f*M_PI)*ts;
            }
            v.pos.add(dpos);
            if(v.pos.z < 0) { v.pos.z = 0; collisions++; }
            d.o = v.pos;
            vec dir = vec(v.pos).sub(curpos);
            extern vec wall;
            if(!collide(&d, dir, 0, false)) 
            { 
                float elasticity = ragdollelasticitymin + v.life*(ragdollelasticitymax - ragdollelasticitymin),
                      c = wall.dot(dir),
                      speed = dir.magnitude(),
                      k = 1.0f + (speed > 1e-6f ? (1.0f-elasticity)*c/speed : 0.0f);
                dir.mul(k).sub(vec(wall).mul(elasticity*2.0f*c));
                v.oldpos = vec(curpos).sub(dir); 
                v.pos = curpos; 
                if(wall.z > 0) v.collided = true; 
            }
            else { v.oldpos = curpos; break; } 
        }
        if(v.collided) 
        {
            v.life = collidemillis ? max(v.life - ts*1000.0f/ragdollvertlife, 0.0f) : 1;
            collisions++;
        }
    }
    loopi(ragdollconstrain) constrain();
    calctris();
    calcboundsphere();
    timestep = ts;
    if(collisions)
    {
        if(!collidemillis) collidemillis = lastmillis + ragdollexpireoffset;
    }
    else if(lastmillis < collidemillis) collidemillis = 0;
}    

VAR(ragdolltimestepmin, 1, 5, 50);
VAR(ragdolltimestepmax, 1, 25, 50);

FVAR(ragdolleyesmooth, 0, 0.5f, 1);
FVAR(ragdolleyesmoothmillis, 1, 500, 10000);

void moveragdoll(dynent *d)
{
    if(!curtime || !d->ragdoll || (d->ragdoll->collidemillis && lastmillis > d->ragdoll->collidemillis + ragdollexpiremillis)) return;

    int lastmove = d->ragdoll->lastmove;
    while(d->ragdoll->lastmove + (lastmove == d->ragdoll->lastmove ? ragdolltimestepmin : ragdolltimestepmax) <= lastmillis)
    {
        int timestep = min(ragdolltimestepmax, lastmillis - d->ragdoll->lastmove);
        d->ragdoll->move(d, timestep/1000.0f);
        d->ragdoll->lastmove += timestep;
    }

    vec eye = d->ragdoll->skel->eye >= 0 ? d->ragdoll->verts[d->ragdoll->skel->eye].pos : d->ragdoll->center;
    eye.add(d->ragdoll->offset);
    float k = pow(ragdolleyesmooth, float(d->ragdoll->lastmove - lastmove)/ragdolleyesmoothmillis);
    d->o.mul(k).add(eye.mul(1-k));
}

void cleanragdoll(dynent *d)
{
    DELETEP(d->ragdoll);
}


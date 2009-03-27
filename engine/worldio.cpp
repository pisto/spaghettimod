// worldio.cpp: loading & saving of maps and savegames

#include "cube.h"
#include "engine.h"

void backup(char *name, char *backupname)
{
    string backupfile;
    s_strcpy(backupfile, findfile(backupname, "wb"));
    remove(backupfile);
    rename(findfile(name, "wb"), backupfile);
}

string ogzname, bakname, pcfname, mcfname, picname;

VARP(savebak, 0, 2, 2);

void cutogz(char *s)
{
    char *ogzp = strstr(s, ".ogz");
    if(ogzp) *ogzp = '\0';
}

void getnames(const char *fname, const char *cname, char *pakname, char *mapname, char *cfgname)
{
    if(!cname) cname = fname;
    string name;
    s_strncpy(name, cname, 100);
    cutogz(name);
    char *slash = strpbrk(name, "/\\");
    if(slash)
    {
        s_strncpy(pakname, name, slash-name+1);
        s_strcpy(cfgname, slash+1);
    }
    else
    {
        s_strcpy(pakname, "base");
        s_strcpy(cfgname, name);
    }
    if(strpbrk(fname, "/\\")) s_strcpy(mapname, fname);
    else s_sprintf(mapname)("base/%s", fname);
    cutogz(mapname);
}

void setnames(const char *fname, const char *cname = 0)
{
    string pakname, mapname, cfgname;
    getnames(fname, cname, pakname, mapname, cfgname);

    s_sprintf(ogzname)("packages/%s.ogz", mapname);
    if(savebak==1) s_sprintf(bakname)("packages/%s.BAK", mapname);
    else s_sprintf(bakname)("packages/%s_%d.BAK", mapname, totalmillis);
    s_sprintf(pcfname)("packages/%s/package.cfg", pakname);
    s_sprintf(mcfname)("packages/%s/%s.cfg", pakname, cfgname);
    s_sprintf(picname)("packages/%s.jpg", mapname);

    path(ogzname);
    path(bakname);
    path(picname);
}

void mapcfgname()
{
    const char *mname = game::getclientmap();
    if(!*mname) mname = "untitled";

    string pakname, mapname, cfgname;
    getnames(mname, NULL, pakname, mapname, cfgname);
    s_sprintfd(mcfname)("packages/%s/%s.cfg", pakname, cfgname);
    path(mcfname);
    result(mcfname);
}

COMMAND(mapcfgname, "");

ushort readushort(gzFile f)
{
    ushort t = 0;
    gzread(f, &t, sizeof(ushort));
    endianswap(&t, sizeof(ushort), 1);
    return t;
}

int readint(gzFile f)
{
    int t = 0;
    gzread(f, &t, sizeof(int));
    endianswap(&t, sizeof(int), 1);
    return t;
}

float readfloat(gzFile f)
{
    int t = 0;
    gzread(f, &t, sizeof(float));
    endianswap(&t, sizeof(float), 1);
    return t;
}

void writeushort(gzFile f, ushort u)
{
    endianswap(&u, sizeof(ushort), 1);
    gzwrite(f, &u, sizeof(ushort));
}

void writeint(gzFile f, int i)
{
    endianswap(&i, sizeof(int), 1);
    gzwrite(f, &i, sizeof(int));
}

void writefloat(gzFile f, float n)
{
    endianswap(&n, sizeof(float), 1);
    gzwrite(f, &n, sizeof(float));
}

enum { OCTSAV_CHILDREN = 0, OCTSAV_EMPTY, OCTSAV_SOLID, OCTSAV_NORMAL, OCTSAV_LODCUBE };

void savec(cube *c, gzFile f, bool nolms)
{
    loopi(8)
    {
        if(c[i].children && (!c[i].ext || !c[i].ext->surfaces))
        {
            gzputc(f, OCTSAV_CHILDREN);
            savec(c[i].children, f, nolms);
        }
        else
        {
            int oflags = 0;
            if(c[i].ext && c[i].ext->merged) oflags |= 0x80;
            if(c[i].children) gzputc(f, oflags | OCTSAV_LODCUBE);
            else if(isempty(c[i])) gzputc(f, oflags | OCTSAV_EMPTY);
            else if(isentirelysolid(c[i])) gzputc(f, oflags | OCTSAV_SOLID);
            else
            {
                gzputc(f, oflags | OCTSAV_NORMAL);
                gzwrite(f, c[i].edges, 12);
            }
            loopj(6) writeushort(f, c[i].texture[j]);
            uchar mask = 0;
            if(c[i].ext)
            {
                if(c[i].ext->material != MAT_AIR) mask |= 0x80;
                if(c[i].ext->normals && !nolms)
                {
                    mask |= 0x40;
                    loopj(6) if(c[i].ext->normals[j].normals[0] != bvec(128, 128, 128)) mask |= 1 << j;
                }
            }
            // save surface info for lighting
            if(!c[i].ext || !c[i].ext->surfaces || nolms)
            {
                gzputc(f, mask);
                if(c[i].ext)
                {
                    if(c[i].ext->material != MAT_AIR) gzputc(f, c[i].ext->material);
                    if(c[i].ext->normals && !nolms) loopj(6) if(mask & (1 << j))
                    {
                        loopk(sizeof(surfaceinfo)) gzputc(f, 0);
                        gzwrite(f, &c[i].ext->normals[j], sizeof(surfacenormals));
                    } 
                }
            }
            else
            {
                int numsurfs = 6;
                loopj(6) 
                {
                    surfaceinfo &surface = c[i].ext->surfaces[j];
                    if(surface.lmid >= LMID_RESERVED || surface.layer!=LAYER_TOP) 
                    {
                        mask |= 1 << j;
                        if(surface.layer&LAYER_BLEND) numsurfs++;
                    }
                }
                gzputc(f, mask);
                if(c[i].ext->material != MAT_AIR) gzputc(f, c[i].ext->material);
                loopj(numsurfs) if(j >= 6 || mask & (1 << j))
                {
                    surfaceinfo tmp = c[i].ext->surfaces[j];
                    endianswap(&tmp.x, sizeof(ushort), 2);
                    gzwrite(f, &tmp, sizeof(surfaceinfo));
                    if(j < 6 && c[i].ext->normals) gzwrite(f, &c[i].ext->normals[j], sizeof(surfacenormals));
                }
            }
            if(c[i].ext && c[i].ext->merged)
            {
                gzputc(f, c[i].ext->merged | (c[i].ext->mergeorigin ? 0x80 : 0));
                if(c[i].ext->mergeorigin)
                {
                    gzputc(f, c[i].ext->mergeorigin);
                    int index = 0;
                    loopj(6) if(c[i].ext->mergeorigin&(1<<j))
                    {
                        mergeinfo tmp = c[i].ext->merges[index++];
                        endianswap(&tmp, sizeof(ushort), 4);
                        gzwrite(f, &tmp, sizeof(mergeinfo));
                    }
                }
            }
            if(c[i].children) savec(c[i].children, f, nolms);
        }
    }
}

cube *loadchildren(gzFile f);

void loadc(gzFile f, cube &c)
{
    bool haschildren = false;
    int octsav = gzgetc(f);
    switch(octsav&0x7)
    {
        case OCTSAV_CHILDREN:
            c.children = loadchildren(f);
            return;

        case OCTSAV_LODCUBE: haschildren = true;    break;
        case OCTSAV_EMPTY:  emptyfaces(c);          break;
        case OCTSAV_SOLID:  solidfaces(c);          break;
        case OCTSAV_NORMAL: gzread(f, c.edges, 12); break;

        default:
            fatal("garbage in map");
    }
    loopi(6) c.texture[i] = mapversion<14 ? gzgetc(f) : readushort(f);
    if(mapversion < 7) loopi(3) gzgetc(f); //gzread(f, c.colour, 3);
    else
    {
        uchar mask = gzgetc(f);
        if(mask & 0x80) 
        {
            int mat = gzgetc(f);
            if(mapversion < 27)
            {
                static uchar matconv[] = { MAT_AIR, MAT_WATER, MAT_CLIP, MAT_GLASS|MAT_CLIP, MAT_NOCLIP, MAT_LAVA|MAT_DEATH, MAT_AICLIP, MAT_DEATH };
                mat = size_t(mat) < sizeof(matconv)/sizeof(matconv[0]) ? matconv[mat] : MAT_AIR;
            }
            ext(c).material = mat;
        }
        if(mask & 0x3F)
        {
            uchar lit = 0, bright = 0;
            static surfaceinfo surfaces[12];
            memset(surfaces, 0, 6*sizeof(surfaceinfo));
            if(mask & 0x40) newnormals(c);
            int numsurfs = 6;
            loopi(numsurfs)
            {
                if(i >= 6 || mask & (1 << i))
                {
                    gzread(f, &surfaces[i], sizeof(surfaceinfo));
                    endianswap(&surfaces[i].x, sizeof(ushort), 2);
                    if(mapversion < 10) ++surfaces[i].lmid;
                    if(mapversion < 18)
                    {
                        if(surfaces[i].lmid >= LMID_AMBIENT1) ++surfaces[i].lmid;
                        if(surfaces[i].lmid >= LMID_BRIGHT1) ++surfaces[i].lmid;
                    }
                    if(mapversion < 19)
                    {
                        if(surfaces[i].lmid >= LMID_DARK) surfaces[i].lmid += 2;
                    }
                    if(i < 6)
                    {
                        if(mask & 0x40) gzread(f, &c.ext->normals[i], sizeof(surfacenormals));
                        if(surfaces[i].layer != LAYER_TOP) lit |= 1 << i;
                        else if(surfaces[i].lmid == LMID_BRIGHT) bright |= 1 << i;
                        else if(surfaces[i].lmid != LMID_AMBIENT) lit |= 1 << i;
                        if(surfaces[i].layer&LAYER_BLEND) numsurfs++;
                    }
                }
                else surfaces[i].lmid = LMID_AMBIENT;
            }
            if(lit) newsurfaces(c, surfaces, numsurfs);
            else if(bright) brightencube(c);
        }
        if(mapversion >= 20)
        {
            if(octsav&0x80)
            {
                int merged = gzgetc(f);
                ext(c).merged = merged&0x3F;
                if(merged&0x80)
                {
                    c.ext->mergeorigin = gzgetc(f);
                    int nummerges = 0;
                    loopi(6) if(c.ext->mergeorigin&(1<<i)) nummerges++;
                    if(nummerges)
                    {
                        c.ext->merges = new mergeinfo[nummerges];
                        loopi(nummerges)
                        {
                            mergeinfo *m = &c.ext->merges[i];
                            gzread(f, m, sizeof(mergeinfo));
                            endianswap(m, sizeof(ushort), 4);
                            if(mapversion <= 25)
                            {
                                int uorigin = m->u1 & 0xE000, vorigin = m->v1 & 0xE000;
                                m->u1 = (m->u1 - uorigin) << 2;
                                m->u2 = (m->u2 - uorigin) << 2;
                                m->v1 = (m->v1 - vorigin) << 2;
                                m->v2 = (m->v2 - vorigin) << 2;
                            }
                        }
                    }
                }
            }    
        }                
    }
    c.children = (haschildren ? loadchildren(f) : NULL);
}

cube *loadchildren(gzFile f)
{
    cube *c = newcubes();
    loopi(8) loadc(f, c[i]);
    // TODO: remip c from children here
    return c;
}

VAR(dbgvars, 0, 0, 1);

bool save_world(const char *mname, bool nolms)
{
    if(!*mname) mname = game::getclientmap();
    setnames(*mname ? mname : "untitled");
    if(savebak) backup(ogzname, bakname);
    gzFile f = opengzfile(ogzname, "wb9");
    if(!f) { conoutf(CON_WARN, "could not write map to %s", ogzname); return false; }
    octaheader hdr;
    memcpy(hdr.magic, "OCTA", 4);
    hdr.version = MAPVERSION;
    hdr.headersize = sizeof(hdr);
    hdr.worldsize = worldsize;
    hdr.numents = 0;
    const vector<extentity *> &ents = entities::getents();
    loopv(ents) if(ents[i]->type!=ET_EMPTY || nolms) hdr.numents++;
    hdr.numpvs = nolms ? 0 : getnumviewcells();
    hdr.blendmap = nolms ? 0 : shouldsaveblendmap();
    hdr.lightmaps = nolms ? 0 : lightmaps.length();
    hdr.numvars = 0;
    enumerate(*idents, ident, id, 
    {
        if((id.type == ID_VAR || id.type == ID_FVAR || id.type == ID_SVAR) && id.flags&IDF_OVERRIDE && !(id.flags&IDF_READONLY) && id.override!=NO_OVERRIDE) hdr.numvars++;
    });
    endianswap(&hdr.version, sizeof(int), 8);
    gzwrite(f, &hdr, sizeof(hdr));
   
    enumerate(*idents, ident, id, 
    {
        if((id.type!=ID_VAR && id.type!=ID_FVAR && id.type!=ID_SVAR) || !(id.flags&IDF_OVERRIDE) || id.flags&IDF_READONLY || id.override==NO_OVERRIDE) continue;
        gzputc(f, id.type);
        writeushort(f, strlen(id.name));
        gzwrite(f, id.name, strlen(id.name));
        switch(id.type)
        {
            case ID_VAR:
                if(dbgvars) conoutf(CON_DEBUG, "wrote var %s: %d", id.name, *id.storage.i);
                writeint(f, *id.storage.i);
                break;

            case ID_FVAR:
                if(dbgvars) conoutf(CON_DEBUG, "wrote fvar %s: %f", id.name, *id.storage.f);
                writeint(f, *id.storage.f);
                break;

            case ID_SVAR:
                if(dbgvars) conoutf(CON_DEBUG, "wrote svar %s: %s", id.name, *id.storage.s);
                writeushort(f, strlen(*id.storage.s));
                gzwrite(f, *id.storage.s, strlen(*id.storage.s));
                break;
        }
    });

    if(dbgvars) conoutf(CON_DEBUG, "wrote %d vars", hdr.numvars);

    gzputc(f, (int)strlen(game::gameident()));
    gzwrite(f, game::gameident(), (int)strlen(game::gameident())+1);
    writeushort(f, entities::extraentinfosize());
    vector<char> extras;
    game::writegamedata(extras);
    writeushort(f, extras.length());
    gzwrite(f, extras.getbuf(), extras.length());
    
    writeushort(f, texmru.length());
    loopv(texmru) writeushort(f, texmru[i]);
    char *ebuf = new char[entities::extraentinfosize()];
    loopv(ents)
    {
        if(ents[i]->type!=ET_EMPTY || nolms)
        {
            entity tmp = *ents[i];
            endianswap(&tmp.o, sizeof(int), 3);
            endianswap(&tmp.attr1, sizeof(short), 5);
            gzwrite(f, &tmp, sizeof(entity));
            entities::writeent(*ents[i], ebuf);
            if(entities::extraentinfosize()) gzwrite(f, ebuf, entities::extraentinfosize());
        }
    }
    delete[] ebuf;

    savec(worldroot, f, nolms);
    if(!nolms) 
    {
        loopv(lightmaps)
        {
            LightMap &lm = lightmaps[i];
            gzputc(f, lm.type | (lm.unlitx>=0 ? 0x80 : 0));
            if(lm.unlitx>=0)
            {
                writeushort(f, ushort(lm.unlitx));
                writeushort(f, ushort(lm.unlity));
            }
            gzwrite(f, lm.data, lm.bpp*LM_PACKW*LM_PACKH);
        }
        if(getnumviewcells()>0) savepvs(f);
        if(shouldsaveblendmap()) saveblendmap(f);
    }

    gzclose(f);
    conoutf("wrote map file %s", ogzname);
    return true;
}

static void swapXZ(cube *c)
{	
	loopi(8) 
	{
		swap(c[i].faces[0],   c[i].faces[2]);
		swap(c[i].texture[0], c[i].texture[4]);
		swap(c[i].texture[1], c[i].texture[5]);
		if(c[i].ext && c[i].ext->surfaces)
		{
			swap(c[i].ext->surfaces[0], c[i].ext->surfaces[4]);
			swap(c[i].ext->surfaces[1], c[i].ext->surfaces[5]);
		}
		if(c[i].children) swapXZ(c[i].children);
	}
}

static void fixoversizedcubes(cube *c, int size)
{
    if(size <= VVEC_INT_MASK+1) return;
    loopi(8)
    {
        if(!c[i].children) subdividecube(c[i], true, false);
        fixoversizedcubes(c[i].children, size>>1);
    }
}

bool load_world(const char *mname, const char *cname)        // still supports all map formats that have existed since the earliest cube betas!
{
    int loadingstart = SDL_GetTicks();
    setnames(mname, cname);
    gzFile f = opengzfile(ogzname, "rb9");
    if(!f) { conoutf(CON_ERROR, "could not read map %s", ogzname); return false; }
    octaheader hdr;
    if(gzread(f, &hdr, 5*sizeof(int))!=5*sizeof(int)) { conoutf(CON_ERROR, "map %s has malformatted header", ogzname); gzclose(f); return false; }
    endianswap(&hdr.version, sizeof(int), 4);
    if(strncmp(hdr.magic, "OCTA", 4)!=0 || hdr.worldsize <= 0|| hdr.numents < 0) { conoutf(CON_ERROR, "map %s has malformatted header", ogzname); gzclose(f); return false; }
    if(hdr.version>MAPVERSION) { conoutf(CON_ERROR, "map %s requires a newer version of cube 2", ogzname); gzclose(f); return false; }
    compatheader chdr;
    if(hdr.version <= 28 || /* hack for obsolete older mapversion 29 */ (size_t)hdr.headersize > sizeof(octaheader))
    {
        if(gzread(f, &chdr.numpvs, sizeof(chdr) - 5*sizeof(int)) != sizeof(chdr) - 5*sizeof(int)) { conoutf(CON_ERROR, "map %s has malformatted header", ogzname); gzclose(f); return false; }
        if(hdr.version > 28) conoutf(CON_WARN, "using an obsolete map format: please resave this map before release or your map will fail to load");
    }
    else if(gzread(f, &hdr.numpvs, sizeof(hdr) - 5*sizeof(int)) != sizeof(hdr) - 5*sizeof(int)) { conoutf(CON_ERROR, "map %s has malformatted header", ogzname); gzclose(f); return false; }

    resetmap();

    Texture *mapshot = textureload(picname, 3, true, false);
    renderbackground("loading...", mapshot, mname, game::getmapinfo());

    setvar("mapversion", hdr.version, true, false);

    if(hdr.version <= 28 || /* hack for obsolete older mapversion 29 */ (size_t)hdr.headersize > sizeof(octaheader))
    {
        endianswap(&chdr.numpvs, sizeof(int), 3);
        endianswap(&chdr.waterfog, sizeof(ushort), 3);
        if(hdr.version<=20) conoutf(CON_WARN, "loading older / less efficient map format, may benefit from \"calclight 2\", then \"savecurrentmap\"");
        if(hdr.version<=28)
        {
            int lightprecision = chdr.fog, lighterror = chdr.waterfog, lightlod = chdr.lightprecision, ambient = chdr.ambient[2];
            chdr.lightprecision = lightprecision;
            chdr.lighterror = lighterror;
            chdr.lightlod = lightlod;
            memset(chdr.ambient, ambient, sizeof(chdr.ambient));
        } 
        if(chdr.lightprecision) setvar("lightprecision", chdr.lightprecision);
        if(chdr.lighterror) setvar("lighterror", chdr.lighterror);
        if(chdr.bumperror) setvar("bumperror", chdr.bumperror);
        setvar("lightlod", chdr.lightlod);
        if(chdr.ambient[0] || chdr.ambient[1] || chdr.ambient[2]) setvar("ambient", (int(chdr.ambient[0])<<16) | (int(chdr.ambient[1])<<8) | int(chdr.ambient[2]));
        setvar("skylight", (int(chdr.skylight[0])<<16) | (int(chdr.skylight[1])<<8) | int(chdr.skylight[2]));
        setvar("watercolour", (int(chdr.watercolour[0])<<16) | (int(chdr.watercolour[1])<<8) | int(chdr.watercolour[2]), true);
        setvar("waterfallcolour", (int(chdr.waterfallcolour[0])<<16) | (int(chdr.waterfallcolour[1])<<8) | int(chdr.waterfallcolour[2]));
        setvar("lavacolour", (int(chdr.lavacolour[0])<<16) | (int(chdr.lavacolour[1])<<8) | int(chdr.lavacolour[2]));
        setvar("fullbright", 0, true);
        if(chdr.lerpsubdivsize || chdr.lerpangle) setvar("lerpangle", chdr.lerpangle);
        if(chdr.lerpsubdivsize)
        {
            setvar("lerpsubdiv", chdr.lerpsubdiv);
            setvar("lerpsubdivsize", chdr.lerpsubdivsize);
        }
        setsvar("maptitle", chdr.maptitle);
        hdr.numpvs = chdr.numpvs;
        hdr.lightmaps = chdr.lightmaps;
        hdr.blendmap = chdr.blendmap;
        hdr.numvars = 0; 
    }
    else endianswap(&hdr.numpvs, sizeof(int), 3);
 
    loopi(hdr.numvars)
    {
        int type = gzgetc(f), ilen = readushort(f);
        string name;
        gzread(f, name, min(ilen, MAXSTRLEN-1));
        name[min(ilen, MAXSTRLEN-1)] = '\0';
        if(ilen >= MAXSTRLEN) gzseek(f, ilen - (MAXSTRLEN-1), SEEK_CUR);
        ident *id = getident(name);
        bool exists = id && id->type == type;
        switch(type)
        {
            case ID_VAR:
            {
                int val = readint(f);
                if(exists && id->minval <= id->maxval) setvar(name, val);
                if(dbgvars) conoutf(CON_DEBUG, "read var %s: %d", name, val);
                break;
            }
 
            case ID_FVAR:
            {
                float val = readfloat(f);
                if(exists && id->minvalf <= id->maxvalf) setfvar(name, val);
                if(dbgvars) conoutf(CON_DEBUG, "read fvar %s: %f", name, val);
                break;
            }
    
            case ID_SVAR:
            {
                int slen = readushort(f);
                string val;
                gzread(f, val, min(slen, MAXSTRLEN-1));
                val[min(slen, MAXSTRLEN-1)] = '\0';
                if(slen >= MAXSTRLEN) gzseek(f, slen - (MAXSTRLEN-1), SEEK_CUR);
                if(exists) setsvar(name, val);
                if(dbgvars) conoutf(CON_DEBUG, "read svar %s: %s", name, val);
                break;
            }
        }
    }
    if(dbgvars) conoutf(CON_DEBUG, "read %d vars", hdr.numvars);

    string gametype;
    s_strcpy(gametype, "fps");
    bool samegame = true;
    int eif = 0;
    if(hdr.version>=16)
    {
        int len = gzgetc(f);
        gzread(f, gametype, len+1);
    }
    if(strcmp(gametype, game::gameident())!=0)
    {
        samegame = false;
        conoutf(CON_WARN, "WARNING: loading map from %s game, ignoring entities except for lights/mapmodels)", gametype);
    }
    if(hdr.version>=16)
    {
        eif = readushort(f);
        int extrasize = readushort(f);
        vector<char> extras;
        loopj(extrasize) extras.add(gzgetc(f));
        if(samegame) game::readgamedata(extras);
    }
    
    renderprogress(0, "clearing world...");

    texmru.setsize(0);
    if(hdr.version<14)
    {
        uchar oldtl[256];
        gzread(f, oldtl, sizeof(oldtl));
        loopi(256) texmru.add(oldtl[i]);
    }
    else
    {
        ushort nummru = readushort(f);
        loopi(nummru) texmru.add(readushort(f));
    }

    freeocta(worldroot);
    worldroot = NULL;

    setvar("mapsize", hdr.worldsize, true, false);
    int worldscale = 0;
    while(1<<worldscale < hdr.worldsize) worldscale++;
    setvar("mapscale", worldscale, true, false);

    renderprogress(0, "loading entities...");

    vector<extentity *> &ents = entities::getents();
    int einfosize = entities::extraentinfosize();
    char *ebuf = einfosize > 0 ? new char[einfosize] : NULL;
    loopi(min(hdr.numents, MAXENTS))
    {
        extentity &e = *entities::newentity();
        ents.add(&e);
        gzread(f, &e, sizeof(entity));
        endianswap(&e.o, sizeof(int), 3);
        endianswap(&e.attr1, sizeof(short), 5);
        e.spawned = false;
        e.inoctanode = false;
        if(samegame)
        {
            if(einfosize > 0) gzread(f, ebuf, einfosize);
            entities::readent(e, ebuf); 
        }
        else
        {
            loopj(eif) gzgetc(f);
        }
        if(hdr.version <= 14 && e.type >= ET_MAPMODEL && e.type <= 16)
        {
            if(e.type == 16) e.type = ET_MAPMODEL;
            else e.type++;
        }
        if(hdr.version <= 20 && e.type >= ET_ENVMAP) e.type++;
        if(hdr.version <= 21 && e.type >= ET_PARTICLES) e.type++;
        if(hdr.version <= 22 && e.type >= ET_SOUND) e.type++;
        if(hdr.version <= 23 && e.type >= ET_SPOTLIGHT) e.type++;
        if(!samegame)
        {
            if(e.type>=ET_GAMESPECIFIC || hdr.version<=14)
            {
                entities::deleteentity(ents.pop());
                continue;
            }
        }
        if(!insideworld(e.o))
        {
            if(e.type != ET_LIGHT && e.type != ET_SPOTLIGHT)
            {
                conoutf(CON_WARN, "warning: ent outside of world: enttype[%s] index %d (%f, %f, %f)", entities::entname(e.type), i, e.o.x, e.o.y, e.o.z);
            }
        }
        if(hdr.version <= 14 && e.type == ET_MAPMODEL)
        {
            e.o.z += e.attr3;
            if(e.attr4) conoutf(CON_WARN, "warning: mapmodel ent (index %d) uses texture slot %d", i, e.attr4);
            e.attr3 = e.attr4 = 0;
        }
    }
    if(ebuf) delete[] ebuf;

    if(hdr.numents > MAXENTS) 
    {
        conoutf(CON_WARN, "warning: map has %d entities", hdr.numents);
        gzseek(f, (hdr.numents-MAXENTS)*(sizeof(entity) + einfosize), SEEK_CUR);
    }

    renderprogress(0, "loading octree...");
    worldroot = loadchildren(f);

	if(hdr.version <= 11)
		swapXZ(worldroot);

    if(hdr.version <= 8)
        converttovectorworld();

    if(hdr.version <= 25 && hdr.worldsize > VVEC_INT_MASK+1)
        fixoversizedcubes(worldroot, hdr.worldsize>>1);

    renderprogress(0, "validating...");
    validatec(worldroot, hdr.worldsize>>1);

    if(hdr.version >= 7) loopi(hdr.lightmaps)
    {
        renderprogress(i/(float)hdr.lightmaps, "loading lightmaps...");
        LightMap &lm = lightmaps.add();
        if(hdr.version >= 17)
        {
            int type = gzgetc(f);
            lm.type = type&0x7F;
            if(hdr.version >= 20 && type&0x80)
            {
                lm.unlitx = readushort(f);
                lm.unlity = readushort(f);
            }
        }
        if(lm.type&LM_ALPHA && (lm.type&LM_TYPE)!=LM_BUMPMAP1) lm.bpp = 4;
        lm.data = new uchar[lm.bpp*LM_PACKW*LM_PACKH];
        gzread(f, lm.data, lm.bpp * LM_PACKW * LM_PACKH);
        lm.finalize();
    }

    if(hdr.version >= 25 && hdr.numpvs > 0) loadpvs(f, hdr.numpvs);
    if(hdr.version >= 28 && hdr.blendmap) loadblendmap(f, hdr.blendmap);

    gzclose(f);

    conoutf("read map %s (%.1f seconds)", ogzname, (SDL_GetTicks()-loadingstart)/1000.0f);
    if(maptitle[0]) conoutf(CON_ECHO, "%s", maptitle);

    clearmainmenu();

    overrideidents = true;
    execfile("data/default_map_settings.cfg");
    execfile(pcfname);
    execfile(mcfname);
    overrideidents = false;
   
    extern void fixlightmapnormals();
    if(hdr.version <= 25) fixlightmapnormals();

    vector<int> mapmodels;
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type==ET_MAPMODEL && e.attr2 >= 0)
        {
            if(mapmodels.find(e.attr2) < 0) mapmodels.add(e.attr2);
        }
    }

    loopv(mapmodels)
    {
        loadprogress = float(i+1)/mapmodels.length();
        int mmindex = mapmodels[i];
        mapmodelinfo &mmi = getmminfo(mmindex);
        if(!&mmi) conoutf(CON_WARN, "could not find map model: %d", mmindex);
        else if(!loadmodel(NULL, mmindex, true)) conoutf(CON_WARN, "could not load model: %s", mmi.name);
    }
    loadprogress = 0;

    game::preload();
    flushpreloadedmodels();

    entitiesinoctanodes();
    attachentities();
    initlights();
    allchanged(true);

    renderbackground("loading...", mapshot, mname, game::getmapinfo());

    startmap(cname ? cname : mname);
    
    return true;
}

void savecurrentmap() { save_world(game::getclientmap()); }
void savemap(char *mname) { save_world(mname); }

COMMAND(savemap, "s");
COMMAND(savecurrentmap, "");

void writeobj(char *name)
{
    s_sprintfd(fname)("%s.obj", name);
    FILE *f = openfile(path(fname), "w"); 
    if(!f) return;
    fprintf(f, "# obj file of sauerbraten level\n");
    extern vector<vtxarray *> valist;
    loopv(valist)
    {
        vtxarray &va = *valist[i];
        ushort *edata = NULL;
        uchar *vdata = NULL;
        if(!readva(&va, edata, vdata)) continue;
        int vtxsize = VTXSIZE;
        uchar *vert = vdata;
        loopj(va.verts) 
        {
            vec v;
            if(floatvtx) (v = *(vec *)vert).div(1<<VVEC_FRAC); 
            else v = ((vvec *)vert)->tovec(va.o).add(0x8000>>VVEC_FRAC);
            if(v.x != floor(v.x)) fprintf(f, "v %.3f ", v.x); else fprintf(f, "v %d ", int(v.x));
            if(v.y != floor(v.y)) fprintf(f, "%.3f ", v.y); else fprintf(f, "%d ", int(v.y));
            if(v.z != floor(v.z)) fprintf(f, "%.3f\n", v.z); else fprintf(f, "%d\n", int(v.z));
            vert += vtxsize;
        }
        ushort *tri = edata;
        loopi(va.tris)
        {
            fprintf(f, "f");
            for(int k = 0; k<3; k++) fprintf(f, " %d", tri[k]-va.verts-va.voffset);
            tri += 3;
            fprintf(f, "\n");
        }
        delete[] edata;
        delete[] vdata;
    }
    fclose(f);
}  
    
COMMAND(writeobj, "s"); 


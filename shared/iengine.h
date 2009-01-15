// the interface the game uses to access the engine

extern int curtime;                     // current frame time
extern int lastmillis;                  // last time
extern int totalmillis;                 // total elapsed time

enum
{
    MATF_VOLUME_SHIFT = 0,
    MATF_CLIP_SHIFT   = 3,
    MATF_FLAG_SHIFT   = 5,

    MATF_VOLUME = 7 << MATF_VOLUME_SHIFT,
    MATF_CLIP   = 3 << MATF_CLIP_SHIFT,
    MATF_FLAGS  = 7 << MATF_FLAG_SHIFT
};

enum // cube empty-space materials
{
    MAT_AIR   = 0,                      // the default, fill the empty space with air
    MAT_WATER = 1 << MATF_VOLUME_SHIFT, // fill with water, showing waves at the surface
    MAT_LAVA  = 2 << MATF_VOLUME_SHIFT, // fill with lava
    MAT_GLASS = 3 << MATF_VOLUME_SHIFT, // behaves like clip but is blended blueish

    MAT_NOCLIP = 1 << MATF_CLIP_SHIFT,  // collisions always treat cube as empty
    MAT_CLIP   = 2 << MATF_CLIP_SHIFT,  // collisions always treat cube as solid
    MAT_AICLIP = 3 << MATF_CLIP_SHIFT,  // clip monsters only

    MAT_DEATH  = 1 << MATF_FLAG_SHIFT,  // force player suicide
    MAT_EDIT   = 4 << MATF_FLAG_SHIFT   // edit-only surfaces
};

extern void lightent(extentity &e, float height = 8.0f);
extern void lightreaching(const vec &target, vec &color, vec &dir, extentity *e = 0, float ambient = 0.4f);
extern entity *brightestlight(const vec &target, const vec &dir);

enum { RAY_BB = 1, RAY_POLY = 3, RAY_ALPHAPOLY = 7, RAY_ENTS = 9, RAY_CLIPMAT = 16, RAY_SKIPFIRST = 32, RAY_EDITMAT = 64, RAY_SHADOW = 128, RAY_PASS = 256 };

extern float raycube   (const vec &o, const vec &ray,     float radius = 0, int mode = RAY_CLIPMAT, int size = 0, extentity *t = 0);
extern float raycubepos(const vec &o, const vec &ray, vec &hit, float radius = 0, int mode = RAY_CLIPMAT, int size = 0);
extern float rayfloor  (const vec &o, vec &floor, int mode = 0, float radius = 0);
extern bool  raycubelos(const vec &o, const vec &dest, vec &hitpos);

extern bool isthirdperson();

extern void settexture(const char *name, int clamp = 0);

// octaedit

enum { EDIT_FACE = 0, EDIT_TEX, EDIT_MAT, EDIT_FLIP, EDIT_COPY, EDIT_PASTE, EDIT_ROTATE, EDIT_REPLACE, EDIT_DELCUBE, EDIT_REMIP };

struct selinfo
{
    int corner;
    int cx, cxs, cy, cys;
    ivec o, s;
    int grid, orient;
    int size() const    { return s.x*s.y*s.z; }
    int us(int d) const { return s[d]*grid; }
    bool operator==(const selinfo &sel) const { return o==sel.o && s==sel.s && grid==sel.grid && orient==sel.orient; }
};

struct editinfo;

extern bool editmode;

extern void freeeditinfo(editinfo *&e);
extern void cursorupdate();
extern void pruneundos(int maxremain = 0);
extern bool noedit(bool view = false);
extern void toggleedit(bool force = true);
extern void mpeditface(int dir, int mode, selinfo &sel, bool local);
extern void mpedittex(int tex, int allfaces, selinfo &sel, bool local);
extern void mpeditmat(int matid, selinfo &sel, bool local);
extern void mpflip(selinfo &sel, bool local);
extern void mpcopy(editinfo *&e, selinfo &sel, bool local);
extern void mppaste(editinfo *&e, selinfo &sel, bool local);
extern void mprotate(int cw, selinfo &sel, bool local);
extern void mpreplacetex(int oldtex, int newtex, selinfo &sel, bool local);
extern void mpdelcube(selinfo &sel, bool local);
extern void mpremip(bool local);

// command
extern int variable(const char *name, int min, int cur, int max, int *storage, void (*fun)(), int flags);
extern float fvariable(const char *name, float min, float cur, float max, float *storage, void (*fun)(), int flags);
extern char *svariable(const char *name, const char *cur, char **storage, void (*fun)(), int flags);
extern void setvar(const char *name, int i, bool dofunc = false);
extern void setfvar(const char *name, float f, bool dofunc = false);
extern void setsvar(const char *name, const char *str, bool dofunc = false);
extern void touchvar(const char *name);
extern int getvar(const char *name);
extern int getvarmin(const char *name);
extern int getvarmax(const char *name);
extern bool identexists(const char *name);
extern ident *getident(const char *name);
extern bool addcommand(const char *name, void (*fun)(), const char *narg);
extern int execute(const char *p);
extern char *executeret(const char *p);
extern void exec(const char *cfgfile);
extern bool execfile(const char *cfgfile);
extern void alias(const char *name, const char *action);
extern const char *getalias(const char *name);

// console

enum
{
    CON_INFO  = 1<<0,
    CON_WARN  = 1<<1,
    CON_ERROR = 1<<2,
    CON_DEBUG = 1<<3,
    CON_INIT  = 1<<4,
    CON_ECHO  = 1<<5
};

extern void keypress(int code, bool isdown, int cooked);
extern int rendercommand(int x, int y, int w);
extern int renderconsole(int w, int h);
extern void conoutf(const char *s, ...);
extern void conoutf(int type, const char *s, ...);
extern void resetcomplete();
extern void complete(char *s);

// menus
extern vec menuinfrontofplayer();
extern void newgui(char *name, char *contents, char *header = NULL);
extern void showgui(const char *name);

// world
extern bool emptymap(int factor, bool force, const char *mname = "", bool usecfg = true);
extern bool enlargemap(bool force);
extern int findentity(int type, int index = 0, int attr1 = -1, int attr2 = -1);
extern void mpeditent(int i, const vec &o, int type, int attr1, int attr2, int attr3, int attr4, int attr5, bool local);
extern int getworldsize();
extern int getmapversion();
extern void resettriggers();
extern void checktriggers();

// main
struct igame;

extern void fatal(const char *s, ...);
extern void keyrepeat(bool on);
extern void registergame(const char *name, igame *ig);

#define REGISTERGAME(t, n, c, s) struct t : igame { t() { registergame(n, this); } igameclient *newclient() { return c; } igameserver *newserver() { return s; } } reg_##t

// rendertext
extern bool setfont(const char *name);
extern void gettextres(int &w, int &h);
extern void draw_text(const char *str, int left, int top, int r = 255, int g = 255, int b = 255, int a = 255, int cursor = -1, int maxwidth = -1);
extern void draw_textf(const char *fstr, int left, int top, ...);
extern int text_width(const char *str);
extern void text_bounds(const char *str, int &width, int &height, int maxwidth = -1);
extern int text_visible(const char *str, int hitx, int hity, int maxwidth);
extern void text_pos(const char *str, int cursor, int &cx, int &cy, int maxwidth);

// renderva
enum
{
    DL_SHRINK = 1<<0,
    DL_EXPAND = 1<<1,
    DL_FLASH  = 1<<2
};

extern void adddynlight(const vec &o, float radius, const vec &color, int fade = 0, int peak = 0, int flags = 0, float initradius = 0, const vec &initcolor = vec(0, 0, 0));
extern void dynlightreaching(const vec &target, vec &color, vec &dir);

// rendergl
extern vec worldpos, camdir, camright, camup;
extern void damageblend(int n);
extern void damagecompass(int n, const vec &loc);

// renderparticles
enum
{
    PART_BLOOD = 0,
    PART_WATER,
    PART_SMOKE_RISE_SLOW, PART_SMOKE_RISE_FAST, PART_SMOKE_SINK,
    PART_FIREBALL1, PART_FIREBALL2, PART_FIREBALL3,
    PART_STREAK, PART_LIGHTNING,
    PART_EXPLOSION, PART_EXPLOSION_NO_GLARE,
    PART_SPARK, PART_EDIT,
    PART_TEXT, PART_TEXT_RISE,
    PART_METER, PART_METER_VS,
    PART_LENS_FLARE
};

extern void render_particles(int time);
extern void regular_particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150, int delay = 0);
extern void particle_splash(int type, int num, int fade, const vec &p, int color = 0xFFFFFF, float size = 1.0f, int radius = 150);
extern void particle_trail(int type, int fade, const vec &from, const vec &to, int color = 0xFFFFFF, float size = 1.0f);
extern void particle_text(const vec &s, const char *t, int type, int fade = 2000, int color = 0xFFFFFF, float size = 2.0f);
extern void particle_meter(const vec &s, float val, int type, int fade = 1, int color = 0xFFFFFF, int color2 = 0xFFFFF, float size = 2.0f);
extern void particle_flare(const vec &p, const vec &dest, int fade, int type, int color = 0xFFFFFF, float size = 0.28f, physent *owner = NULL);
extern void particle_fireball(const vec &dest, float max, int type, int fade = -1, int color = 0xFFFFFF, float size = 4.0f);
extern void removetrackedparticles(physent *owner = NULL);

// decal
enum
{
    DECAL_SCORCH = 0,
    DECAL_BLOOD,
    DECAL_BULLET
};

extern void adddecal(int type, const vec &center, const vec &surface, float radius, const bvec &color = bvec(0xFF, 0xFF, 0xFF), int info = 0);

// worldio
extern bool load_world(const char *mname, const char *cname = NULL);
extern bool save_world(const char *mname, bool nolms = false);

// physics
extern void moveplayer(physent *pl, int moveres, bool local);
extern bool moveplayer(physent *pl, int moveres, bool local, int curtime);
extern bool collide(physent *d, const vec &dir = vec(0, 0, 0), float cutoff = 0.0f, bool playercol = true);
extern bool bounce(physent *d, float secs, float elasticity, float waterfric);
extern bool bounce(physent *d, float elasticity, float waterfric);
extern void avoidcollision(physent *d, const vec &dir, physent *obstacle, float space);
extern void physicsframe();
extern void dropenttofloor(entity *e);
extern bool droptofloor(vec &o, float radius, float height);
extern void clamproll(physent *d);

extern void vecfromyawpitch(float yaw, float pitch, int move, int strafe, vec &m);
extern void vectoyawpitch(const vec &v, float &yaw, float &pitch);
extern bool intersect(physent *d, const vec &from, const vec &to);
extern bool moveplatform(physent *p, const vec &dir);
extern void updatephysstate(physent *d);
extern void cleardynentcache();
extern void updatedynentcache(physent *d);
extern bool entinmap(dynent *d, bool avoidplayers = false);
extern void findplayerspawn(dynent *d, int forceent = -1, int tag = 0);

// sound
extern void playsound    (int n,   const vec *loc = NULL, extentity *ent = NULL);
extern void playsoundname(const char *s, const vec *loc = NULL, int vol = 0);
extern void initsound();


// rendermodel
enum { MDL_CULL_VFC = 1<<0, MDL_CULL_DIST = 1<<1, MDL_CULL_OCCLUDED = 1<<2, MDL_CULL_QUERY = 1<<3, MDL_SHADOW = 1<<4, MDL_DYNSHADOW = 1<<5, MDL_LIGHT = 1<<6, MDL_DYNLIGHT = 1<<7, MDL_TRANSLUCENT = 1<<8, MDL_FULLBRIGHT = 1<<9 };

struct model;
struct modelattach
{
    const char *name, *tag;
    int anim, basetime;
    model *m;
};

extern void startmodelbatches();
extern void endmodelbatches();
extern void rendermodel(entitylight *light, const char *mdl, int anim, const vec &o, float yaw = 0, float pitch = 0, int cull = MDL_CULL_VFC | MDL_CULL_DIST | MDL_CULL_OCCLUDED | MDL_LIGHT, dynent *d = NULL, modelattach *a = NULL, int basetime = 0, float speed = 0);
extern void abovemodel(vec &o, const char *mdl);
extern void rendershadow(dynent *d);
extern void renderclient(dynent *d, const char *mdlname, modelattach *attachments, int attack, int attackdelay, int lastaction, int lastpain, bool ragdoll = false);
extern void interpolateorientation(dynent *d, float &interpyaw, float &interppitch);
extern void setbbfrommodel(dynent *d, const char *mdl);
extern const char *mapmodelname(int i);
extern model *loadmodel(const char *name, int i = -1, bool msg = false);

// ragdoll

extern void moveragdoll(dynent *d);
extern void cleanragdoll(dynent *d);

// server
#define MAXCLIENTS 256                  // in a multiplayer game, can be arbitrarily changed
#define MAXTRANS 5000                  // max amount of data to swallow in 1 go

extern int maxclients;

enum { DISC_NONE = 0, DISC_EOP, DISC_CN, DISC_KICK, DISC_TAGT, DISC_IPBAN, DISC_PRIVATE, DISC_MAXCLIENTS, DISC_TIMEOUT, DISC_NUM };

extern void *getinfo(int i);
extern void sendf(int cn, int chan, const char *format, ...);
extern void sendfile(int cn, int chan, FILE *file, const char *format = "", ...);
extern void sendpacket(int cn, int chan, ENetPacket *packet, int exclude = -1);
extern int getnumclients();
extern uint getclientip(int n);
extern void putint(ucharbuf &p, int n);
extern int getint(ucharbuf &p);
extern void putuint(ucharbuf &p, int n);
extern int getuint(ucharbuf &p);
extern void sendstring(const char *t, ucharbuf &p);
extern void getstring(char *t, ucharbuf &p, int len = MAXTRANS);
extern void filtertext(char *dst, const char *src, bool whitespace = true, int len = sizeof(string)-1);
extern void disconnect_client(int n, int reason);
extern void kicknonlocalclients(int reason = DISC_NONE);
extern bool hasnonlocalclients();
extern bool haslocalclients();
extern void sendserverinforeply(ucharbuf &p);

// client
extern void c2sinfo(dynent *d, int rate = 33);
extern void sendpackettoserv(ENetPacket *packet, int chan);
extern void disconnect(int onlyclean = 0, int async = 0);
extern bool isconnected();
extern bool multiplayer(bool msg = true);
extern void neterr(const char *s);
extern void gets2c();

// 3dgui
struct Texture;

enum { G3D_DOWN = 1, G3D_UP = 2, G3D_PRESSED = 4, G3D_ROLLOVER = 8, G3D_DRAGGED = 16 };

enum { EDITORFOCUSED = 1, EDITORUSED, EDITORFOREVER };

struct g3d_gui
{
    virtual ~g3d_gui() {}

    virtual void start(int starttime, float basescale, int *tab = NULL, bool allowinput = true) = 0;
    virtual void end() = 0;

    virtual int text(const char *text, int color, const char *icon = NULL) = 0;
    int textf(const char *fmt, int color, const char *icon = NULL, ...)
    {
        s_sprintfdlv(str, icon, fmt);
        return text(str, color, icon);
    }
    virtual int button(const char *text, int color, const char *icon = NULL) = 0;
    int buttonf(const char *fmt, int color, const char *icon = NULL, ...)
    {
        s_sprintfdlv(str, icon, fmt);
        return button(str, color, icon);
    }
    virtual void background(int color, int parentw = 0, int parenth = 0) = 0;

    virtual void pushlist() {}
    virtual void poplist() {}

    virtual void allowautotab(bool on) = 0;
    virtual bool shouldtab() { return false; }
	virtual void tab(const char *name = NULL, int color = 0) = 0;
    virtual int title(const char *text, int color, const char *icon = NULL) = 0;
    virtual int image(Texture *t, float scale, bool overlaid = false) = 0;
    virtual int texture(Texture *t, float scale, int rotate = 0, int xoff = 0, int yoff = 0, Texture *glowtex = NULL, const vec &glowcolor = vec(1, 1, 1), Texture *layertex = NULL) = 0;
    virtual void slider(int &val, int vmin, int vmax, int color, char *label = NULL) = 0;
    virtual void separator() = 0;
	virtual void progress(float percent) = 0;
	virtual void strut(int size) = 0;
    virtual void space(int size) = 0;
    virtual char *keyfield(const char *name, int color, int length, int height = 0, const char *initval = NULL, int initmode = EDITORFOCUSED) = 0;
    virtual char *field(const char *name, int color, int length, int height = 0, const char *initval = NULL, int initmode = EDITORFOCUSED) = 0;
    virtual void mergehits(bool on) = 0;
};

struct g3d_callback
{
    virtual ~g3d_callback() {}

    int starttime() { return totalmillis; }

    virtual void gui(g3d_gui &g, bool firstpass) = 0;
};

enum
{
    GUI_2D       = 1<<0,
    GUI_FOLLOW   = 1<<1,
    GUI_FORCE_2D = 1<<2
};

extern void g3d_addgui(g3d_callback *cb, vec &origin, int flags = 0);
extern bool g3d_movecursor(int dx, int dy);
extern void g3d_cursorpos(float &x, float &y);
extern void g3d_resetcursor();
extern void g3d_limitscale(float scale);


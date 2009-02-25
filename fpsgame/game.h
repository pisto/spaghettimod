// console message types

enum
{
    CON_CHAT       = 1<<8,
    CON_TEAMCHAT   = 1<<9,
    CON_GAMEINFO   = 1<<10,
    CON_FRAG_SELF  = 1<<11,
    CON_FRAG_OTHER = 1<<12
};

// network quantization scale
#define DMF 16.0f                // for world locations
#define DNF 100.0f              // for normalized vectors
#define DVELF 1.0f              // for playerspeed based velocity vectors

enum                            // static entity types
{
    NOTUSED = ET_EMPTY,         // entity slot not in use in map
    LIGHT = ET_LIGHT,           // lightsource, attr1 = radius, attr2 = intensity
    MAPMODEL = ET_MAPMODEL,     // attr1 = angle, attr2 = idx
    PLAYERSTART,                // attr1 = angle, attr2 = team
    ENVMAP = ET_ENVMAP,         // attr1 = radius
    PARTICLES = ET_PARTICLES,
    MAPSOUND = ET_SOUND,
    SPOTLIGHT = ET_SPOTLIGHT,
    I_SHELLS, I_BULLETS, I_ROCKETS, I_ROUNDS, I_GRENADES, I_CARTRIDGES,
    I_HEALTH, I_BOOST,
    I_GREENARMOUR, I_YELLOWARMOUR,
    I_QUAD,
    TELEPORT,                   // attr1 = idx
    TELEDEST,                   // attr1 = angle, attr2 = idx
    MONSTER,                    // attr1 = angle, attr2 = monstertype
    CARROT,                     // attr1 = tag, attr2 = type
    JUMPPAD,                    // attr1 = zpush, attr2 = ypush, attr3 = xpush
    BASE,
    RESPAWNPOINT,
    BOX,                        // attr1 = angle, attr2 = idx, attr3 = weight
    BARREL,                     // attr1 = angle, attr2 = idx, attr3 = weight, attr4 = health
    PLATFORM,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    ELEVATOR,                   // attr1 = angle, attr2 = idx, attr3 = tag, attr4 = speed
    FLAG,                       // attr1 = angle, attr2 = team
    MAXENTTYPES
};

struct fpsentity : extentity
{
    // extend with additional fields if needed...
};

enum { GUN_FIST = 0, GUN_SG, GUN_CG, GUN_RL, GUN_RIFLE, GUN_GL, GUN_PISTOL, GUN_FIREBALL, GUN_ICEBALL, GUN_SLIMEBALL, GUN_BITE, GUN_BARREL, NUMGUNS };
enum { A_BLUE, A_GREEN, A_YELLOW };     // armour types... take 20/40/60 % off
enum { M_NONE = 0, M_SEARCH, M_HOME, M_ATTACKING, M_PAIN, M_SLEEP, M_AIMING };  // monster states

enum
{
    M_TEAM       = 1<<0,
    M_NOITEMS    = 1<<1,
    M_NOAMMO     = 1<<2,
    M_INSTA      = 1<<3,
    M_EFFICIENCY = 1<<4,
    M_TACTICS    = 1<<5,
    M_CAPTURE    = 1<<6,
    M_REGEN      = 1<<7,
    M_CTF        = 1<<8,
    M_EDIT       = 1<<9,
    M_DEMO       = 1<<10,
    M_LOCAL      = 1<<11,
    M_LOBBY      = 1<<12,
    M_DMSP       = 1<<13,
    M_CLASSICSP  = 1<<14,
    M_SLOWMO     = 1<<15
};

static struct gamemodeinfo
{
    const char *name;
    int flags;
} gamemodes[] =
{
    { "slowmo SP", M_LOCAL | M_CLASSICSP | M_SLOWMO },
    { "slowmo DMSP", M_LOCAL | M_DMSP | M_SLOWMO },
    { "SP", M_LOCAL | M_CLASSICSP },
    { "DMSP", M_LOCAL | M_DMSP },
    { "demo", M_DEMO | M_LOCAL},
    { "ffa", M_LOBBY },
    { "coop edit", M_EDIT },
    { "teamplay", M_TEAM },
    { "instagib", M_NOITEMS | M_INSTA },
    { "instagib team", M_NOITEMS | M_INSTA | M_TEAM },
    { "efficiency", M_NOITEMS | M_EFFICIENCY },
    { "efficiency team", M_NOITEMS | M_EFFICIENCY | M_TEAM },
    { "tactics", M_NOITEMS | M_TACTICS },
    { "tactics team", M_NOITEMS | M_TACTICS | M_TEAM },
    { "capture", M_NOAMMO | M_TACTICS | M_CAPTURE | M_TEAM },
    { "regen capture", M_NOITEMS | M_CAPTURE | M_REGEN | M_TEAM },
    { "ctf", M_CTF | M_TEAM },
    { "insta ctf", M_NOITEMS | M_INSTA | M_CTF | M_TEAM }
};

#define STARTGAMEMODE (-5)
#define NUMGAMEMODES ((int)(sizeof(gamemodes)/sizeof(gamemodes[0])))

#define m_valid(mode)          ((mode) >= STARTGAMEMODE && (mode) < STARTGAMEMODE + NUMGAMEMODES)
#define m_check(mode, flag)    (m_valid(mode) && gamemodes[(mode) - STARTGAMEMODE].flags&(flag))
#define m_checknot(mode, flag) (m_valid(mode) && !(gamemodes[(mode) - STARTGAMEMODE].flags&(flag)))
#define m_checkall(mode, flag) (m_valid(mode) && (gamemodes[(mode) - STARTGAMEMODE].flags&(flag)) == (flag))

#define m_noitems      (m_check(gamemode, M_NOITEMS))
#define m_noammo       (m_check(gamemode, M_NOAMMO|M_NOITEMS))
#define m_insta        (m_check(gamemode, M_INSTA))
#define m_tactics      (m_check(gamemode, M_TACTICS))
#define m_efficiency   (m_check(gamemode, M_EFFICIENCY))
#define m_capture      (m_check(gamemode, M_CAPTURE))
#define m_regencapture (m_checkall(gamemode, M_CAPTURE | M_REGEN))
#define m_ctf          (m_check(gamemode, M_CTF))
#define m_teammode     (m_check(gamemode, M_TEAM))
#define isteam(a,b)    (m_teammode && strcmp(a, b)==0)

#define m_demo         (m_check(gamemode, M_DEMO))
#define m_edit         (m_check(gamemode, M_EDIT))
#define m_lobby        (m_check(gamemode, M_LOBBY))
#define m_timed        (m_checknot(gamemode, M_DEMO|M_EDIT|M_LOCAL))
#define m_mp(mode)     (m_checknot(mode, M_LOCAL))

#define m_sp           (m_check(gamemode, M_DMSP | M_CLASSICSP))
#define m_dmsp         (m_check(gamemode, M_DMSP))
#define m_classicsp    (m_check(gamemode, M_CLASSICSP))
#define m_slowmo       (m_check(gamemode, M_SLOWMO))

#define modename(mode, failval) (m_valid(mode) ? gamemodes[(mode) - STARTGAMEMODE].name : failval)

// hardcoded sounds, defined in sounds.cfg
enum
{
    S_JUMP = 0, S_LAND, S_RIFLE, S_PUNCH1, S_SG, S_CG,
    S_RLFIRE, S_RLHIT, S_WEAPLOAD, S_ITEMAMMO, S_ITEMHEALTH,
    S_ITEMARMOUR, S_ITEMPUP, S_ITEMSPAWN, S_TELEPORT, S_NOAMMO, S_PUPOUT,
    S_PAIN1, S_PAIN2, S_PAIN3, S_PAIN4, S_PAIN5, S_PAIN6,
    S_DIE1, S_DIE2,
    S_FLAUNCH, S_FEXPLODE,
    S_SPLASH1, S_SPLASH2,
    S_GRUNT1, S_GRUNT2, S_RUMBLE,
    S_PAINO,
    S_PAINR, S_DEATHR,
    S_PAINE, S_DEATHE,
    S_PAINS, S_DEATHS,
    S_PAINB, S_DEATHB,
    S_PAINP, S_PIGGR2,
    S_PAINH, S_DEATHH,
    S_PAIND, S_DEATHD,
    S_PIGR1, S_ICEBALL, S_SLIMEBALL,
    S_JUMPPAD, S_PISTOL,
    
    S_V_BASECAP, S_V_BASELOST,
    S_V_FIGHT,
    S_V_BOOST, S_V_BOOST10,
    S_V_QUAD, S_V_QUAD10,
    S_V_RESPAWNPOINT, 

    S_FLAGPICKUP,
    S_FLAGDROP,
    S_FLAGRETURN,
    S_FLAGSCORE,
    S_FLAGRESET,

    S_BURN
};

// network messages codes, c2s, c2c, s2c

enum { PRIV_NONE = 0, PRIV_MASTER, PRIV_ADMIN };

enum
{
    SV_CONNECT = 0, SV_INITS2C, SV_WELCOME, SV_INITC2S, SV_POS, SV_TEXT, SV_SOUND, SV_CDIS,
    SV_SHOOT, SV_EXPLODE, SV_SUICIDE, 
    SV_DIED, SV_DAMAGE, SV_HITPUSH, SV_SHOTFX,
    SV_TRYSPAWN, SV_SPAWNSTATE, SV_SPAWN, SV_FORCEDEATH,
    SV_GUNSELECT, SV_TAUNT,
    SV_MAPCHANGE, SV_MAPVOTE, SV_ITEMSPAWN, SV_ITEMPICKUP, SV_ITEMACC,
    SV_PING, SV_PONG, SV_CLIENTPING,
    SV_TIMEUP, SV_MAPRELOAD, SV_FORCEINTERMISSION,
    SV_SERVMSG, SV_ITEMLIST, SV_RESUME,
    SV_EDITMODE, SV_EDITENT, SV_EDITF, SV_EDITT, SV_EDITM, SV_FLIP, SV_COPY, SV_PASTE, SV_ROTATE, SV_REPLACE, SV_DELCUBE, SV_REMIP, SV_NEWMAP, SV_GETMAP, SV_SENDMAP,
    SV_MASTERMODE, SV_KICK, SV_CLEARBANS, SV_CURRENTMASTER, SV_SPECTATOR, SV_SETMASTER, SV_SETTEAM,
    SV_BASES, SV_BASEINFO, SV_TEAMSCORE, SV_REPAMMO, SV_BASEREGEN, SV_ANNOUNCE,
    SV_LISTDEMOS, SV_SENDDEMOLIST, SV_GETDEMO, SV_SENDDEMO,
    SV_DEMOPLAYBACK, SV_RECORDDEMO, SV_STOPDEMO, SV_CLEARDEMOS,
    SV_TAKEFLAG, SV_RETURNFLAG, SV_RESETFLAG, SV_TRYDROPFLAG, SV_DROPFLAG, SV_SCOREFLAG, SV_INITFLAGS,
    SV_SAYTEAM,
    SV_CLIENT,
    SV_AUTHTRY, SV_AUTHCHAL, SV_AUTHANS
};

static char msgsizelookup(int msg)
{
    static char msgsizesl[] =               // size inclusive message token, 0 for variable or not-checked sizes
    {
        SV_CONNECT, 0, SV_INITS2C, 5, SV_WELCOME, 2, SV_INITC2S, 0, SV_POS, 0, SV_TEXT, 0, SV_SOUND, 2, SV_CDIS, 2,
        SV_SHOOT, 0, SV_EXPLODE, 0, SV_SUICIDE, 1,
        SV_DIED, 4, SV_DAMAGE, 6, SV_HITPUSH, 7, SV_SHOTFX, 9,
        SV_TRYSPAWN, 1, SV_SPAWNSTATE, 13, SV_SPAWN, 3, SV_FORCEDEATH, 2,
        SV_GUNSELECT, 2, SV_TAUNT, 1,
        SV_MAPCHANGE, 0, SV_MAPVOTE, 0, SV_ITEMSPAWN, 2, SV_ITEMPICKUP, 2, SV_ITEMACC, 3,
        SV_PING, 2, SV_PONG, 2, SV_CLIENTPING, 2,
        SV_TIMEUP, 2, SV_MAPRELOAD, 1, SV_FORCEINTERMISSION, 1,
        SV_SERVMSG, 0, SV_ITEMLIST, 0, SV_RESUME, 0,
        SV_EDITMODE, 2, SV_EDITENT, 11, SV_EDITF, 16, SV_EDITT, 16, SV_EDITM, 15, SV_FLIP, 14, SV_COPY, 14, SV_PASTE, 14, SV_ROTATE, 15, SV_REPLACE, 16, SV_DELCUBE, 14, SV_REMIP, 1, SV_NEWMAP, 2, SV_GETMAP, 1, SV_SENDMAP, 0,
        SV_MASTERMODE, 2, SV_KICK, 2, SV_CLEARBANS, 1, SV_CURRENTMASTER, 3, SV_SPECTATOR, 3, SV_SETMASTER, 0, SV_SETTEAM, 0,
        SV_BASES, 0, SV_BASEINFO, 0, SV_TEAMSCORE, 0, SV_REPAMMO, 1, SV_BASEREGEN, 6, SV_ANNOUNCE, 2,
        SV_LISTDEMOS, 1, SV_SENDDEMOLIST, 0, SV_GETDEMO, 2, SV_SENDDEMO, 0,
        SV_DEMOPLAYBACK, 3, SV_RECORDDEMO, 2, SV_STOPDEMO, 1, SV_CLEARDEMOS, 2,
        SV_TAKEFLAG, 2, SV_RETURNFLAG, 3, SV_RESETFLAG, 2, SV_TRYDROPFLAG, 1, SV_DROPFLAG, 6, SV_SCOREFLAG, 5, SV_INITFLAGS, 6,   
        SV_SAYTEAM, 0, 
        SV_CLIENT, 0,
        SV_AUTHTRY, 0, SV_AUTHCHAL, 0, SV_AUTHANS, 0,
        -1
    };
    for(char *p = msgsizesl; *p>=0; p += 2) if(*p==msg) return p[1];
    return -1;
}

#define SAUERBRATEN_SERVER_PORT 28785
#define SAUERBRATEN_SERVINFO_PORT 28786
#define SAUERBRATEN_AUTH_PORT 28787
#define PROTOCOL_VERSION 257            // bump when protocol changes
#define DEMO_VERSION 1                  // bump when demo format changes
#define DEMO_MAGIC "SAUERBRATEN_DEMO"

struct demoheader
{
    char magic[16]; 
    int version, protocol;
};

#define MAXNAMELEN 15
#define MAXTEAMLEN 4

static struct itemstat { int add, max, sound; const char *name; int info; } itemstats[] =
{
    {10,    30,    S_ITEMAMMO,   "SG", GUN_SG},
    {20,    60,    S_ITEMAMMO,   "CG", GUN_CG},
    {5,     15,    S_ITEMAMMO,   "RL", GUN_RL},
    {5,     15,    S_ITEMAMMO,   "RI", GUN_RIFLE},
    {10,    30,    S_ITEMAMMO,   "GL", GUN_GL},
    {30,    120,   S_ITEMAMMO,   "PI", GUN_PISTOL},
    {25,    100,   S_ITEMHEALTH, "H"},
    {10,    1000,  S_ITEMHEALTH, "MH"},
    {100,   100,   S_ITEMARMOUR, "GA", A_GREEN},
    {200,   200,   S_ITEMARMOUR, "YA", A_YELLOW},
    {20000, 30000, S_ITEMPUP,    "Q"},
};

#define SGRAYS 20
#define SGSPREAD 4
#define RL_DAMRAD 40
#define RL_SELFDAMDIV 2
#define RL_DISTSCALE 1.5f

static struct guninfo { short sound, attackdelay, damage, projspeed, part, kickamount, range; const char *name, *file; } guns[NUMGUNS] =
{
    { S_PUNCH1,    250,  50, 0,   0, 0,   12,  "fist",            "fist"  },
    { S_SG,       1400,  10, 0,   0, 20, 1024, "shotgun",         "shotg" },  // *SGRAYS
    { S_CG,        100,  30, 0,   0, 7, 1024,  "chaingun",        "chaing"},
    { S_RLFIRE,    800, 120, 80,  0, 10, 1024, "rocketlauncher",  "rocket"},
    { S_RIFLE,    1500, 100, 0,   0, 30, 2048, "rifle",           "rifle" },
    { S_FLAUNCH,   500,  75, 80,  0, 10, 1024, "grenadelauncher", "gl" },
    { S_PISTOL,    500,  25, 0,   0,  7, 1024, "pistol",          "pistol" },
    { S_FLAUNCH,   200,  20, 50,  PART_FIREBALL1,  1, 1024, "fireball",  NULL },
    { S_ICEBALL,   200,  40, 30,  PART_FIREBALL2,  1, 1024, "iceball",   NULL },
    { S_SLIMEBALL, 200,  30, 160, PART_FIREBALL3,  1, 1024, "slimeball", NULL },
    { S_PIGR1,     250,  50, 0,   0,  1,   12, "bite",            NULL },
    { -1,            0, 120, 0,   0,  0,    0, "barrel",          NULL }
};

// inherited by fpsent and server clients
struct fpsstate
{
    int health, maxhealth;
    int armour, armourtype;
    int quadmillis;
    int gunselect, gunwait;
    int ammo[NUMGUNS];

    fpsstate() : maxhealth(100) {}

    void baseammo(int gun, int k = 2, int scale = 1)
    {
        ammo[gun] = (itemstats[gun-GUN_SG].add*k)/scale;
    }

    void addammo(int gun, int k = 1, int scale = 1)
    {
        itemstat &is = itemstats[gun-GUN_SG];
        ammo[gun] = min(ammo[gun] + (is.add*k)/scale, is.max);
    }

    bool hasmaxammo(int type)
    {
       const itemstat &is = itemstats[type-I_SHELLS];
       return ammo[type-I_SHELLS+GUN_SG]>=is.max;
    }

    bool canpickup(int type)
    {
        if(type<I_SHELLS || type>I_QUAD) return false;
        itemstat &is = itemstats[type-I_SHELLS];
        switch(type)
        {
            case I_BOOST: return maxhealth<is.max;
            case I_HEALTH: return health<maxhealth;
            case I_GREENARMOUR:
                // (100h/100g only absorbs 200 damage)
                if(armourtype==A_YELLOW && armour>=100) return false;
            case I_YELLOWARMOUR: return !armourtype || armour<is.max;
            case I_QUAD: return quadmillis<is.max;
            default: return ammo[is.info]<is.max;
        }
    }
 
    void pickup(int type)
    {
        if(type<I_SHELLS || type>I_QUAD) return;
        itemstat &is = itemstats[type-I_SHELLS];
        switch(type)
        {
            case I_BOOST:
                maxhealth = min(maxhealth+is.add, is.max);
            case I_HEALTH: // boost also adds to health
                health = min(health+is.add, maxhealth);
                break;
            case I_GREENARMOUR:
            case I_YELLOWARMOUR:
                armour = min(armour+is.add, is.max);
                armourtype = is.info;
                break;
            case I_QUAD:
                quadmillis = min(quadmillis+is.add, is.max);
                break;
            default:
                ammo[is.info] = min(ammo[is.info]+is.add, is.max);
                break;
        }
    }

    void respawn()
    {
        health = maxhealth;
        armour = 0;
        armourtype = A_BLUE;
        quadmillis = 0;
        gunselect = GUN_PISTOL;
        gunwait = 0;
        loopi(NUMGUNS) ammo[i] = 0;
        ammo[GUN_FIST] = 1;
    }

    void spawnstate(int gamemode)
    {
        if(m_demo)
        {
            gunselect = GUN_FIST;
        }
        else if(m_insta)
        {
            armour = 0;
            health = 1;
            gunselect = GUN_RIFLE;
            ammo[GUN_RIFLE] = 100;
        }
        else if(m_regencapture)
        {
            armourtype = A_GREEN;
            armour = 0;
            gunselect = GUN_PISTOL;
            ammo[GUN_PISTOL] = 40;
            ammo[GUN_GL] = 1;
        }
        else if(m_tactics)
        {
            armourtype = A_GREEN;
            armour = 100;
            ammo[GUN_PISTOL] = 80;
            int spawngun1 = rnd(5)+1, spawngun2;
            gunselect = spawngun1;
            baseammo(spawngun1, m_capture ? 1 : 2);
            do spawngun2 = rnd(5)+1; while(spawngun1==spawngun2);
            baseammo(spawngun2, m_capture ? 1 : 2);
            if(!m_capture) ammo[GUN_GL] += 1;
        }
        else if(m_efficiency)
        {
            armourtype = A_GREEN;
            armour = 100;
            loopi(5) baseammo(i+1);
            gunselect = GUN_CG;
            ammo[GUN_CG] /= 2;
        }
        else if(m_ctf)
        {
            armourtype = A_BLUE;
            armour = 50;
            ammo[GUN_PISTOL] = 40;
            ammo[GUN_GL] = 1;
        }
        else
        {
            ammo[GUN_PISTOL] = m_sp ? 80 : 40;
            ammo[GUN_GL] = 1;
        }
    }

    // just subtract damage here, can set death, etc. later in code calling this 
    int dodamage(int damage)
    {
        int ad = damage*(armourtype+1)*25/100; // let armour absorb when possible
        if(ad>armour) ad = armour;
        armour -= ad;
        damage -= ad;
        health -= damage;
        return damage;        
    }
};

struct fpsent : dynent, fpsstate
{   
    int weight;                         // affects the effectiveness of hitpush
    int clientnum, privilege, lastupdate, plag, ping;
    int lifesequence;                   // sequence id for each respawn, used in damage test
    int lastpain;
    int lastaction, lastattackgun;
    bool attacking;
    int lasttaunt;
    int lastpickup, lastpickupmillis, lastbase;
    int superdamage;
    int frags, deaths, totaldamage, totalshots;
    editinfo *edit;
    float deltayaw, deltapitch, newyaw, newpitch;
    int smoothmillis;

    string name, team, info;
    int playermodel;

    vec muzzle;

    fpsent() : weight(100), clientnum(-1), privilege(PRIV_NONE), lastupdate(0), plag(0), ping(0), lifesequence(0), lastpain(0), frags(0), deaths(0), totaldamage(0), totalshots(0), edit(NULL), smoothmillis(-1), playermodel(-1), muzzle(-1, -1, -1)
               { name[0] = team[0] = info[0] = 0; respawn(); }
    ~fpsent() { freeeditinfo(edit); }

    void damageroll(float damage)
    {
        float damroll = 2.0f*damage;
        roll += roll>0 ? damroll : (roll<0 ? -damroll : (rnd(2) ? damroll : -damroll)); // give player a kick
        clamproll(this);
    }

    void hitpush(int damage, const vec &dir, fpsent *actor, int gun)
    {
        vec push(dir);
        push.mul(80*damage/weight);
        if(gun==GUN_RL || gun==GUN_GL) push.mul(actor==this ? 5 : (type==ENT_AI ? 3 : 2));
        vel.add(push);
    }

    void respawn()
    {
        dynent::reset();
        fpsstate::respawn();
        lastaction = 0;
        lastattackgun = gunselect;
        attacking = false;
        lasttaunt = 0;
        lastpickup = -1;
        lastpickupmillis = 0;
        lastbase = -1;
        superdamage = 0;
    }
};

struct teamscore
{
    const char *team;
    int score;
    teamscore() {}
    teamscore(const char *s, int n) : team(s), score(n) {}
};


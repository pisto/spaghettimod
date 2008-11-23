// sound.cpp: basic positional sound using sdl_mixer

#include "pch.h"
#include "engine.h"

#include "SDL_mixer.h"
#define MAXVOL MIX_MAX_VOLUME
Mix_Music *mod = NULL;

bool nosound = true;

struct sample
{
    char *name;
    Mix_Chunk *sound;

    sample() : name(NULL) {}
    ~sample() { DELETEA(name); }
};

struct soundslot
{
    sample *s;
    int vol;
    int uses, maxuses;
};

struct soundloc { vec loc; bool inuse; soundslot *slot; extentity *ent; };
vector<soundloc> soundlocs;

void setmusicvol(int musicvol)
{
    if(nosound) return;
    if(mod) Mix_VolumeMusic((musicvol*MAXVOL)/255);
}

VARP(soundvol, 0, 255, 255);
VARFP(musicvol, 0, 128, 255, setmusicvol(musicvol));

char *musicfile = NULL, *musicdonecmd = NULL;

void stopsound()
{
    if(nosound) return;
    DELETEA(musicfile);
    DELETEA(musicdonecmd);
    if(mod)
    {
        Mix_HaltMusic();
        Mix_FreeMusic(mod);
        mod = NULL;
    }
}

VARF(soundchans, 0, 32, 128, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundfreq, 0, MIX_DEFAULT_FREQUENCY, 44100, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));
VARF(soundbufferlen, 128, 1024, 4096, initwarning("sound configuration", INIT_RESET, CHANGE_SOUND));

void initsound()
{
    if(Mix_OpenAudio(soundfreq, MIX_DEFAULT_FORMAT, 2, soundbufferlen)<0)
    {
        conoutf(CON_ERROR, "sound init failed (SDL_mixer): %s", (size_t)Mix_GetError());
        return;
    }
	Mix_AllocateChannels(soundchans);	
    nosound = false;
}

void musicdone()
{
    if(!musicdonecmd) return;
    if(mod) Mix_FreeMusic(mod);
    mod = NULL;
    DELETEA(musicfile);
    char *cmd = musicdonecmd;
    musicdonecmd = NULL;
    execute(cmd);
    delete[] cmd;
}

void music(char *name, char *cmd)
{
    if(nosound) return;
    stopsound();
    if(soundvol && musicvol && *name)
    {
        if(cmd[0]) musicdonecmd = newstring(cmd);
        s_sprintfd(sn)("packages/%s", name);
        const char *file = findfile(path(sn), "rb");
        if((mod = Mix_LoadMUS(file)))
        {
            musicfile = newstring(file);
            Mix_PlayMusic(mod, cmd[0] ? 0 : -1);
            Mix_VolumeMusic((musicvol*MAXVOL)/255);
        }
        else
        {
            conoutf(CON_ERROR, "could not play music: %s", sn);
        }
    }
}

COMMAND(music, "ss");

hashtable<const char *, sample> samples;
vector<soundslot> gamesounds, mapsounds;

int findsound(const char *name, int vol, vector<soundslot> &sounds)
{
    loopv(sounds)
    {
        if(!strcmp(sounds[i].s->name, name) && (!vol || sounds[i].vol==vol)) return i;
    }
    return -1;
}

int addsound(const char *name, int vol, int maxuses, vector<soundslot> &sounds)
{
    sample *s = samples.access(name);
    if(!s)
    {
        char *n = newstring(name);
        s = &samples[n];
        s->name = n;
        s->sound = NULL;
    }
    soundslot &slot = sounds.add();
    slot.s = s;
    slot.vol = vol ? vol : 100;
    slot.uses = 0;
    slot.maxuses = maxuses;
    return sounds.length()-1;
}

void registersound(char *name, int *vol) { intret(addsound(name, *vol, 0, gamesounds)); }
COMMAND(registersound, "si");

void mapsound(char *name, int *vol, int *maxuses) { intret(addsound(name, *vol, *maxuses < 0 ? 0 : max(1, *maxuses), mapsounds)); }
COMMAND(mapsound, "sii");

void clear_sound()
{
    closemumble();
    if(nosound) return;
    stopsound();
    gamesounds.setsizenodelete(0);
    mapsounds.setsizenodelete(0);
    samples.clear();
    Mix_CloseAudio();
}

void clearsoundlocs()
{
    loopv(soundlocs) if(soundlocs[i].inuse && soundlocs[i].ent)
    {
        if(Mix_Playing(i)) Mix_HaltChannel(i);
        soundlocs[i].inuse = false;
        soundlocs[i].ent->visible = false;
        soundlocs[i].slot->uses--;
    }
    soundlocs.setsize(0);
}

void clearmapsounds()
{
    clearsoundlocs();
    mapsounds.setsizenodelete(0);
}

void checkmapsounds()
{
    const vector<extentity *> &ents = et->getents();
    loopv(ents)
    {
        extentity &e = *ents[i];
        if(e.type!=ET_SOUND || e.visible || camera1->o.dist(e.o)>=e.attr2) continue;
        playsound(e.attr1, NULL, &e);
    }
}

VAR(stereo, 0, 1, 1);

void updatechanvol(int chan, int svol, const vec *loc = NULL, extentity *ent = NULL)
{
    int vol = soundvol, pan = 255/2;
    if(loc)
    {
        vec v;
        float dist = camera1->o.dist(*loc, v);
        if(ent)
        {
            int rad = ent->attr2;
            if(ent->attr3)
            {
                rad -= ent->attr3;
                dist -= ent->attr3;
            }
            vol -= (int)(min(max(dist/rad, 0.0f), 1.0f)*soundvol);
        }
        else
        {
            vol -= (int)(dist*3/4*soundvol/255); // simple mono distance attenuation
            if(vol<0) vol = 0;
        }
        if(stereo && (v.x != 0 || v.y != 0) && dist>0)
        {
            float yaw = -atan2f(v.x, v.y) - camera1->yaw*RAD; // relative angle of sound along X-Y axis
            pan = int(255.9f*(0.5f*sinf(yaw)+0.5f)); // range is from 0 (left) to 255 (right)
        }
    }
    vol = (vol*MAXVOL*svol)/255/255;
    vol = min(vol, MAXVOL);
    Mix_Volume(chan, vol);
    Mix_SetPanning(chan, 255-pan, pan);
}  

void newsoundloc(int chan, const vec *loc, soundslot *slot, extentity *ent = NULL)
{
    while(chan >= soundlocs.length()) soundlocs.add().inuse = false;
    soundlocs[chan].loc = *loc;
    soundlocs[chan].inuse = true;
    soundlocs[chan].slot = slot;
    soundlocs[chan].ent = ent;
}

void updatevol()
{
    updatemumble();
    if(nosound) return;
    loopv(soundlocs) if(soundlocs[i].inuse)
    {
        if(Mix_Playing(i))
            updatechanvol(i, soundlocs[i].slot->vol, &soundlocs[i].loc, soundlocs[i].ent);
        else 
        {
            soundlocs[i].inuse = false;
            if(soundlocs[i].ent) 
            {
                soundlocs[i].ent->visible = false;
                soundlocs[i].slot->uses--;
            }
        }
    }
    if(mod && !Mix_PlayingMusic()) musicdone();
}

VARP(maxsoundsatonce, 0, 5, 100);

void playsound(int n, const vec *loc, extentity *ent)
{
    if(nosound) return;
    if(!soundvol) return;

    if(!ent)
    {
        static int soundsatonce = 0, lastsoundmillis = 0;
        if(totalmillis==lastsoundmillis) soundsatonce++; else soundsatonce = 1;
        lastsoundmillis = totalmillis;
        if(maxsoundsatonce && soundsatonce>maxsoundsatonce) return;  // avoid bursts of sounds with heavy packetloss and in sp
    }

    vector<soundslot> &sounds = ent ? mapsounds : gamesounds;
    if(!sounds.inrange(n)) { conoutf(CON_WARN, "unregistered sound: %d", n); return; }
    soundslot &slot = sounds[n];
    if(ent && slot.maxuses && slot.uses>=slot.maxuses) return;

    if(!slot.s->sound)
    {
        const char *exts[] = { "", ".wav", ".ogg" };
        string buf;
        loopi(sizeof(exts)/sizeof(exts[0]))
        {
            s_sprintf(buf)("packages/sounds/%s%s", slot.s->name, exts[i]);
            const char *file = findfile(path(buf), "rb");
            slot.s->sound = Mix_LoadWAV(file);
            if(slot.s->sound) break;
        }

        if(!slot.s->sound) { conoutf(CON_ERROR, "failed to load sample: %s", buf); return; }
    }

    int chan = Mix_PlayChannel(-1, slot.s->sound, 0);
    if(chan<0) return;

    if(ent)
    {
        loc = &ent->o;
        ent->visible = true;
        slot.uses++;
    }
    if(loc) newsoundloc(chan, loc, &slot, ent);
    updatechanvol(chan, slot.vol, loc, ent);
}

void playsoundname(const char *s, const vec *loc, int vol) 
{ 
    if(!vol) vol = 100;
    int id = findsound(s, vol, gamesounds);
    if(id < 0) id = addsound(s, vol, 0, gamesounds);
    playsound(id, loc);
}

void sound(int *n) { playsound(*n); }
COMMAND(sound, "i");

void resetsound()
{
    const SDL_version *v = Mix_Linked_Version();
    if(SDL_VERSIONNUM(v->major, v->minor, v->patch) <= SDL_VERSIONNUM(1, 2, 8))
    {
        conoutf(CON_ERROR, "Sound reset not available in-game due to SDL_mixer-1.2.8 bug. Please restart for changes to take effect.");
        return;
    }
    clearchanges(CHANGE_SOUND);
    if(!nosound) 
    {
        clearsoundlocs();
        enumerate(samples, sample, s, { Mix_FreeChunk(s.sound); s.sound = NULL; });
        if(mod)
        {
            Mix_HaltMusic();
            Mix_FreeMusic(mod);
        }
        Mix_CloseAudio();
    }
    initsound();
    if(nosound)
    {
        DELETEA(musicfile);
        DELETEA(musicdonecmd);
        mod = NULL;
        gamesounds.setsizenodelete(0);
        mapsounds.setsizenodelete(0);
        samples.clear();
        return;
    }
    if(mod && (mod = Mix_LoadMUS(musicfile)))
    {
        Mix_PlayMusic(mod, musicdonecmd[0] ? 0 : -1);
        Mix_VolumeMusic((musicvol*MAXVOL)/255);
    }
}

COMMAND(resetsound, "");

#ifdef WIN32

#include <wchar.h>

#else

#include <unistd.h>

#ifdef _POSIX_SHARED_MEMORY_OBJECTS
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <wchar.h>
#endif

#endif

#if defined(WIN32) || defined(_POSIX_SHARED_MEMORY_OBJECTS)
struct MumbleInfo
{
    int version, timestamp;
    vec pos, front, top;
    wchar_t name[256];
};
#endif

#ifdef WIN32
static HANDLE mumblelink = NULL;
static MumbleInfo *mumbleinfo = NULL;
#define VALID_MUMBLELINK (mumblelink && mumbleinfo)
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
static int mumblelink = -1;
static MumbleInfo *mumbleinfo = (MumbleInfo *)-1; 
#define VALID_MUMBLELINK (mumblelink >= 0 && mumbleinfo != (MumbleInfo *)-1)
#endif

#ifdef VALID_MUMBLELINK
VARFP(mumble, 0, 1, 1, { if(mumble) initmumble(); else closemumble(); });
#else
VARFP(mumble, 0, 0, 1, { if(mumble) initmumble(); else closemumble(); });
#endif

void initmumble()
{
    if(!mumble) return;
#ifdef VALID_MUMBLELINK
    if(VALID_MUMBLELINK) return;

    #ifdef WIN32
        mumblelink = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, "MumbleLink");
        if(mumblelink)
        {
            mumbleinfo = (MumbleInfo *)MapViewOfFile(mumblelink, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(MumbleInfo));
            if(mumbleinfo) wcsncpy(mumbleinfo->name, L"Sauerbraten", 256);
        }
    #elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
        s_sprintfd(shmname)("/MumbleLink.%d", getuid());
        mumblelink = shm_open(shmname, O_RDWR, 0);
        if(mumblelink >= 0)
        {
            mumbleinfo = (MumbleInfo *)mmap(NULL, sizeof(MumbleInfo), PROT_READ|PROT_WRITE, MAP_SHARED, mumblelink, 0);
            if(mumbleinfo != (MumbleInfo *)-1) wcsncpy(mumbleinfo->name, L"Sauerbraten", 256);
        }
    #endif
    if(!VALID_MUMBLELINK) closemumble();
#else
    conoutf(CON_ERROR, "Mumble positional audio is not available on this platform.");
#endif
}

void closemumble()
{
#ifdef WIN32
    if(mumbleinfo) { UnmapViewOfFile(mumbleinfo); mumbleinfo = NULL; }
    if(mumblelink) { CloseHandle(mumblelink); mumblelink = NULL; }
#elif defined(_POSIX_SHARED_MEMORY_OBJECTS)
    if(mumbleinfo != (MumbleInfo *)-1) { munmap(mumbleinfo, sizeof(MumbleInfo)); mumbleinfo = (MumbleInfo *)-1; } 
    if(mumblelink >= 0) { close(mumblelink); mumblelink = -1; }
#endif
}

static inline vec mumblevec(const vec &v, bool pos = false)
{
    // change from Z up, -Y forward to Y up, +Z forward
    // 8 cube units = 1 meter
    vec m(v.x, v.z, -v.y);
    if(pos) m.div(8);
    return m;
}

void updatemumble()
{
#ifdef VALID_MUMBLELINK
    if(!VALID_MUMBLELINK) return;

    static int timestamp = 0;

    mumbleinfo->version = 1;
    mumbleinfo->timestamp = ++timestamp;

    mumbleinfo->pos = mumblevec(player->o, true);
    mumbleinfo->front = mumblevec(vec(RAD*player->yaw, RAD*player->pitch));
    mumbleinfo->top = mumblevec(vec(RAD*player->yaw, RAD*(player->pitch+90)));
#endif
}


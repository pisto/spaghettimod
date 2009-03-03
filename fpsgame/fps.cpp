#include "pch.h"

#include "cube.h"

#include "iengine.h"
#include "igame.h"

#include "game.h"

#include "hash.h"
#include "fpsserver.h"

#ifndef STANDALONE

#include "crypto.h"

struct fpsclient : igameclient
{
    struct clientmode
    {
        fpsclient &cl;

        clientmode(fpsclient &cl) : cl(cl) {}
        virtual ~clientmode() {}

        virtual void preload() {}
        virtual void drawhud(fpsent *d, int w, int h) {}
        virtual void rendergame() {}
        virtual void respawned() {}
        virtual void setup() {}
        virtual void checkitems(fpsent *d) {}
        virtual int respawnwait(fpsent *d) { return 0; }
        virtual void pickspawn(fpsent *d) { findplayerspawn(d); }
        virtual void senditems(ucharbuf &p) {}
        virtual const char *prefixnextmap() { return ""; }
        virtual void removeplayer(fpsent *d) {}
        virtual bool hidefrags() { return false; }
        virtual int getteamscore(const char *team) { return 0; }
        virtual void getteamscores(vector<teamscore> &scores) {}
    };

    #define gamemode cl.gamemode

    // these define classes local to fpsclient
    #include "weapon.h"
    #include "monster.h"
    #include "movable.h"
    #include "scoreboard.h"
    #include "fpsrender.h"
    #include "entities.h"
    #include "client.h"
    #include "capture.h"
    #include "ctf.h"

    #undef gamemode

    int nextmode, gamemode;         // nextmode becomes gamemode after next map load
    bool intermission;
    string clientmap;
    int maptime, maprealtime, minremain;
    int respawnent;
    int swaymillis;
    vec swaydir;
    int respawned, suicided;
    int lasthit, lastspawnattempt;

    int following, followdir;

    bool openmainmenu;

    fpsent *player1;                // our client
    vector<fpsent *> players;       // other clients
    fpsent lastplayerstate;

    weaponstate ws;
    monsterset  ms;
    movableset  mo;
    scoreboard  sb;
    fpsrender   fr;
    entities    et;
    clientcom   cc;

    IVARP(maxradarscale, 0, 1024, 10000);

    clientmode *cmode;
    captureclientmode capturemode;
    ctfclientmode ctfmode;

    int lastauth;
    string authname;
    gfint authkey;

    fpsclient()
        : nextmode(0), gamemode(0), intermission(false),
          maptime(0), minremain(0), respawnent(-1),
          swaymillis(0), swaydir(0, 0, 0),
          respawned(-1), suicided(-1), 
          lasthit(0), lastspawnattempt(0),
          following(-1), followdir(0), openmainmenu(true),
          player1(spawnstate(new fpsent())),
          ws(*this), ms(*this), mo(*this), sb(*this), fr(*this), et(*this), cc(*this), 
          cmode(NULL), capturemode(*this), ctfmode(*this),
          lastauth(0)
    {
        authname[0] = '\0';

        CCOMMAND(mode, "i", (fpsclient *self, int *val), { self->setmode(*val); });
        CCOMMAND(kill, "",  (fpsclient *self), { self->suicide(self->player1); });
        CCOMMAND(taunt, "", (fpsclient *self), { self->taunt(); });
        CCOMMAND(follow, "s", (fpsclient *self, char *s), { self->follow(s); });
        CCOMMAND(nextfollow, "i", (fpsclient *self, int *dir), { self->nextfollow(*dir < 0 ? -1 : 1); });
        CCOMMAND(authkey, "ss", (fpsclient *self, char *name, char *key), { self->setauthkey(name, key); });
    }

    iclientcom      *getcom()  { return &cc; }
    icliententities *getents() { return &et; }

    void setmode(int mode)
    {
        if(multiplayer(false) && !m_mp(mode)) { conoutf(CON_ERROR, "mode %d not supported in multiplayer", mode); return; }
        nextmode = mode;
    }

    void taunt()
    {
        if(player1->state!=CS_ALIVE || player1->physstate<PHYS_SLOPE) return;
        if(lastmillis-player1->lasttaunt<1000) return;
        player1->lasttaunt = lastmillis;
        cc.addmsg(SV_TAUNT, "r");
    }

	void follow(char *arg)
    {
        if(arg[0] ? player1->state==CS_SPECTATOR : following>=0)
        {
            following = arg[0] ? cc.parseplayer(arg) : -1;
            if(following==player1->clientnum) following = -1;
            followdir = 0;
            conoutf("follow %s", following>=0 ? "on" : "off");
        }
	}

    void nextfollow(int dir)
    {
        if(player1->state!=CS_SPECTATOR || players.empty())
        {
            stopfollowing();
            return;
        }
        int cur = following >= 0 ? following : (dir < 0 ? players.length() - 1 : 0);
        loopv(players) 
        {
            cur = (cur + dir + players.length()) % players.length();
            if(players[cur] && players[cur]->state!=CS_SPECTATOR)
            {
                if(following<0) conoutf("follow on");
                following = cur;
                followdir = dir;
                return;
            }
        }
        stopfollowing();
    }

    void setauthkey(const char *name, const char *key)
    {
        s_strcpy(authname, name);
        authkey.parse(key);
    }

    char *getclientmap() { return clientmap; }

    void adddynlights() { ws.adddynlights(); }

    void rendergame(bool mainpass) { fr.rendergame(mainpass); }

    void resetgamestate()
    {
        if(m_classicsp)
        {
            mo.clear();
            ms.monsterclear();                 // all monsters back at their spawns for editing
            resettriggers();
        }
        ws.projreset();
    }

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }

    void respawnself()
    {
        if(m_mp(gamemode)) 
        {
            if(respawned!=player1->lifesequence)
            {
                cc.addmsg(SV_TRYSPAWN, "r");
                respawned = player1->lifesequence;
            }
        }
        else
        {
            spawnplayer(player1);
            sb.showscores(false);
            lasthit = 0;
            if(cmode) cmode->respawned();
        }
    }

    fpsent *pointatplayer()
    {
        loopv(players)
        {
            fpsent *o = players[i];
            if(!o) continue;
            if(intersect(o, player1->o, worldpos)) return o;
        }
        return NULL;
    }

    void stopfollowing()
    {
        if(following<0) return;
        following = -1;
        followdir = 0;
        conoutf("follow off");
    }

    fpsent *followingplayer()
    {
        if(player1->state!=CS_SPECTATOR || following<0) return NULL;
        fpsent *target = getclient(following);
        if(target && target->state!=CS_SPECTATOR) return target;
        return NULL;
    }

    fpsent *hudplayer()
    {
        extern int thirdperson;
        if(thirdperson) return player1;
        fpsent *target = followingplayer();
        return target ? target : player1;
    }

    void setupcamera()
    {
        fpsent *target = followingplayer();
        if(target) 
        {
            player1->yaw = target->yaw;    
            player1->pitch = target->state==CS_DEAD ? 0 : target->pitch;
            player1->o = target->o;
            player1->resetinterp();
        }
    }

    bool detachcamera()
    {
        fpsent *d = hudplayer();
        return d->state==CS_DEAD;
    }

    IVARP(smoothmove, 0, 75, 100);
    IVARP(smoothdist, 0, 32, 64);

    void predictplayer(fpsent *d, bool move)
    {
        d->o = d->newpos;
        d->yaw = d->newyaw;
        d->pitch = d->newpitch;
        if(move)
        {
            moveplayer(d, 1, false);
            d->newpos = d->o;
        }
        float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove();
        if(k>0)
        {
            d->o.add(vec(d->deltapos).mul(k));
            d->yaw += d->deltayaw*k;
            if(d->yaw<0) d->yaw += 360;
            else if(d->yaw>=360) d->yaw -= 360;
            d->pitch += d->deltapitch*k;
        }
    }
         
    void otherplayers(int curtime)
    {
        loopv(players) if(players[i])
        {
            fpsent *d = players[i];
            
            if(d->state==CS_ALIVE)
            {
                if(lastmillis - d->lastaction >= d->gunwait) d->gunwait = 0; 
                if(d->quadmillis) et.checkquad(curtime, d);
            }
            else if(d->state==CS_DEAD && d->ragdoll) moveragdoll(d);

            const int lagtime = lastmillis-d->lastupdate;
            if(!lagtime || intermission) continue;
            else if(lagtime>1000 && d->state==CS_ALIVE)
            {
                d->state = CS_LAGGED;
                continue;
            }
            if(d->state==CS_ALIVE || d->state==CS_EDITING)
            {
                if(smoothmove() && d->smoothmillis>0) predictplayer(d, true);
                else moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
        }
    }

    void addsway(int curtime)
    {
        fpsent *d = hudplayer();
        if(d->state!=CS_SPECTATOR)
        {
            if(d->physstate>=PHYS_SLOPE) swaymillis += curtime;
            float k = pow(0.7f, curtime/10.0f);
            swaydir.mul(k);
            vec vel(d->vel);
            vel.add(d->falling);
            swaydir.add(vec(vel).mul((1-k)/(15*max(vel.magnitude(), d->maxspeed))));
        }
    }

    void checkslowmo()
    {
        static int lastslowmohealth = 0;
        setvar("gamespeed", intermission ? 100 : player1->health);
        if(player1->health<player1->maxhealth && lastmillis-max(maptime, lastslowmohealth)>player1->health*player1->health/2)
        {
            lastslowmohealth = lastmillis;
            player1->health++;
        }
    }

    void updateworld()        // main game update loop
    {
        if(!maptime) { maptime = lastmillis; maprealtime = totalmillis; return; }
        if(!curtime) return;

        physicsframe();
        et.checkquad(curtime, player1);
        ws.moveprojectiles(curtime);
        if(player1->clientnum>=0 && player1->state==CS_ALIVE) ws.shoot(player1, worldpos); // only shoot when connected to server
        ws.bounceupdate(curtime); // need to do this after the player shoots so grenades don't end up inside player's BB next frame
        otherplayers(curtime);
        fr.moveragdolls();
        gets2c();
        mo.update(curtime);
        ms.monsterthink(curtime);
        if(player1->state==CS_DEAD)
        {
            if(player1->ragdoll) moveragdoll(player1);
            else if(lastmillis-player1->lastpain<2000)
            {
                player1->move = player1->strafe = 0;
                moveplayer(player1, 10, false);
            }
        }
        else if(!intermission)
        {
            if(player1->ragdoll) cleanragdoll(player1);
            moveplayer(player1, 10, true);
            addsway(curtime);
            et.checkitems(player1);
            if(m_slowmo) checkslowmo();
            if(m_classicsp) checktriggers();
            else if(cmode) cmode->checkitems(player1);
        }
        if(player1->clientnum>=0) c2sinfo(player1);   // do this last, to reduce the effective frame lag
    }

    void spawnplayer(fpsent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, respawnent>=0 ? respawnent : -1);
        spawnstate(d);
        d->state = cc.spectator ? CS_SPECTATOR : (d==player1 && editmode ? CS_EDITING : CS_ALIVE);
    }

    IVARP(spawnwait, 0, 0, 1000);

    void respawn()
    {
        if(player1->state==CS_DEAD)
        {
            player1->attacking = false;
            int wait = cmode ? cmode->respawnwait(player1) : 0;
            if(wait>0)
            {
                lastspawnattempt = lastmillis; 
                //conoutf(CON_GAMEINFO, "\f2you must wait %d second%s before respawn!", wait, wait!=1 ? "s" : "");
                return;
            }
            if(lastmillis < player1->lastpain + spawnwait()) return;
            if(m_dmsp) { nextmode = gamemode; cc.changemap(clientmap); return; }    // if we die in SP we try the same map again
            if(m_classicsp)
            {
                respawnself();
                conoutf(CON_GAMEINFO, "\f2You wasted another life! The monsters stole your armour and some ammo...");
                loopi(NUMGUNS) if(i!=GUN_PISTOL && (player1->ammo[i] = lastplayerstate.ammo[i])>5) player1->ammo[i] = max(player1->ammo[i]/3, 5);
                return;
            }
            respawnself();
        }
    }

    // inputs

    void doattack(bool on)
    {
        if(intermission) return;
        if((player1->attacking = on)) respawn();
    }

    bool canjump() 
    { 
        if(!intermission) respawn(); 
        return player1->state!=CS_DEAD && !intermission; 
    }

    bool allowmove(physent *d)
    {
        if(d->type!=ENT_PLAYER) return true;
        return !((fpsent *)d)->lasttaunt || lastmillis-((fpsent *)d)->lasttaunt>=1000;
    }

    void damaged(int damage, fpsent *d, fpsent *actor, bool local = true)
    {
        if(d->state!=CS_ALIVE || intermission) return;

        fpsent *h = local ? player1 : hudplayer();
        if(actor==h && d!=actor) lasthit = lastmillis;

        if(local) damage = d->dodamage(damage);
        else if(actor==player1) return;

        if(d==h)
        {
            damageblend(damage);
            damagecompass(damage, actor->o);
        }
        ws.damageeffect(damage, d, d!=h);

        if(d->health<=0) { if(local) killed(d, actor); }
        else if(d==player1) playsound(S_PAIN6);
        else playsound(S_PAIN1+rnd(5), &d->o);
    }

    void deathstate(fpsent *d, bool restore = false)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        d->superdamage = restore ? 0 : max(-d->health, 0);
        if(d==player1)
        {
            sb.showscores(true);
            setvar("zoom", -1, true);
            if(!restore) lastplayerstate = *player1;
            d->attacking = false;
            if(!restore) d->deaths++;
            //d->pitch = 0;
            d->roll = 0;
            playsound(S_DIE1+rnd(2));
        }
        else
        {
            d->move = d->strafe = 0;
            d->resetinterp();
            playsound(S_DIE1+rnd(2), &d->o);
            if(!restore) ws.superdamageeffect(d->vel, d);
        }
    }

    void killed(fpsent *d, fpsent *actor)
    {
        if(d->state==CS_EDITING)
        {
            d->editstate = CS_DEAD;
            if(d==player1) d->deaths++;
            else d->resetinterp();
            return;
        }
        else if(d->state!=CS_ALIVE || intermission) return;

        fpsent *h = followingplayer();
        if(!h) h = player1;
        int contype = d==h || actor==h ? CON_FRAG_SELF : CON_FRAG_OTHER;
        string dname, aname;
        s_strcpy(dname, d==player1 ? "you" : colorname(d));
        s_strcpy(aname, actor==player1 ? "you" : colorname(actor));
        if(actor->type==ENT_AI)
            conoutf(contype, "\f2%s got killed by %s!", dname, aname);
        else if(d==actor || actor->type==ENT_INANIMATE)
            conoutf(contype, "\f2%s suicided%s", dname, d==player1 ? "!" : "");
        else if(isteam(d->team, actor->team))
        {
            if(d==player1) conoutf(contype, "\f2you got fragged by a teammate (%s)", aname);
            else conoutf(contype, "\f2%s fragged a teammate (%s)", aname, dname);
        }
        else
        {
            if(d==player1) conoutf(contype, "\f2you got fragged by %s", aname);
            else conoutf(contype, "\f2%s fragged %s", aname, dname);
        }

        deathstate(d);
    }

    void timeupdate(int timeremain)
    {
        minremain = timeremain;
        if(!timeremain)
        {
            intermission = true;
            player1->attacking = false;
            conoutf(CON_GAMEINFO, "\f2intermission:");
            conoutf(CON_GAMEINFO, "\f2game has ended!");
            conoutf(CON_GAMEINFO, "\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            int accuracy = player1->totaldamage*100/max(player1->totalshots, 1);
            conoutf(CON_GAMEINFO, "\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);               

            if(m_sp)
            {
                conoutf(CON_GAMEINFO, "\f2--- single player time score: ---");
                int pen, score = 0;
                pen = (totalmillis-maprealtime)/1000; score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time taken: %d seconds (%d simulated seconds)", pen, (lastmillis-maptime)/1000); 
                pen = player1->deaths*60; score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for %d deaths (1 minute each): %d seconds", player1->deaths, pen);
                pen = ms.remain*10;       score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for %d monsters remaining (10 seconds each): %d seconds", ms.remain, pen);
                pen = (10-ms.skill())*20; score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for lower skill level (20 seconds each): %d seconds", pen);
                pen = 100-accuracy;       score += pen; if(pen) conoutf(CON_GAMEINFO, "\f2time penalty for missed shots (1 second each %%): %d seconds", pen);
                s_sprintfd(aname)("bestscore_%s", getclientmap());
                const char *bestsc = getalias(aname);
                int bestscore = *bestsc ? atoi(bestsc) : score;
                if(score<bestscore) bestscore = score;
                s_sprintfd(nscore)("%d", bestscore);
                alias(aname, nscore);
                conoutf(CON_GAMEINFO, "\f2TOTAL SCORE (time + time penalties): %d seconds (best so far: %d seconds)", score, bestscore);
            }

            sb.showscores(true);
            setvar("zoom", -1, true);
        }
        else if(timeremain > 0)
        {
            conoutf(CON_GAMEINFO, "\f2time remaining: %d %s", timeremain, timeremain==1 ? "minute" : "minutes");
        }
    }

    fpsent *newclient(int cn)   // ensure valid entity
    {
        if(cn<0 || cn>=MAXCLIENTS)
        {
            neterr("clientnum");
            return NULL;
        }
        while(cn>=players.length()) players.add(NULL);
        if(!players[cn])
        {
            fpsent *d = new fpsent();
            d->clientnum = cn;
            players[cn] = d;
        }
        return players[cn];
    }

    fpsent *getclient(int cn)   // ensure valid entity
    {
        return players.inrange(cn) ? players[cn] : NULL;
    }

    void clientdisconnected(int cn, bool notify = true)
    {
        if(!players.inrange(cn)) return;
        if(following==cn) 
        {
            if(followdir) nextfollow(followdir);
            else stopfollowing();
        }
        fpsent *d = players[cn];
        if(!d) return; 
        if(notify && d->name[0]) conoutf("player %s disconnected", colorname(d));
        ws.removebouncers(d);
        ws.removeprojectiles(d);
        removetrackedparticles(d);
        if(cmode) cmode->removeplayer(d);
        DELETEP(players[cn]);
        cleardynentcache();
    }

    void initclient()
    {
        clientmap[0] = 0;
        cc.initclientnet();
    }

    void preloadweapons()
    {
        const playermodelinfo &mdl = fr.getplayermodelinfo(player1);
        const char *dir = getalias("hudgunsdir");
        loopi(NUMGUNS)
        {
            const char *file = guns[i].file;
            if(!file) continue;
            string fname;
            if((m_teammode || fr.teamskins()) && teamhudguns())
            {
                s_sprintf(fname)("%s/%s/blue", dir[0] ? dir : mdl.hudguns, file);
                preloadmodel(fname);
            }
            else
            {
                s_sprintf(fname)("%s/%s", dir[0] ? dir : mdl.hudguns, file);
                preloadmodel(fname);
            }
            s_sprintf(fname)("vwep/%s", file);
            preloadmodel(fname);
        }
    }

    void preloadbouncers()
    {
        const char *mdls[] =
        {
            "gibc", "gibh",
            "projectiles/grenade", "projectiles/rocket",
            "debris/debris01", "debris/debris02", "debris/debris03", "debris/debris04",
            "barreldebris/debris01", "barreldebris/debris02", "barreldebris/debris03", "barreldebris/debris04"
        };
        loopi(sizeof(mdls)/sizeof(mdls[0]))
        {
            preloadmodel(mdls[i]);
        }
    }

    void preload()
    {
        preloadweapons();
        preloadbouncers();
        fr.preloadplayermodel();
        et.preloadentities();
        if(m_sp) ms.preloadmonsters();
    }

    IVARP(startmenu, 0, 1, 1);

    void startmap(const char *name)   // called just after a map load
    {
        if(multiplayer(false) && !m_mp(gamemode)) { conoutf(CON_ERROR, "%s not supported in multiplayer", fpsserver::modestr(gamemode)); gamemode = 0; }

        respawned = suicided = -1;
        respawnent = -1;
        lasthit = 0;
        cc.mapstart();
        mo.clear();
        ms.monsterclear();
        ws.projreset();
        fr.clearragdolls();

        // reset perma-state
        player1->frags = 0;
        player1->deaths = 0;
        player1->totaldamage = 0;
        player1->totalshots = 0;
        player1->maxhealth = 100;
        loopv(players) if(players[i])
        {
            players[i]->frags = 0;
            players[i]->deaths = 0;
            players[i]->totaldamage = 0;
            players[i]->totalshots = 0;
            players[i]->maxhealth = 100;
        }

        if(m_capture) cmode = &capturemode;
        else if(m_ctf) cmode = &ctfmode;
        else cmode = NULL;

        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1);
        et.resetspawns();
        s_strcpy(clientmap, name);
        sb.showscores(false);
        setvar("zoom", -1, true);
        intermission = false;
        maptime = 0;
        if(*name) 
        {
            if(cmode) cmode->preload();

            conoutf(CON_GAMEINFO, "\f2game mode is %s", fpsserver::modestr(gamemode));

            if(m_sp)
            {
                s_sprintfd(aname)("bestscore_%s", name);
                const char *best = getalias(aname);
                if(*best) conoutf(CON_GAMEINFO, "\f2try to beat your best score so far: %s", best);
            }

            if(openmainmenu && startmenu())
            {
                showgui("main");
                openmainmenu = false;
            }
        }

        if(identexists("mapstart")) execute("mapstart");

        if(player1->playermodel != fr.playermodel())
        {
            player1->playermodel = fr.playermodel();
            cc.c2sinit = false;
        }
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if(d->type==ENT_INANIMATE) return;
        if     (waterlevel>0) { if(material!=MAT_LAVA) playsound(S_SPLASH1, d==player1 ? NULL : &d->o); }
        else if(waterlevel<0) playsound(material==MAT_LAVA ? S_BURN : S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(d==player1 || d->type!=ENT_PLAYER) playsoundc(S_JUMP, (fpsent *)d); }
        else if(floorlevel<0) { if(d==player1 || d->type!=ENT_PLAYER) playsoundc(S_LAND, (fpsent *)d); }
    }

    void playsoundc(int n, fpsent *d = NULL) 
    { 
        if(!d || d==player1)
        {
            cc.addmsg(SV_SOUND, "i", n); 
            playsound(n); 
        }
        else playsound(n, &d->o);
    }

    int numdynents() { return 1+players.length()+ms.monsters.length()+mo.movables.length(); }

    dynent *iterdynents(int i)
    {
        if(!i) return player1;
        i--;
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<ms.monsters.length()) return ms.monsters[i];
        i -= ms.monsters.length();
        if(i<mo.movables.length()) return mo.movables[i];
        return NULL;
    }

    bool duplicatename(fpsent *d, char *name = NULL)
    {
        if(!name) name = d->name;
        if(d!=player1 && !strcmp(name, player1->name)) return true;
        loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    char *colorname(fpsent *d, char *name = NULL, const char *prefix = "")
    {
        if(!name) name = d->name;
        if(name[0] && !duplicatename(d, name)) return name;
        static string cname;
        s_sprintf(cname)("%s%s \fs\f5(%d)\fr", prefix, name, d->clientnum);
        return cname;
    }

    void suicide(physent *d)
    {
        if(d==player1)
        {
            if(d->state!=CS_ALIVE) return;
            if(!m_mp(gamemode)) killed(player1, player1);
            else if(suicided!=player1->lifesequence)
            {
                cc.addmsg(SV_SUICIDE, "r");
                suicided = player1->lifesequence;
            }
        }
        else if(d->type==ENT_AI) ((monsterset::monster *)d)->monsterpain(400, player1);
        else if(d->type==ENT_INANIMATE) ((movableset::movable *)d)->suicide();
    }

    IVARP(hudgun, 0, 1, 1);
    IVARP(hudgunsway, 0, 1, 1);
    IVARP(teamhudguns, 0, 1, 1);
    IVARP(interphudguns, 0, 1, 1);
    IVAR(testhudgun, 0, 0, 1);

    dynent guninterp;
 
    void drawhudmodel(fpsent *d, bool norender, int anim, float speed = 0, int base = 0)
    {
        if(d->gunselect>GUN_PISTOL) return;

        vec sway;
        vecfromyawpitch(d->yaw, d->pitch, 1, 0, sway);
        float swayspeed = sqrtf(d->vel.x*d->vel.x + d->vel.y*d->vel.y);
        swayspeed = min(4.0f, swayspeed);
        sway.mul(swayspeed);
        float swayxy = sinf(swaymillis/115.0f)/100.0f,
              swayz = cosf(swaymillis/115.0f)/100.0f;
        swap(sway.x, sway.y);
        sway.x *= -swayxy;
        sway.y *= swayxy;
        sway.z = -fabs(swayspeed*swayz);
        sway.add(swaydir).add(d->o);
        if(!hudgunsway()) sway = d->o;

#if 0
        if(player1->state!=CS_DEAD && player1->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
        const playermodelinfo &mdl = fr.getplayermodelinfo(d);
        const char *dir = getalias("hudgunsdir");
        s_sprintfd(gunname)("%s/%s", dir[0] ? dir : mdl.hudguns, guns[d->gunselect].file);
        if((m_teammode || fr.teamskins()) && teamhudguns()) 
            s_strcat(gunname, d==player1 || isteam(d->team, player1->team) ? "/blue" : "/red");
        else if(fr.testteam() > 1)
            s_strcat(gunname, fr.testteam()==2 ? "/blue" : "/red");
        modelattach a[2];
        if(norender)
        {
            d->muzzle = vec(-1, -1, -1);
            a[0] = modelattach("tag_muzzle", &d->muzzle);
        }
        dynent *interp = NULL;
        if(d->gunselect==GUN_FIST && interphudguns())
        {
            anim |= ANIM_LOOP;
            base = 0;
            interp = &guninterp;
        }
        rendermodel(NULL, gunname, anim, sway, testhudgun() ? 0 : d->yaw+90, testhudgun() ? 0 : d->pitch, norender ? MDL_NORENDER : MDL_LIGHT, interp, norender ? a : NULL, base, (int)ceil(speed));
        if(norender && d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 4);
    }

    void drawhudgun(bool norender)
    {
        if(!hudgun() || editmode) return;

        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_EDITING) return;

        int rtime = ws.reloadtime(d->gunselect);
        if(d->lastaction && d->lastattackgun==d->gunselect && lastmillis-d->lastaction<rtime)
        {
            drawhudmodel(d, norender, ANIM_GUN_SHOOT|ANIM_SETSPEED, rtime/17.0f, d->lastaction);
        }
        else
        {
            drawhudmodel(d, norender, ANIM_GUN_IDLE|ANIM_LOOP);
        }
    }

    void setupavatar()
    {
        drawhudgun(true);
    }

    void renderavatar()
    {
        drawhudgun(false);
    }

    enum
    {
        HICON_BLUE_ARMOUR = 0,
        HICON_GREEN_ARMOUR,
        HICON_YELLOW_ARMOUR,

        HICON_HEALTH,

        HICON_FIST,
        HICON_SG,
        HICON_CG,
        HICON_RL,
        HICON_RIFLE,
        HICON_GL,
        HICON_PISTOL,

        HICON_FLAG,

        HICON_QUAD
    };

    void drawicon(int icon, int x, int y)
    {
        settexture("packages/hud/items.png");
        glBegin(GL_QUADS);
        float tsz = 0.25f, tx = tsz*(icon%4), ty = tsz*(icon/4);
        int s = 120;
        glTexCoord2f(tx,     ty);     glVertex2f(x,   y);
        glTexCoord2f(tx+tsz, ty);     glVertex2f(x+s, y);
        glTexCoord2f(tx+tsz, ty+tsz); glVertex2f(x+s, y+s);
        glTexCoord2f(tx,     ty+tsz); glVertex2f(x,   y+s);
        glEnd();
    }
 
    float abovegameplayhud()
    {
        return 1650.0f/1800.0f;
    }

    void gameplayhud(int w, int h)
    {
        if(player1->state==CS_SPECTATOR)
        {
            glLoadIdentity();
            glOrtho(0, w*1800/h, 1800, 0, -1, 1);

            int pw, ph, tw, th, fw, fh;
            text_bounds("  ", pw, ph);
            text_bounds("SPECTATOR", tw, th);
            th = max(th, ph);
            fpsent *f = followingplayer();
            text_bounds(f ? colorname(f) : " ", fw, fh);
            fh = max(fh, ph);
            draw_text("SPECTATOR", w*1800/h - tw - pw, 1650 - th - fh);
            if(f) draw_text(colorname(f), w*1800/h - fw - pw, 1650 - fh);
        }

        fpsent *d = hudplayer();
        if(d->state==CS_EDITING) return;

        if(d->state==CS_SPECTATOR)
        {
            if(cmode)
            {
                glLoadIdentity();
                glOrtho(0, w*1800/h, 1800, 0, -1, 1);
                cmode->drawhud(d, w, h);
            }
            return;
        }

        glLoadIdentity();
        glOrtho(0, w*900/h, 900, 0, -1, 1);

        draw_textf("%d",  90, 822, d->state==CS_DEAD ? 0 : d->health);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) draw_textf("%d", 390, 822, d->armour);
            draw_textf("%d", 690, 822, d->ammo[d->gunselect]);        
        }

        glLoadIdentity();
        glOrtho(0, w*1800/h, 1800, 0, -1, 1);

        glDisable(GL_BLEND);

        drawicon(HICON_HEALTH, 20, 1650);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) drawicon(HICON_BLUE_ARMOUR+d->armourtype, 620, 1650);
            drawicon(HICON_FIST+d->gunselect, 1220, 1650);
            if(d->quadmillis) drawicon(HICON_QUAD, 1820, 1650);
        }

        glEnable(GL_BLEND);

        if(cmode) cmode->drawhud(d, w, h);
    }

    IVARP(teamcrosshair, 0, 1, 1);
    IVARP(hitcrosshair, 0, 425, 1000);

    const char *defaultcrosshair(int index)
    {
        switch(index)
        {
            case 2: return "data/hit.png";
            case 1: return "data/teammate.png";
            default: return "data/crosshair.png";
        }
    }

    int selectcrosshair(float &r, float &g, float &b)
    {
        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_DEAD) return -1;

        if(d->state!=CS_ALIVE) return 0;

        int crosshair = 0;
        if(lasthit && lastmillis - lasthit < hitcrosshair()) crosshair = 2;
        else if(teamcrosshair())
        {
            dynent *o = ws.intersectclosest(d->o, worldpos, d);
            if(o && o->type==ENT_PLAYER && isteam(((fpsent *)o)->team, d->team))
            {
                crosshair = 1;
                r = g = 0;
            }
        }

        if(crosshair!=1 && !editmode && !m_insta)
        {
            if(d->health<=25) { r = 1.0f; g = b = 0; }
            else if(d->health<=50) { r = 1.0f; g = 0.5f; b = 0; }
        }
        if(d->gunwait) { r *= 0.5f; g *= 0.5f; b *= 0.5f; }
        return crosshair;
    }

    void lighteffects(dynent *e, vec &color, vec &dir)
    {
#if 0
        fpsent *d = (fpsent *)e;
        if(d->state!=CS_DEAD && d->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
    }

    void particletrack(physent *owner, vec &o, vec &d)
    {
        if(owner->type!=ENT_PLAYER && owner->type!=ENT_AI) return;
        fpsent *pl = (fpsent *)owner;
        if(pl->muzzle.x < 0 || pl->lastattackgun != pl->gunselect) return;
        float dist = o.dist(d);
        o = pl->muzzle;
        if(dist <= 0) d = o;
        else
        { 
            vecfromyawpitch(owner->yaw, owner->pitch, 1, 0, d);
            float newdist = raycube(owner->o, d, dist, RAY_CLIPMAT|RAY_ALPHAPOLY);
            d.mul(min(newdist, dist)).add(owner->o);
        }
    }

    void newmap(int size)
    {
        cc.addmsg(SV_NEWMAP, "ri", size);
    }

    void edittrigger(const selinfo &sel, int op, int arg1, int arg2, int arg3)
    {
        if(gamemode==1) switch(op)
        {
            case EDIT_FLIP:
            case EDIT_COPY:
            case EDIT_PASTE:
            case EDIT_DELCUBE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i4",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
                break;
            }
            case EDIT_MAT:
            case EDIT_ROTATE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i5",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1);
                break;
            }
            case EDIT_FACE:
            case EDIT_TEX:
            case EDIT_REPLACE:
            {
                cc.addmsg(SV_EDITF + op, "ri9i6",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2);
                break;
            }
            case EDIT_REMIP:
            {
                cc.addmsg(SV_EDITF + op, "r");
                break;
            }
        }
    }

    bool serverinfostartcolumn(g3d_gui *g, int i)
    {
        static const char *names[] = { "ping ", "players ", "map ", "mode ", "master ", "host ", "description " };
        if(size_t(i) >= sizeof(names)/sizeof(names[0])) return false;
        g->pushlist();
        g->text(names[i], 0xFFFF80, !i ? "server" : NULL);
        g->mergehits(true);
        return true;
    }

    void serverinfoendcolumn(g3d_gui *g, int i)
    {
        g->mergehits(false);
        g->poplist();
    }

    bool serverinfoentry(g3d_gui *g, int i, const char *name, const char *sdesc, const char *map, int ping, const vector<int> &attr, int np)
    {
        if(ping < 0 || attr.empty() || attr[0]!=PROTOCOL_VERSION)
        {
            switch(i)
            {
                case 0:
                    if(g->button(" ", 0xFFFFDD, "server")&G3D_UP) return true;
                    break;

                case 1:
                case 2:
                case 3:
                case 4:
                    if(g->button(" ", 0xFFFFDD)&G3D_UP) return true; 
                    break;

                case 5:
                    if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                    break;

                case 6:
                    if(ping < 0)
                    {
                        if(g->button(sdesc, 0xFFFFDD)&G3D_UP) return true;
                    }
                    else if(g->buttonf("[%s protocol] ", 0xFFFFDD, NULL, attr.empty() ? "unknown" : (attr[0] < PROTOCOL_VERSION ? "older" : "newer"))&G3D_UP) return true;
                    break;
            }        
            return false;
        }
    
        switch(i)
        {
            case 0:
                if(g->buttonf("%d ", 0xFFFFDD, "server", ping)&G3D_UP) return true;
                break;

            case 1:
                if(attr.length()>=4)
                {
                    if(g->buttonf("%d/%d ", 0xFFFFDD, NULL, np, attr[3])&G3D_UP) return true;
                }
                else if(g->buttonf("%d ", 0xFFFFDD, NULL, np)&G3D_UP) return true;
                break;

            case 2:
                if(g->buttonf("%.25s ", 0xFFFFDD, NULL, map)&G3D_UP) return true;
                break;

            case 3:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=2 ? fpsserver::modestr(attr[1], "") : "")&G3D_UP) return true;
                break;

            case 4:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=5 ? fpsserver::mastermodestr(attr[4], "") : "")&G3D_UP) return true;
                break;
            
            case 5:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, name)&G3D_UP) return true;
                break;

            case 6:
            {
                if(g->buttonf("%.25s", 0xFFFFDD, NULL, sdesc)&G3D_UP) return true;
                break;
            }
        }
        return false;
    }
 
    void g3d_gamemenus() { sb.show(); }

    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameident() { return "fps"; }
    const char *defaultmap() { return "metl4"; }
    const char *savedconfig() { return "config.cfg"; }
    const char *defaultconfig() { return "data/defaults.cfg"; }
    const char *autoexec() { return "autoexec.cfg"; }
    const char *savedservers() { return "servers.cfg"; }
};

REGISTERGAME(fpsgame, "fps", new fpsclient(), new fpsserver());

#else

REGISTERGAME(fpsgame, "fps", NULL, new fpsserver());

#endif




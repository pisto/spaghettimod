#include "cube.h"
#include "game.h"

namespace game
{
    int gamemode = 0;
    bool intermission = false;
    string clientmap = "";
    int maptime = 0, maprealtime = 0, minremain = 0;
    int respawnent = -1;
    int respawned = -1, suicided = -1;
    int lasthit = 0, lastspawnattempt = 0;

    int following = -1, followdir = 0;

    fpsent *player1 = NULL;         // our client
    vector<fpsent *> players;       // other clients
    fpsent lastplayerstate;

    bool clientoption(const char *arg) { return false; }

    void taunt()
    {
        if(player1->state!=CS_ALIVE || player1->physstate<PHYS_SLOPE) return;
        if(lastmillis-player1->lasttaunt<1000) return;
        player1->lasttaunt = lastmillis;
        addmsg(SV_TAUNT, "r");
    }
    COMMAND(taunt, "");

	void follow(char *arg)
    {
        if(arg[0] ? player1->state==CS_SPECTATOR : following>=0)
        {
            following = arg[0] ? parseplayer(arg) : -1;
            if(following==player1->clientnum) following = -1;
            followdir = 0;
            conoutf("follow %s", following>=0 ? "on" : "off");
        }
	}
    COMMAND(follow, "s");

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
    ICOMMAND(nextfollow, "i", (int *dir), nextfollow(*dir < 0 ? -1 : 1));


    char *getclientmap() { return clientmap; }

    void resetgamestate()
    {
        if(m_classicsp)
        {
            clearmovables();
            clearmonsters();                 // all monsters back at their spawns for editing
            resettriggers();
        }
        clearprojectiles();
        clearbouncers();
    }

    fpsent *spawnstate(fpsent *d)              // reset player state not persistent accross spawns
    {
        d->respawn();
        d->spawnstate(gamemode);
        return d;
    }

    void respawnself()
    {
        if(paused || ispaused()) return;
        if(m_mp(gamemode)) 
        {
            if(respawned!=player1->lifesequence)
            {
                addmsg(SV_TRYSPAWN, "r");
                respawned = player1->lifesequence;
            }
        }
        else
        {
            spawnplayer(player1);
            showscores(false);
            lasthit = 0;
            if(cmode) cmode->respawned(player1);
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

    VARP(smoothmove, 0, 75, 100);
    VARP(smoothdist, 0, 32, 64);

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
        float k = 1.0f - float(lastmillis - d->smoothmillis)/smoothmove;
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
                if(d->quadmillis) entities::checkquad(curtime, d);
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
                if(smoothmove && d->smoothmillis>0) predictplayer(d, true);
                else moveplayer(d, 1, false);
            }
            else if(d->state==CS_DEAD && !d->ragdoll && lastmillis-d->lastpain<2000) moveplayer(d, 1, true);
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
        if(!curtime) { gets2c(); if(player1->clientnum>=0) c2sinfo(player1); return; }

        physicsframe();
        entities::checkquad(curtime, player1);
        updateprojectiles(curtime);
        if(player1->clientnum>=0 && player1->state==CS_ALIVE) shoot(player1, worldpos); // only shoot when connected to server
        updatebouncers(curtime); // need to do this after the player shoots so grenades don't end up inside player's BB next frame
        otherplayers(curtime);
        moveragdolls();
        gets2c();
        updatemovables(curtime);
        updatemonsters(curtime);
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
            swayhudgun(curtime);
            entities::checkitems(player1);
            if(m_slowmo) checkslowmo();
            if(m_classicsp) checktriggers();
            else if(cmode) cmode->checkitems(player1);
        }
        if(player1->clientnum>=0) c2sinfo(player1);   // do this last, to reduce the effective frame lag
    }

    void spawnplayer(fpsent *d)   // place at random spawn
    {
        if(cmode) cmode->pickspawn(d);
        else findplayerspawn(d, d==player1 && respawnent>=0 ? respawnent : -1);
        spawnstate(d);
        d->state = d==player1 ? (spectator ? CS_SPECTATOR : (editmode ? CS_EDITING : CS_ALIVE)) : CS_ALIVE;
    }

    VARP(spawnwait, 0, 0, 1000);

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
            if(lastmillis < player1->lastpain + spawnwait) return;
            if(m_dmsp) { changemap(clientmap, gamemode); return; }    // if we die in SP we try the same map again
            respawnself();
            if(m_classicsp)
            {
                conoutf(CON_GAMEINFO, "\f2You wasted another life! The monsters stole your armour and some ammo...");
                loopi(NUMGUNS) if(i!=GUN_PISTOL && (player1->ammo[i] = lastplayerstate.ammo[i])>5) player1->ammo[i] = max(player1->ammo[i]/3, 5);
            }
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

    void damaged(int damage, fpsent *d, fpsent *actor, bool local)
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
        damageeffect(damage, d, d!=h);

        if(d->health<=0) { if(local) killed(d, actor); }
        else if(d==player1) playsound(S_PAIN6);
        else playsound(S_PAIN1+rnd(5), &d->o);
    }

    void deathstate(fpsent *d, bool restore)
    {
        d->state = CS_DEAD;
        d->lastpain = lastmillis;
        d->superdamage = restore ? 0 : max(-d->health, 0);
        if(d==player1)
        {
            showscores(true);
            disablezoom();
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
            if(!restore) superdamageeffect(d->vel, d);
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
            if(actor==player1) conoutf(contype, "\f3you fragged a teammate (%s)", dname);
            else if(d==player1) conoutf(contype, "\f3you got fragged by a teammate (%s)", aname);
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
            if(cmode) cmode->gameover();
            conoutf(CON_GAMEINFO, "\f2intermission:");
            conoutf(CON_GAMEINFO, "\f2game has ended!");
            conoutf(CON_GAMEINFO, "\f2player frags: %d, deaths: %d", player1->frags, player1->deaths);
            int accuracy = player1->totaldamage*100/max(player1->totalshots, 1);
            conoutf(CON_GAMEINFO, "\f2player total damage dealt: %d, damage wasted: %d, accuracy(%%): %d", player1->totaldamage, player1->totalshots-player1->totaldamage, accuracy);               
            if(m_sp) spsummary(accuracy);

            showscores(true);
            disablezoom();
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
            neterr("clientnum", false);
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

    void clientdisconnected(int cn, bool notify)
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
        removebouncers(d);
        removeprojectiles(d);
        removetrackedparticles(d);
        if(cmode) cmode->removeplayer(d);
        DELETEP(players[cn]);
        cleardynentcache();
    }

    void initclient()
    {
        clientmap[0] = 0;
        player1 = spawnstate(new fpsent);
    }

    VARP(showmodeinfo, 0, 1, 1);

    void startmap(const char *name)   // called just after a map load
    {
        if(multiplayer(false) && !m_mp(gamemode)) { conoutf(CON_ERROR, "%s not supported in multiplayer", server::modename(gamemode)); gamemode = 0; }

        respawned = suicided = -1;
        respawnent = -1;
        lasthit = 0;
        sendmapinfo();
        clearmovables();
        clearmonsters();
        clearprojectiles();
        clearbouncers();
        clearragdolls();

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
    
        setclientmode();

        if(!m_mp(gamemode)) spawnplayer(player1);
        else findplayerspawn(player1, -1);
        entities::resetspawns();
        s_strcpy(clientmap, name);
        showscores(false);
        disablezoom();
        intermission = false;
        maptime = 0;
        if(*name) 
        {
            if(cmode) cmode->preload();

            conoutf(CON_GAMEINFO, "\f2game mode is %s", server::modename(gamemode));

            if(m_sp)
            {
                s_sprintfd(aname)("bestscore_%s", name);
                const char *best = getalias(aname);
                if(*best) conoutf(CON_GAMEINFO, "\f2try to beat your best score so far: %s", best);
            }
            else
            {
                const char *info = m_valid(gamemode) ? gamemodes[gamemode - STARTGAMEMODE].info : NULL;
                if(showmodeinfo && info) conoutf(CON_GAMEINFO, "\f0%s", info);
            }
        }

        if(identexists("mapstart")) execute("mapstart");

        if(player1->playermodel != playermodel) switchplayermodel(playermodel);
    }

    void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material)
    {
        if(d->type==ENT_INANIMATE) return;
        if     (waterlevel>0) { if(material!=MAT_LAVA) playsound(S_SPLASH1, d==player1 ? NULL : &d->o); }
        else if(waterlevel<0) playsound(material==MAT_LAVA ? S_BURN : S_SPLASH2, d==player1 ? NULL : &d->o);
        if     (floorlevel>0) { if(d==player1 || d->type!=ENT_PLAYER) msgsound(S_JUMP, (fpsent *)d); }
        else if(floorlevel<0) { if(d==player1 || d->type!=ENT_PLAYER) msgsound(S_LAND, (fpsent *)d); }
    }

    void msgsound(int n, fpsent *d) 
    { 
        if(!d || d==player1)
        {
            addmsg(SV_SOUND, "i", n); 
            playsound(n); 
        }
        else playsound(n, &d->o);
    }

    int numdynents() { return 1+players.length()+monsters.length()+movables.length(); }

    dynent *iterdynents(int i)
    {
        if(!i) return player1;
        i--;
        if(i<players.length()) return players[i];
        i -= players.length();
        if(i<monsters.length()) return (dynent *)monsters[i];
        i -= monsters.length();
        if(i<movables.length()) return (dynent *)movables[i];
        return NULL;
    }

    bool duplicatename(fpsent *d, char *name = NULL)
    {
        if(!name) name = d->name;
        if(d!=player1 && !strcmp(name, player1->name)) return true;
        loopv(players) if(players[i] && d!=players[i] && !strcmp(name, players[i]->name)) return true;
        return false;
    }

    char *colorname(fpsent *d, char *name, const char *prefix)
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
                addmsg(SV_SUICIDE, "r");
                suicided = player1->lifesequence;
            }
        }
        else if(d->type==ENT_AI) suicidemonster((monster *)d);
        else if(d->type==ENT_INANIMATE) suicidemovable((movable *)d);
    }
    ICOMMAND(kill, "", (), suicide(player1));

    void drawicon(int icon, float x, float y, float sz)
    {
        settexture("packages/hud/items.png");
        glBegin(GL_QUADS);
        float tsz = 0.25f, tx = tsz*(icon%4), ty = tsz*(icon/4);
        glTexCoord2f(tx,     ty);     glVertex2f(x,    y);
        glTexCoord2f(tx+tsz, ty);     glVertex2f(x+sz, y);
        glTexCoord2f(tx+tsz, ty+tsz); glVertex2f(x+sz, y+sz);
        glTexCoord2f(tx,     ty+tsz); glVertex2f(x,    y+sz);
        glEnd();
    }
 
    float abovegameplayhud()
    {
        return 1650.0f/1800.0f;
    }

    int ammohudup[3] = { GUN_CG, GUN_RL, GUN_GL },
        ammohuddown[3] = { GUN_RIFLE, GUN_SG, GUN_PISTOL };

    ICOMMAND(ammohudup, "sss", (char *w1, char *w2, char *w3),
    {
        int i = 0;
        if(w1[0]) ammohudup[i++] = atoi(w1);
        if(w2[0]) ammohudup[i++] = atoi(w2);
        if(w3[0]) ammohudup[i++] = atoi(w3);
        while(i < 3) ammohudup[i++] = -1;
    });

    ICOMMAND(ammohuddown, "sss", (char *w1, char *w2, char *w3),
    {
        int i = 0;
        if(w1[0]) ammohuddown[i++] = atoi(w1);
        if(w2[0]) ammohuddown[i++] = atoi(w2);
        if(w3[0]) ammohuddown[i++] = atoi(w3);
        while(i < 3) ammohuddown[i++] = -1;
    });

    void drawhudicons(fpsent *d)
    {
        glPushMatrix();
        glScalef(2, 2, 1);

        draw_textf("%d",  90, 822, d->state==CS_DEAD ? 0 : d->health);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) draw_textf("%d", 390, 822, d->armour);
            draw_textf("%d", 690, 822, d->ammo[d->gunselect]);
        }

        glPopMatrix();

        drawicon(HICON_HEALTH, 20, 1650);
        if(d->state!=CS_DEAD)
        {
            if(d->armour) drawicon(HICON_BLUE_ARMOUR+d->armourtype, 620, 1650);
            drawicon(HICON_FIST+d->gunselect, 1220, 1650);
            if(d->quadmillis) drawicon(HICON_QUAD, 1820, 1650);
            glPushMatrix();
            float x = 1220, y = 1650, sz = 120;
            glScalef(1/3.0f, 1/3.0f, 1);
            float xup = (x+sz)*3, yup = y*3;
            loopi(3)
            {
                int gun = ammohudup[i];
                if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
                drawicon(HICON_FIST+gun, xup, yup, sz);
                yup += sz;
            }
            float xdown = x*3 - sz, ydown = (y+sz)*3;
            loopi(3)
            {
                int gun = ammohuddown[3-i-1];
                if(gun < GUN_FIST || gun > GUN_PISTOL || gun == d->gunselect || !d->ammo[gun]) continue;
                ydown -= sz;
                drawicon(HICON_FIST+gun, xdown, ydown, sz);
            }
            glPopMatrix(); 
        }
    }

    void gameplayhud(int w, int h)
    {
        glPushMatrix();
        glScalef(h/1800.0f, h/1800.0f, 1);

        if(player1->state==CS_SPECTATOR)
        {
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
        if(d->state!=CS_EDITING)
        {
            if(d->state!=CS_SPECTATOR) drawhudicons(d);
            if(cmode) cmode->drawhud(d, w, h);
        } 

        glPopMatrix();
    }

    VARP(teamcrosshair, 0, 1, 1);
    VARP(hitcrosshair, 0, 425, 1000);

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
        if(lasthit && lastmillis - lasthit < hitcrosshair) crosshair = 2;
        else if(teamcrosshair)
        {
            dynent *o = intersectclosest(d->o, worldpos, d);
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
        addmsg(SV_NEWMAP, "ri", size);
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
                addmsg(SV_EDITF + op, "ri9i4",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner);
                break;
            }
            case EDIT_MAT:
            case EDIT_ROTATE:
            {
                addmsg(SV_EDITF + op, "ri9i5",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1);
                break;
            }
            case EDIT_FACE:
            case EDIT_TEX:
            case EDIT_REPLACE:
            {
                addmsg(SV_EDITF + op, "ri9i6",
                   sel.o.x, sel.o.y, sel.o.z, sel.s.x, sel.s.y, sel.s.z, sel.grid, sel.orient,
                   sel.cx, sel.cxs, sel.cy, sel.cys, sel.corner,
                   arg1, arg2);
                break;
            }
            case EDIT_REMIP:
            {
                addmsg(SV_EDITF + op, "r");
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
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=2 ? server::modename(attr[1], "") : "")&G3D_UP) return true;
                break;

            case 4:
                if(g->buttonf("%s ", 0xFFFFDD, NULL, attr.length()>=5 ? server::mastermodename(attr[4], "") : "")&G3D_UP) return true;
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
 
    // any data written into this vector will get saved with the map data. Must take care to do own versioning, and endianess if applicable. Will not get called when loading maps from other games, so provide defaults.
    void writegamedata(vector<char> &extras) {}
    void readgamedata(vector<char> &extras) {}

    const char *gameident() { return "fps"; }
    const char *defaultmap() { return "metl4"; }
    const char *savedconfig() { return "config.cfg"; }
    const char *defaultconfig() { return "data/defaults.cfg"; }
    const char *autoexec() { return "autoexec.cfg"; }
    const char *savedservers() { return "servers.cfg"; }
}


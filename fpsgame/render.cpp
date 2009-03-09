#include "cube.h"
#include "game.h"

namespace game
{      
    vector<fpsent *> bestplayers;
    vector<const char *> bestteams;

    VARP(ragdoll, 0, 1, 1);
    VARP(ragdollmillis, 0, 10000, 60000);
    VARP(playermodel, 0, 0, 2);
    VARP(forceplayermodels, 0, 0, 1);

    vector<fpsent *> ragdolls;

    void saveragdoll(fpsent *d)
    {
        if(!d->ragdoll || !ragdollmillis || lastmillis > d->lastpain + ragdollmillis) return;
        fpsent *r = new fpsent(*d);
        r->edit = NULL;
        ragdolls.add(r);
        d->ragdoll = NULL;   
    }

    void clearragdolls()
    {
        ragdolls.deletecontentsp();
    }

    void moveragdolls()
    {
        loopv(ragdolls)
        {
            fpsent *d = ragdolls[i];
            if(lastmillis > d->lastpain + ragdollmillis)
            {
                delete ragdolls.remove(i--);
                continue;
            }
            moveragdoll(d);
        }
    }
 
    const playermodelinfo *getplayermodelinfo(int n)
    {
        static const playermodelinfo playermodels[3] =
        {
            { "mrfixit", "mrfixit/blue", "mrfixit/red", "mrfixit/hudguns", NULL, "mrfixit/horns", { "mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow" }, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
            { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", "snoutx10k/hudguns", NULL, NULL, { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "ironsnout", "ironsnout_blue", "ironsnout_red", true, true },
            { "ogro/green", "ogro/blue", "ogro/red", "mrfixit/hudguns", "ogro/vwep", NULL, { NULL, NULL, NULL }, "ogro", "ogro", "ogro", false, false }
        };
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0])) return NULL;
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(fpsent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(d==player1 || forceplayermodels ? playermodel : d->playermodel);
        if(!mdl || !mdl->selectable) mdl = getplayermodelinfo(playermodel);
        return *mdl;
    }

    void preloadplayermodel()
    {
        loopi(3)
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) break;
            if(i != playermodel && (!multiplayer(false) || forceplayermodels || !mdl->selectable)) continue;
            if(m_teammode)
            {
                preloadmodel(mdl->blueteam);
                preloadmodel(mdl->redteam);
            }
            else preloadmodel(mdl->ffa);
            if(mdl->vwep) preloadmodel(mdl->vwep);
            if(mdl->quad) preloadmodel(mdl->quad);
            loopj(3) if(mdl->armour[j]) preloadmodel(mdl->armour[j]);
        }
    }
    
    VAR(testquad, 0, 0, 1);
    VAR(testarmour, 0, 0, 1);
    VAR(testteam, 0, 0, 3);

    void renderplayer(fpsent *d, const playermodelinfo &mdl, int team, bool mainpass)
    {
        int lastaction = d->lastaction, hold = d->gunselect==GUN_PISTOL ? 0 : (ANIM_HOLD1+d->gunselect)|ANIM_LOOP, attack = ANIM_ATTACK1+d->gunselect, delay = mdl.vwep ? 300 : guns[d->gunselect].attackdelay+50;
        if(intermission && d->state!=CS_DEAD)
        {
            lastaction = 0;
            hold = attack = ANIM_LOSE|ANIM_LOOP;
            delay = 0;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { hold = attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) hold = attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            hold = attack = ANIM_TAUNT;
            delay = 1000;
        }
        modelattach a[5];
        static const char *vweps[] = {"vwep/fist", "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        int ai = 0;
        if((!mdl.vwep || d->gunselect!=GUN_FIST) && d->gunselect<=GUN_PISTOL)
        {
            int vanim = ANIM_VWEP_IDLE|ANIM_LOOP, vtime = 0;
            if(lastaction && d->lastattackgun==d->gunselect && lastmillis < lastaction + delay)
            {
                vanim = ANIM_VWEP_SHOOT;
                vtime = lastaction;
            }
            a[ai++] = modelattach("tag_weapon", mdl.vwep ? mdl.vwep : vweps[d->gunselect], vanim, vtime);
        }
        if(d->state==CS_ALIVE)
        {
            if((testquad || d->quadmillis) && mdl.quad)
                a[ai++] = modelattach("tag_powerup", mdl.quad, ANIM_POWERUP|ANIM_LOOP, 0);
            if(testarmour || d->armour)
            {
                int type = clamp(d->armourtype, (int)A_BLUE, (int)A_YELLOW);
                if(mdl.armour[type])
                    a[ai++] = modelattach("tag_shield", mdl.armour[type], ANIM_SHIELD|ANIM_LOOP, 0);
            }
        }
        if(mainpass)
        {
            d->muzzle = vec(-1, -1, -1);
            a[ai++] = modelattach("tag_muzzle", &d->muzzle);
        }
        const char *mdlname = mdl.ffa;
        switch(testteam ? testteam-1 : team)
        {
            case 1: mdlname = mdl.blueteam; break;
            case 2: mdlname = mdl.redteam; break;
        }
        renderclient(d, mdlname, a[0].tag ? a : NULL, hold, attack, delay, lastaction, intermission && d->state!=CS_DEAD ? 0 : d->lastpain, ragdoll && mdl.ragdoll);
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            entitylight light;
            rendermodel(&light, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*lastmillis/1000.0f, 0, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
        }
#endif
    }

    VARP(teamskins, 0, 0, 1);

    void rendergame(bool mainpass)
    {
        if(intermission)
        {
            bestteams.setsize(0);
            bestplayers.setsize(0);
            if(m_teammode) getbestteams(bestteams);
            else getbestplayers(bestplayers);
        }

        startmodelbatches();

        fpsent *exclude = NULL;
        if(player1->state==CS_SPECTATOR && following>=0 && !isthirdperson())
            exclude = getclient(following);

        fpsent *d;
        loopv(players) if((d = players[i]) && d->state!=CS_SPECTATOR && d->state!=CS_SPAWNING && d!=exclude)
        {
            int team = 0;
            if(teamskins || m_teammode) team = isteam(player1->team, d->team) ? 1 : 2;
            renderplayer(d, getplayermodelinfo(d), team, mainpass);
            s_strcpy(d->info, colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, PART_TEXT, 1, team ? (team==1 ? 0x6496FF : 0xFF4B19) : 0x1EC850, 2.0f);
        }
        loopv(ragdolls)
        {
            fpsent *d = ragdolls[i];
            int team = 0;
            if(teamskins || m_teammode) team = isteam(player1->team, d->team) ? 1 : 2;
            renderplayer(d, getplayermodelinfo(d), team, mainpass);
        } 
        if(isthirdperson() && !followingplayer()) renderplayer(player1, getplayermodelinfo(player1), teamskins || m_teammode ? 1 : 0, mainpass);
        rendermonsters();
        rendermovables();
        entities::renderentities();
        renderbouncers();
        renderprojectiles();
        if(cmode) cmode->rendergame();

        endmodelbatches();
    }

    VARP(hudgun, 0, 1, 1);
    VARP(hudgunsway, 0, 1, 1);
    VARP(teamhudguns, 0, 1, 1);
    VARP(interphudguns, 0, 1, 1);
    VAR(testhudgun, 0, 0, 1);

    int swaymillis = 0;
    vec swaydir(0, 0, 0);

    void swayhudgun(int curtime)
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
        if(!hudgunsway) sway = d->o;

#if 0
        if(player1->state!=CS_DEAD && player1->quadmillis)
        {
            float t = 0.5f + 0.5f*sinf(2*M_PI*lastmillis/1000.0f);
            color.y = color.y*(1-t) + t;
        }
#endif
        const playermodelinfo &mdl = getplayermodelinfo(d);
        const char *dir = getalias("hudgunsdir");
        s_sprintfd(gunname)("%s/%s", dir[0] ? dir : mdl.hudguns, guns[d->gunselect].file);
        if((m_teammode || teamskins) && teamhudguns)
            s_strcat(gunname, d==player1 || isteam(d->team, player1->team) ? "/blue" : "/red");
        else if(testteam > 1)
            s_strcat(gunname, testteam==2 ? "/blue" : "/red");
        modelattach a[2];
        if(norender)
        {
            d->muzzle = vec(-1, -1, -1);
            a[0] = modelattach("tag_muzzle", &d->muzzle);
        }
        dynent *interp = NULL;
        if(d->gunselect==GUN_FIST && interphudguns)
        {
            anim |= ANIM_LOOP;
            base = 0;
            interp = &guninterp;
        }
        rendermodel(NULL, gunname, anim, sway, testhudgun ? 0 : d->yaw+90, testhudgun ? 0 : d->pitch, norender ? MDL_NORENDER : MDL_LIGHT, interp, norender ? a : NULL, base, (int)ceil(speed));
        if(norender && d->muzzle.x >= 0) d->muzzle = calcavatarpos(d->muzzle, 4);
    }

    void drawhudgun(bool norender)
    {
        if(!hudgun || editmode) return;

        fpsent *d = hudplayer();
        if(d->state==CS_SPECTATOR || d->state==CS_EDITING) return;

        int rtime = guns[d->gunselect].attackdelay;
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

    vec hudgunorigin(int gun, const vec &from, const vec &to, fpsent *d)
    {
        if(d->muzzle.x >= 0) return d->muzzle;
        vec offset(from);
        if(d!=hudplayer() || isthirdperson())
        {
            vec front, right;
            vecfromyawpitch(d->yaw, d->pitch, 1, 0, front);
            offset.add(front.mul(d->radius));
            if(d->type!=ENT_AI)
            {
                offset.z += (d->aboveeye + d->eyeheight)*0.75f - d->eyeheight;
                vecfromyawpitch(d->yaw, 0, 0, -1, right);
                offset.add(right.mul(0.5f*d->radius));
                offset.add(front);
            }
            return offset;
        }
        offset.add(vec(to).sub(from).normalize().mul(2));
        if(hudgun)
        {
            offset.sub(vec(camup).mul(1.0f));
            offset.add(vec(camright).mul(0.8f));
        }
        return offset;
    }

    void preloadweapons()
    {
        const playermodelinfo &mdl = getplayermodelinfo(player1);
        const char *dir = getalias("hudgunsdir");
        loopi(NUMGUNS)
        {
            const char *file = guns[i].file;
            if(!file) continue;
            string fname;
            if((m_teammode || teamskins) && teamhudguns)
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

    void preload()
    {
        if(hudgun) preloadweapons();
        preloadbouncers();
        preloadplayermodel();
        entities::preloadentities();
        if(m_sp) preloadmonsters();
    }

}


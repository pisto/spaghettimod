#ifndef PARSEMESSAGES

#define ctfteamflag(s) (!strcmp(s, "good") ? 1 : (!strcmp(s, "evil") ? 2 : 0))
#define ctfflagteam(i) (i==1 ? "good" : (i==2 ? "evil" : NULL))

#ifdef SERVMODE
struct ctfservmode : servmode
#else
struct ctfclientmode : clientmode
#endif
{
    static const int MAXFLAGS = 100;
    static const int FLAGRADIUS = 16;
    static const int FLAGLIMIT = 10;

    struct flag
    {
        vec droploc, spawnloc;
        int team, droptime;
#ifdef SERVMODE
        int owner, invistime;
#else
        bool pickup, invis;
        fpsent *owner;
        extentity *ent;
        vec interploc;
        float interpangle;
        int interptime;
#endif

        flag() 
#ifndef SERVMODE
          : ent(NULL) 
#endif
        { reset(); }

        void reset()
        {
            droploc = spawnloc = vec(0, 0, 0);
#ifdef SERVMODE
            owner = -1;
            invistime = 0;
#else
            pickup = invis = false;
            owner = NULL;
            interploc = vec(0, 0, 0);
            interpangle = 0;
            interptime = 0;
#endif
            team = 0;
            droptime = 0;
        }
    };
    vector<flag> flags;
    int scores[2];

    void resetflags()
    {
        flags.setsize(0);
        loopk(2) scores[k] = 0;
    }

    void addflag(int i, const vec &o, int team)
    {
        if(i<0 || i>=MAXFLAGS) return;
        while(flags.length()<=i) flags.add();
        flag &f = flags[i];
        f.reset();
        f.team = team;
        f.spawnloc = o;
    }

#ifdef SERVMODE
    void ownflag(int i, int owner)
#else
    void ownflag(int i, fpsent *owner)
#endif
    {
        flag &f = flags[i];
        f.owner = owner;
#ifdef SERVMODE
        f.invistime = 0;
#else
        f.pickup = f.invis = false;
#endif
    }

    void dropflag(int i, const vec &o, int droptime)
    {
        flag &f = flags[i];
        f.droploc = o;
        f.droptime = droptime;
#ifdef SERVMODE
        f.owner = -1;
        f.invistime = 0;
#else
        f.pickup = f.invis = false;
        f.owner = NULL;
#endif
    }

#ifdef SERVMODE
    void returnflag(int i, int invistime = 0)
#else
    void returnflag(int i, bool invis = false)
#endif
    {
        flag &f = flags[i];
        f.droptime = 0;
#ifdef SERVMODE
        f.owner = -1;
        f.invistime = invistime;
#else
        f.pickup = false;
        f.invis = invis;
        f.owner = NULL;
#endif
    }

    int totalscore(int team)
    {
        return team >= 1 && team <= 2 ? scores[team-1] : 0;
    }

    int setscore(int team, int score)
    {
        if(team >= 1 && team <= 2) return scores[team-1] = score;
        return 0;
    }

    int addscore(int team, int score)
    {
        if(team >= 1 && team <= 2) return scores[team-1] += score;
        return 0;
    }

    bool hidefrags() { return true; }

    int getteamscore(const char *team)
    {
        return totalscore(ctfteamflag(team));
    }

    void getteamscores(vector<teamscore> &tscores)
    {
        loopk(2) if(scores[k]) tscores.add(teamscore(ctfflagteam(k+1), scores[k]));
    }

#ifdef SERVMODE
    static const int RESETFLAGTIME = 10000;
    static const int INVISFLAGTIME = 10000;

    bool notgotflags;

    ctfservmode() : notgotflags(false) {}

    void reset(bool empty)
    {
        resetflags();
        notgotflags = !empty;
    }

    void dropflag(clientinfo *ci)
    {
        if(notgotflags) return;
        loopv(flags) if(flags[i].owner==ci->clientnum)
        {
            ivec o(vec(ci->state.o).mul(DMF));
            sendf(-1, 1, "ri6", SV_DROPFLAG, ci->clientnum, i, o.x, o.y, o.z); 
            dropflag(i, o.tovec().div(DMF), lastmillis);
        }
    } 

    void leavegame(clientinfo *ci, bool disconnecting = false)
    {
        dropflag(ci);
    }

    void died(clientinfo *ci, clientinfo *actor)
    {
        dropflag(ci);
    }

    bool canchangeteam(clientinfo *ci, const char *oldteam, const char *newteam)
    {
        return ctfteamflag(newteam) > 0;
    }

    void changeteam(clientinfo *ci, const char *oldteam, const char *newteam)
    {
        dropflag(ci);
    }

    void scoreflag(clientinfo *ci, int goal, int relay = -1)
    {
        returnflag(relay >= 0 ? relay : goal, relay >= 0 ? 0 : lastmillis);
        ci->state.flags++;
        int team = ctfteamflag(ci->team), score = addscore(team, 1);                              
        sendf(-1, 1, "ri6", SV_SCOREFLAG, ci->clientnum, relay, goal, team, score);
        if(score >= FLAGLIMIT) startintermission();
    }

    void takeflag(clientinfo *ci, int i)
    {
        if(notgotflags || !flags.inrange(i) || ci->state.state!=CS_ALIVE || !ci->team[0]) return;
        flag &f = flags[i];
        if(!ctfflagteam(f.team) || f.owner>=0) return;
        int team = ctfteamflag(ci->team);
        if(m_protect == (f.team==team))
        {
            loopvj(flags) if(flags[j].owner==ci->clientnum) return;
            ownflag(i, ci->clientnum);
            sendf(-1, 1, "ri3", SV_TAKEFLAG, ci->clientnum, i);
        }
        else if(m_protect)
        {
            if(!f.invistime) scoreflag(ci, i); 
        }
        else if(f.droptime) 
        {
            returnflag(i);
            sendf(-1, 1, "ri3", SV_RETURNFLAG, ci->clientnum, i);
        }
        else
        {
            loopvj(flags) if(flags[j].owner==ci->clientnum) { scoreflag(ci, i, j); break; }
        }
    }

    void update()
    {
        if(minremain<=0 || notgotflags) return;
        loopv(flags) 
        {
            flag &f = flags[i];
            if(f.owner<0 && f.droptime && lastmillis - f.droptime >= RESETFLAGTIME)
            {
                returnflag(i);
                sendf(-1, 1, "ri4", SV_RESETFLAG, i, f.team, addscore(f.team, m_protect ? -1 : 0));
            }
            if(f.invistime && lastmillis - f.invistime >= INVISFLAGTIME)
            {
                f.invistime = 0;
                sendf(-1, 1, "ri3", SV_INVISFLAG, i, 0);
            } 
        }
    }

    void initclient(clientinfo *ci, ucharbuf &p, bool connecting)
    {
        putint(p, SV_INITFLAGS);
        loopk(2) putint(p, scores[k]);
        putint(p, flags.length());
        loopv(flags)
        {
            flag &f = flags[i];
            putint(p, f.owner);
            putint(p, f.invistime ? 1 : 0);
            if(f.owner<0)
            {
                putint(p, f.droptime ? 1 : 0);
                if(f.droptime)
                {
                    putint(p, int(f.droploc.x*DMF));
                    putint(p, int(f.droploc.y*DMF));
                    putint(p, int(f.droploc.z*DMF));
                }
            }
        }
    }

    void parseflags(ucharbuf &p, bool commit)
    {
        int numflags = getint(p);
        loopi(numflags)
        {
            int team = getint(p);
            vec o;
            loopk(3) o[k] = getint(p)/DMF;
            if(p.overread()) break;
            if(commit && notgotflags) addflag(i, o, team);
        }
        if(commit) notgotflags = false;
    }
};
#else
    static const int RESPAWNSECS = 5;

    float radarscale;

    ctfclientmode() : radarscale(0)
    {
        CCOMMAND(dropflag, "", (ctfclientmode *self), { self->trydropflag(); });
    }

    void respawned()
    {
        loopv(flags) flags[i].pickup = false;
    }

    void preload()
    {
        static const char *flagmodels[2] = { "flags/red", "flags/blue" };
        loopi(2) preloadmodel(flagmodels[i]);
    }

    void drawradar(float x, float y, float s)
    {
        glTexCoord2f(0.0f, 0.0f); glVertex2f(x,   y);
        glTexCoord2f(1.0f, 0.0f); glVertex2f(x+s, y);
        glTexCoord2f(1.0f, 1.0f); glVertex2f(x+s, y+s);
        glTexCoord2f(0.0f, 1.0f); glVertex2f(x,   y+s);
    }

    void drawblips(fpsent *d, int x, int y, int s, int i, bool flagblip)
    {
        flag &f = flags[i];
        settexture(f.team==ctfteamflag(player1->team) ? 
                    (flagblip ? "packages/hud/blip_blue_flag.png" : "packages/hud/blip_blue.png") : 
                    (flagblip ? "packages/hud/blip_red_flag.png" : "packages/hud/blip_red.png"));
        float scale = radarscale<=0 || radarscale>maxradarscale ? maxradarscale : radarscale;
        vec dir;
        if(flagblip) dir = f.owner ? f.owner->o : (f.droptime ? f.droploc : f.spawnloc);
        else dir = f.spawnloc;
        dir.sub(d->o);
        dir.z = 0.0f;
        float size = flagblip ? 0.1f : 0.05f,
              xoffset = flagblip ? -2*(3/32.0f)*size : -size,
              yoffset = flagblip ? -2*(1 - 3/32.0f)*size : -size,
              dist = dir.magnitude();
        if(dist >= scale*(1 - 0.05f)) dir.mul(scale*(1 - 0.05f)/dist);
        dir.rotate_around_z(-d->yaw*RAD);
        glBegin(GL_QUADS);
        drawradar(x + s*0.5f*(1.0f + dir.x/scale + xoffset), y + s*0.5f*(1.0f + dir.y/scale + yoffset), size*s);
        glEnd();
    }

    void drawhud(fpsent *d, int w, int h)
    {
        if(d->state == CS_ALIVE)
        {
            loopv(flags) if(flags[i].owner == d)
            {
                drawicon(flags[i].team==ctfteamflag(d->team) ? HICON_BLUE_FLAG : HICON_RED_FLAG, 1820, 1650);
                break;
            }
        }

        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        int x = 1800*w/h*34/40, y = 1800*1/40, s = 1800*w/h*5/40;
        glColor3f(1, 1, 1);
        settexture("packages/hud/radar.png");
        glBegin(GL_QUADS);
        drawradar(float(x), float(y), float(s));
        glEnd();
        loopv(flags)
        {
            flag &f = flags[i];
            if(!ctfflagteam(f.team)) continue;
            drawblips(d, x, y, s, i, false);
            if(!f.ent) continue;
            if(f.owner)
            {
                if(lastmillis%1000 >= 500) continue;
            }
            else if(f.droptime && (f.droploc.x < 0 || lastmillis%300 >= 150)) continue;
            drawblips(d, x, y, s, i, true);
        }
        if(d->state == CS_DEAD)
        {
            int wait = respawnwait(d);
            if(wait>=0)
            {
                glPushMatrix();
                glLoadIdentity();
                glOrtho(0, w*900/h, 900, 0, -1, 1);
                bool flash = wait>0 && d==player1 && lastspawnattempt>=d->lastpain && lastmillis < lastspawnattempt+100;
                draw_textf("%s%d", (x+s/2)/2-(wait>=10 ? 28 : 16), (y+s/2)/2-32, flash ? "\f3" : "", wait);
                glPopMatrix();
            }
        }
    }

    void removeplayer(fpsent *d)
    {
        loopv(flags) if(flags[i].owner == d) 
        {
            flag &f = flags[i];
            f.interploc.x = -1;
            f.interptime = 0;
            dropflag(i, f.owner->o, 1);
        }
    }

    vec interpflagpos(flag &f, float &angle)
    {
        vec pos = f.owner ? vec(f.owner->abovehead()).add(vec(0, 0, 1)) : (f.droptime ? f.droploc : f.spawnloc);
        angle = f.owner ? f.owner->yaw : (f.ent ? f.ent->attr1 : 0);
        if(pos.x < 0) return pos;
        if(f.interptime && f.interploc.x >= 0) 
        {
            float t = min((lastmillis - f.interptime)/500.0f, 1.0f);
            pos.lerp(f.interploc, pos, t);
            angle += (1-t)*(f.interpangle - angle);
        }
        return pos;
    }

    vec interpflagpos(flag &f) { float angle; return interpflagpos(f, angle); }

    void rendergame()
    {
        loopv(flags)
        {
            flag &f = flags[i];
            if(!f.ent || (!f.owner && f.droptime && f.droploc.x < 0)) continue;
            const char *flagname = f.team==ctfteamflag(player1->team) ? "flags/blue" : "flags/red";
            float angle;
            vec pos = interpflagpos(f, angle);
            rendermodel(!f.droptime && !f.owner ? &f.ent->light : NULL, flagname, ANIM_MAPMODEL|ANIM_LOOP,
                        interpflagpos(f), angle, 0,  
                        MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_OCCLUDED | (f.droptime || f.owner ? MDL_LIGHT : 0) | (f.invis ? MDL_TRANSLUCENT : 0));
        }
    }

    void setup()
    {
        resetflags();
        loopv(entities::ents)
        {
            extentity *e = entities::ents[i];
            if(e->type!=FLAG || e->attr2<1 || e->attr2>2) continue;
            int index = flags.length();
            addflag(index, e->o, e->attr2);
            flags[index].ent = e;
        }
        vec center(0, 0, 0);
        loopv(flags) center.add(flags[i].spawnloc);
        center.div(flags.length());
        radarscale = 0;
        loopv(flags) radarscale = max(radarscale, 2*center.dist(flags[i].spawnloc));
    }

    void senditems(ucharbuf &p)
    {
        putint(p, SV_INITFLAGS);
        putint(p, flags.length());
        loopv(flags)
        {
            flag &f = flags[i];
            putint(p, f.team);
            loopk(3) putint(p, int(f.spawnloc[k]*DMF));
        }
    } 

    void parseflags(ucharbuf &p, bool commit)
    {
        loopk(2)
        {
            int score = getint(p);
            if(commit) scores[k] = score;
        }
        int numflags = getint(p);
        loopi(numflags)
        {
            int owner = getint(p), invis = getint(p), dropped = 0;
            vec droploc(0, 0, 0);
            if(owner<0)
            {
                dropped = getint(p);
                if(dropped) loopk(3) droploc[k] = getint(p)/DMF;
            }
            if(commit && flags.inrange(i))
            {
                flag &f = flags[i];
                f.owner = owner>=0 ? (owner==player1->clientnum ? player1 : newclient(owner)) : NULL;
                f.droptime = dropped;
                f.droploc = dropped ? droploc : f.spawnloc;
                f.invis = invis>0;
                f.interptime = 0;
                
                if(dropped)
                {
                    f.droploc.z += 4;
                    if(!droptofloor(f.droploc, 4, 0)) f.droploc = vec(-1, -1, -1);
                }
            }
        }
    }

    void trydropflag()
    {
        if(!m_ctf) return;
        loopv(flags) if(flags[i].owner == player1)
        {
            addmsg(SV_TRYDROPFLAG, "r");
            return;
        }            
    }

    void dropflag(fpsent *d, int i, const vec &droploc)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        dropflag(i, droploc, 1);
        f.droploc.z += 4;
        if(d==player1) f.pickup = true;
        if(!droptofloor(f.droploc, 4, 0)) 
        {
            f.droploc = vec(-1, -1, -1);
            f.interptime = 0;
        }
        conoutf(CON_GAMEINFO, "%s dropped %s flag", d==player1 ? "you" : colorname(d), f.team==ctfteamflag(player1->team) ? "your" : "the enemy");
        playsound(S_FLAGDROP);
    }

    void flagexplosion(int i, const vec &loc)
    {
        int fcolor;
        vec color;
        if(flags[i].team==ctfteamflag(player1->team)) { fcolor = 0x2020FF; color = vec(0.25f, 0.25f, 1); }
        else { fcolor = 0x802020; color = vec(1, 0.25f, 0.25f); }
        particle_fireball(loc, 30, PART_EXPLOSION, -1, fcolor, 4.8f);
        adddynlight(loc, 35, color, 900, 100);
        particle_splash(PART_SPARK, 150, 300, loc, 0xB49B4B, 0.24f);
    }

    void flageffect(int i, const vec &from, const vec &to)
    {
        vec fromexp(from), toexp(to);
        if(from.x >= 0) 
        {
            fromexp.z += 8;
            flagexplosion(i, fromexp);
        }
        if(from==to) return;
        if(to.x >= 0) 
        {
            toexp.z += 8;
            flagexplosion(i, toexp);
        }
        if(from.x >= 0 && to.x >= 0)
            particle_flare(fromexp, toexp, 600, PART_LIGHTNING, flags[i].team==ctfteamflag(player1->team) ? 0x2222FF : 0xFF2222, 0.28f);
    }
 
    void returnflag(fpsent *d, int i)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        flageffect(i, interpflagpos(f), f.spawnloc);
        f.interptime = 0;
        returnflag(i);
        conoutf(CON_GAMEINFO, "%s returned %s flag", d==player1 ? "you" : colorname(d), f.team==ctfteamflag(player1->team) ? "your" : "the enemy");
        playsound(S_FLAGRETURN);
    }

    void resetflag(int i, int team, int score)
    {
        setscore(team, score);
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        flageffect(i, interpflagpos(f), f.spawnloc);
        f.interptime = 0;
        returnflag(i);
        conoutf(CON_GAMEINFO, "%s flag reset", f.team==ctfteamflag(player1->team) ? "your" : "the enemy");
        playsound(S_FLAGRESET);
    }

    void scoreflag(fpsent *d, int relay, int goal, int team, int score)
    {
        setscore(team, score);
        if(flags.inrange(goal))
        {
            flag &f = flags[goal];
            if(relay >= 0) flageffect(goal, f.spawnloc, flags[relay].spawnloc);
            else flageffect(goal, interpflagpos(f), f.spawnloc);
            f.interptime = 0;
            returnflag(relay >= 0 ? relay : goal, relay < 0);
            f.pickup = d==player1 && d->feetpos().dist(f.spawnloc) < FLAGRADIUS;
        }
        if(d!=player1)
        {
            s_sprintfd(ds)("@%d", score);
            particle_text(d->abovehead(), ds, PART_TEXT_RISE, 2000, 0x32FF64, 4.0f);
        }
        conoutf(CON_GAMEINFO, "%s scored for %s team", d==player1 ? "you" : colorname(d), team==ctfteamflag(player1->team) ? "your" : "the enemy");
        playsound(S_FLAGSCORE);

        if(score >= FLAGLIMIT) conoutf(CON_GAMEINFO, "%s team captured %d flags", team==ctfteamflag(player1->team) ? "your" : "the enemy", score);
    }

    void takeflag(fpsent *d, int i)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.interploc = interpflagpos(f, f.interpangle);
        f.interptime = lastmillis;
        conoutf(CON_GAMEINFO, "%s %s %s flag", d==player1 ? "you" : colorname(d), m_protect || f.droptime ? "picked up" : "stole", f.team==ctfteamflag(player1->team) ? "your" : "the enemy");
        ownflag(i, d);
        playsound(S_FLAGPICKUP);
    }

    void invisflag(int i, int invis)
    {
        if(!flags.inrange(i)) return;
        flag &f = flags[i];
        f.invis = invis>0;
    }

    void checkitems(fpsent *d)
    {
        vec o = d->feetpos();
        loopv(flags)
        {
            flag &f = flags[i];
            if(!f.ent || !ctfflagteam(f.team) || f.owner || (f.droptime && f.droploc.x<0)) continue;
            if(o.dist(f.droptime ? f.droploc : f.spawnloc) < FLAGRADIUS)
            {
                if(f.pickup) continue;
                addmsg(SV_TAKEFLAG, "ri", i);
                f.pickup = true;
            }
            else f.pickup = false;
       }
    }

    int respawnwait(fpsent *d)
    {
        return max(0, RESPAWNSECS-(lastmillis-d->lastpain)/1000);
    }

    void pickspawn(fpsent *d)
    {
        findplayerspawn(d, -1, ctfteamflag(player1->team));
    }

    const char *prefixnextmap() { return "ctf_"; }
};
#endif

#elif SERVMODE

case SV_TRYDROPFLAG:
{
    if(ci->state.state!=CS_SPECTATOR && smode==&ctfmode) ctfmode.dropflag(ci);
    break;
}

case SV_TAKEFLAG:
{
    int flag = getint(p);
    if(ci->state.state!=CS_SPECTATOR && smode==&ctfmode) ctfmode.takeflag(ci, flag);
    break;
}

case SV_INITFLAGS:
    if(smode==&ctfmode) ctfmode.parseflags(p, ci->state.state!=CS_SPECTATOR || ci->privilege || ci->local);
    break;

#else

case SV_INITFLAGS:
{
    ctfmode.parseflags(p, m_ctf);
    break;
}

case SV_DROPFLAG:
{
    int ocn = getint(p), flag = getint(p);
    vec droploc;
    loopk(3) droploc[k] = getint(p)/DMF;
    fpsent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
    if(o && m_ctf) ctfmode.dropflag(o, flag, droploc);
    break;
}

case SV_SCOREFLAG:
{
    int ocn = getint(p), relayflag = getint(p), goalflag = getint(p), team = getint(p), score = getint(p);
    fpsent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
    if(o && m_ctf) ctfmode.scoreflag(o, relayflag, goalflag, team, score);
    break;
}

case SV_RETURNFLAG:
{
    int ocn = getint(p), flag = getint(p);
    fpsent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
    if(o && m_ctf) ctfmode.returnflag(o, flag);
    break;
}

case SV_TAKEFLAG:
{
    int ocn = getint(p), flag = getint(p);
    fpsent *o = ocn==player1->clientnum ? player1 : newclient(ocn);
    if(o && m_ctf) ctfmode.takeflag(o, flag);
    break;
}

case SV_RESETFLAG:
{
    int flag = getint(p), team = getint(p), score = getint(p);
    if(m_ctf) ctfmode.resetflag(flag, team, score);
    break;
}

case SV_INVISFLAG:
{
    int flag = getint(p), invis = getint(p);
    if(m_ctf) ctfmode.invisflag(flag, invis);
    break;
}

#endif


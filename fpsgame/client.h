struct clientcom : iclientcom
{
    fpsclient &cl;

    bool c2sinit;       // whether we need to tell the other clients our stats

    bool senditemstoserver;     // after a map change, since server doesn't have map data
    int lastping;

    bool connected, remote, demoplayback;

    bool spectator;

    fpsent *player1;

    clientcom(fpsclient &_cl) : cl(_cl), c2sinit(false), senditemstoserver(false), lastping(0), connected(false), remote(false), demoplayback(false), spectator(false), player1(_cl.player1)
    {
        CCOMMAND(say, "C", (clientcom *self, char *s), self->toserver(s));
        CCOMMAND(sayteam, "C", (clientcom *self, char *s), self->sayteam(s));
        CCOMMAND(name, "s", (clientcom *self, char *s), self->switchname(s));
        CCOMMAND(team, "s", (clientcom *self, char *s), self->switchteam(s));
        CCOMMAND(map, "s", (clientcom *self, char *s), self->changemap(s));
        CCOMMAND(clearbans, "", (clientcom *self, char *s), self->clearbans());
        CCOMMAND(kick, "s", (clientcom *self, char *s), self->kick(s));
        CCOMMAND(goto, "s", (clientcom *self, char *s), self->gotoplayer(s));
        CCOMMAND(spectator, "is", (clientcom *self, int *val, char *who), self->togglespectator(*val, who));
        CCOMMAND(mastermode, "i", (clientcom *self, int *val), if(self->remote) self->addmsg(SV_MASTERMODE, "ri", *val));
        CCOMMAND(setmaster, "s", (clientcom *self, char *s), self->setmaster(s));
        CCOMMAND(approve, "s", (clientcom *self, char *s), self->approvemaster(s));
        CCOMMAND(setteam, "ss", (clientcom *self, char *who, char *team), self->setteam(who, team));
        CCOMMAND(getmap, "", (clientcom *self), self->getmap());
        CCOMMAND(sendmap, "", (clientcom *self), self->sendmap());
        CCOMMAND(listdemos, "", (clientcom *self), self->listdemos());
        CCOMMAND(getdemo, "i", (clientcom *self, int *val), self->getdemo(*val));
        CCOMMAND(recorddemo, "i", (clientcom *self, int *val), self->recorddemo(*val));
        CCOMMAND(stopdemo, "", (clientcom *self), self->stopdemo());
        CCOMMAND(cleardemos, "i", (clientcom *self, int *val), self->cleardemos(*val));
        CCOMMAND(auth, "", (clientcom *self), self->tryauth()); 
        CCOMMAND(getmode, "", (clientcom *self), intret(self->cl.gamemode));
        CCOMMAND(getname, "", (clientcom *self), result(self->player1->name));
        CCOMMAND(getteam, "", (clientcom *self), result(self->player1->team));
        CCOMMAND(getclientfocus, "", (clientcom *self), intret(self->getclientfocus()));
        CCOMMAND(getclientname, "i", (clientcom *self, int *cn), result(self->getclientname(*cn)));
        CCOMMAND(getclientnum, "s", (clientcom *self, char *name), intret(name[0] ? self->parseplayer(name) : self->player1->clientnum));
        CCOMMAND(listclients, "i", (clientcom *self, int *local), self->listclients(*local!=0));
    }

    void switchname(const char *name)
    {
        if(name[0]) 
        { 
            c2sinit = false; 
            filtertext(player1->name, name, false, MAXNAMELEN);
            if(!player1->name[0]) s_strcpy(player1->name, "unnamed");
        }
        else conoutf("your name is: %s", cl.colorname(player1));
    }

    void switchteam(const char *team)
    {
        if(team[0]) 
        { 
            c2sinit = false; 
            filtertext(player1->team, team, false, MAXTEAMLEN);
        }
        else conoutf("your team is: %s", player1->team);
    }

    int numchannels() { return 3; }

    void mapstart() { if(!spectator || player1->privilege) senditemstoserver = true; }

    void initclientnet()
    {
    }

    void writeclientinfo(FILE *f)
    {
        fprintf(f, "name \"%s\"\nteam \"%s\"\n", player1->name, player1->team);
    }

    void gameconnect(bool _remote)
    {
        connected = true;
        remote = _remote;
        if(editmode) toggleedit();
    }

    void gamedisconnect()
    {
        if(remote) cl.stopfollowing();
        connected = false;
        player1->clientnum = -1;
        c2sinit = false;
        player1->lifesequence = 0;
        player1->privilege = PRIV_NONE;
        spectator = false;
        loopv(cl.players) if(cl.players[i]) cl.clientdisconnected(i, false);
    }

    bool allowedittoggle()
    {
        bool allow = !connected || !remote || cl.gamemode==1;
        if(!allow) conoutf(CON_ERROR, "editing in multiplayer requires coopedit mode (1)");
        if(allow && spectator) return false;
        return allow;
    }

    void edittoggled(bool on)
    {
        addmsg(SV_EDITMODE, "ri", on ? 1 : 0);
        if(player1->state==CS_DEAD) cl.deathstate(player1, true);
        else if(player1->state==CS_EDITING && player1->editstate==CS_DEAD) cl.sb.showscores(false);
        setvar("zoom", -1, true);
    }

    int getclientfocus()
    {
        fpsent *d = cl.pointatplayer();
        return d ? d->clientnum : -1;
    }

    const char *getclientname(int cn)
    {
        if(cn == cl.player1->clientnum) return cl.player1->name;

        fpsent *d = cl.getclient(cn);
        return d ? d->name : "";
    }

    int parseplayer(const char *arg)
    {
        char *end;
        int n = strtol(arg, &end, 10);
        if(*arg && !*end) 
        {
            if(n!=player1->clientnum && !cl.players.inrange(n)) return -1;
            return n;
        }
        // try case sensitive first
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && !strcmp(arg, o->name)) return o->clientnum;
        }
        // nothing found, try case insensitive
        loopi(cl.numdynents())
        {
            fpsent *o = (fpsent *)cl.iterdynents(i);
            if(o && !strcasecmp(arg, o->name)) return o->clientnum;
        }
        return -1;
    }

    void listclients(bool local)
    {
        vector<char> buf;
        string cn;
        int numclients = 0;
        if(local)
        {
            s_sprintf(cn)("%d", player1->clientnum);
            buf.put(cn, strlen(cn));
            numclients++;
        }
        loopv(cl.players) if(cl.players[i])
        {
            s_sprintf(cn)("%d", cl.players[i]->clientnum);
            if(numclients++) buf.add(' ');
            buf.put(cn, strlen(cn));
        }
        buf.add('\0');
        result(buf.getbuf());
    }

    void clearbans()
    {
        if(!remote) return;
        addmsg(SV_CLEARBANS, "r");
    }

    void kick(const char *arg)
    {
        if(!remote) return;
        int i = parseplayer(arg);
        if(i>=0 && i!=player1->clientnum) addmsg(SV_KICK, "ri", i);
    }

    void setteam(const char *arg1, const char *arg2)
    {
        if(!remote) return;
        int i = parseplayer(arg1);
        if(i>=0 && i!=player1->clientnum) addmsg(SV_SETTEAM, "ris", i, arg2);
    }

    void setmaster(const char *arg)
    {
        if(!remote || !arg[0]) return;
        int val = 1;
        const char *passwd = "";
        if(!arg[1] && isdigit(arg[0])) val = atoi(arg); 
        else passwd = arg;
        addmsg(SV_SETMASTER, "ris", val, passwd);
    }

    void approvemaster(const char *who)
    {
        if(!remote) return;
        int i = parseplayer(who);
        if(i>=0) addmsg(SV_APPROVEMASTER, "ri", i);
    }

    void tryauth()
    {
        if(!remote || !cl.authname[0]) return;
        cl.lastauth = cl.lastmillis;
        addmsg(SV_AUTHTRY, "rs", cl.authname);
    }

    void togglespectator(int val, const char *who)
    {
        if(!remote) return;
        int i = who[0] ? parseplayer(who) : player1->clientnum;
        if(i>=0) addmsg(SV_SPECTATOR, "rii", i, val);
    }

    // collect c2s messages conveniently
    vector<uchar> messages;

    void addmsg(int type, const char *fmt = NULL, ...)
    {
        if(remote && spectator && (!player1->privilege || type<SV_MASTERMODE))
        {
            static int spectypes[] = { SV_MAPVOTE, SV_GETMAP, SV_TEXT, SV_SETMASTER };
            bool allowed = false;
            loopi(sizeof(spectypes)/sizeof(spectypes[0])) if(type==spectypes[i]) 
            {
                allowed = true;
                break;
            }
            if(!allowed) return;
        }
        static uchar buf[MAXTRANS];
        ucharbuf p(buf, MAXTRANS);
        putint(p, type);
        int numi = 1, nums = 0;
        bool reliable = false;
        if(fmt)
        {
            va_list args;
            va_start(args, fmt);
            while(*fmt) switch(*fmt++)
            {
                case 'r': reliable = true; break;
                case 'v':
                {
                    int n = va_arg(args, int);
                    int *v = va_arg(args, int *);
                    loopi(n) putint(p, v[i]);
                    numi += n;
                    break;
                }

                case 'i': 
                {
                    int n = isdigit(*fmt) ? *fmt++-'0' : 1;
                    loopi(n) putint(p, va_arg(args, int));
                    numi += n;
                    break;
                }
                case 's': sendstring(va_arg(args, const char *), p); nums++; break;
            }
            va_end(args);
        } 
        int num = nums?0:numi, msgsize = msgsizelookup(type);
        if(msgsize && num!=msgsize) { s_sprintfd(s)("inconsistent msg size for %d (%d != %d)", type, num, msgsize); fatal(s); }
        int len = p.length();
        messages.add(len&0xFF);
        messages.add((len>>8)|(reliable ? 0x80 : 0));
        loopi(len) messages.add(buf[i]);
    }

    void toserver(char *text) { conoutf(CON_CHAT, "%s:\f0 %s", cl.colorname(player1), text); addmsg(SV_TEXT, "rs", text); }
    void sayteam(char *text) { conoutf(CON_TEAMCHAT, "%s:\f1 %s", cl.colorname(player1), text); addmsg(SV_SAYTEAM, "rs", text); }

    int sendpacketclient(ucharbuf &p, bool &reliable, dynent *d)
    {
        if(d->state==CS_ALIVE || d->state==CS_EDITING)
        {
            // send position updates separately so as to not stall out aiming
            ENetPacket *packet = enet_packet_create(NULL, 100, 0);
            ucharbuf q(packet->data, packet->dataLength);
            putint(q, SV_POS);
            putint(q, player1->clientnum);
            putuint(q, (int)(d->o.x*DMF));              // quantize coordinates to 1/4th of a cube, between 1 and 3 bytes
            putuint(q, (int)(d->o.y*DMF));
            putuint(q, (int)(d->o.z*DMF));
            putuint(q, (int)d->yaw);
            putint(q, (int)d->pitch);
            putint(q, (int)d->roll);
            putint(q, (int)(d->vel.x*DVELF));          // quantize to itself, almost always 1 byte
            putint(q, (int)(d->vel.y*DVELF));
            putint(q, (int)(d->vel.z*DVELF));
            putuint(q, d->physstate | (d->falling.x || d->falling.y ? 0x20 : 0) | (d->falling.z ? 0x10 : 0) | ((((fpsent *)d)->lifesequence&1)<<6));
            if(d->falling.x || d->falling.y)
            {
                putint(q, (int)(d->falling.x*DVELF));      // quantize to itself, almost always 1 byte
                putint(q, (int)(d->falling.y*DVELF));
            }
            if(d->falling.z) putint(q, (int)(d->falling.z*DVELF));
            // pack rest in almost always 1 byte: strafe:2, move:2, garmour: 1, yarmour: 1, quad: 1
            uint flags = (d->strafe&3) | ((d->move&3)<<2);
            putuint(q, flags);
            enet_packet_resize(packet, q.length());
            sendpackettoserv(packet, 0);
        }
        if(senditemstoserver)
        {
            int gamemode = cl.gamemode;
            reliable = !m_noitems || m_capture || m_ctf;
            if(!m_noitems) cl.et.putitems(p, gamemode);
            if(m_capture) cl.cpc.sendbases(p);
            else if(m_ctf) cl.ctf.sendflags(p);
            senditemstoserver = false;
        }
        if(!c2sinit)    // tell other clients who I am
        {
            reliable = true;
            c2sinit = true;
            putint(p, SV_INITC2S);
            sendstring(player1->name, p);
            sendstring(player1->team, p);
        }
        int i = 0;
        while(i < messages.length()) // send messages collected during the previous frames
        {
            int len = messages[i] | ((messages[i+1]&0x7F)<<8);
            if(p.remaining() < len) break;
            if(messages[i+1]&0x80) reliable = true;
            p.put(&messages[i+2], len);
            i += 2 + len;
        }
        messages.remove(0, i);
        if(!spectator && p.remaining()>=10 && cl.lastmillis-lastping>250)
        {
            putint(p, SV_PING);
            putint(p, cl.lastmillis);
            lastping = cl.lastmillis;
        }
        return 1;
    }

    void updatepos(fpsent *d)
    {
        // update the position of other clients in the game in our world
        // don't care if he's in the scenery or other players,
        // just don't overlap with our client

        const float r = player1->radius+d->radius;
        const float dx = player1->o.x-d->o.x;
        const float dy = player1->o.y-d->o.y;
        const float dz = player1->o.z-d->o.z;
        const float rz = player1->aboveeye+d->eyeheight;
        const float fx = (float)fabs(dx), fy = (float)fabs(dy), fz = (float)fabs(dz);
        if(fx<r && fy<r && fz<rz && player1->state!=CS_SPECTATOR && d->state!=CS_DEAD)
        {
            if(fx<fy) d->o.y += dy<0 ? r-fy : -(r-fy);  // push aside
            else      d->o.x += dx<0 ? r-fx : -(r-fx);
        }
        int lagtime = cl.lastmillis-d->lastupdate;
        if(lagtime)
        {
            if(d->state!=CS_SPAWNING && d->lastupdate) d->plag = (d->plag*5+lagtime)/6;
            d->lastupdate = cl.lastmillis;
        }
    }

    void parsepositions(ucharbuf &p)
    {
        int type;
        while(p.remaining()) switch(type = getint(p))
        {
            case SV_POS:                        // position of another client
            {
                int cn = getint(p);
                vec o, vel, falling;
                float yaw, pitch, roll;
                int physstate, f;
                o.x = getuint(p)/DMF;
                o.y = getuint(p)/DMF;
                o.z = getuint(p)/DMF;
                yaw = (float)getuint(p);
                pitch = (float)getint(p);
                roll = (float)getint(p);
                vel.x = getint(p)/DVELF;
                vel.y = getint(p)/DVELF;
                vel.z = getint(p)/DVELF;
                physstate = getuint(p);
                falling = vec(0, 0, 0);
                if(physstate&0x20)
                {
                    falling.x = getint(p)/DVELF;
                    falling.y = getint(p)/DVELF;
                }
                if(physstate&0x10) falling.z = getint(p)/DVELF;
                int seqcolor = (physstate>>6)&1;
                f = getuint(p);
                fpsent *d = cl.getclient(cn);
                if(!d || seqcolor!=(d->lifesequence&1)) continue;
                float oldyaw = d->yaw, oldpitch = d->pitch;
                d->yaw = yaw;
                d->pitch = pitch;
                d->roll = roll;
                d->strafe = (f&3)==3 ? -1 : f&3;
                f >>= 2;
                d->move = (f&3)==3 ? -1 : f&3;
                vec oldpos(d->o);
                if(cl.allowmove(d))
                {
                    d->o = o;
                    d->vel = vel;
                    d->falling = falling;
                    d->physstate = physstate & 0x0F;
                    updatephysstate(d);
                    updatepos(d);
                }
                if(d->state==CS_DEAD)
                {
                    d->resetinterp();
                    d->smoothmillis = 0;
                }
                else if(cl.smoothmove() && d->smoothmillis>=0 && oldpos.dist(d->o) < cl.smoothdist())
                {
                    d->newpos = d->o;
                    d->newyaw = d->yaw;
                    d->newpitch = d->pitch;
                    d->o = oldpos;
                    d->yaw = oldyaw;
                    d->pitch = oldpitch;
                    (d->deltapos = oldpos).sub(d->newpos);
                    d->deltayaw = oldyaw - d->newyaw;
                    if(d->deltayaw > 180) d->deltayaw -= 360;
                    else if(d->deltayaw < -180) d->deltayaw += 360;
                    d->deltapitch = oldpitch - d->newpitch;
                    d->smoothmillis = cl.lastmillis;
                }
                else d->smoothmillis = 0;
                if(d->state==CS_LAGGED || d->state==CS_SPAWNING) d->state = CS_ALIVE;
                break;
            }

            default:
                neterr("type");
                return;
        }
    }

    void parsepacketclient(int chan, ucharbuf &p)   // processes any updates from the server
    {
        switch(chan)
        {
            case 0: 
                parsepositions(p); 
                break;

            case 1:
                parsemessages(-1, NULL, p);
                break;

            case 2: 
                receivefile(p.buf, p.maxlen);
                break;
        }
    }

    void parsestate(fpsent *d, ucharbuf &p, bool resume = false)
    {
        if(!d) { static fpsent dummy; d = &dummy; }
        if(resume) 
        {
            if(d==cl.player1) getint(p);
            else d->state = getint(p);
            d->frags = getint(p);
            if(d==cl.player1) getint(p);
            else d->quadmillis = getint(p);
        }
        d->lifesequence = getint(p);
        d->health = getint(p);
        d->maxhealth = getint(p);
        d->armour = getint(p);
        d->armourtype = getint(p);
        if(resume && d==cl.player1) 
        {
            getint(p);
            loopi(GUN_PISTOL-GUN_SG+1) getint(p);
        }
        else
        {
            d->gunselect = getint(p);
            loopi(GUN_PISTOL-GUN_SG+1) d->ammo[GUN_SG+i] = getint(p);
        }
    }

    void parsemessages(int cn, fpsent *d, ucharbuf &p)
    {
        int gamemode = cl.gamemode;
        static char text[MAXTRANS];
        int type;
        bool mapchanged = false;

        while(p.remaining()) switch(type = getint(p))
        {
            case SV_INITS2C:                    // welcome messsage from the server
            {
                int mycn = getint(p), prot = getint(p), hasmap = getint(p);
                if(prot!=PROTOCOL_VERSION)
                {
                    conoutf(CON_ERROR, "you are using a different game protocol (you: %d, server: %d)", PROTOCOL_VERSION, prot);
                    disconnect();
                    return;
                }
                player1->clientnum = mycn;      // we are now fully connected
                if(!hasmap && (cl.gamemode==1 || cl.getclientmap()[0])) changemap(cl.getclientmap()); // we are the first client on this server, set map
                gamemode = cl.gamemode;
                break;
            }

            case SV_CLIENT:
            {
                int cn = getint(p), len = getuint(p);
                ucharbuf q = p.subbuf(len);
                parsemessages(cn, cl.getclient(cn), q);
                break;
            }

            case SV_SOUND:
                if(!d) return;
                playsound(getint(p), &d->o);
                break;

            case SV_TEXT:
            {
                if(!d) return;
                getstring(text, p);
                filtertext(text, text);
                if(d->state!=CS_DEAD && d->state!=CS_SPECTATOR) 
                {
                    s_sprintfd(ds)("@%s", &text);
                    particle_text(d->abovehead(), ds, 9);
                }
                conoutf(CON_CHAT, "%s:\f0 %s", cl.colorname(d), text);
                break;
            }

            case SV_SAYTEAM:
            {
                int tcn = getint(p);
                fpsent *t = tcn==cl.player1->clientnum ? cl.player1 : cl.getclient(tcn);
                getstring(text, p);
                filtertext(text, text);
                if(!t) break;
                if(t->state!=CS_DEAD && t->state!=CS_SPECTATOR)
                {
                    s_sprintfd(ts)("@%s", &text);
                    particle_text(t->abovehead(), ts, 34);
                }
                conoutf(CON_TEAMCHAT, "%s:\f1 %s", cl.colorname(t), text);
                break;
            }

            case SV_MAPCHANGE:
                getstring(text, p);
                changemapserv(text, getint(p));
                mapchanged = true;
                gamemode = cl.gamemode;
                if(getint(p)) cl.et.spawnitems(gamemode);
                else senditemstoserver = false;
                break;

            case SV_ARENAWIN:
            {
                int acn = getint(p);
                fpsent *alive = acn<0 ? NULL : (acn==player1->clientnum ? player1 : cl.getclient(acn));
                conoutf(CON_GAMEINFO, "arena round is over! next round in 5 seconds...");
                if(!alive) conoutf(CON_GAMEINFO, "everyone died!");
                else if(m_teammode) conoutf(CON_GAMEINFO, "team %s has won the round", alive->team);
                else if(alive==player1) conoutf(CON_GAMEINFO, "you are the last man standing!");
                else conoutf(CON_GAMEINFO, "%s is the last man standing", cl.colorname(alive));
                break;
            }

            case SV_FORCEDEATH:
            {
                int cn = getint(p);
                fpsent *d = cn==player1->clientnum ? player1 : cl.newclient(cn);
                if(!d) break;
                if(d==player1)
                {
                    if(editmode) toggleedit();
                    cl.stopfollowing();
                    cl.sb.showscores(true);
                }
                else d->resetinterp();
                d->state = CS_DEAD;
                break;
            }

            case SV_ITEMLIST:
            {
                int n;
                while((n = getint(p))>=0 && !p.overread())
                {
                    if(mapchanged) cl.et.setspawn(n, true);
                    getint(p); // type
                }
                break;
            }

            case SV_MAPRELOAD:          // server requests next map
            {
                s_sprintfd(nextmapalias)("nextmap_%s%s", m_capture ? "capture_" : (m_ctf ? "ctf_" : ""), cl.getclientmap());
                const char *map = getalias(nextmapalias);     // look up map in the cycle
                addmsg(SV_MAPCHANGE, "rsi", *map ? map : cl.getclientmap(), cl.nextmode);
                break;
            }

            case SV_INITC2S:            // another client either connected or changed name/team
            {
                d = cl.newclient(cn);
                if(!d)
                {
                    getstring(text, p);
                    getstring(text, p);
                    break;
                }
                getstring(text, p);
                filtertext(text, text, false, MAXNAMELEN);
                if(!text[0]) s_strcpy(text, "unnamed");
                if(d->name[0])          // already connected
                {
                    if(strcmp(d->name, text))
                    {
                        string oldname, newname;
                        s_strcpy(oldname, cl.colorname(d));
                        s_strcpy(newname, cl.colorname(d, text));
                        conoutf("%s is now known as %s", oldname, newname);
                    }
                }
                else                    // new client
                {
                    c2sinit = false;    // send new players my info again
                    conoutf("connected: %s", cl.colorname(d, text));
                    loopv(cl.players)   // clear copies since new player doesn't have them
                        if(cl.players[i]) freeeditinfo(cl.players[i]->edit);
                    extern editinfo *localedit;
                    freeeditinfo(localedit);
                }
                s_strncpy(d->name, text, MAXNAMELEN+1);
                getstring(text, p);
                filtertext(d->team, text, false, MAXTEAMLEN);
                break;
            }

            case SV_CDIS:
                cl.clientdisconnected(getint(p));
                break;

            case SV_SPAWN:
            {
                if(d) d->respawn();
                parsestate(d, p);
                if(!d) break;
                d->state = CS_SPAWNING;
                if(cl.player1->state==CS_SPECTATOR && cl.following==d->clientnum)
                    cl.lasthit = 0;
                break;
            }

            case SV_SPAWNSTATE:
            {
                if(editmode) toggleedit();
                cl.stopfollowing();
                player1->respawn();
                parsestate(player1, p);
                player1->state = CS_ALIVE;
                findplayerspawn(player1, m_capture ? cl.cpc.pickspawn(player1->team) : -1, m_ctf ? ctfteamflag(player1->team) : 0);
                cl.sb.showscores(false);
                cl.lasthit = 0;
                if(m_arena) conoutf(CON_GAMEINFO, "new round starting... fight!");
                else if(m_capture) cl.cpc.lastrepammo = -1;
                addmsg(SV_SPAWN, "rii", player1->lifesequence, player1->gunselect);
                break;
            }

            case SV_SHOTFX:
            {
                int scn = getint(p), gun = getint(p);
                vec from, to;
                loopk(3) from[k] = getint(p)/DMF;
                loopk(3) to[k] = getint(p)/DMF;
                fpsent *s = cl.getclient(scn);
                if(!s) break;
                if(gun>GUN_FIST && gun<=GUN_PISTOL && s->ammo[gun]) s->ammo[gun]--; 
                if(gun==GUN_SG) cl.ws.createrays(from, to);
                s->gunselect = clamp(gun, (int)GUN_FIST, (int)GUN_PISTOL);
                s->gunwait = guns[s->gunselect].attackdelay;
                s->lastaction = cl.lastmillis;
                s->lastattackgun = s->gunselect;
                cl.ws.shootv(gun, from, to, s, false);
                break;
            }

            case SV_DAMAGE:
            {
                int tcn = getint(p), 
                    acn = getint(p),
                    damage = getint(p), 
                    armour = getint(p),
                    health = getint(p);
                fpsent *target = tcn==player1->clientnum ? player1 : cl.getclient(tcn),
                       *actor = acn==player1->clientnum ? player1 : cl.getclient(acn);
                if(!target || !actor) break;
                target->armour = armour;
                target->health = health;
                cl.damaged(damage, target, actor, false);
                break;
            }

            case SV_HITPUSH:
            {
                int gun = getint(p), damage = getint(p);
                vec dir;
                loopk(3) dir[k] = getint(p)/DNF;
                player1->hitpush(damage, dir, NULL, gun);
                break;
            }

            case SV_DIED:
            {
                int vcn = getint(p), acn = getint(p), frags = getint(p);
                fpsent *victim = vcn==player1->clientnum ? player1 : cl.getclient(vcn),
                       *actor = acn==player1->clientnum ? player1 : cl.getclient(acn);
                if(!actor) break;
                actor->frags = frags;
                if(actor!=player1 && !m_capture && !m_ctf)
                {
                    s_sprintfd(ds)("@%d", actor->frags);
                    particle_text(actor->abovehead(), ds, 9);
                }
                if(!victim) break;
                cl.killed(victim, actor);
                break;
            }

            case SV_GUNSELECT:
            {
                if(!d) return;
                int gun = getint(p);
                d->gunselect = max(gun, 0);
                playsound(S_WEAPLOAD, &d->o);
                break;
            }

            case SV_TAUNT:
            {
                if(!d) return;
                d->lasttaunt = cl.lastmillis;
                break;
            }

            case SV_RESUME:
            {
                for(;;)
                {
                    int cn = getint(p);
                    if(p.overread() || cn<0) break;
                    fpsent *d = (cn == player1->clientnum ? player1 : cl.newclient(cn));
                    parsestate(d, p, true);
                }
                break;
            }

            case SV_ITEMSPAWN:
            {
                int i = getint(p);
                if(!cl.et.ents.inrange(i)) break;
                cl.et.setspawn(i, true);
                playsound(S_ITEMSPAWN, &cl.et.ents[i]->o);
                const char *name = cl.et.itemname(i);
                if(name) particle_text(cl.et.ents[i]->o, name, 9);
                break;
            }

            case SV_ITEMACC:            // server acknowledges that I picked up this item
            {
                int i = getint(p), cn = getint(p);
                fpsent *d = cn==player1->clientnum ? player1 : cl.getclient(cn);
                cl.et.pickupeffects(i, d);
                break;
            }

            case SV_EDITF:              // coop editing messages
            case SV_EDITT:
            case SV_EDITM:
            case SV_FLIP:
            case SV_COPY:
            case SV_PASTE:
            case SV_ROTATE:
            case SV_REPLACE:
            case SV_DELCUBE:
            {
                if(!d) return;
                selinfo sel;
                sel.o.x = getint(p); sel.o.y = getint(p); sel.o.z = getint(p);
                sel.s.x = getint(p); sel.s.y = getint(p); sel.s.z = getint(p);
                sel.grid = getint(p); sel.orient = getint(p);
                sel.cx = getint(p); sel.cxs = getint(p); sel.cy = getint(p), sel.cys = getint(p);
                sel.corner = getint(p);
                int dir, mode, tex, newtex, mat, allfaces;
                ivec moveo;
                switch(type)
                {
                    case SV_EDITF: dir = getint(p); mode = getint(p); mpeditface(dir, mode, sel, false); break;
                    case SV_EDITT: tex = getint(p); allfaces = getint(p); mpedittex(tex, allfaces, sel, false); break;
                    case SV_EDITM: mat = getint(p); mpeditmat(mat, sel, false); break;
                    case SV_FLIP: mpflip(sel, false); break;
                    case SV_COPY: if(d) mpcopy(d->edit, sel, false); break;
                    case SV_PASTE: if(d) mppaste(d->edit, sel, false); break;
                    case SV_ROTATE: dir = getint(p); mprotate(dir, sel, false); break;
                    case SV_REPLACE: tex = getint(p); newtex = getint(p); mpreplacetex(tex, newtex, sel, false); break;
                    case SV_DELCUBE: mpdelcube(sel, false); break;
                }
                break;
            }
            case SV_REMIP:
            {
                if(!d) return;
                conoutf("%s remipped", cl.colorname(d));
                mpremip(false);
                break;
            }
            case SV_EDITENT:            // coop edit of ent
            {
                if(!d) return;
                int i = getint(p);
                float x = getint(p)/DMF, y = getint(p)/DMF, z = getint(p)/DMF;
                int type = getint(p);
                int attr1 = getint(p), attr2 = getint(p), attr3 = getint(p), attr4 = getint(p);

                mpeditent(i, vec(x, y, z), type, attr1, attr2, attr3, attr4, false);
                break;
            }

            case SV_PONG:
                addmsg(SV_CLIENTPING, "i", player1->ping = (player1->ping*5+cl.lastmillis-getint(p))/6);
                break;

            case SV_CLIENTPING:
                if(!d) return;
                d->ping = getint(p);
                break;

            case SV_TIMEUP:
                cl.timeupdate(getint(p));
                break;

            case SV_SERVMSG:
                getstring(text, p);
                conoutf("%s", text);
                break;

            case SV_SENDDEMOLIST:
            {
                int demos = getint(p);
                if(!demos) conoutf("no demos available");
                else loopi(demos)
                {
                    getstring(text, p);
                    conoutf("%d. %s", i+1, text);
                }
                break;
            }

            case SV_DEMOPLAYBACK:
            {
                int on = getint(p);
                if(on) player1->state = CS_SPECTATOR;
                else stopdemo();
                demoplayback = on!=0;
                const char *alias = on ? "demostart" : "demoend";
                if(identexists(alias)) execute(alias);
                break;
            }

            case SV_CURRENTMASTER:
            {
                int mn = getint(p), priv = getint(p);
                player1->privilege = PRIV_NONE;
                loopv(cl.players) if(cl.players[i]) cl.players[i]->privilege = PRIV_NONE;
                if(mn>=0)
                {
                    fpsent *m = mn==player1->clientnum ? player1 : cl.newclient(mn);
                    m->privilege = priv;
                }
                break;
            }

            case SV_EDITMODE:
            {
                int val = getint(p);
                if(!d) break;
                if(val) 
                {
                    d->editstate = d->state;
                    d->state = CS_EDITING;
                }
                else 
                {
                    d->state = d->editstate;
                    if(d->state==CS_DEAD) cl.deathstate(d, true);
                }
                break;
            }

            case SV_SPECTATOR:
            {
                int sn = getint(p), val = getint(p);
                fpsent *s;
                if(sn==player1->clientnum)
                {
                    spectator = val!=0;
                    s = player1;
                    if(spectator && !player1->privilege) senditemstoserver = false;
                }
                else s = cl.newclient(sn);
                if(!s) return;
                if(val)
                {
                    if(s==player1)
                    {
                        if(editmode) toggleedit();
                        if(s->state==CS_DEAD) cl.sb.showscores(false);
                    }
                    s->state = CS_SPECTATOR;
                }
                else if(s->state==CS_SPECTATOR) 
                {
                    s->state = CS_DEAD;
                    if(s==player1) 
                    {
                        cl.stopfollowing();
                        cl.sb.showscores(true);
                    }
                    else s->resetinterp();
                }
                break;
            }

            case SV_SETTEAM:
            {
                int wn = getint(p);
                getstring(text, p);
                fpsent *w = wn==player1->clientnum ? player1 : cl.getclient(wn);
                if(!w) return;
                filtertext(w->team, text, false, MAXTEAMLEN);
                break;
            }

            case SV_BASEINFO:
            {
                int base = getint(p);
                string owner, enemy;
                getstring(text, p);
                s_strcpy(owner, text);
                getstring(text, p);
                s_strcpy(enemy, text);
                int converted = getint(p), ammo = getint(p);
                if(m_capture) cl.cpc.updatebase(base, owner, enemy, converted, ammo);
                break;
            }

            case SV_BASEREGEN:
            {
                int health = getint(p), armour = getint(p), ammotype = getint(p), ammo = getint(p);
                if(m_capture)
                {
                    player1->health = health;
                    player1->armour = armour;
                    if(ammotype>=GUN_SG && ammotype<=GUN_PISTOL) player1->ammo[ammotype] = ammo;
                }
                break;
            }

            case SV_BASES:
            {
                int numbases = getint(p);
                loopi(numbases)
                {
                    int ammotype = getint(p);
                    string owner, enemy;
                    getstring(text, p);
                    s_strcpy(owner, text);
                    getstring(text, p);
                    s_strcpy(enemy, text);
                    int converted = getint(p), ammo = getint(p);
                    cl.cpc.initbase(i, ammotype, owner, enemy, converted, ammo);
                }
                break;
            }

            case SV_TEAMSCORE:
            {
                getstring(text, p);
                int total = getint(p);
                if(m_capture) cl.cpc.setscore(text, total);
                break;
            }

            case SV_REPAMMO:
            {
                int ammotype = getint(p);
                if(m_capture) cl.cpc.receiveammo(ammotype);
                break;
            }

            case SV_INITFLAGS:
            {
                cl.ctf.parseflags(p, m_ctf);
                break;
            }

            case SV_DROPFLAG:
            {
                int ocn = getint(p), flag = getint(p);
                vec droploc;
                loopk(3) droploc[k] = getint(p)/DMF;
                fpsent *o = ocn==cl.player1->clientnum ? cl.player1 : cl.newclient(ocn);  
                if(m_ctf) cl.ctf.dropflag(o, flag, droploc);
                break;
            }

            case SV_SCOREFLAG:
            {
                int ocn = getint(p), relayflag = getint(p), goalflag = getint(p), score = getint(p);
                fpsent *o = ocn==cl.player1->clientnum ? cl.player1 : cl.newclient(ocn);
                if(m_ctf) cl.ctf.scoreflag(o, relayflag, goalflag, score);
                break;
            }

            case SV_RETURNFLAG:
            {
                int ocn = getint(p), flag = getint(p);
                fpsent *o = ocn==cl.player1->clientnum ? cl.player1 : cl.newclient(ocn);
                if(m_ctf) cl.ctf.returnflag(o, flag);
                break;
            }

            case SV_TAKEFLAG:
            {
                int ocn = getint(p), flag = getint(p);
                fpsent *o = ocn==cl.player1->clientnum ? cl.player1 : cl.newclient(ocn);
                if(m_ctf) cl.ctf.takeflag(o, flag);
                break;
            }

            case SV_RESETFLAG:
            {
                int flag = getint(p);
                if(m_ctf) cl.ctf.resetflag(flag);
                break;
            }

            case SV_ANNOUNCE:
            {
                int t = getint(p);
                if     (t==I_QUAD)  { playsound(S_V_QUAD10);  conoutf(CON_GAMEINFO, "\f2quad damage will spawn in 10 seconds!"); }
                else if(t==I_BOOST) { playsound(S_V_BOOST10); conoutf(CON_GAMEINFO, "\f2+10 health will spawn in 10 seconds!"); }
                break;
            }

            /* assassin compat */
            case SV_CLEARTARGETS:
                break;

            case SV_CLEARHUNTERS:
                break;

            case SV_ADDTARGET:
                getint(p);
                break;

            case SV_REMOVETARGET:
                getint(p);
                break;

            case SV_ADDHUNTER:
                getint(p);
                break;

            case SV_REMOVEHUNTER:
                getint(p);
                break;

            case SV_NEWMAP:
            {
                int size = getint(p);
                if(size>=0) emptymap(size, true);
                else enlargemap(true);
                if(d && d!=player1)
                {
                    int newsize = 0;
                    while(1<<newsize < getworldsize()) newsize++;
                    conoutf(size>=0 ? "%s started a new map of size %d" : "%s enlarged the map to size %d", cl.colorname(d), newsize);
                }
                break;
            }

            case SV_AUTHCHAL:
            {
                uint id = (uint)getint(p);
                getstring(text, p);
                if(cl.lastauth && cl.lastmillis - cl.lastauth < 60*1000 && cl.authname[0])
                {
                    ecjacobian answer;
                    answer.parse(text);
                    answer.mul(cl.authkey);
                    answer.normalize();
                    vector<char> buf;
                    answer.x.printdigits(buf);
                    buf.add('\0');
                    //conoutf(CON_DEBUG, "answering %u, challenge %s with %s", id, text, buf.getbuf());
                    addmsg(SV_AUTHANS, "ris", id, buf.getbuf());
                }
                break;
            }

            default:
                neterr("type");
                return;
        }
    }

    void changemapserv(const char *name, int gamemode)        // forced map change from the server
    {
        if(remote && !m_mp(gamemode)) gamemode = 0;
        cl.gamemode = gamemode;
        cl.nextmode = gamemode;
        cl.minremain = -1;
        if(editmode) toggleedit();
        if(m_demo) { cl.et.resetspawns(); return; }
        if((gamemode==1 && !name[0]) || (!load_world(name) && remote)) 
        {
            emptymap(0, true, name);
            senditemstoserver = false;
        }
        if(m_capture) cl.cpc.setupbases();
        else if(m_ctf) cl.ctf.setupflags();
    }

    void changemap(const char *name) // request map change, server may ignore
    {
        if(spectator && !player1->privilege) return;
        int nextmode = cl.nextmode; // in case stopdemo clobbers cl.nextmode
        if(!remote) stopdemo();
        addmsg(SV_MAPVOTE, "rsi", name, nextmode);
    }
        
    void receivefile(uchar *data, int len)
    {
        ucharbuf p(data, len);
        int type = getint(p);
        data += p.length();
        len -= p.length();
        switch(type)
        {
            case SV_SENDDEMO:
            {
                s_sprintfd(fname)("%d.dmo", cl.lastmillis);
                FILE *demo = openfile(fname, "wb");
                if(!demo) return;
                conoutf("received demo \"%s\"", fname);
                fwrite(data, 1, len, demo);
                fclose(demo);
                break;
            }

            case SV_SENDMAP:
            {
                if(cl.gamemode!=1) return;
                string oldname;
                s_strcpy(oldname, cl.getclientmap());
                s_sprintfd(mname)("getmap_%d", cl.lastmillis);
                s_sprintfd(fname)("packages/base/%s.ogz", mname);
                const char *file = findfile(fname, "wb");
                FILE *map = fopen(file, "wb");
                if(!map) return;
                conoutf("received map");
                fwrite(data, 1, len, map);
                fclose(map);
                load_world(mname, oldname[0] ? oldname : NULL);
                remove(file);
                break;
            }
        }
    }

    void getmap()
    {
        if(cl.gamemode!=1) { conoutf(CON_ERROR, "\"getmap\" only works in coopedit mode"); return; }
        conoutf("getting map...");
        addmsg(SV_GETMAP, "r");
    }

    void stopdemo()
    {
        if(remote)
        {
            if(player1->privilege<PRIV_ADMIN) return;
            addmsg(SV_STOPDEMO, "r");
        }
        else
        {
            loopv(cl.players) if(cl.players[i]) cl.clientdisconnected(i);

            extern igameserver *sv;
            ((fpsserver *)sv)->enddemoplayback();
        }
    }

    void recorddemo(int val)
    {
        if(player1->privilege<PRIV_ADMIN) return;
        addmsg(SV_RECORDDEMO, "ri", val);
    }

    void cleardemos(int val)
    {
        if(player1->privilege<PRIV_ADMIN) return;
        addmsg(SV_CLEARDEMOS, "ri", val);
    }

    void getdemo(int i)
    {
        if(i<=0) conoutf("getting demo...");
        else conoutf("getting demo %d...", i);
        addmsg(SV_GETDEMO, "ri", i);
    }

    void listdemos()
    {
        conoutf("listing demos...");
        addmsg(SV_LISTDEMOS, "r");
    }

    void sendmap()
    {
        if(cl.gamemode!=1 || (spectator && !player1->privilege)) { conoutf(CON_ERROR, "\"sendmap\" only works in coopedit mode"); return; }
        conoutf("sending map...");
        s_sprintfd(mname)("sendmap_%d", cl.lastmillis);
        save_world(mname, true);
        s_sprintfd(fname)("packages/base/%s.ogz", mname);
        const char *file = findfile(fname, "rb");
        FILE *map = fopen(file, "rb");
        if(map)
        {
            fseek(map, 0, SEEK_END);
            int len = ftell(map);
            if(len > 1024*1024) conoutf(CON_ERROR, "map is too large");
            else if(!len) conoutf(CON_ERROR, "could not read map");
            else sendfile(-1, 2, map);
            fclose(map);
        }
        else conoutf(CON_ERROR, "could not read map");
        remove(file);
    }

    void gotoplayer(const char *arg)
    {
        if(player1->state!=CS_SPECTATOR && player1->state!=CS_EDITING) return;
        int i = parseplayer(arg);
        if(i>=0 && i!=player1->clientnum) 
        {
            fpsent *d = cl.getclient(i);
            if(!d) return;
            player1->o = d->o;
            vec dir;
            vecfromyawpitch(player1->yaw, player1->pitch, 1, 0, dir);
            player1->o.add(dir.mul(-32));
            player1->resetinterp();
        }
    }
};

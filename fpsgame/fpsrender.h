struct playermodelinfo
{
    const char *ffa, *blueteam, *redteam, 
               *vwep, *quad, *armour[3],
               *ffaicon, *blueicon, *redicon; 
    bool ragdoll, selectable;
};

struct fpsrender
{      
    fpsclient &cl;

    fpsrender(fpsclient &_cl) : cl(_cl) {}

    vector<fpsent *> bestplayers;
    vector<const char *> bestteams;

    IVARP(ragdoll, 0, 1, 1);
    IVARP(playermodel, 0, 0, 2);
    IVARP(forceplayermodels, 0, 0, 1);

    const playermodelinfo *getplayermodelinfo(int n)
    {
        static const playermodelinfo playermodels[3] =
        {
            { "mrfixit", "mrfixit/blue", "mrfixit/red", NULL, "mrfixit/horns", { "mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow" }, "mrfixit", "mrfixit_blue", "mrfixit_red", true, true},
            { "snoutx10k", "snoutx10k/blue", "snoutx10k/red", NULL, NULL, { "snoutx10k/armor/blue", "snoutx10k/armor/green", "snoutx10k/armor/yellow" }, "ironsnout", "ironsnout_blue", "ironsnout_red", true, true },
            { "ogro/green", "ogro/blue", "ogro/red", "ogro/vwep", NULL, { NULL, NULL, NULL }, "ogro", "ogro", "ogro", false, false }
        };
        if(size_t(n) >= sizeof(playermodels)/sizeof(playermodels[0])) return NULL;
        return &playermodels[n];
    }

    const playermodelinfo &getplayermodelinfo(fpsent *d)
    {
        const playermodelinfo *mdl = getplayermodelinfo(d==cl.player1 || forceplayermodels() ? playermodel() : d->playermodel);
        if(!mdl || !mdl->selectable) mdl = getplayermodelinfo(playermodel());
        return *mdl;
    }

    void preloadplayermodel()
    {
        loopi(3)
        {
            const playermodelinfo *mdl = getplayermodelinfo(i);
            if(!mdl) break;
            if(i != playermodel() && (!cl.cc.remote || forceplayermodels() || !mdl->selectable)) continue;
            if(m_teammode)
            {
                loadmodel(mdl->blueteam, -1, true);
                loadmodel(mdl->redteam, -1, true);
            }
            else loadmodel(mdl->ffa, -1, true);
            if(mdl->vwep) loadmodel(mdl->vwep, -1, true);
            if(mdl->quad) loadmodel(mdl->quad, -1, true);
            loopj(3) if(mdl->armour[j]) loadmodel(mdl->armour[j], -1, true);
        }
    }
    
    IVAR(testquad, 0, 0, 1);
    IVAR(testarmour, 0, 0, 1);
    IVAR(testteam, 0, 0, 3);

    void renderplayer(fpsent *d, const playermodelinfo &mdl, int team)
    {
        int lastaction = d->lastaction, attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT, delay = mdl.vwep ? 300 : cl.ws.reloadtime(d->gunselect)+50;
        if(cl.intermission && d->state!=CS_DEAD)
        {
            lastaction = lastmillis;
            attack = ANIM_LOSE|ANIM_LOOP;
            delay = 1000;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && d->lasttaunt && lastmillis-d->lasttaunt<1000 && lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            attack = ANIM_TAUNT;
            delay = 1000;
        }
        modelattach a[4];
        static const char *vweps[] = {"vwep/fist", "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        int ai = 0;
        if((!mdl.vwep || d->gunselect!=GUN_FIST) && d->gunselect<=GUN_PISTOL)
            a[ai++] = modelattach("tag_weapon", mdl.vwep ? mdl.vwep : vweps[d->gunselect], ANIM_VWEP|ANIM_LOOP, 0);
        if(d->state==CS_ALIVE)
        {
            if((testquad() || d->quadmillis) && mdl.quad)
                a[ai++] = modelattach("tag_powerup", mdl.quad, ANIM_POWERUP|ANIM_LOOP, 0);
            if(testarmour() || d->armour)
            {
                int type = clamp(d->armourtype, (int)A_BLUE, (int)A_YELLOW);
                if(mdl.armour[type])
                    a[ai++] = modelattach("tag_shield", mdl.armour[type], ANIM_SHIELD|ANIM_LOOP, 0);
            }
        }
        const char *mdlname = mdl.ffa;
        switch(testteam() ? testteam()-1 : team)
        {
            case 1: mdlname = mdl.blueteam; break;
            case 2: mdlname = mdl.redteam; break;
        }
        renderclient(d, mdlname, a[0].tag ? a : NULL, attack, delay, lastaction, cl.intermission && d->state!=CS_DEAD ? 0 : d->lastpain, ragdoll() && mdl.ragdoll);
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            entitylight light;
            rendermodel(&light, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*lastmillis/1000.0f, 0, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
        }
#endif
    }

    IVARP(teamskins, 0, 0, 1);

    void rendergame()
    {
        if(cl.intermission)
        {
            if(m_teammode) { bestteams.setsize(0); cl.sb.bestteams(bestteams); }
            else { bestplayers.setsize(0); cl.sb.bestplayers(bestplayers); }
        }

        startmodelbatches();

        fpsent *exclude = NULL;
        if(cl.player1->state==CS_SPECTATOR && cl.following>=0 && !isthirdperson())
            exclude = cl.getclient(cl.following);

        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR && d->state!=CS_SPAWNING && d!=exclude)
        {
            int team = 0;
            if(teamskins() || m_teammode) team = isteam(cl.player1->team, d->team) ? 1 : 2;
            renderplayer(d, getplayermodelinfo(d), team);
            s_strcpy(d->info, cl.colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, PART_TEXT, 1, team ? (team==1 ? 0x6496FF : 0xFF4B19) : 0x1EC850, 2.0f);
        }
        if(isthirdperson() && !cl.followingplayer()) renderplayer(cl.player1, getplayermodelinfo(cl.player1), teamskins() || m_teammode ? 1 : 0);
        cl.ms.monsterrender();
        cl.mo.render();
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(cl.cmode) cl.cmode->rendergame();

        endmodelbatches();
    }
};

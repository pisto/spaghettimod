struct playermodelinfo
{
    const char *ffa, *blueteam, *redteam, 
               *vwep, *quad, *armour[3],
               *ffaicon, *blueicon, *redicon; 
};

struct fpsrender
{      
    fpsclient &cl;

    fpsrender(fpsclient &_cl) : cl(_cl) {}

    vector<fpsent *> bestplayers;
    vector<const char *> bestteams;

    IVARP(playermodel, 0, 0, 2);

    const playermodelinfo &getplayermodelinfo()
    {
        static const playermodelinfo playermodels[3] =
        {
            { "mrfixit", "mrfixit/blue", "mrfixit/red", NULL, "mrfixit/horns", { "mrfixit/armor/blue", "mrfixit/armor/green", "mrfixit/armor/yellow" }, "mrfixit", "mrfixit_blue", "mrfixit_red" },
            { "ironsnout", "ironsnout/blue", "ironsnout/red", NULL, "quadspheres", { "shield/blue", "shield/green", "shield/yellow" }, "ironsnout", "ironsnout_blue", "ironsnout_red" },
            { "ogro", "ogro/blue", "ogro/red", "ogro/vwep", NULL, { NULL, NULL, NULL }, "ogro", "ogro", "ogro" }
        };
        return playermodels[playermodel()];
    }

    void preloadplayermodel()
    {
        const playermodelinfo &mdl = getplayermodelinfo();
        loadmodel(mdl.ffa, -1, true);
        loadmodel(mdl.blueteam, -1, true);
        loadmodel(mdl.redteam, -1, true);
        loadmodel(mdl.vwep, -1, true);
        loadmodel(mdl.quad, -1, true);
        loopi(3) loadmodel(mdl.armour[i], -1, true);
    }
    
    IVAR(testquad, 0, 0, 1);
    IVAR(testarmour, 0, 0, 1);

    void renderplayer(fpsent *d, const playermodelinfo &mdl, int team)
    {
        int lastaction = d->lastaction, attack = d->gunselect==GUN_FIST ? ANIM_PUNCH : ANIM_SHOOT, delay = mdl.vwep ? 300 : cl.ws.reloadtime(d->gunselect)+50;
        if(cl.intermission && d->state!=CS_DEAD)
        {
            lastaction = cl.lastmillis;
            attack = ANIM_LOSE|ANIM_LOOP;
            delay = 1000;
            if(m_teammode) loopv(bestteams) { if(!strcmp(bestteams[i], d->team)) { attack = ANIM_WIN|ANIM_LOOP; break; } }
            else if(bestplayers.find(d)>=0) attack = ANIM_WIN|ANIM_LOOP;
        }
        else if(d->state==CS_ALIVE && d->lasttaunt && cl.lastmillis-d->lasttaunt<1000 && cl.lastmillis-d->lastaction>delay)
        {
            lastaction = d->lasttaunt;
            attack = ANIM_TAUNT;
            delay = 1000;
        }
        modelattach a[4] = { { NULL }, { NULL }, { NULL }, { NULL } };
        static const char *vweps[] = {"vwep/fist", "vwep/shotg", "vwep/chaing", "vwep/rocket", "vwep/rifle", "vwep/gl", "vwep/pistol"};
        int ai = 0;
        if((!mdl.vwep || d->gunselect!=GUN_FIST) && d->gunselect<=GUN_PISTOL)
        {
            a[ai].name = mdl.vwep ? mdl.vwep : vweps[d->gunselect];
            a[ai].tag = "tag_weapon";
            a[ai].anim = ANIM_VWEP|ANIM_LOOP;
            a[ai].basetime = 0;
            ai++;
        }
        if(d->state==CS_ALIVE)
        {
            if((testquad() || d->quadmillis) && mdl.quad)
            {
                a[ai].name = mdl.quad;
                a[ai].tag = "tag_powerup";
                a[ai].anim = ANIM_POWERUP|ANIM_LOOP;
                a[ai].basetime = 0;
                ai++;
            }
            if(testarmour() || d->armour)
            {
                int type = clamp(d->armourtype, (int)A_BLUE, (int)A_YELLOW);
                if(mdl.armour[type])
                {
                    a[ai].name = mdl.armour[type];
                    a[ai].tag = "tag_shield";
                    a[ai].anim = ANIM_SHIELD|ANIM_LOOP;
                    a[ai].basetime = 0;
                    ai++;
                }
            }
        }
        const char *mdlname = mdl.ffa;
        switch(team)
        {
            case 1: mdlname = mdl.blueteam; break;
            case 2: mdlname = mdl.redteam; break;
        }
        renderclient(d, mdlname, a[0].name ? a : NULL, attack, delay, lastaction, cl.intermission ? 0 : d->lastpain);
#if 0
        if(d->state!=CS_DEAD && d->quadmillis) 
        {
            vec color(1, 1, 1), dir(0, 0, 1);
            rendermodel(color, dir, "quadrings", ANIM_MAPMODEL|ANIM_LOOP, vec(d->o).sub(vec(0, 0, d->eyeheight/2)), 360*cl.lastmillis/1000.0f, 0, MDL_DYNSHADOW | MDL_CULL_VFC | MDL_CULL_DIST);
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

        const playermodelinfo &mdl = getplayermodelinfo();

        fpsent *exclude = NULL;
        if(cl.player1->state==CS_SPECTATOR && cl.following>=0 && !isthirdperson())
            exclude = cl.getclient(cl.following);

        fpsent *d;
        loopv(cl.players) if((d = cl.players[i]) && d->state!=CS_SPECTATOR && d->state!=CS_SPAWNING && d!=exclude)
        {
            int team = 0;
            if(teamskins() || m_teammode) team = isteam(cl.player1->team, d->team) ? 1 : 2;
            if(d->state!=CS_DEAD || d->superdamage<50) renderplayer(d, mdl, team);
            s_strcpy(d->info, cl.colorname(d, NULL, "@"));
            if(d->maxhealth>100) { s_sprintfd(sn)(" +%d", d->maxhealth-100); s_strcat(d->info, sn); }
            if(d->state!=CS_DEAD) particle_text(d->abovehead(), d->info, PART_TEXT, 1, team ? (team==1 ? 0x6496FF : 0xFF4B19) : 0x1EC850, 2.0f);
        }
        if(isthirdperson() && !cl.followingplayer()) renderplayer(cl.player1, mdl, teamskins() || m_teammode ? 1 : 0);
        cl.et.renderentities();
        cl.ws.renderprojectiles();
        if(m_capture) cl.cpc.renderbases();
        else if(m_ctf) cl.ctf.renderflags();

        endmodelbatches();
    }
};

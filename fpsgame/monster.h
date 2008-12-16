// monster.h: implements AI for single player monsters, currently client only

struct monsterset
{
    fpsclient &cl;
    vector<extentity *> &ents;
    vector<int> teleports;

    static const int TOTMFREQ = 14;
    static const int NUMMONSTERTYPES = 9;

    IVAR(killsendsp, 0, 1, 1);

    struct monstertype      // see docs for how these values modify behaviour
    {
        short gun, speed, health, freq, lag, rate, pain, loyalty, bscale, weight; 
        short painsound, diesound;
        const char *name, *mdlname, *vwepname;
    };
    
    struct monster : fpsent
    {
        fpsclient &cl;

        monstertype *monstertypes;
        monsterset *ms;
        
        int monsterstate;                   // one of M_*, M_NONE means human
    
        int mtype, tag;                     // see monstertypes table
        fpsent *enemy;                      // monster wants to kill this entity
        float targetyaw;                    // monster wants to look in this direction
        int trigger;                        // millis at which transition to another monsterstate takes place
        vec attacktarget;                   // delayed attacks
        int anger;                          // how many times already hit by fellow monster
    
        monster(int _type, int _yaw, int _tag, int _state, int _trigger, int _move, monsterset *_ms) : cl(_ms->cl), monstertypes(_ms->monstertypes), ms(_ms), monsterstate(_state), tag(_tag)
        {
            type = ENT_AI;
            respawn();
            if(_type>=NUMMONSTERTYPES || _type < 0)
            {
                conoutf(CON_WARN, "warning: unknown monster in spawn: %d", _type);
                _type = 0;
            }
            monstertype *t = monstertypes+(mtype = _type);
            eyeheight = 8.0f;
            aboveeye = 7.0f;
            radius *= t->bscale/10.0f;
            xradius = yradius = radius;
            eyeheight *= t->bscale/10.0f;
            aboveeye *= t->bscale/10.0f;
            weight = t->weight;
            if(_state!=M_SLEEP) cl.spawnplayer(this);
            trigger = lastmillis+_trigger;
            targetyaw = yaw = (float)_yaw;
            move = _move;
            enemy = cl.player1;
            gunselect = t->gun;
            maxspeed = (float)t->speed*4;
            health = t->health;
            armour = 0;
            loopi(NUMGUNS) ammo[i] = 10000;
            pitch = 0;
            roll = 0;
            state = CS_ALIVE;
            anger = 0;
            s_strcpy(name, t->name);
        }
        
        // monster AI is sequenced using transitions: they are in a particular state where
        // they execute a particular behaviour until the trigger time is hit, and then they
        // reevaluate their situation based on the current state, the environment etc., and
        // transition to the next state. Transition timeframes are parametrized by difficulty
        // level (skill), faster transitions means quicker decision making means tougher AI.

        void transition(int _state, int _moving, int n, int r) // n = at skill 0, n/2 = at skill 10, r = added random factor
        {
            monsterstate = _state;
            move = _moving;
            n = n*130/100;
            trigger = lastmillis+n-ms->skill()*(n/16)+rnd(r+1);
        }

        void monsteraction(int curtime)           // main AI thinking routine, called every frame for every monster
        {
            if(enemy->state==CS_DEAD) { enemy = cl.player1; anger = 0; }
            normalize_yaw(targetyaw);
            if(targetyaw>yaw)             // slowly turn monster towards his target
            {
                yaw += curtime*0.5f;
                if(targetyaw<yaw) yaw = targetyaw;
            }
            else
            {
                yaw -= curtime*0.5f;
                if(targetyaw>yaw) yaw = targetyaw;
            }
            float dist = enemy->o.dist(o);
            if(monsterstate!=M_SLEEP) pitch = asin((enemy->o.z - o.z) / dist) / RAD; 

            if(blocked)                                                              // special case: if we run into scenery
            {
                blocked = false;
                if(!rnd(20000/monstertypes[mtype].speed))                            // try to jump over obstackle (rare)
                {
                    jumpnext = true;
                }
                else if(trigger<lastmillis && (monsterstate!=M_HOME || !rnd(5)))  // search for a way around (common)
                {
                    targetyaw += 90+rnd(180);                                        // patented "random walk" AI pathfinding (tm) ;)
                    transition(M_SEARCH, 1, 100, 1000);
                }
            }
            
            float enemyyaw = -(float)atan2(enemy->o.x - o.x, enemy->o.y - o.y)/RAD+180;
            
            switch(monsterstate)
            {
                case M_PAIN:
                case M_ATTACKING:
                case M_SEARCH:
                    if(trigger<lastmillis) transition(M_HOME, 1, 100, 200);
                    break;
                    
                case M_SLEEP:                       // state classic sp monster start in, wait for visual contact
                {
                    if(editmode) break;          
                    normalize_yaw(enemyyaw);
                    float angle = (float)fabs(enemyyaw-yaw);
                    if(dist<32                   // the better the angle to the player, the further the monster can see/hear
                    ||(dist<64 && angle<135)
                    ||(dist<128 && angle<90)
                    ||(dist<256 && angle<45)
                    || angle<10
                    || (ms->monsterhurt && o.dist(ms->monsterhurtpos)<128))
                    {
                        vec target;
                        if(raycubelos(o, enemy->o, target))
                        {
                            transition(M_HOME, 1, 500, 200);
                            playsound(S_GRUNT1+rnd(2), &o);
                        }
                    }
                    break;
                }
                
                case M_AIMING:                      // this state is the delay between wanting to shoot and actually firing
                    if(trigger<lastmillis)
                    {
                        lastaction = 0;
                        attacking = true;
                        cl.ws.shoot(this, attacktarget);
                        transition(M_ATTACKING, 0, 600, 0);
                    }
                    break;

                case M_HOME:                        // monster has visual contact, heads straight for player and may want to shoot at any time
                    targetyaw = enemyyaw;
                    if(trigger<lastmillis)
                    {
                        vec target;
                        if(!raycubelos(o, enemy->o, target))    // no visual contact anymore, let monster get as close as possible then search for player
                        {
                            transition(M_HOME, 1, 800, 500);
                        }
                        else 
                        {
                            bool melee = false, longrange = false;
                            switch(monstertypes[mtype].gun)
                            {
                                case GUN_BITE:
                                case GUN_FIST: melee = true; break;
                                case GUN_RIFLE: longrange = true; break;
                            }
                            // the closer the monster is the more likely he wants to shoot, 
                            if((!melee || dist<20) && !rnd(longrange ? (int)dist/12+1 : min((int)dist/12+1,6)) && enemy->state==CS_ALIVE)      // get ready to fire
                            { 
                                attacktarget = target;
                                transition(M_AIMING, 0, monstertypes[mtype].lag, 10);
                            }
                            else                                                        // track player some more
                            {
                                transition(M_HOME, 1, monstertypes[mtype].rate, 0);
                            }
                        }
                    }
                    break;
                    
            }

            if(move || moving || (onplayer && (onplayer->state!=CS_ALIVE || lastmoveattempt <= onplayer->lastmove)))
            {
                vec pos(o);
                pos.sub(eyeheight);
                loopv(ms->teleports) // equivalent of player entity touch, but only teleports are used
                {
                    entity &e = *ms->ents[ms->teleports[i]];
                    float dist = e.o.dist(pos);
                    if(dist<16) cl.et.teleport(ms->teleports[i], this);
                }

                moveplayer(this, 1, true);        // use physics to move monster
            }
        }

        void monsterpain(int damage, fpsent *d)
        {
            if(d->type==ENT_AI)     // a monster hit us
            {
                if(this!=d)            // guard for RL guys shooting themselves :)
                {
                    anger++;     // don't attack straight away, first get angry
                    int _anger = d->type==ENT_AI && mtype==((monster *)d)->mtype ? anger/2 : anger;
                    if(_anger>=monstertypes[mtype].loyalty) enemy = d;     // monster infight if very angry
                }
            }
            else if(d->type==ENT_PLAYER) // player hit us
            {
                anger = 0;
                enemy = d;
                ms->monsterhurt = true;
                ms->monsterhurtpos = o;
            }
            cl.ws.damageeffect(damage, this);
            if((health -= damage)<=0)
            {
                state = CS_DEAD;
                lastpain = lastmillis;
                playsound(monstertypes[mtype].diesound, &o);
                ms->monsterkilled();
                superdamage = -health;
                cl.ws.superdamageeffect(vel, this);

                s_sprintfd(id)("monster_dead_%d", tag);
                if(identexists(id)) execute(id);
            }
            else
            {
                transition(M_PAIN, 0, monstertypes[mtype].pain, 200);      // in this state monster won't attack
                playsound(monstertypes[mtype].painsound, &o);
            }
        }
    };

    monstertype *monstertypes;
        
    monsterset(fpsclient &_cl) : cl(_cl), ents(_cl.et.ents)
    {
        static monstertype _monstertypes[NUMMONSTERTYPES] =
        {   
            { GUN_FIREBALL,  15, 100, 3, 0,   100, 800, 1, 10,  90, S_PAINO, S_DIE1,   "an ogro",     "ogro",       "ogro/vwep"},
            { GUN_CG,        18,  70, 2, 70,   10, 400, 2, 10,  50, S_PAINR, S_DEATHR, "a rhino",     "monster/rhino",      NULL},
            { GUN_SG,        13, 120, 1, 100, 300, 400, 4, 14, 115, S_PAINE, S_DEATHE, "ratamahatta", "monster/rat",        "monster/rat/vwep"},
            { GUN_RIFLE,     14, 200, 1, 80,  400, 300, 4, 18, 145, S_PAINS, S_DEATHS, "a slith",     "monster/slith",      "monster/slith/vwep"},
            { GUN_RL,        12, 500, 1, 0,   200, 200, 6, 24, 210, S_PAINB, S_DEATHB, "bauul",       "monster/bauul",      "monster/bauul/vwep"},
            { GUN_BITE,      24,  50, 3, 0,   100, 400, 1, 15,  75, S_PAINP, S_PIGGR2, "a hellpig",   "monster/hellpig",    NULL},
            { GUN_ICEBALL,   11, 250, 1, 0,    10, 400, 6, 18, 160, S_PAINH, S_DEATHH, "a knight",    "monster/knight",     "monster/knight/vwep"},
            { GUN_SLIMEBALL, 15, 100, 1, 0,   200, 400, 2, 10,  60, S_PAIND, S_DEATHD, "a goblin",    "monster/goblin",     "monster/goblin/vwep"},
            { GUN_GL,        22,  50, 1, 0,   200, 400, 1, 10,  40, S_PAIND, S_DEATHD, "a spider",    "monster/spider",      NULL }, 
        };
        monstertypes = _monstertypes;

        CCOMMAND(endsp, "", (monsterset *self), self->endsp(false));
        CCOMMAND(nummonsters, "ii", (monsterset *self, int *tag, int *state), intret(self->nummonsters(*tag, *state))); 
    }
  
    int nummonsters(int tag, int state)
    {
        int n = 0;
        loopv(monsters) if(monsters[i]->tag==tag && (monsters[i]->state==CS_ALIVE ? state!=1 : state>=1)) n++;
        return n;
    }

    void preloadmonsters()
    {
        loopi(NUMMONSTERTYPES) loadmodel(monstertypes[i].mdlname, -1, true);
    }

    vector<monster *> monsters;
    
    int nextmonster, spawnremain, numkilled, monstertotal, mtimestart, remain;
    
    bool monsterhurt;
    vec monsterhurtpos;

    IVAR(skill, 1, 3, 10);

    void spawnmonster()     // spawn a random monster according to freq distribution in DMSP
    {
        int n = rnd(TOTMFREQ), type;
        for(int i = 0; ; i++) if((n -= monstertypes[i].freq)<0) { type = i; break; }
        monsters.add(new monster(type, rnd(360), 0, M_SEARCH, 1000, 1, this));
    }

    void monsterclear()     // called after map start or when toggling edit mode to reset/spawn all monsters to initial state
    {
        removetrackedparticles();
        loopv(monsters) delete monsters[i]; 
        cleardynentcache();
        monsters.setsize(0);
        numkilled = 0;
        monstertotal = 0;
        spawnremain = 0;
        remain = 0;
        monsterhurt = false;
        if(m_dmsp)
        {
            nextmonster = mtimestart = lastmillis+10000;
            monstertotal = spawnremain = skill()*10;
        }
        else if(m_classicsp)
        {
            mtimestart = lastmillis;
            loopv(ents) if(ents[i]->type==MONSTER)
            {
                monster *m = new monster(ents[i]->attr2, ents[i]->attr1, ents[i]->attr3, M_SLEEP, 100, 0, this);  
                monsters.add(m);
                m->o = ents[i]->o;
                entinmap(m);
                updatedynentcache(m);
                monstertotal++;
            }
        }
        teleports.setsizenodelete(0);
        if(m_dmsp || m_classicsp)
        {
            loopv(ents) if(ents[i]->type==TELEPORT) teleports.add(i);
        }
    }

    void endsp(bool allkilled)
    {
        conoutf(CON_GAMEINFO, allkilled ? "\f2you have cleared the map!" : "\f2you reached the exit!");
        monstertotal = 0;
        cl.cc.addmsg(SV_FORCEINTERMISSION, "r");
    }
    
    void monsterkilled()
    {
        numkilled++;
        cl.player1->frags = numkilled;
        remain = monstertotal-numkilled;
        if(remain>0 && remain<=5) conoutf(CON_GAMEINFO, "\f2only %d monster(s) remaining", remain);
    }

    void monsterthink(int curtime)
    {
        if(m_dmsp && spawnremain && lastmillis>nextmonster)
        {
            if(spawnremain--==monstertotal) { conoutf(CON_GAMEINFO, "\f2The invasion has begun!"); playsound(S_V_FIGHT); }
            nextmonster = lastmillis+1000;
            spawnmonster();
        }
        
        if(killsendsp() && monstertotal && !spawnremain && numkilled==monstertotal) endsp(true);
        
        bool monsterwashurt = monsterhurt;
        
        loopv(monsters)
        {
            if(monsters[i]->state==CS_ALIVE) monsters[i]->monsteraction(curtime);
            else if(monsters[i]->state==CS_DEAD)
            {
                if(lastmillis-monsters[i]->lastpain<2000)
                {
                    //monsters[i]->move = 0;
                    monsters[i]->move = monsters[i]->strafe = 0;
                    moveplayer(monsters[i], 1, true);
                }
            }
        }
        
        if(monsterwashurt) monsterhurt = false;
    }

    void monsterrender()
    {
        loopv(monsters)
        {
            monster &m = *monsters[i];
            if(m.state!=CS_DEAD || m.superdamage<50) 
            {
                modelattach vwep[] = { { monstertypes[m.mtype].vwepname, "tag_weapon", ANIM_VWEP|ANIM_LOOP, 0 }, { NULL } };
                renderclient(&m, monstertypes[m.mtype].mdlname, vwep, m.monsterstate==M_ATTACKING ? -ANIM_SHOOT : 0, 300, m.lastaction, m.lastpain);
            }
        }
    }
};

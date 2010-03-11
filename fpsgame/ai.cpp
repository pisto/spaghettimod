#include "game.h"

namespace ai
{
    using namespace game;

    avoidset obstacles;
    int updatemillis = 0, forcegun = -1;
    vec aitarget(0, 0, 0);

    VAR(aidebug, 0, 0, 6);
    VAR(aiforcegun, -1, -1, NUMGUNS-1);

    ICOMMAND(addbot, "s", (char *s), addmsg(SV_ADDBOT, "ri", *s ? clamp(atoi(s), 1, 101) : -1));
    ICOMMAND(delbot, "", (), addmsg(SV_DELBOT, "r"));
    ICOMMAND(botlimit, "i", (int *n), addmsg(SV_BOTLIMIT, "ri", *n));
    ICOMMAND(botbalance, "i", (int *n), addmsg(SV_BOTBALANCE, "ri", *n));

    float viewdist(int x)
    {
        int fog = getvar("fog");
        return x <= 100 ? clamp((SIGHTMIN+(SIGHTMAX-SIGHTMIN))/100.f*float(x), float(SIGHTMIN), float(fog)) : float(fog);
    }

    float viewfieldx(int x)
    {
        return x <= 100 ? clamp((VIEWMIN+(VIEWMAX-VIEWMIN))/100.f*float(x), float(VIEWMIN), float(VIEWMAX)) : float(VIEWMAX);
    }

    float viewfieldy(int x)
    {
        return viewfieldx(x)*3.f/4.f;
    }

    bool canmove(fpsent *d)
    {
        return d->state != CS_DEAD && !intermission;
    }

    bool targetable(fpsent *d, fpsent *e, bool anyone)
    {
        if(d == e || !canmove(d)) return false;
        aistate &b = d->ai->getstate();
        if(b.type != AI_S_WAIT)
            return e->state == CS_ALIVE && (!anyone || !isteam(d->team, e->team));
        return false;
    }

    bool getsight(vec &o, float yaw, float pitch, vec &q, vec &v, float mdist, float fovx, float fovy)
    {
        float dist = o.dist(q);

        if(dist <= mdist)
        {
            float x = fabs((asin((q.z-o.z)/dist)/RAD)-pitch);
            float y = fabs((-(float)atan2(q.x-o.x, q.y-o.y)/PI*180+180)-yaw);
            if(x <= fovx && y <= fovy) return raycubelos(o, q, v);
        }
        return false;
    }

    bool cansee(fpsent *d, vec &x, vec &y, vec &targ)
    {
        aistate &b = d->ai->getstate();
        if(canmove(d) && b.type != AI_S_WAIT)
            return getsight(x, d->yaw, d->pitch, y, targ, d->ai->views[2], d->ai->views[0], d->ai->views[1]);
        return false;
    }

    vec getaimpos(fpsent *d, fpsent *e)
    {
        vec o = e->o;
        if(d->skill <= 100)
        {
			float optimal = 1.f, scale = 1.f/float(d->skill);
			if(d->gunselect == GUN_RL) optimal = (e->aboveeye*0.2f)-(0.8f*d->eyeheight);
			else if(d->gunselect != GUN_GL) optimal = (e->aboveeye-e->eyeheight)*0.5f;
			o.z += rnd(d->skill) ? optimal*scale : -optimal*scale;
        }
        return o;
    }

    void create(fpsent *d)
    {
        if(!d->ai) d->ai = new aiinfo;
        if(d->ai)
        {
            d->ai->views[0] = viewfieldx(d->skill);
            d->ai->views[1] = viewfieldy(d->skill);
            d->ai->views[2] = viewdist(d->skill);
        }
    }

    void destroy(fpsent *d)
    {
        if(d->ai) DELETEP(d->ai);
    }

    void init(fpsent *d, int at, int ocn, int sk, int bn, int pm, const char *name, const char *team)
    {
        loadwaypoints();

        fpsent *o = newclient(ocn);

        d->aitype = at;

        bool resetthisguy = false;
        if(!d->name[0])
        {
            if(aidebug) conoutf("%s assigned to %s at skill %d", colorname(d, name), o ? colorname(o) : "?", sk);
            else conoutf("connected: %s", colorname(d, name));
            resetthisguy = true;
        }
        else
        {
            if(d->ownernum != ocn)
            {
                if(aidebug) conoutf("%s reassigned to %s", colorname(d, name), o ? colorname(o) : "?");
                resetthisguy = true;
            }
            if(d->skill != sk && aidebug) conoutf("%s changed skill to %d", colorname(d, name), sk);
        }

        copystring(d->name, name, MAXNAMELEN+1);
        copystring(d->team, team, MAXTEAMLEN+1);
        d->ownernum = ocn;
        d->skill = sk;
        d->playermodel = chooserandomplayermodel(pm);

        if(resetthisguy) removeweapons(d);
        if(d->ownernum >= 0 && player1->clientnum == d->ownernum) create(d);
        else if(d->ai) destroy(d);
    }

    void update()
    {
        bool updating = lastmillis-updatemillis > 100; // fixed rate logic at 10fps
		loopv(players) if(players[i]->ai)
		{
            if(updating && updatemillis < lastmillis)
            {
                avoid();
                forcegun = multiplayer(false) ? -1 : aiforcegun;
                updatemillis = lastmillis;
            }
			if(!intermission) think(players[i], updating);
			else players[i]->stopmoving();
		}
    }

    bool checkothers(vector<int> &targets, fpsent *d, int state, int targtype, int target, bool teams)
    { // checks the states of other ai for a match
        targets.setsize(0);
        loopv(players)
        {
            fpsent *e = players[i];
            if(e == d || !e->ai || e->state != CS_ALIVE) continue;
            if(targets.find(e->clientnum) >= 0) continue;
            if(teams && d && !isteam(d->team, e->team)) continue;
            aistate &b = e->ai->getstate();
            if(state >= 0 && b.type != state) continue;
            if(target >= 0 && b.target != target) continue;
            if(targtype >=0 && b.targtype != targtype) continue;
            targets.add(e->clientnum);
        }
        return !targets.empty();
    }

    bool makeroute(fpsent *d, aistate &b, int node, bool changed, int retries)
    {
		if(changed && d->ai->route.length() > 1 && d->ai->route[0] == node) return true;
		if(route(d, d->lastnode, node, d->ai->route, obstacles, retries <= 1))
		{
			b.override = false;
			return true;
		}
		d->ai->clear(true);
		if(retries <= 1) return makeroute(d, b, node, true, retries+1);
		return false;
    }

    bool makeroute(fpsent *d, aistate &b, const vec &pos, bool changed, int retries)
    {
        int node = closestwaypoint(pos, NEARDIST, true);
        return makeroute(d, b, node, changed, retries);
    }

    bool randomnode(fpsent *d, aistate &b, const vec &pos, float guard, float wander)
    {
        static vector<int> candidates;
        candidates.setsize(0);
        findwaypointswithin(pos, guard, wander, candidates);

        while(!candidates.empty())
        {
            int w = rnd(candidates.length()), n = candidates.removeunordered(w);
            if(n != d->lastnode && !d->ai->hasprevnode(n) && !obstacles.find(n, d) && makeroute(d, b, n)) return true;
        }
        return false;
    }

    bool randomnode(fpsent *d, aistate &b, float guard, float wander)
    {
        return randomnode(d, b, d->feetpos(), guard, wander);
    }

    bool badhealth(fpsent *d)
    {
        if(d->skill <= 100) return d->health <= (111-d->skill)/4;
        return false;
    }

    bool enemy(fpsent *d, aistate &b, const vec &pos, float guard = NEARDIST, bool pursue = false)
    {
        if(canmove(d))
        {
            fpsent *t = NULL;
            vec dp = d->headpos(), tp(0, 0, 0);
            bool insight = false, tooclose = false;
            float mindist = guard*guard;
            loopv(players)
            {
                fpsent *e = players[i];
                if(e == d || !targetable(d, e, true)) continue;
                vec ep = getaimpos(d, e);
                bool close = ep.squaredist(pos) < mindist;
                if(!t || ep.squaredist(dp) < tp.squaredist(dp) || close)
                {
                    bool see = cansee(d, dp, ep);
                    if(!insight || see || close)
                    {
                        t = e; tp = ep;
                        if(!insight && see) insight = see;
                        if(!tooclose && close) tooclose = close;
                    }
                }
            }
            if(t && violence(d, b, t, pursue && (tooclose || insight))) return insight || tooclose;
        }
        return false;
    }

    void noenemy(fpsent *d)
    {
        d->ai->enemy = -1;
        d->ai->enemymillis = 0;
    }

    bool patrol(fpsent *d, aistate &b, const vec &pos, float guard, float wander, int walk, bool retry)
    {
        vec feet = d->feetpos();
        if(walk == 2 || b.override || (walk && feet.squaredist(pos) <= guard*guard) || !makeroute(d, b, pos))
        { // run away and back to keep ourselves busy
            if(!b.override && randomnode(d, b, pos, guard, wander))
            {
                b.override = true;
                return true;
            }
            else if(d->ai->route.length() > 1) return true;
            else if(!retry)
            {
                b.override = false;
                return patrol(d, b, pos, guard, wander, walk, true);
            }
            b.override = false;
            return false;
        }
        b.override = false;
        return true;
    }

    bool defend(fpsent *d, aistate &b, const vec &pos, float guard, float wander, int walk)
    {
		bool hasenemy = enemy(d, b, pos, wander, false);
		if(!walk && d->feetpos().squaredist(pos) <= guard*guard)
		{
			b.idle = hasenemy ? 2 : 1;
			return true;
		}
        return patrol(d, b, pos, guard, wander, walk);
    }

    bool violence(fpsent *d, aistate &b, fpsent *e, bool pursue)
    {
        if(targetable(d, e, true))
        {
            if(pursue || d->gunselect == GUN_FIST || (b.type == AI_S_INTEREST && b.targtype == AI_T_NODE))
                d->ai->switchstate(b, AI_S_PURSUE, AI_T_PLAYER, e->clientnum);
            if(d->ai->enemy != e->clientnum) d->ai->enemymillis = lastmillis;
            d->ai->enemy = e->clientnum;
            d->ai->enemyseen = lastmillis;
            vec dp = d->headpos(), ep = getaimpos(d, e);
            if(!cansee(d, dp, ep)) d->ai->enemyseen -= ((111-d->skill)*10)+10; // so we don't "quick"
            return true;
        }
        return false;
    }

    bool target(fpsent *d, aistate &b, bool pursue = false, bool force = false)
    {
        fpsent *t = NULL;
        vec dp = d->headpos(), tp(0, 0, 0);
        loopv(players)
        {
            fpsent *e = players[i];
            if(e == d || !targetable(d, e, true)) continue;
            vec ep = getaimpos(d, e);
            if((!t || ep.squaredist(dp) < tp.squaredist(dp)) && (force || cansee(d, dp, ep)))
            {
                t = e;
                tp = ep;
            }
        }
        if(t) return violence(d, b, t, pursue);
        return false;
    }

    int isgoodammo(int gun) { return gun >= GUN_SG && gun <= GUN_GL; }

    bool hasgoodammo(fpsent *d)
    {
        static const int goodguns[] = { GUN_CG, GUN_RL, GUN_SG, GUN_RIFLE };
        loopi(sizeof(goodguns)/sizeof(goodguns[0])) if(d->hasammo(goodguns[0])) return true;
        if(d->ammo[GUN_GL] > 5) return true;
        return false;
    }

    void assist(fpsent *d, aistate &b, vector<interest> &interests, bool all = false, bool force = false)
    {
        loopv(players)
        {
            fpsent *e = players[i];
            if(e == d || (!all && e->aitype != AI_NONE) || !isteam(d->team, e->team)) continue;
            interest &n = interests.add();
            n.state = AI_S_DEFEND;
            n.node = e->lastnode;
            n.target = e->clientnum;
            n.targtype = AI_T_PLAYER;
            n.score = e->o.squaredist(d->o)/(force || hasgoodammo(d) ? 10.f : 1.f);
        }
    }

    static void tryitem(fpsent *d, extentity &e, int id, aistate &b, vector<interest> &interests, bool force = false)
    {
        float score = 1.0f;
        if(force) score = 100.0f;
        else switch(e.type)
        {
            case I_HEALTH:
                if(d->health < min(d->skill, 75)) score = 100.0f;
                break;
            case I_QUAD: score = 70.0f; break;
            case I_BOOST: score = 50.0f; break;
            case I_GREENARMOUR: case I_YELLOWARMOUR:
            {
                int atype = A_GREEN + e.type - I_GREENARMOUR;
                if(atype > d->armourtype) score = atype == A_YELLOW ? 50.0f : 25.0f;
                else if(d->armour < 50) score = 20.0f;
                break;
            }
            default:
                if(e.type >= I_SHELLS && e.type <= I_CARTRIDGES)
                {
                    int gun = e.type - I_SHELLS + GUN_SG;
                    // go get a weapon upgrade
                    if(gun == d->ai->weappref) score = 100.0f;
                    else if(isgoodammo(gun)) score = hasgoodammo(d) ? 15.0f : 75.0f;
                }
                break;
        }
        interest &n = interests.add();
        n.state = AI_S_INTEREST;
        n.node = closestwaypoint(e.o, NEARDIST, true);
        n.target = id;
        n.targtype = AI_T_ENTITY;
        n.score = d->feetpos().squaredist(e.o)/score;
    }

    void items(fpsent *d, aistate &b, vector<interest> &interests, bool force = false)
    {
        loopv(entities::ents)
        {
            extentity &e = *(extentity *)entities::ents[i];
            if(!e.spawned || !d->canpickup(e.type)) continue;
            tryitem(d, e, i, b, interests, force);
        }
    }

    static vector<int> targets;

    bool find(fpsent *d, aistate &b, bool override = false)
    {
        static vector<interest> interests;
        interests.setsize(0);
        if(!m_noitems)
        {
            if((!m_noammo && !hasgoodammo(d)) || d->health < min(d->skill - 15, 75))
                items(d, b, interests);
            else
            {
                static vector<int> nearby;
                nearby.setsize(0);
                findents(I_SHELLS, I_QUAD, false, d->feetpos(), vec(32, 32, 24), nearby);
                loopv(nearby)
                {
                    int id = nearby[i];
                    extentity &e = *(extentity *)entities::ents[id];
                    if(d->canpickup(e.type)) tryitem(d, e, id, b, interests);
                }
            }
        }
        if(cmode) cmode->aifind(d, b, interests);
        if(m_teammode) assist(d, b, interests);
        while(!interests.empty())
        {
            int q = interests.length()-1;
            loopi(interests.length()-1) if(interests[i].score < interests[q].score) q = i;
            interest n = interests.removeunordered(q);
            bool proceed = true;
            switch(n.state)
            {
                case AI_S_DEFEND: // don't get into herds
                    proceed = !checkothers(targets, d, n.state, n.targtype, n.target, true);
                    break;
                default: break;
            }
            if(proceed && makeroute(d, b, n.node, false))
            {
                d->ai->setstate(n.state, n.targtype, n.target, override);
                return true;
            }
        }
        return false;
    }

    void damaged(fpsent *d, fpsent *e)
    {
        if(d->ai && canmove(d) && targetable(d, e, true)) // see if this ai is interested in a grudge
        {
            aistate &b = d->ai->getstate();
            if(violence(d, b, e, false)) return;
        }
        if(checkothers(targets, d, AI_S_DEFEND, AI_T_PLAYER, d->clientnum, true))
        {
            loopv(targets)
            {
                fpsent *t = getclient(targets[i]);
                if(!t->ai || !canmove(t) || !targetable(t, e, true)) continue;
                aistate &c = t->ai->getstate();
                if(violence(t, c, e, false)) return;
            }
        }
    }

    void setup(fpsent *d, bool tryreset = false)
    {
        d->ai->reset(tryreset);
        aistate &b = d->ai->getstate();
        b.next = lastmillis+((111-d->skill)*10)+rnd((111-d->skill)*10);
        if(m_insta) d->ai->weappref = GUN_RIFLE;
        else
        {
        	if(forcegun >= 0 && forcegun < NUMGUNS) d->ai->weappref = forcegun;
        	else if(m_noammo) d->ai->weappref = -1;
			else d->ai->weappref = rnd(GUN_GL-GUN_SG+1)+GUN_SG;
        }
    }

    void spawned(fpsent *d)
    {
        if(d->ai)
        {
        	d->ai->cleartimers();
        	setup(d, false);
        }
    }

    void killed(fpsent *d, fpsent *e)
    {
        if(d->ai) d->ai->reset();
    }

    bool check(fpsent *d, aistate &b)
    {
        if(cmode && cmode->aicheck(d, b)) return true;
        return false;
    }

    int dowait(fpsent *d, aistate &b)
    {
        if(check(d, b)) return 1;
        if(find(d, b)) return 1;
        if(target(d, b, true, true)) return 1;
        if(randomnode(d, b, NEARDIST, 1e16f))
        {
            d->ai->addstate(AI_S_INTEREST, AI_T_NODE, d->ai->route[0]);
            return 1;
        }
        return 0; // but don't pop the state
    }

    int dodefend(fpsent *d, aistate &b)
    {
        if(d->state == CS_ALIVE)
        {
            switch(b.targtype)
            {
                case AI_T_NODE:
                    if(check(d, b)) return 1;
                    if(waypoints.inrange(b.target)) return defend(d, b, waypoints[b.target].o) ? 1 : 0;
                    break;
                case AI_T_ENTITY:
                    if(check(d, b)) return 1;
                    if(entities::ents.inrange(b.target)) return defend(d, b, entities::ents[b.target]->o) ? 1 : 0;
                    break;
                case AI_T_AFFINITY:
                    if(cmode) return cmode->aidefend(d, b) ? 1 : 0;
                    break;
                case AI_T_PLAYER:
                {
                    if(check(d, b)) return 1;
                    fpsent *e = getclient(b.target);
                    if(e && e->state == CS_ALIVE) return defend(d, b, e->feetpos()) ? 1 : 0;
                    break;
                }
                default: break;
            }
        }
        return 0;
    }

    int dointerest(fpsent *d, aistate &b)
    {
        if(d->state != CS_ALIVE) return 0;
        switch(b.targtype)
        {
            case AI_T_NODE:
				if(d->lastnode != b.target && waypoints.inrange(b.target))
					return makeroute(d, b, b.target) ? 1 : 0;
				break;
            case AI_T_ENTITY:
                //if(d->hasammo(d->ai->weappref)) return 0;
                if(entities::ents.inrange(b.target))
                {
                    extentity &e = *(extentity *)entities::ents[b.target];
                    if(e.type < I_SHELLS || e.type > I_CARTRIDGES) return 0;
                    if(!e.spawned) return 0; // || d->hasmaxammo(e.type)) return 0;
                    return makeroute(d, b, e.o) ? 1 : 0;
                }
                break;
        }
        return 0;
    }

    int dopursue(fpsent *d, aistate &b)
    {
        if(d->state == CS_ALIVE)
        {
            switch(b.targtype)
            {
                case AI_T_AFFINITY:
                {
                    if(cmode) return cmode->aipursue(d, b) ? 1 : 0;
                    break;
                }

                case AI_T_PLAYER:
                {
                    if(check(d, b)) return 1;
                    fpsent *e = getclient(b.target);
                    if(e && e->state == CS_ALIVE)
                    {
                    	float guard = NEARDIST, wander = guns[d->gunselect].range;
                    	if(d->gunselect == GUN_FIST) guard = 0.f;
                    	return patrol(d, b, e->feetpos(), guard, wander) ? 1 : 0;
                    }
                    break;
                }
                default: break;
            }
        }
        return 0;
    }

    int closenode(fpsent *d, bool force = false)
    {
        vec pos = d->feetpos();
        int node = -1;
        float mindist = NEARDISTSQ;
        loopv(d->ai->route) if(waypoints.inrange(d->ai->route[i]))
        {
            waypoint &w = waypoints[d->ai->route[i]];
            vec wpos = w.o;
            int id = obstacles.remap(d, d->ai->route[i], wpos);
            if(waypoints.inrange(id) && (force || id == d->ai->route[i] || !d->ai->hasprevnode(id)))
            {
                float dist = wpos.squaredist(pos);
                if(dist < mindist)
                {
                    node = id;
                    mindist = dist;
                }
            }
        }
        return node;
    }

    bool wpspot(fpsent *d, int n, bool force = false)
    {
        if(waypoints.inrange(n))
        {
            waypoint &w = waypoints[n];
            vec wpos = w.o;
            int id = obstacles.remap(d, n, wpos);
            if(waypoints.inrange(id) && (force || id == n || !d->ai->hasprevnode(id)))
            {
				d->ai->spot = wpos;
				d->ai->targnode = n;
				return true;
            }
        }
        return false;
    }

    bool anynode(fpsent *d, aistate &b, bool retry = false)
    {
        if(!waypoints.inrange(d->lastnode)) return false;
        waypoint &w = waypoints[d->lastnode];
		static vector<int> anyremap; anyremap.setsize(0);
		if(w.links[0])
		{
			loopi(MAXWAYPOINTLINKS)
			{
				int link = w.links[i];
				if(!link) break;
				if(waypoints.inrange(link) && (retry || !d->ai->hasprevnode(link)))
					anyremap.add(link);
			}
		}
		while(!anyremap.empty())
		{
			int r = rnd(anyremap.length()), t = anyremap[r];
			if(wpspot(d, t, retry))
			{
				d->ai->route.add(t);
				d->ai->route.add(d->lastnode);
				return true;
			}
			anyremap.remove(r);
		}
		if(!retry) return anynode(d, b, true);
        return false;
    }

	bool hunt(fpsent *d, aistate &b, int retries = 0)
	{
		if(!d->ai->route.empty())
		{
			int n = retries%2 ? d->ai->route.find(d->lastnode) : closenode(d, retries >= 2);
			if(retries%2 && d->ai->route.inrange(n))
			{
				while(d->ai->route.length() > n+1) d->ai->route.pop(); // waka-waka-waka-waka
				if(!n)
				{
					if(wpspot(d, d->ai->route[n], retries >= 2))
					{
						b.next = lastmillis;
						d->ai->clear(false);
						return true;
					}
					else if(retries <= 2) return hunt(d, b, retries+1); // try again
				}
				else n--; // otherwise, we want the next in line
			}
			if(d->ai->route.inrange(n) && wpspot(d, d->ai->route[n], retries >= 2)) return true;
			else if(retries <= 2) return hunt(d, b, retries+1); // try again
		}
		b.override = false;
		d->ai->clear(false);
		return anynode(d, b);
	}

	fpsent *getenemy(fpsent *d, vec &dp)
	{
		fpsent *e = (fpsent *)intersectclosest(dp, d->ai->target, d);
		if(!e) e = getclient(d->ai->enemy);
		return e && targetable(d, e, true) ? e : NULL;
	}

	bool hastarget(fpsent *d, aistate &b, fpsent *e, float yaw, float pitch, float dist)
	{ // add margins of error
		if(d->skill <= 100 && !rnd(d->skill*10)) return true; // random margin of error
        float mindist = d->radius*d->radius, range = guns[d->gunselect].range + d->radius;
        if(guns[d->gunselect].projspeed && (b.type != AI_S_DEFEND || b.targtype != AI_T_AFFINITY)) mindist = RL_DAMRAD*RL_DAMRAD; // do if we're stuck guarding
        if(d->skill <= 100) mindist -= mindist*(1.f/float(d->skill));
        if((d->gunselect == GUN_FIST || mindist <= dist) && dist <= range*range)
		{
			float skew = clamp(float(lastmillis-d->ai->enemymillis)/float((d->skill*guns[d->gunselect].attackdelay/200.f)), 0.f, guns[d->gunselect].projspeed ? 0.25f : 1e16f);
			if(fabs(yaw-d->yaw) <= d->ai->views[0]*skew && fabs(pitch-d->pitch) <= d->ai->views[1]*skew) return true;
		}
		return false;
	}

    void jumpto(fpsent *d, aistate &b, const vec &pos)
    {
		vec off = vec(pos).sub(d->feetpos()), dir(off.x, off.y, 0);
		float magxy = dir.magnitude();
		bool offground = d->timeinair && !d->inwater, jumper = magxy <= JUMPMIN && off.z >= JUMPMIN,
			jump = !offground && (jumper || lastmillis >= d->ai->jumprand) && lastmillis >= d->ai->jumpseed;
		if(jump)
		{
			vec old = d->o;
			d->o = vec(pos).add(vec(0, 0, d->eyeheight));
			if(!collide(d, vec(0, 0, 1))) jump = false;
			d->o = old;
			if(jump)
			{
				float radius = 18*18;
				loopv(entities::ents) if(entities::ents[i]->type == JUMPPAD)
				{
					fpsentity &e = *(fpsentity *)entities::ents[i];
					if(e.o.squaredist(pos) <= radius) { jump = false; break; }
				}
			}
		}
		if(jump)
		{
			d->jumping = true;
			if(jumper && magxy < JUMPMIN*2 && !d->inwater) d->ai->dontmove = true; // going up
			int seed = (111-d->skill)*(d->inwater ? 2 : 10);
			d->ai->jumpseed = lastmillis+seed+rnd(seed);
			seed *= b.idle ? 50 : 25;
			d->ai->jumprand = lastmillis+seed+rnd(seed);
		}
    }

    void fixfullrange(float &yaw, float &pitch, float &roll, bool full)
    {
        if(full)
        {
            while(pitch < -180.0f) pitch += 360.0f;
            while(pitch >= 180.0f) pitch -= 360.0f;
            while(roll < -180.0f) roll += 360.0f;
            while(roll >= 180.0f) roll -= 360.0f;
        }
        else
        {
            if(pitch > 89.9f) pitch = 89.9f;
            if(pitch < -89.9f) pitch = -89.9f;
            if(roll > 89.9f) roll = 89.9f;
            if(roll < -89.9f) roll = -89.9f;
        }
        while(yaw < 0.0f) yaw += 360.0f;
        while(yaw >= 360.0f) yaw -= 360.0f;
    }

    void fixrange(float &yaw, float &pitch)
    {
        float r = 0.f;
        fixfullrange(yaw, pitch, r, false);
    }

    void getyawpitch(const vec &from, const vec &pos, float &yaw, float &pitch)
    {
        float dist = from.dist(pos);
        yaw = -(float)atan2(pos.x-from.x, pos.y-from.y)/PI*180+180;
        pitch = asin((pos.z-from.z)/dist)/RAD;
    }

    void scaleyawpitch(float &yaw, float &pitch, float targyaw, float targpitch, float frame, float scale)
    {
        if(yaw < targyaw-180.0f) yaw += 360.0f;
        if(yaw > targyaw+180.0f) yaw -= 360.0f;
        float offyaw = fabs(targyaw-yaw)*frame, offpitch = fabs(targpitch-pitch)*frame*scale;
        if(targyaw > yaw)
        {
            yaw += offyaw;
            if(targyaw < yaw) yaw = targyaw;
        }
        else if(targyaw < yaw)
        {
            yaw -= offyaw;
            if(targyaw > yaw) yaw = targyaw;
        }
        if(targpitch > pitch)
        {
            pitch += offpitch;
            if(targpitch < pitch) pitch = targpitch;
        }
        else if(targpitch < pitch)
        {
            pitch -= offpitch;
            if(targpitch > pitch) pitch = targpitch;
        }
        fixrange(yaw, pitch);
    }

    bool canshoot(fpsent *d)
    {
        return !d->ai->becareful && d->ammo[d->gunselect] > 0 && lastmillis - d->lastaction >= d->gunwait;
    }

    int process(fpsent *d, aistate &b)
    {
        int result = 0, stupify = d->skill <= 30+rnd(20) ? rnd(d->skill*1111) : 0, skmod = (111-d->skill)*10;
        float frame = float(lastmillis-d->ai->lastrun)/float(skmod/2);
        vec dp = d->headpos();
        if(b.idle == 1 || (stupify && stupify <= skmod))
        {
            d->ai->lastaction = d->ai->lasthunt = lastmillis;
            d->ai->dontmove = true;
        }
        else if(hunt(d, b))
        {
            getyawpitch(dp, vec(d->ai->spot).add(vec(0, 0, d->eyeheight)), d->ai->targyaw, d->ai->targpitch);
            d->ai->lasthunt = lastmillis;
        }
        else d->ai->dontmove = true;
		jumpto(d, b, d->ai->spot);

        fpsent *e = getenemy(d, dp);
        if(e)
        {
            vec ep = getaimpos(d, e);
			float yaw, pitch;
			getyawpitch(dp, ep, yaw, pitch);
			fixrange(yaw, pitch);
            bool insight = cansee(d, dp, ep), hasseen = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= (d->skill*50)+1000,
                quick = d->ai->enemyseen && lastmillis-d->ai->enemyseen <= skmod, targeted = hastarget(d, b, e, yaw, pitch, dp.squaredist(ep)), idle = b.idle == 1;
			if(d->gunselect == GUN_FIST && targeted)
			{
				d->ai->spot = e->feetpos();
				getyawpitch(dp, vec(d->ai->spot).add(vec(0, 0, d->eyeheight)), d->ai->targyaw, d->ai->targpitch);
				idle = d->ai->dontmove = false;
				insight = true;
			}
            if(insight) d->ai->enemyseen = lastmillis;
            if(idle || insight || hasseen)
            {
                float sskew = (insight ? 2.f : (hasseen ? 1.f : 0.5f))*((insight || hasseen) && (d->jumping || d->timeinair) ? 1.5f : 1.f);
                if(idle)
                {
                    d->ai->targyaw = yaw;
                    d->ai->targpitch = pitch;
                    if(!insight) frame /= 3.f;
                }
                else if(!insight) frame /= 2.f;
                scaleyawpitch(d->yaw, d->pitch, yaw, pitch, frame, sskew);
                if(insight || quick)
                {
                    if(targeted && canshoot(d))
                    {
                        d->attacking = true;
                        d->ai->lastaction = lastmillis;
                        result = 4;
                    }
                    else result = insight ? 3 : 2;
                }
                else
                {
                    if(!b.idle && !hasseen) noenemy(d);
                    result = hasseen ? 2 : 1;
                }
            }
            else
            {
                if(!b.idle) noenemy(d);
                result = 0;
                frame /= 2.f;
            }
        }
        else
        {
            if(!b.idle) noenemy(d);
            result = 0;
            frame /= 2.f;
        }

        fixrange(d->ai->targyaw, d->ai->targpitch);
        if(!result) scaleyawpitch(d->yaw, d->pitch, d->ai->targyaw, d->ai->targpitch, frame, 1.f);

		if(d->ai->becareful)
		{
			if(d->physstate != PHYS_FALL) d->ai->becareful = false;
			else
			{
				float yaw, pitch;
				vec v = vec(d->vel).normalize();
				vectoyawpitch(v, yaw, pitch);
				yaw -= d->ai->targyaw; pitch -= d->ai->targpitch;
				fixrange(yaw, pitch);
				if(yaw >= 90 || pitch >= 90) d->ai->becareful = false;
				else d->ai->dontmove = true;
			}
		}

        if(d->ai->dontmove) d->move = d->strafe = 0;
        else
        { // our guys move one way.. but turn another?! :)
            const struct aimdir { int move, strafe, offset; } aimdirs[8] =
            {
                {  1,  0,   0 },
                {  1,  -1,  45 },
                {  0,  -1,  90 },
                { -1,  -1, 135 },
                { -1,  0, 180 },
                { -1, 1, 225 },
                {  0, 1, 270 },
                {  1, 1, 315 }
            };
            float yaw = d->ai->targyaw-d->yaw;
            while(yaw < 0.0f) yaw += 360.0f;
            while(yaw >= 360.0f) yaw -= 360.0f;
            int r = clamp(((int)floor((yaw+22.5f)/45.0f))&7, 0, 7);
            const aimdir &ad = aimdirs[r];
            d->move = ad.move;
            d->strafe = ad.strafe;
            //aimyaw -= ad.offset;
            //fixrange(d->aimyaw, d->aimpitch);
        }
        d->ai->dontmove = false;
        return result;
    }

    void chooseweapon(fpsent *d)
    {
        static const int gunprefs[] = { GUN_CG, GUN_RL, GUN_SG, GUN_RIFLE, GUN_GL, GUN_PISTOL, GUN_FIST };
        int gun = -1;
        if(d->hasammo(d->ai->weappref)) gun = d->ai->weappref;
        else
        {
        	loopi(sizeof(gunprefs)/sizeof(gunprefs[0])) if(d->hasammo(gunprefs[i]))
        	{
        		gun = gunprefs[i];
        		break;
			}
        }
        if(gun >= 0 && gun != d->gunselect) gunselect(gun, d);
    }

    void pickup(fpsent *d, extentity &e)
    {
        if(!d->hasammo(d->gunselect) || (d->gunselect != d->ai->weappref && (!isgoodammo(d->gunselect) || d->hasammo(d->ai->weappref))))
            chooseweapon(d);
    }

    bool request(fpsent *d, aistate &b)
    {
        if(!d->hasammo(d->gunselect) || (d->gunselect != d->ai->weappref && (!isgoodammo(d->gunselect) || d->hasammo(d->ai->weappref))))
            chooseweapon(d);
        return process(d, b) >= 2;
    }

    void findorientation(vec &o, float yaw, float pitch, vec &pos)
    {
        vec dir;
        vecfromyawpitch(yaw, pitch, 1, 0, dir);
        if(raycubepos(o, dir, pos, 0, RAY_CLIPMAT|RAY_SKIPFIRST) == -1)
            pos = dir.mul(2*getworldsize()).add(o); //otherwise 3dgui won't work when outside of map
    }

	void timeouts(fpsent *d, aistate &b)
	{
		if(d->blocked)
		{
			d->ai->blocktime += lastmillis-d->ai->lastrun;
			if(d->ai->blocktime > ((d->ai->blockseq+1)*500))
			{
				switch(d->ai->blockseq)
				{
					case 0: case 1: case 2:
						if(waypoints.inrange(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
						d->ai->clear(false);
						break;
					case 3: d->ai->reset(true); break;
					case 4: d->ai->reset(false); break;
					case 5: suicide(d); return; break;
					default: break;
				}
				d->ai->blockseq++;
			}
		}
		else d->ai->blocktime = d->ai->blockseq = 0;
		if(d->ai->lasthunt)
		{
			int millis = lastmillis-d->ai->lasthunt;
			if(millis <= 2000) { d->ai->tryreset = false; d->ai->huntseq = 0; }
			else if(millis > ((d->ai->huntseq+1)*1000))
			{
				switch(d->ai->huntseq)
				{
					case 0: d->ai->reset(true); break;
					case 1: d->ai->reset(false); break;
					case 2: suicide(d); return; break; // better off doing something than nothing
					default: break;
				}
				d->ai->huntseq++;
			}
		}
		if(d->ai->targnode == d->ai->targlast)
		{
			d->ai->targtime += lastmillis-d->ai->lastrun;
			if(d->ai->targtime > ((d->ai->targseq+1)*3000))
			{
				switch(d->ai->targseq)
				{
					case 0: case 1: case 2:
						if(waypoints.inrange(d->ai->targnode)) d->ai->addprevnode(d->ai->targnode);
						d->ai->clear(false);
						break;
					case 3: d->ai->reset(true); break;
					case 4: d->ai->reset(false); break;
					case 5: suicide(d); return; break;
					default: break;
				}
				d->ai->targseq++;
			}
		}
		else
		{
			d->ai->targtime = d->ai->targseq = 0;
			d->ai->targlast = d->ai->targnode;
		}
	}

    void logic(fpsent *d, aistate &b, bool run)
    {
        vec dp = d->headpos();
        findorientation(dp, d->yaw, d->pitch, d->ai->target);
        bool allowmove = canmove(d) && b.type != AI_S_WAIT;
        if(d->state != CS_ALIVE || !allowmove) d->stopmoving();
        if(d->state == CS_ALIVE)
        {
            if(allowmove)
            {
                if(!request(d, b)) target(d, b, false, b.idle ? true : false);
                shoot(d, d->ai->target);
            }
            if(!intermission)
            {
                if(d->ragdoll) cleanragdoll(d);
                moveplayer(d, 10, true);
                if(allowmove) timeouts(d, b);
				entities::checkitems(d);
				if(cmode) cmode->checkitems(d);
            }
        }
        else if(d->state == CS_DEAD)
        {
            if(d->ragdoll) moveragdoll(d);
            else if(lastmillis-d->lastpain<2000)
            {
                d->move = d->strafe = 0;
                moveplayer(d, 10, false);
            }
        }
        d->attacking = d->jumping = false;
    }

    void avoid()
    {
        // guess as to the radius of ai and other critters relying on the avoid set for now
        float guessradius = player1->radius;
        obstacles.clear();
        loopv(players)
        {
            dynent *d = players[i];
            if(d->state != CS_ALIVE) continue;
            obstacles.avoidnear(d, d->o.z + d->aboveeye + 1, d->feetpos(), guessradius + d->radius);
        }
        avoidweapons(obstacles, guessradius);
    }

    void think(fpsent *d, bool run)
    {
        // the state stack works like a chain of commands, certain commands simply replace each other
        // others spawn new commands to the stack the ai reads the top command from the stack and executes
        // it or pops the stack and goes back along the history until it finds a suitable command to execute
        if(d->ai->state.empty())
        {
        	d->ai->cleartimers();
        	setup(d);
        }
        bool cleannext = false;
        loopvrev(d->ai->state)
        {
            aistate &c = d->ai->state[i];
            if(cleannext)
            {
                c.millis = lastmillis;
                c.override = false;
                cleannext = false;
            }
            if(d->state == CS_DEAD && d->respawned!=d->lifesequence && (!cmode || cmode->respawnwait(d) <= 0) && lastmillis - d->lastpain >= 500)
            {
                addmsg(SV_TRYSPAWN, "rc", d);
                d->respawned = d->lifesequence;
            }
            if(d->state == CS_ALIVE && run && lastmillis >= c.next)
            {
                int result = 0;
                c.idle = 0;
                switch(c.type)
                {
                    case AI_S_WAIT: result = dowait(d, c); break;
                    case AI_S_DEFEND: result = dodefend(d, c); break;
                    case AI_S_PURSUE: result = dopursue(d, c); break;
                    case AI_S_INTEREST: result = dointerest(d, c); break;
                    default: result = 0; break;
                }
                if(result <= 0)
                {
                    d->ai->clear(true);
                    if(c.type != AI_S_WAIT)
                    {
                        d->ai->removestate(i);
                        switch(result)
                        {
                            case 0: default: cleannext = true; break;
                            case -1: i = d->ai->state.length()-1; break;
                        }
						continue; // shouldn't interfere
                    }
                    else c.next = lastmillis+250+rnd(250);
                }
				else c.next = lastmillis+125+rnd(125);
            }
            logic(d, c, run);
            break;
        }
        if(d->ai->trywipe) d->ai->wipe();
        d->ai->lastrun = lastmillis;
    }

    void drawstate(fpsent *d, aistate &b, bool top, int above)
    {
        const char *bnames[AI_S_MAX] = {
            "wait", "defend", "pursue", "interest"
        }, *btypes[AI_T_MAX+1] = {
            "none", "node", "player", "affinity", "entity"
        };
        string s;
        if(top)
        {
            formatstring(s)("\f0%s (%d[%d]) %s:%d (%d[%d])",
                bnames[b.type],
                lastmillis-b.millis, b.next-lastmillis,
                btypes[clamp(b.targtype+1, 0, AI_T_MAX+1)], b.target,
                !d->ai->route.empty() ? d->ai->route[0] : -1,
                d->ai->route.length()
            );
        }
        else
        {
            formatstring(s)("\f2%s (%d[%d]) %s:%d",
                bnames[b.type],
                lastmillis-b.millis, b.next-lastmillis,
                btypes[clamp(b.targtype+1, 0, AI_T_MAX+1)], b.target
            );
        }
        particle_textcopy(vec(d->abovehead()).add(vec(0, 0, above)), s, PART_TEXT, 1);
        if(b.targtype == AI_T_ENTITY && entities::ents.inrange(b.target))
        {
            formatstring(s)("GOAL: %s", colorname(d));
            particle_textcopy(entities::ents[b.target]->o, s, PART_TEXT, 1);
        }
    }

    void drawroute(fpsent *d, aistate &b, float amt = 1.f)
    {
        int colour = 0xFFFFFF, last = -1;

        loopvrev(d->ai->route)
        {
            if(d->ai->route.inrange(last))
            {
                int index = d->ai->route[i], prev = d->ai->route[last];
                if(waypoints.inrange(index) && waypoints.inrange(prev))
                {
                    waypoint &e = waypoints[index],
                        &f = waypoints[prev];
                    vec fr(vec(f.o).add(vec(0, 0, 4.f*amt))),
                        dr(vec(e.o).add(vec(0, 0, 4.f*amt)));
                    particle_flare(fr, dr, 1, PART_STREAK, colour);
                }
            }
            last = i;
        }
        if(aidebug > 4)
        {
            vec pos = d->feetpos();
            if(d->ai->spot != vec(0, 0, 0)) particle_flare(pos, d->ai->spot, 1, PART_LIGHTNING, 0xFFFFFF);
            if(waypoints.inrange(d->lastnode))
                particle_flare(pos, waypoints[d->lastnode].o, 1, PART_LIGHTNING, 0x00FFFF);
            if(waypoints.inrange(d->ai->prevnodes[1]))
                particle_flare(pos, waypoints[d->ai->prevnodes[1]].o, 1, PART_LIGHTNING, 0xFF00FF);
        }
    }

    VAR(showwaypoints, 0, 0, 1);
    VAR(showwaypointsradius, 0, 200, 10000);

    void render()
    {
        if(aidebug > 1)
        {
            int total = 0, alive = 0;
            loopv(players) if(players[i]->ai) total++;
            loopv(players) if(players[i]->state == CS_ALIVE && players[i]->ai)
            {
                fpsent *d = players[i];
                bool top = true;
                int above = 0;
                alive++;
                loopvrev(d->ai->state)
                {
                    aistate &b = d->ai->state[i];
                    drawstate(d, b, top, above += 2);
                    if(aidebug > 3 && top && b.type != AI_S_WAIT)
                        drawroute(d, b, float(alive)/float(total));
                    if(top)
                    {
                        if(aidebug > 2) top = false;
                        else break;
                    }
                }
            }
        }
        if(showwaypoints || aidebug > 5)
        {
            vector<int> close;
            int len = waypoints.length();
            if(showwaypointsradius)
            {
                findwaypointswithin(camera1->o, 0, showwaypointsradius, close);
                len = close.length();
            }
            loopi(len)
            {
                waypoint &w = waypoints[showwaypointsradius ? close[i] : i];
                loopj(MAXWAYPOINTLINKS)
                {
                     int link = w.links[j];
                     if(!link) break;
                     particle_flare(w.o, waypoints[link].o, 1, PART_STREAK, 0x0000FF);
                }
            }

        }
    }
}


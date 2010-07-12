#include "game.h"

extern selinfo sel;

namespace ai
{
    using namespace game;

    vector<waypoint> waypoints;

    int getweight(const vec &o)
    {
        vec pos = o; pos.z += ai::JUMPMIN;
        if(!insideworld(vec(pos.x, pos.y, min(pos.z, getworldsize() - 1e-3f)))) return -2;
        float dist = raycube(pos, vec(0, 0, -1), 0, RAY_CLIPMAT);
        int posmat = lookupmaterial(pos), weight = 1;
        if(isliquid(posmat&MATF_VOLUME)) weight *= 5;
        if(dist >= 0)
        {
            weight = int(dist/ai::JUMPMIN);
            pos.z -= clamp(dist-8.0f, 0.0f, pos.z);
            int trgmat = lookupmaterial(pos);
            if(trgmat&MAT_DEATH || (trgmat&MATF_VOLUME) == MAT_LAVA) weight *= 10;
            else if(isliquid(trgmat&MATF_VOLUME)) weight *= 2;
        }
        return weight;
    }

    struct wpcachenode
    {
        float split[2];
        uint child[2];

        int axis() const { return child[0]>>30; }
        int childindex(int which) const { return child[which]&0x3FFFFFFF; }
        bool isleaf(int which) const { return (child[1]&(1<<(30+which)))!=0; }
    };

    vector<wpcachenode> wpcache;
    int wpcachedepth = -1;
    vec wpcachemin(1e16f, 1e16f, 1e16f), wpcachemax(-1e16f, -1e16f, -1e16f);
	avoidset wpavoid;

    static void buildwpcache(int *indices, int numindices, int depth = 1)
    {
        vec vmin(1e16f, 1e16f, 1e16f), vmax(-1e16f, -1e16f, -1e16f);
        loopi(numindices)
        {
            waypoint &w = waypoints[indices[i]];
            float radius = WAYPOINTRADIUS;
            loopk(3)
            {
                vmin[k] = min(vmin[k], w.o[k]-radius);
                vmax[k] = max(vmax[k], w.o[k]+radius);
            }
        }
        if(depth==1)
        {
            wpcachemin = vmin;
            wpcachemax = vmax;
        }

        int axis = 2;
        loopk(2) if(vmax[k] - vmin[k] > vmax[axis] - vmin[axis]) axis = k;

        float split = 0.5f*(vmax[axis] + vmin[axis]), splitleft = -1e16f, splitright = 1e16f;
        int left, right;
        for(left = 0, right = numindices; left < right;)
        {
            waypoint &w = waypoints[indices[left]];
            float radius = WAYPOINTRADIUS;
            if(max(split - (w.o[axis]-radius), 0.0f) > max((w.o[axis]+radius) - split, 0.0f))
            {
                ++left;
                splitleft = max(splitleft, w.o[axis]+radius);
            }
            else
            {
                --right;
                swap(indices[left], indices[right]);
                splitright = min(splitright, w.o[axis]-radius);
            }
        }

        if(!left || right==numindices)
        {
            left = right = numindices/2;
            splitleft = -1e16f;
            splitright = 1e16f;
            loopi(numindices)
            {
                waypoint &w = waypoints[indices[i]];
                float radius = WAYPOINTRADIUS;
                if(i < left) splitleft = max(splitleft, w.o[axis]+radius);
                else splitright = min(splitright, w.o[axis]-radius);
            }
        }

        int node = wpcache.length();
        wpcache.add();
        wpcache[node].split[0] = splitleft;
        wpcache[node].split[1] = splitright;

        if(left==1) wpcache[node].child[0] = (axis<<30) | indices[0];
        else
        {
            wpcache[node].child[0] = (axis<<30) | wpcache.length();
            if(left) buildwpcache(indices, left, depth+1);
        }

        if(numindices-right==1) wpcache[node].child[1] = (1<<31) | (left==1 ? 1<<30 : 0) | indices[right];
        else
        {
            wpcache[node].child[1] = (left==1 ? 1<<30 : 0) | wpcache.length();
            if(numindices-right) buildwpcache(&indices[right], numindices-right, depth+1);
        }

        wpcachedepth = max(wpcachedepth, depth);
    }

    void clearwpcache()
	{
        wpcache.setsize(0);
        wpcachedepth = -1;
        wpcachemin = vec(1e16f, 1e16f, 1e16f);
        wpcachemax = vec(-1e16f, -1e16f, -1e16f);
		wpavoid.clear();
	}
    COMMAND(clearwpcache, "");

    void buildwpcache()
    {
        wpcache.setsize(0);
        vector<int> indices;
        loopv(waypoints) indices.add(i);
        buildwpcache(indices.getbuf(), indices.length());
		wpavoid.clear();
		loopv(waypoints) if(waypoints[i].weight < 0) wpavoid.avoidnear(NULL, WAYPOINTRADIUS, waypoints[i].o, WAYPOINTRADIUS);
    }

    struct wpcachestack
    {
        wpcachenode *node;
        float tmin, tmax;
    };

    vector<wpcachenode *> wpcachestack;

	static inline bool allowuse(fpsent *d, int n, bool force = true)
	{
		if(!d || !d->ai || force || (!d->ai->hasprevnode(n) && !ai::obstacles.find(n, d))) return true;
		return false;
	}

    int closestwaypoint(const vec &pos, float mindist, bool links, fpsent *d)
    {
        if(waypoints.empty()) return -1;

        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsize(0);

        #define CHECKCLOSEST(branch) do { \
            int n = curnode->childindex(branch); \
            const waypoint &w = waypoints[n]; \
            if((!links || w.links[0]) && allowuse(d, n, force!=0)) \
            { \
                float dist = w.o.squaredist(pos); \
                if(dist < mindist*mindist) { closest = n; mindist = sqrtf(dist); } \
            } \
        } while(0)
        int closest = -1;
        loop(force, 2) for(wpcachenode *curnode = &wpcache[0];;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= mindist)
            {
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKCLOSEST(1);
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKCLOSEST(0);
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKCLOSEST(1);
                }
            }
            else
            {
                if(dist2 < mindist)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(&wpcache[curnode->childindex(1)]);
                    else CHECKCLOSEST(1);
                }
                curnode = &wpcache[curnode->childindex(0)];
                continue;
            }
            if(wpcachestack.empty()) { if(closest >= 0) return closest; else break; }
            curnode = wpcachestack.pop();
        }
        return -1;
    }

    void findwaypointswithin(const vec &pos, float mindist, float maxdist, vector<int> &results)
    {
        if(waypoints.empty()) return;

        float mindist2 = mindist*mindist, maxdist2 = maxdist*maxdist;

        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsize(0);

        wpcachenode *curnode = &wpcache[0];
        #define CHECKWITHIN(branch) do { \
            int n = curnode->childindex(branch); \
            const waypoint &w = waypoints[n]; \
            float dist = w.o.squaredist(pos); \
            if(dist > mindist2 && dist < maxdist2) results.add(n); \
        } while(0)
        for(;;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= maxdist)
            {
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKWITHIN(1);
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKWITHIN(0);
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKWITHIN(1);
                }
            }
            else
            {
                if(dist2 < maxdist)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(&wpcache[curnode->childindex(1)]);
                    else CHECKWITHIN(1);
                }
                curnode = &wpcache[curnode->childindex(0)];
                continue;
            }
            if(wpcachestack.empty()) return;
            curnode = wpcachestack.pop();
        }
    }

    void avoidset::avoidnear(void *owner, float above, const vec &pos, float limit)
    {
        if(waypoints.empty()) return;

        float limit2 = limit*limit;

        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsize(0);

        wpcachenode *curnode = &wpcache[0];
        #define CHECKNEAR(branch) do { \
            int n = curnode->childindex(branch); \
            const waypoint &w = ai::waypoints[n]; \
            if(w.o.squaredist(pos) < limit2) add(owner, above, n); \
        } while(0)
        for(;;)
        {
            int axis = curnode->axis();
            float dist1 = pos[axis] - curnode->split[0], dist2 = curnode->split[1] - pos[axis];
            if(dist1 >= limit)
            {
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKNEAR(1);
                }
            }
            else if(curnode->isleaf(0))
            {
                CHECKNEAR(0);
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) { curnode = &wpcache[curnode->childindex(1)]; continue; }
                    CHECKNEAR(1);
                }
            }
            else
            {
                if(dist2 < limit)
                {
                    if(!curnode->isleaf(1)) wpcachestack.add(&wpcache[curnode->childindex(1)]);
                    else CHECKNEAR(1);
                }
                curnode = &wpcache[curnode->childindex(0)];
                continue;
            }
            if(wpcachestack.empty()) return;
            curnode = wpcachestack.pop();
        }
    }

    int avoidset::remap(fpsent *d, int n, vec &pos, bool retry)
    {
        if(!obstacles.empty())
        {
            int cur = 0;
            loopv(obstacles)
            {
                obstacle &ob = obstacles[i];
                int next = cur + ob.numwaypoints;
                if(ob.owner != d)
                {
                    for(; cur < next; cur++) if(waypoints[cur] == n)
                    {
                        if(ob.above < 0) return retry ? n : -1;
                        vec above(pos.x, pos.y, ob.above);
                        if(above.z-d->o.z >= ai::JUMPMAX)
                            return retry ? n : -1; // too much scotty
                        int node = closestwaypoint(above, ai::SIGHTMIN, true, d);
                        if(ai::waypoints.inrange(node) && node != n)
                        { // try to reroute above their head?
                            if(!find(node, d))
                            {
                                pos = ai::waypoints[node].o;
                                return node;
                            }
                            else return retry ? n : -1;
                        }
                        else
                        {
                            vec old = d->o;
                            d->o = vec(above).add(vec(0, 0, d->eyeheight));
                            bool col = collide(d, vec(0, 0, 1));
                            d->o = old;
                            if(col)
                            {
                                pos = above;
                                return n;
                            }
                            else return retry ? n : -1;
                        }
                    }
                }
                cur = next;
            }
        }
        return n;
    }

    static inline float heapscore(waypoint *q) { return q->score(); }

    bool route(fpsent *d, int node, int goal, vector<int> &route, const avoidset &obstacles, bool retry)
    {
        if(!waypoints.inrange(node) || !waypoints.inrange(goal) || goal == node || !waypoints[node].links[0])
            return false;

        static ushort routeid = 1;
        static vector<waypoint *> queue;

        if(!routeid)
        {
            loopv(waypoints) waypoints[i].route = 0;
            routeid = 1;
        }

        if(d && !retry)
        {
            if(d->ai) loopi(ai::NUMPREVNODES) if(d->ai->prevnodes[i] != node && waypoints.inrange(d->ai->prevnodes[i]))
            {
                waypoints[d->ai->prevnodes[i]].route = routeid;
                waypoints[d->ai->prevnodes[i]].curscore = -1;
                waypoints[d->ai->prevnodes[i]].estscore = 0;
            }
            vec pos = d->o;
            pos.z -= d->eyeheight;
            loopavoid(obstacles, d,
            {
                if(waypoints.inrange(wp) && wp != node && wp != goal && waypoints[node].find(wp) < 0 && waypoints[goal].find(wp) < 0)
                {
                    waypoints[wp].route = routeid;
                    waypoints[wp].curscore = -1;
                    waypoints[wp].estscore = 0;
                }
            });
        }

        waypoints[node].route = routeid;
        waypoints[node].curscore = waypoints[node].estscore = 0;
        waypoints[node].prev = 0;
        queue.setsize(0);
        queue.add(&waypoints[node]);
        route.setsize(0);

        int lowest = -1;
        while(!queue.empty())
        {
            waypoint &m = *queue.removeheap();
            float prevscore = m.curscore;
            m.curscore = -1;
            loopi(MAXWAYPOINTLINKS)
            {
                int link = m.links[i];
                if(!link) break;
                if(waypoints.inrange(link) && (link == node || link == goal || waypoints[link].links[0]))
                {
                    waypoint &n = waypoints[link];
                    int weight = max(n.weight, 1);
                    float curscore = prevscore + n.o.dist(m.o)*weight;
                    if(n.route == routeid && curscore >= n.curscore) continue;
                    n.curscore = curscore;
                    n.prev = ushort(&m - &waypoints[0]);
                    if(n.route != routeid)
                    {
                        n.estscore = n.o.dist(waypoints[goal].o)*weight;
                        if(n.estscore <= WAYPOINTRADIUS*4 && (lowest < 0 || n.estscore <= waypoints[lowest].estscore))
                            lowest = link;
                        n.route = routeid;
                        if(link == goal) goto foundgoal;
                        queue.addheap(&n);
                    }
                    else loopvj(queue) if(queue[j] == &n) { queue.upheap(j); break; }
                }
            }
        }
        foundgoal:

        routeid++;

        if(lowest >= 0) // otherwise nothing got there
        {
            for(waypoint *m = &waypoints[lowest]; m > &waypoints[0]; m = &waypoints[m->prev])
                route.add(m - &waypoints[0]); // just keep it stored backward
        }

        return !route.empty();
    }

    VAR(dropwaypoints, 0, 0, 1);

    int addwaypoint(const vec &o, int weight = -1)
    {
        if(waypoints.length() > MAXWAYPOINTS) return -1;
        int n = waypoints.length();
        waypoints.add(waypoint(o, weight >= 0 ? weight : getweight(o)));
        clearwpcache();
        return n;
    }

    void linkwaypoint(waypoint &a, int n)
    {
        loopi(MAXWAYPOINTLINKS)
        {
            if(a.links[i] == n) return;
            if(!a.links[i]) { a.links[i] = n; return; }
        }
        a.links[rnd(MAXWAYPOINTLINKS)] = n;
    }

    string loadedwaypoints = "";

    static inline bool shouldnavigate()
    {
        if(dropwaypoints) return true;
        loopvrev(players) if(players[i]->aitype != AI_NONE) return true;
        return false;
    }

    static inline bool shoulddrop(fpsent *d)
    {
        return !d->ai && (dropwaypoints || !loadedwaypoints[0]);
    }

    void inferwaypoints(fpsent *d, const vec &o, const vec &v, float mindist)
    {
        if(!shouldnavigate()) return;
    	if(shoulddrop(d))
    	{
			if(waypoints.empty()) seedwaypoints();
			int from = closestwaypoint(o, mindist, false), to = closestwaypoint(v, mindist, false);
			if(!waypoints.inrange(from)) from = addwaypoint(o);
			if(!waypoints.inrange(to)) to = addwaypoint(v);
			if(d->lastnode != from && waypoints.inrange(d->lastnode) && waypoints.inrange(from))
				linkwaypoint(waypoints[d->lastnode], from);
			if(waypoints.inrange(to))
			{
				if(from != to && waypoints.inrange(from) && waypoints.inrange(to))
					linkwaypoint(waypoints[from], to);
				d->lastnode = to;
			}
		}
		else d->lastnode = closestwaypoint(v, WAYPOINTRADIUS*2, false, d);
    }

    void navigate(fpsent *d)
    {
        vec v(d->feetpos());
        if(d->state != CS_ALIVE) { d->lastnode = -1; return; }
        bool dropping = shoulddrop(d);
        int mat = lookupmaterial(v);
        if((mat&MATF_CLIP) == MAT_CLIP || (mat&MATF_VOLUME) == MAT_LAVA || mat&MAT_DEATH) dropping = false;
        float dist = dropping ? WAYPOINTRADIUS : (d->ai ? WAYPOINTRADIUS : SIGHTMIN);
        int curnode = closestwaypoint(v, dist, false, d), prevnode = d->lastnode;
        if(!waypoints.inrange(curnode) && dropping)
        {
			if(waypoints.empty()) seedwaypoints();
        	curnode = addwaypoint(v);
        }
        if(waypoints.inrange(curnode))
        {
            if(dropping && d->lastnode != curnode && waypoints.inrange(d->lastnode))
            {
                linkwaypoint(waypoints[d->lastnode], curnode);
                if(!d->timeinair) linkwaypoint(waypoints[curnode], d->lastnode);
            }
            d->lastnode = curnode;
            if(d->ai && waypoints.inrange(prevnode) && d->lastnode != prevnode) d->ai->addprevnode(prevnode);
        }
        else if(!waypoints.inrange(d->lastnode) || waypoints[d->lastnode].o.squaredist(v) > SIGHTMIN*SIGHTMIN)
			d->lastnode = closestwaypoint(v, SIGHTMAX, false, d);
    }

    void navigate()
    {
    	if(shouldnavigate())
    	{
			loopv(players) ai::navigate(players[i]);
    	}
    }

    void clearwaypoints(bool full)
    {
        waypoints.setsize(0);
        clearwpcache();
        if(full)
        {
            loadedwaypoints[0] = '\0';
            dropwaypoints = 0;
        }
    }
    ICOMMAND(clearwaypoints, "", (), clearwaypoints());

    void seedwaypoints()
    {
        if(waypoints.empty()) addwaypoint(vec(0, 0, 0));
        loopv(entities::ents)
        {
            extentity &e = *entities::ents[i];
            switch(e.type)
            {
                case PLAYERSTART: case TELEPORT: case JUMPPAD: case FLAG: case BASE:
                    addwaypoint(e.o);
                    break;
                default:
                    if(e.type >= I_SHELLS && e.type <= I_QUAD) addwaypoint(e.o);
                    break;
            }
        }
    }

    bool unlinkwaypoint(waypoint &w, int link)
    {
        int found = -1, highest = MAXWAYPOINTLINKS-1;
        loopi(MAXWAYPOINTLINKS)
        {
            if(w.links[i] == link) { found = -1; }
            if(!w.links[i]) { highest = i-1; break; }
        }
        if(found < 0) return false;
        w.links[found] = w.links[highest];
        w.links[highest] = 0;
        return true;
    }

    bool relinkwaypoint(waypoint &w, int olink, int nlink)
    {
        loopi(MAXWAYPOINTLINKS)
        {
            if(!w.links[i]) break;
            if(w.links[i] == olink) { w.links[i] = nlink; return true; }
        }
        return false;
    }

    FVAR(waypointmergescale, 1e-3f, 0.75f, 1000);
    VAR(waypointmergepasses, 0, 4, 10);

    void remapwaypoints()
    {
        vector<ushort> remap;
        int total = 0;
        loopv(waypoints) remap.add(waypoints[i].links[1] == 0xFFFF ? 0 : total++);
        total = 0;
        loopvj(waypoints)
        {
            if(waypoints[j].links[1] == 0xFFFF) continue;
            waypoint &w = waypoints[total];
            if(j != total) w = waypoints[j];
            loopi(MAXWAYPOINTLINKS)
            {
                int link = w.links[i];
                if(!link) break;
                w.links[i] = remap[link];
            }
            total++;
        }
        waypoints.setsize(total);
    }

    bool getwaypointfile(const char *mname, char *wptname)
    {
        if(!mname || !*mname) mname = getclientmap();
        if(!*mname) return false;

        string pakname, mapname, cfgname;
        getmapfilenames(mname, NULL, pakname, mapname, cfgname);
        formatstring(wptname)("packages/%s.wpt", mapname);
        path(wptname);
        return true;
    }

    void loadwaypoints(bool force, const char *mname)
    {
        string wptname;
        if(!getwaypointfile(mname, wptname)) return;
        if(!force && (waypoints.length() || !strcmp(loadedwaypoints, wptname))) return;

        stream *f = opengzfile(wptname, "rb");
        if(!f) return;
        char magic[4];
        if(f->read(magic, 4) < 4 || memcmp(magic, "OWPT", 4)) { delete f; return; }

        copystring(loadedwaypoints, wptname);

        waypoints.setsize(0);
        waypoints.add(vec(0, 0, 0));
        ushort numwp = f->getlil<ushort>();
        loopi(numwp)
        {
            if(f->end()) break;
            vec o;
            o.x = f->getlil<float>();
            o.y = f->getlil<float>();
            o.z = f->getlil<float>();
            waypoint &w = waypoints.add(waypoint(o, getweight(o)));
            int numlinks = clamp(f->getchar(), 0, MAXWAYPOINTLINKS);
            loopi(numlinks) w.links[i] = f->getlil<ushort>();
        }

        delete f;
        conoutf("loaded %d waypoints from %s", numwp, wptname);

        clearwpcache();
    }
    ICOMMAND(loadwaypoints, "s", (char *mname), loadwaypoints(true, mname));

    void savewaypoints(bool force, const char *mname)
    {
        if((!dropwaypoints && !force) || waypoints.empty()) return;

        string wptname;
        if(!getwaypointfile(mname, wptname)) return;

        stream *f = opengzfile(wptname, "wb");
        if(!f) return;
        f->write("OWPT", 4);
        f->putlil<ushort>(waypoints.length()-1);
        for(int i = 1; i < waypoints.length(); i++)
        {
            waypoint &w = waypoints[i];
            f->putlil<float>(w.o.x);
            f->putlil<float>(w.o.y);
            f->putlil<float>(w.o.z);
            int numlinks = 0;
            loopj(MAXWAYPOINTLINKS) { if(!w.links[j]) break; numlinks++; }
            f->putchar(numlinks);
            loopj(numlinks) f->putlil<ushort>(w.links[j]);
        }

        delete f;
        conoutf("saved %d waypoints to %s", waypoints.length()-1, wptname);
    }

    ICOMMAND(savewaypoints, "s", (char *mname), savewaypoints(true, mname));

    void delselwaypoints()
    {
        if(noedit(true)) return;
        vec o = sel.o.tovec().sub(0.1f), s = sel.s.tovec().mul(sel.grid).add(o).add(0.1f);
        int cleared = 0;
        loopv(waypoints)
        {
            waypoint &w = waypoints[i];
            if(w.o.x >= o.x && w.o.x <= s.x && w.o.y >= o.y && w.o.y <= s.y && w.o.z >= o.z && w.o.z <= s.z)
            {
                w.links[0] = 0;
                w.links[1] = 0xFFFF;
                cleared++;
            }
        }
        if(cleared)
        {
            player1->lastnode = -1;
            remapwaypoints();
            clearwpcache();
        }
    }
    COMMAND(delselwaypoints, "");
}


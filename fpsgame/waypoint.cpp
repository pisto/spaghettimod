#include "game.h"

namespace ai
{
    using namespace game;

    vector<waypoint> waypoints;

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
        wpcache.setsizenodelete(0);
        wpcachedepth = -1;
        wpcachemin = vec(1e16f, 1e16f, 1e16f);
        wpcachemax = vec(-1e16f, -1e16f, -1e16f);
    }
    COMMAND(clearwpcache, "");

    void buildwpcache()
    {
        wpcache.setsizenodelete(0);
        vector<int> indices;
        loopv(waypoints) indices.add(i);
        buildwpcache(indices.getbuf(), indices.length());
    }

    struct wpcachestack
    {
        wpcachenode *node;
        float tmin, tmax;
    };

    vector<wpcachenode *> wpcachestack;

    int closestwaypoint(const vec &pos, float mindist, bool links)
    {
        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsizenodelete(0);

        int closest = -1;
        wpcachenode *curnode = &wpcache[0];
        #define CHECKCLOSEST(branch) do { \
            int n = curnode->childindex(branch); \
            const waypoint &w = waypoints[n]; \
            if(!links || w.links[0]) \
            { \
                float dist = w.o.squaredist(pos); \
                if(dist < mindist*mindist) { closest = n; mindist = sqrtf(dist); } \
            } \
        } while(0)
        for(;;)
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
            if(wpcachestack.empty()) return closest;
            curnode = wpcachestack.pop();
        }
    }

    void findwaypointswithin(const vec &pos, float mindist, float maxdist, vector<int> &results)
    {
        float mindist2 = mindist*mindist, maxdist2 = maxdist*maxdist;

        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsizenodelete(0);

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
        float limit2 = limit*limit;

        if(wpcachedepth<0) buildwpcache();

        wpcachestack.setsizenodelete(0);

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

    int avoidset::remap(fpsent *d, int n, vec &pos)
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
                        if(ob.above < 0) return -1;
                        vec above(pos.x, pos.y, ob.above);
                        if(above.z-d->o.z >= ai::JUMPMAX)
                            return -1; // too much scotty
                        int node = closestwaypoint(above, ai::NEARDIST, true);
                        if(ai::waypoints.inrange(node) && node != n)
                        { // try to reroute above their head?
                            if(!find(node, d))
                            {
                                pos = ai::waypoints[node].o;
                                return node;
                            }
                            else return -1;
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
                            else return -1;
                        }
                    }
                }
                cur = next;
            }
        }
        return n;
    }

    bool route(fpsent *d, int node, int goal, vector<int> &route, const avoidset &obstacles, float obdist)
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

        if(obdist >= 0)
        {
            obdist *= obdist;

            vec pos = d->o;
            pos.z -= d->eyeheight;
            loopavoid(obstacles, d,
            {
                if(waypoints.inrange(wp) && (wp == node || wp == goal || waypoints[wp].links[0]) && (obdist <= 0 || waypoints[wp].o.squaredist(pos) <= obdist))
                {
                    waypoints[wp].route = routeid;
                    waypoints[wp].curscore = -1;
                    waypoints[wp].estscore = 0;
                }
            });
        }

        waypoints[node].route = waypoints[goal].route = routeid;
        waypoints[goal].curscore = waypoints[node].curscore = 0.f;
        waypoints[goal].estscore = waypoints[node].estscore = 0.f;
        waypoints[goal].prev = waypoints[node].prev = 0;
        queue.setsizenodelete(0);
        queue.add(&waypoints[node]);
        route.setsizenodelete(0);
        
        int lowest = -1;
        while(!queue.empty())
        {
            int q = queue.length()-1;
            loopi(queue.length()-1) if(queue[i]->score() < queue[q]->score()) q = i;
            waypoint &m = *queue.removeunordered(q);
            int prevscore = m.curscore;
            m.curscore = -1;
            loopi(MAXWAYPOINTLINKS)
            {
                int link = m.links[i];
                if(!link) break;
                if(waypoints.inrange(link) && (link == node || link == goal || waypoints[link].links[0]))
                {
                    waypoint &n = waypoints[link];
                    float curscore = prevscore + n.o.dist(m.o);
                    if(n.route == routeid && curscore >= n.curscore) continue;
                    n.curscore = short(curscore);
                    n.prev = ushort(&m - &waypoints[0]);
                    if(n.route != routeid)
                    {
                        n.estscore = short(n.o.dist(waypoints[goal].o));
                        if(n.estscore <= WAYPOINTRADIUS*4 && (lowest < 0 || n.estscore < waypoints[lowest].estscore))
                            lowest = link;
                        if(link != goal) queue.add(&n);
                        else queue.setsizenodelete(0);
                        n.route = routeid;
                    }
                }
            }
        }

        routeid++;

        if(lowest >= 0) // otherwise nothing got there
        {
            for(waypoint *m = &waypoints[lowest]; m > &waypoints[0]; m = &waypoints[m->prev])
                route.add(m - &waypoints[0]); // just keep it stored backward
        }

        return !route.empty();
    }

    VAR(dropwaypoints, 0, 0, 1);

    int addwaypoint(const vec &o)
    {
        if(waypoints.length() > MAXWAYPOINTS) return -1;
        int n = waypoints.length();
        waypoints.add(o);
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

    void trydropwaypoint(fpsent *d)
    {
        vec v(d->feetpos());
        if(d->state != CS_ALIVE) { d->lastnode = -1; return; }
        bool shoulddrop = (m_timed || dropwaypoints) && !d->ai;
        float dist = shoulddrop ? WAYPOINTRADIUS*(2 - dropwaypoints) : NEARDIST;
        int curnode = closestwaypoint(v, dist, false);
        if(!waypoints.inrange(curnode) && shoulddrop) 
        {
            if(waypoints.empty()) seedwaypoints();
            curnode = addwaypoint(v);
        }
        if(waypoints.inrange(curnode))
        {
            if(shoulddrop && waypoints.inrange(d->lastnode) && d->lastnode != curnode)
            {
                linkwaypoint(waypoints[d->lastnode], curnode);
                if(!d->timeinair) linkwaypoint(waypoints[curnode], d->lastnode);
            }
            d->lastnode = curnode;
        }
        else d->lastnode = -1; 
    }

    void trydropwaypoints()
    {
        if(waypoints.empty() && !dropwaypoints)
        {
            bool hasbot = false;
            loopvrev(players) if(players[i] && players[i]->aitype != AI_NONE) hasbot = true;
            if(!hasbot) return;
        }
        ai::trydropwaypoint(player1);
        loopv(players) if(players[i]) ai::trydropwaypoint(players[i]);
    }

    void clearwaypoints()
    {
        waypoints.setsizenodelete(0);
    }
    COMMAND(clearwaypoints, "");

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
}


// server-side ai manager
namespace aiman
{
    void calcteams(vector<teamscore> &teams)
    {
        const char *defaults[2] = { "good", "evil" };
        loopv(clients)
        {
            clientinfo *ci = clients[i];
            if(ci->state.state==CS_SPECTATOR || !ci->team[0]) continue;
            teamscore *t = NULL;
            loopvj(teams) if(!strcmp(teams[j].team, ci->team)) { t = &teams[j]; break; }
            if(t) t->score++;
            else teams.add(teamscore(ci->team, 1));
        }
        teams.sort(teamscore::compare);
        if(teams.length() < int(sizeof(defaults)/sizeof(defaults[0])))
        {
            loopi(sizeof(defaults)/sizeof(defaults[0]))
            {
                loopvj(teams) if(!strcmp(teams[j].team, defaults[i])) goto nextteam;
                teams.add(teamscore(defaults[i], 0));
            nextteam:;
            }
        }
    }

    void balanceteams()
    {
        vector<teamscore> teams;
        calcteams(teams);
        vector<clientinfo *> reassign;
        loopv(bots) if(bots[i]) reassign.add(bots[i]);
        while(reassign.length() && teams.length() && teams[0].score > teams.last().score + 1)
        {
            teamscore &t = teams.last();
            clientinfo *bot = NULL;
            loopv(reassign) if(reassign[i] && !strcmp(reassign[i]->team, teams[0].team))
            {
                bot = reassign.removeunordered(i);
                teams[0].score--;
                t.score++;
                for(int j = teams.length() - 2; j >= 0; j--)
                {
                    if(teams[j].score >= teams[j+1].score) break;
                    swap(teams[j], teams[j+1]);
                }
                break;
            }
            if(bot)
            {
                if(smode && bot->state.state==CS_ALIVE) smode->changeteam(bot, bot->team, t.team);
                s_strncpy(bot->team, t.team, MAXTEAMLEN+1);
                sendf(-1, 1, "riis", SV_SETTEAM, bot->clientnum, bot->team);
            }
            else teams.remove(0, 1);
        }
    }

    const char *chooseteam()
    {
        vector<teamscore> teams;
        calcteams(teams);
        return teams.length() ? teams.last().team : "";
    }

	int findaiclient(int exclude)
	{
        int leastcn = -1, leastbots = INT_MAX;
		loopv(clients)
		{
			clientinfo *ci = clients[i];
			if(ci->clientnum < 0 || ci->state.aitype != AI_NONE || !ci->name[0] || !ci->connected || ci->clientnum == exclude) continue;
		    int numbots = 0;
			loopvj(bots) if(bots[j] && bots[j]->ownernum == ci->clientnum) numbots++;
            if(numbots < leastbots) { leastcn = ci->clientnum; leastbots = numbots; }
		}
        return leastcn;
	}

	bool addai(int skill, bool req)
	{
		int numai = 0, cn = -1;
		loopv(bots)
        {
            clientinfo *ci = bots[i];
            if(!ci) { if(cn < 0) cn = i; continue; }
			if(ci->ownernum < 0)
			{ // reuse a slot that was going to removed
				ci->ownernum = findaiclient();
				ci->aireinit = 2;
				if(req) autooverride = true;
				return true;
			}
			numai++;
		}
        if(numai >= MAXBOTS) return false;
        if(cn < 0) { cn = bots.length(); bots.add(NULL); }
        const char *team = m_teammode ? chooseteam() : "";
        if(!bots[cn]) bots[cn] = new clientinfo;
        clientinfo *ci = bots[cn];
		ci->clientnum = MAXCLIENTS + cn;
		ci->state.aitype = AI_BOT;
		ci->ownernum = findaiclient();
        ci->state.skill = skill <= 0 ? rnd(50) + 51 : clamp(skill, 1, 101);
	    clients.add(ci);
		ci->state.lasttimeplayed = lastmillis;
		s_strncpy(ci->name, "bot", MAXNAMELEN+1);
		ci->state.state = CS_DEAD;
        s_strncpy(ci->team, team, MAXTEAMLEN+1);
		ci->aireinit = 2;
		ci->connected = true;
		if(req) autooverride = true;
		return true;
	}

	void deleteai(clientinfo *ci)
	{
        int cn = ci->clientnum - MAXCLIENTS;
        if(!bots.inrange(cn)) return;
        if(smode) smode->leavegame(ci, true);
        sendf(-1, 1, "ri2", SV_CDIS, ci->clientnum);
        clients.removeobj(ci);
        DELETEP(bots[cn]);
		dorefresh = true;
	}

	bool delai(bool req)
	{
        loopv(bots) if(bots[i] && bots[i]->ownernum >= 0)
        {
			deleteai(bots[i]);
			if(req) autooverride = true;
			return true;
		}
		if(req)
		{
			autooverride = false;
			return true;
		}
		return false;
	}

	void reinitai(clientinfo *ci)
	{
		if(ci->ownernum < 0) deleteai(ci);
		else if(ci->aireinit >= 1)
		{
			sendf(-1, 1, "ri5ss", SV_INITAI, ci->clientnum, ci->ownernum, ci->state.aitype, ci->state.skill, ci->name, ci->team);
			if(ci->aireinit == 2)
			{
                ci->state.state = CS_DEAD;
                ci->state.respawn();
			}
			ci->aireinit = 0;
		}
	}

	void shiftai(clientinfo *ci, int cn = -1)
	{
		if(cn < 0) { ci->aireinit = 0; ci->ownernum = -1; }
		else { ci->aireinit = 2; ci->ownernum = cn; }
	}

	void removeai(clientinfo *ci, bool complete)
	{ // either schedules a removal, or someone else to assign to
		loopv(bots) if(bots[i] && bots[i]->ownernum == ci->clientnum)
			shiftai(bots[i], complete ? -1 : findaiclient(ci->clientnum));
	}

	bool reassignai(int exclude)
	{
		vector<int> siblings;
		while(siblings.length() < clients.length()) siblings.add(-1);
        clientinfo *hi = NULL, *lo = NULL;
		int hibots = INT_MIN, lobots = INT_MAX;
		loopv(clients)
		{
			clientinfo *ci = clients[i];
			if(ci->clientnum < 0 || ci->state.aitype != AI_NONE || !ci->name[0] || !ci->connected || ci->clientnum == exclude) continue;
            int numbots = 0;
            loopvj(bots) if(bots[j] && bots[j]->ownernum == ci->clientnum) numbots++;
            if(numbots < lobots) { lo = ci; lobots = numbots; }
            if(numbots > hibots) { hi = ci; hibots = numbots; }
		}
		if(hi && lo && hibots - lobots > 1)
		{
			loopv(bots) if(bots[i] && bots[i]->ownernum == hi->ownernum)
			{
				shiftai(bots[i], lo->ownernum);
				return true;
			}
		}
		return false;
	}


	void checksetup()
	{
		if(dorefresh)
		{
			if(m_teammode && !autooverride) balanceteams();
			dorefresh = false;
		}
		loopvrev(bots) if(bots[i]) reinitai(bots[i]);
	}

	void clearai()
	{ // clear and remove all ai immediately
        loopvrev(bots) if(bots[i]) deleteai(bots[i]);
		dorefresh = autooverride = false;
	}

	void checkai()
	{
        if(m_timed && numclients(-1, false, true))
		{
			checksetup();
			while(reassignai());
		}
		else clearai();
	}

	void reqadd(clientinfo *ci, int skill)
	{
        if(!ci->local && ci->privilege < PRIV_ADMIN) return;
        if(!addai(skill, true)) sendf(ci->clientnum, 1, "ris", SV_SERVMSG, "failed to create or assign bot");
	}

	void reqdel(clientinfo *ci)
	{
        if(!ci->local && ci->privilege < PRIV_ADMIN) return;
        if(!delai(true)) sendf(ci->clientnum, 1, "ris", SV_SERVMSG, "failed to remove any bots");
	}
}

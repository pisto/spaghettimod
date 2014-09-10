// server.cpp: little more than enhanced multicaster
// runs dedicated or as client coroutine

#include "engine.h"
#include "spaghetti.h"

#define LOGSTRLEN 512

static FILE *logfile = NULL;

void closelogfile()
{
    if(logfile)
    {
        fclose(logfile);
        logfile = NULL;
    }
}

FILE *getlogfile()
{
#ifdef WIN32
    return logfile;
#else
    return logfile ? logfile : stdout;
#endif
}

void setlogfile(const char *fname)
{
    closelogfile();
    if(fname && fname[0])
    {
        fname = findfile(fname, "w");
        if(fname) logfile = fopen(fname, "a");
    }
    FILE *f = getlogfile();
    if(f) setvbuf(f, NULL, _IOLBF, BUFSIZ);
}

void logoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    logoutfv(fmt, args);
    va_end(args);
}


static void writelog(FILE *file, std::string s)
{
    static bool recursion = false;
    if(!recursion)
    {
        recursion = true;
        bool skip = spaghetti::simplehook(spaghetti::hotstring::log, s);
        recursion = false;
        if(skip) return;
    }
    const char* buf = s.c_str();
    static uchar ubuf[512];
    size_t len = strlen(buf), carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &((const uchar *)buf)[carry], len - carry, &carry);
        if(carry >= len) ubuf[numu++] = '\n';
        fwrite(ubuf, 1, numu, file);
    }
}

static void writelogv(FILE *file, const char *fmt, va_list args)
{
    static char buf[LOGSTRLEN];
    vformatstring(buf, fmt, args, sizeof(buf));
    writelog(file, buf);
}
 
#ifdef STANDALONE
void fatal(const char *fmt, ...) 
{ 
    void cleanupserver();
    cleanupserver(); 
	defvformatstring(msg,fmt,fmt);
	if(logfile) logoutf("%s", msg);
#ifdef WIN32
	MessageBox(NULL, msg, "Cube 2: Sauerbraten fatal error", MB_OK|MB_SYSTEMMODAL);
#else
    fprintf(stderr, "server error: %s\n", msg);
#endif
    closelogfile();
    exit(EXIT_FAILURE); 
}

void conoutfv(int type, const char *fmt, va_list args)
{
    logoutfv(fmt, args);
}

void conoutf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    conoutfv(CON_INFO, fmt, args);
    va_end(args);
}

void conoutf(int type, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    conoutfv(type, fmt, args);
    va_end(args);
}
#endif

#define DEFAULTCLIENTS 8

enum { ST_EMPTY, ST_LOCAL, ST_TCPIP };

namespace server{
struct clientinfo;
}

struct client                   // server side version of "dynent" type
{
    int type;
    int num;
    ENetPeer *peer;
    lua_string hostname;
    server::clientinfo *info;
};

vector<client *> clients;

ENetHost *serverhost = NULL;
int laststatus = 0; 
ENetSocket pongsock = ENET_SOCKET_NULL, lansock = ENET_SOCKET_NULL;

int localclients = 0, nonlocalclients = 0;

bool hasnonlocalclients() { return nonlocalclients!=0; }
bool haslocalclients() { return localclients!=0; }

client &addclient(int type)
{
    client *c = NULL;
    loopv(clients) if(clients[i]->type==ST_EMPTY)
    {
        c = clients[i];
        break;
    }
    if(!c)
    {
        c = new client;
        c->num = clients.length();
        clients.add(c);
    }
    c->info = (server::clientinfo*)server::newclientinfo();
    c->type = type;
    switch(type)
    {
        case ST_TCPIP: nonlocalclients++; break;
        case ST_LOCAL: localclients++; break;
    }
    return *c;
}

void delclient(client *c)
{
    if(!c) return;
    switch(c->type)
    {
        case ST_TCPIP: nonlocalclients--; if(c->peer) c->peer->data = NULL; break;
        case ST_LOCAL: localclients--; break;
        case ST_EMPTY: return;
    }
    c->type = ST_EMPTY;
    c->peer = NULL;
    if(c->info)
    {
        server::deleteclientinfo(c->info);
        c->info = NULL;
    }
}

void cleanupserver()
{
    loopvrev(clients){
        if(clients[i]->type == ST_LOCAL) break;
        delclient(clients[i]);
        delete clients.pop();
    }
    if(serverhost) enet_host_destroy(serverhost);
    serverhost = NULL;

    if(pongsock != ENET_SOCKET_NULL) enet_socket_destroy(pongsock);
    if(lansock != ENET_SOCKET_NULL) enet_socket_destroy(lansock);
    pongsock = lansock = ENET_SOCKET_NULL;
}

VARF(maxclients, 0, DEFAULTCLIENTS, MAXCLIENTS, { if(!maxclients) maxclients = DEFAULTCLIENTS; });
VARF(maxdupclients, 0, 0, MAXCLIENTS, { if(serverhost) serverhost->duplicatePeers = maxdupclients ? maxdupclients : MAXCLIENTS; });

void process(ENetPacket *packet, int sender, int chan);
//void disconnect_client(int n, int reason);

int getservermtu() { return serverhost ? serverhost->mtu : -1; }
server::clientinfo *getclientinfo(int i) { return !clients.inrange(i) || clients[i]->type==ST_EMPTY ? NULL : clients[i]->info; }
ENetPeer *getclientpeer(int i) { return clients.inrange(i) && clients[i]->type==ST_TCPIP ? clients[i]->peer : NULL; }
int getnumclients()        { return clients.length(); }
uint getclientip(int n)    { return clients.inrange(n) && clients[n]->type==ST_TCPIP ? clients[n]->peer->address.host : 0; }

void sendpacket(int n, int chan, ENetPacket *packet, int exclude)
{
    if(n<0)
    {
        server::recordpacket(chan, packet->data, packet->dataLength);
        loopv(clients) if(i!=exclude && server::allowbroadcast(i)) sendpacket(i, chan, packet);
        return;
    }
    switch(clients[n]->type)
    {
        case ST_TCPIP:
        {
            auto const ci = clients[n]->info;
            if(spaghetti::simplehook(spaghetti::hotstring::send, ci, chan, packet)) break;
            enet_peer_send(clients[n]->peer, chan, packet);
            break;
        }

#ifndef STANDALONE
        case ST_LOCAL:
            localservertoclient(chan, packet);
            break;
#endif
    }
}

ENetPacket *sendf(int cn, int chan, const char *format, ...)
{
    int exclude = -1;
    bool reliable = false;
    if(*format=='r') { reliable = true; ++format; }
    packetbuf p(MAXTRANS, reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'x':
            exclude = va_arg(args, int);
            break;

        case 'v':
        {
            int n = va_arg(args, int);
            int *v = va_arg(args, int *);
            loopi(n) putint(p, v[i]);
            break;
        }

        case 'i': 
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }
        case 'f':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putfloat(p, (float)va_arg(args, double));
            break;
        }
        case 's': sendstring(va_arg(args, const char *), p); break;
        case 'm':
        {
            int n = va_arg(args, int);
            p.put(va_arg(args, uchar *), n);
            break;
        }
    }
    va_end(args);
    ENetPacket *packet = p.finalize();
    sendpacket(cn, chan, packet, exclude);
    return packet->referenceCount > 0 ? packet : NULL;
}

ENetPacket *sendfile(int cn, int chan, stream *file, const char *format, ...)
{
    if(cn < 0)
    {
#ifdef STANDALONE
        return NULL;
#endif
    }
    else if(!clients.inrange(cn)) return NULL;

    int len = (int)min(file->size(), stream::offset(INT_MAX));
    if(len <= 0 || len > 16<<20) return NULL;

    packetbuf p(MAXTRANS+len, ENET_PACKET_FLAG_RELIABLE);
    va_list args;
    va_start(args, format);
    while(*format) switch(*format++)
    {
        case 'i':
        {
            int n = isdigit(*format) ? *format++-'0' : 1;
            loopi(n) putint(p, va_arg(args, int));
            break;
        }
        case 's': sendstring(va_arg(args, const char *), p); break;
        case 'l': putint(p, len); break;
    }
    va_end(args);

    file->seek(0, SEEK_SET);
    file->read(p.subbuf(len).buf, len);

    ENetPacket *packet = p.finalize();
    if(cn >= 0) sendpacket(cn, chan, packet, -1);
#ifndef STANDALONE
    else sendclientpacket(packet, chan);
#endif
    return packet->referenceCount > 0 ? packet : NULL;
}

const char *disconnectreason(int reason)
{
    switch(reason)
    {
        case DISC_EOP: return "end of packet";
        case DISC_LOCAL: return "server is in local mode";
        case DISC_KICK: return "kicked/banned";
        case DISC_MSGERR: return "message error";
        case DISC_IPBAN: return "ip is banned";
        case DISC_PRIVATE: return "server is in private mode";
        case DISC_MAXCLIENTS: return "server FULL";
        case DISC_TIMEOUT: return "connection timed out";
        case DISC_OVERFLOW: return "overflow";
        case DISC_PASSWORD: return "invalid password";
        default: return NULL;
    }
}

void disconnect_client(int n, int reason)
{
    if(!clients.inrange(n) || clients[n]->type!=ST_TCPIP) return;
    server::clientdisconnect(n, reason);
    enet_peer_disconnect(clients[n]->peer, reason);
    delclient(clients[n]);
    const char *msg = disconnectreason(reason);
    string s;
    if(msg) formatstring(s)("client (%s) disconnected because: %s", (const char*)clients[n]->hostname, msg);
    else formatstring(s)("client (%s) disconnected", (const char*)clients[n]->hostname);
    logoutf("%s", s);
    server::sendservmsg(s);
}

void kicknonlocalclients(int reason)
{
    loopv(clients) if(clients[i]->type==ST_TCPIP) disconnect_client(i, reason);
}

void process(ENetPacket *packet, int sender, int chan)   // sender may be -1
{
    packetbuf p(packet);
    server::parsepacket(sender, chan, p);
    if(p.overread()) { disconnect_client(sender, DISC_EOP); return; }
}

void localclienttoserver(int chan, ENetPacket *packet)
{
    client *c = NULL;
    loopv(clients) if(clients[i]->type==ST_LOCAL) { c = clients[i]; break; }
    if(c) process(packet, c->num, chan);
}

#ifdef STANDALONE
bool resolverwait(const char *name, ENetAddress *address)
{
    return enet_address_set_host(address, name) >= 0;
}

int connectwithtimeout(ENetSocket sock, const char *hostname, const ENetAddress &remoteaddress)
{
    return enet_socket_connect(sock, &remoteaddress);
}
#endif

ENetSocket mastersock = ENET_SOCKET_NULL;
ENetAddress masteraddress = { ENET_HOST_ANY, ENET_PORT_ANY }, serveraddress = { ENET_HOST_ANY, ENET_PORT_ANY };
int lastupdatemaster = 0, lastconnectmaster = 0, masterconnecting = 0, masterconnected = 0;
vector<char> masterout, masterin;
int masteroutpos = 0, masterinpos = 0;
VARN(updatemaster, allowupdatemaster, 0, 1, 1);

void disconnectmaster()
{
    if(mastersock != ENET_SOCKET_NULL) 
    {
        server::masterdisconnected();
        enet_socket_destroy(mastersock);
        mastersock = ENET_SOCKET_NULL;
    }

    masterout.setsize(0);
    masterin.setsize(0);
    masteroutpos = masterinpos = 0;

    masteraddress.host = ENET_HOST_ANY;
    masteraddress.port = ENET_PORT_ANY;

    lastupdatemaster = masterconnecting = masterconnected = 0;
}

SVARF(mastername, server::defaultmaster(), disconnectmaster());
VARF(masterport, 1, server::masterport(), 0xFFFF, disconnectmaster());

ENetSocket connectmaster(bool wait)
{
    if(!mastername[0]) return ENET_SOCKET_NULL;
    if(masteraddress.host == ENET_HOST_ANY)
    {
        if(isdedicatedserver()) logoutf("looking up %s...", (const char*)mastername);
        masteraddress.port = masterport;
        if(!resolverwait(mastername, &masteraddress)) return ENET_SOCKET_NULL;
    }
    ENetSocket sock = enet_socket_create(ENET_SOCKET_TYPE_STREAM);
    if(sock == ENET_SOCKET_NULL)
    {
        if(isdedicatedserver()) logoutf("could not open master server socket");
        return ENET_SOCKET_NULL;
    }
    if(wait || serveraddress.host == ENET_HOST_ANY || !enet_socket_bind(sock, &serveraddress))
    {
        enet_socket_set_option(sock, ENET_SOCKOPT_NONBLOCK, 1);
        if(wait)
        {
            if(!connectwithtimeout(sock, mastername, masteraddress)) return sock;
        }
        else if(!enet_socket_connect(sock, &masteraddress)) return sock;
    }
    enet_socket_destroy(sock);
    if(isdedicatedserver()) logoutf("could not connect to master server");
    return ENET_SOCKET_NULL;
}

bool requestmaster(const char *req)
{
    if(mastersock == ENET_SOCKET_NULL)
    {
        mastersock = connectmaster(false);
        if(mastersock == ENET_SOCKET_NULL) return false;
        lastconnectmaster = masterconnecting = totalmillis ? totalmillis : 1;
    }

    masterout.put(req, strlen(req));
    return true;
}

bool requestmasterf(const char *fmt, ...)
{
    defvformatstring(req, fmt, fmt);
    return requestmaster(req);
}

void processmasterinput()
{
    if(masterinpos >= masterin.length()) return;

    char *input = &masterin[masterinpos], *end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    while(end)
    {
        *end++ = '\0';

        {
            std::string buff = input;
            {
                std::string& input = buff;
                if(spaghetti::simplehook(spaghetti::hotstring::masterin, input)) goto nextinput;
            }
            const char *input = buff.c_str(), *end = buff.c_str() + buff.size();
            const char *args = input;
            while(args < end && !iscubespace(*args)) args++;
            int cmdlen = args - input;
            while(args < end && iscubespace(*args)) args++;

            if(!strncmp(input, "failreg", cmdlen))
                conoutf(CON_ERROR, "master server registration failed: %s", args);
            else if(!strncmp(input, "succreg", cmdlen))
                conoutf("master server registration succeeded");
            else server::processmasterinput(input, cmdlen, args);
        }

        nextinput:
        masterinpos = end - masterin.getbuf();
        input = end;
        end = (char *)memchr(input, '\n', masterin.length() - masterinpos);
    } 

    if(masterinpos >= masterin.length())
    {
        masterin.setsize(0);
        masterinpos = 0;
    }
}

void flushmasteroutput()
{
    if(masterconnecting && totalmillis - masterconnecting >= 60000)
    {
        logoutf("could not connect to master server");
        disconnectmaster();
    }
    if(masterout.empty() || !masterconnected) return;

    ENetBuffer buf;
    buf.data = &masterout[masteroutpos];
    buf.dataLength = masterout.length() - masteroutpos;
    int sent = enet_socket_send(mastersock, NULL, &buf, 1);
    if(sent >= 0)
    {
        masteroutpos += sent;
        if(masteroutpos >= masterout.length())
        {
            masterout.setsize(0);
            masteroutpos = 0;
        }
    }
    else disconnectmaster();
}

void flushmasterinput()
{
    if(masterin.length() >= masterin.capacity())
        masterin.reserve(4096);

    ENetBuffer buf;
    buf.data = masterin.getbuf() + masterin.length();
    buf.dataLength = masterin.capacity() - masterin.length();
    int recv = enet_socket_receive(mastersock, NULL, &buf, 1);
    if(recv > 0)
    {
        masterin.advance(recv);
        processmasterinput();
    }
    else disconnectmaster();
}

static ENetAddress pongaddr, localpongaddr;

void sendserverinforeply(ucharbuf &p)
{
    ENetBuffer buf;
    buf.data = p.buf;
    buf.dataLength = p.length();
    enet_socket_send_local(pongsock, &pongaddr, &buf, 1, &localpongaddr);
}

#define MAXPINGDATA 32

void checkserversockets()        // reply all server info requests
{
    static ENetSocketSet readset, writeset;
    ENET_SOCKETSET_EMPTY(readset);
    ENET_SOCKETSET_EMPTY(writeset);
    ENetSocket maxsock = pongsock;
    ENET_SOCKETSET_ADD(readset, pongsock);
    if(mastersock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, mastersock);
        ENET_SOCKETSET_ADD(readset, mastersock);
        if(!masterconnected) ENET_SOCKETSET_ADD(writeset, mastersock);
    }
    if(lansock != ENET_SOCKET_NULL)
    {
        maxsock = max(maxsock, lansock);
        ENET_SOCKETSET_ADD(readset, lansock);
    }
    if(enet_socketset_select(maxsock, &readset, &writeset, 0) <= 0) return;

    ENetBuffer buf;
    uchar pong[MAXTRANS];
    loopi(2)
    {
        ENetSocket sock = i ? lansock : pongsock;
        if(sock == ENET_SOCKET_NULL || !ENET_SOCKETSET_CHECK(readset, sock)) continue;

        buf.data = pong;
        buf.dataLength = sizeof(pong);
        int len = enet_socket_receive_local(sock, &pongaddr, &buf, 1, &localpongaddr);
        ucharbuf req(pong, len), p(pong, sizeof(pong));
        p.len += len;
        const bool lan = i;
        if(spaghetti::simplehook(spaghetti::hotstring::ping, req, p, lan)) continue;
        if(len < 0 || len > MAXPINGDATA) continue;
        server::serverinforeply(req, p);
    }

    if(mastersock != ENET_SOCKET_NULL)
    {
        if(!masterconnected)
        {
            if(ENET_SOCKETSET_CHECK(readset, mastersock) || ENET_SOCKETSET_CHECK(writeset, mastersock)) 
            { 
                int error = 0;
                if(enet_socket_get_option(mastersock, ENET_SOCKOPT_ERROR, &error) < 0 || error)
                {
                    logoutf("could not connect to master server");
                    disconnectmaster();
                }
                else
                {
                    masterconnecting = 0; 
                    masterconnected = totalmillis ? totalmillis : 1; 
                    server::masterconnected(); 
                }
            }
        }
        if(mastersock != ENET_SOCKET_NULL && ENET_SOCKETSET_CHECK(readset, mastersock)) flushmasterinput();
    }
}

VAR(serveruprate, 0, 0, INT_MAX);
SVAR(serverip, "");
VARF(serverport, 0, server::serverport(), 0xFFFF-1, { if(!serverport) serverport = server::serverport(); });

#ifdef STANDALONE
int curtime = 0, lastmillis = 0, elapsedtime = 0, totalmillis = 0;
#endif

void updatemasterserver()
{
    if(!masterconnected && lastconnectmaster && totalmillis-lastconnectmaster <= 5*60*1000) return;
    if(mastername[0] && allowupdatemaster) requestmasterf("regserv %d\n", serverport);
    lastupdatemaster = totalmillis ? totalmillis : 1;
}

uint totalsecs = 0;

void updatetime()
{
    static int lastsec = 0;
    if(totalmillis - lastsec >= 1000) 
    {
        int cursecs = (totalmillis - lastsec) / 1000;
        totalsecs += cursecs;
        lastsec += cursecs * 1000;
    }
}

void serverslice(bool dedicated, uint timeout)   // main server update, called from main loop in sp, or from below in dedicated server
{
    if(!serverhost) 
    {
        server::serverupdate();
        server::sendpackets();
        return;
    }
       
    // below is network only

    if(dedicated) 
    {
        int millis = (int)enet_time_get();
        elapsedtime = millis - totalmillis;
        static int timeerr = 0;
        int scaledtime = server::scaletime(elapsedtime) + timeerr;
        curtime = scaledtime/100;
        timeerr = scaledtime%100;
        if(server::ispaused()) curtime = 0;
        lastmillis += curtime;
        totalmillis = millis;
        updatetime();
    }
    spaghetti::simpleconstevent(spaghetti::hotstring::tick);
    server::serverupdate();

    flushmasteroutput();
    checkserversockets();

    if(!lastupdatemaster || totalmillis-lastupdatemaster>60*60*1000)       // send alive signal to masterserver every hour of uptime
        updatemasterserver();
    
    if(totalmillis-laststatus>60*1000)   // display bandwidth stats, useful for server ops
    {
        laststatus = totalmillis;     
        if(nonlocalclients || serverhost->totalSentData || serverhost->totalReceivedData) logoutf("status: %d remote clients, %.1f send, %.1f rec (K/sec)", nonlocalclients, serverhost->totalSentData/60.0f/1024, serverhost->totalReceivedData/60.0f/1024);
        serverhost->totalSentData = serverhost->totalReceivedData = 0;
    }

    ENetEvent event;
    bool serviced = false;
    while(!serviced)
    {
        if(enet_host_check_events(serverhost, &event) <= 0)
        {
            if(enet_host_service(serverhost, &event, timeout) <= 0) break;
            serviced = true;
        }
        server::clientinfo * const ci = event.peer->data ? ((client*)event.peer->data)->info : 0;
        uchar pbuff[sizeof(packetbuf)];
        packetbuf* const p = event.type == ENET_EVENT_TYPE_RECEIVE ? new(pbuff)packetbuf(event.packet): 0;
        if(spaghetti::simplehook(spaghetti::hotstring::enetevent, event, ci, p)) continue;
        switch(event.type)
        {
            case ENET_EVENT_TYPE_CONNECT:
            {
                client &c = addclient(ST_TCPIP);
                c.peer = event.peer;
                c.peer->data = &c;
                string hn;
                copystring(c.hostname, (enet_address_get_host_ip(&c.peer->address, hn, sizeof(hn))==0) ? hn : "unknown");
                logoutf("client connected (%s)", (const char*)c.hostname);
                int reason = server::clientconnect(c.num, c.peer->address.host);
                if(reason) disconnect_client(c.num, reason);
                break;
            }
            case ENET_EVENT_TYPE_RECEIVE:
            {
                client *c = (client *)event.peer->data;
                if(c) process(event.packet, c->num, event.channelID);
                if(event.packet->referenceCount==0) enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT: 
            {
                client *c = (client *)event.peer->data;
                if(!c) break;
                logoutf("disconnected client (%s)", (const char*)c->hostname);
                server::clientdisconnect(c->num);
                delclient(c);
                break;
            }
            default:
                break;
        }
    }
    if(server::sendpackets()) enet_host_flush(serverhost);
}

void flushserver(bool force)
{
    if(server::sendpackets(force) && serverhost) enet_host_flush(serverhost);
}

#ifndef STANDALONE
void localdisconnect(bool cleanup)
{
    bool disconnected = false;
    loopv(clients) if(clients[i]->type==ST_LOCAL) 
    {
        server::localdisconnect(i);
        delclient(clients[i]);
        disconnected = true;
    }
    if(!disconnected) return;
    game::gamedisconnect(cleanup);
    mainmenu = 1;
}

void localconnect()
{
    client &c = addclient(ST_LOCAL);
    copystring(c.hostname, "local");
    game::gameconnect(false);
    server::localconnect(c.num);
}
#endif

#ifdef WIN32
#include "shellapi.h"

#define IDI_ICON1 1

static string apptip = "";
static HINSTANCE appinstance = NULL;
static ATOM wndclass = 0;
static HWND appwindow = NULL, conwindow = NULL;
static HICON appicon = NULL;
static HMENU appmenu = NULL;
static HANDLE outhandle = NULL;
static const int MAXLOGLINES = 200;
struct logline { int len; char buf[LOGSTRLEN]; };
static queue<logline, MAXLOGLINES> loglines;

static void cleanupsystemtray()
{
    NOTIFYICONDATA nid;
    memset(&nid, 0, sizeof(nid));
    nid.cbSize = sizeof(nid);
    nid.hWnd = appwindow;
    nid.uID = IDI_ICON1;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

static bool setupsystemtray(UINT uCallbackMessage)
{
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = appwindow;
	nid.uID = IDI_ICON1;
	nid.uCallbackMessage = uCallbackMessage;
	nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	nid.hIcon = appicon;
	strcpy(nid.szTip, apptip);
	if(Shell_NotifyIcon(NIM_ADD, &nid) != TRUE)
        return false;
    atexit(cleanupsystemtray);
    return true;
}

#if 0
static bool modifysystemtray()
{
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.hWnd = appwindow;
	nid.uID = IDI_ICON1;
	nid.uFlags = NIF_TIP;
	strcpy(nid.szTip, apptip);
	return Shell_NotifyIcon(NIM_MODIFY, &nid) == TRUE;
}
#endif

static void cleanupwindow()
{
	if(!appinstance) return;
	if(appmenu)
	{
		DestroyMenu(appmenu);
		appmenu = NULL;
	}
	if(wndclass)
	{
		UnregisterClass(MAKEINTATOM(wndclass), appinstance);
		wndclass = 0;
	}
}

static BOOL WINAPI consolehandler(DWORD dwCtrlType)
{
    switch(dwCtrlType)
    {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            spaghetti::quit = true;
            return TRUE;
    }
    return FALSE;
}

static void writeline(logline &line)
{
    static uchar ubuf[512];
    size_t len = strlen(line.buf), carry = 0;
    while(carry < len)
    {
        size_t numu = encodeutf8(ubuf, sizeof(ubuf), &((uchar *)line.buf)[carry], len - carry, &carry);
        DWORD written = 0;
        WriteConsole(outhandle, ubuf, numu, &written, NULL);
    }     
}

static void setupconsole()
{
	if(conwindow) return;
    if(!AllocConsole()) return;
	SetConsoleCtrlHandler(consolehandler, TRUE);
	conwindow = GetConsoleWindow();
    SetConsoleTitle(apptip);
	//SendMessage(conwindow, WM_SETICON, ICON_SMALL, (LPARAM)appicon);
	SendMessage(conwindow, WM_SETICON, ICON_BIG, (LPARAM)appicon);
    outhandle = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO coninfo;
    GetConsoleScreenBufferInfo(outhandle, &coninfo);
    coninfo.dwSize.Y = MAXLOGLINES;
    SetConsoleScreenBufferSize(outhandle, coninfo.dwSize);
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    loopv(loglines) writeline(loglines[i]);
}

enum
{
	MENU_OPENCONSOLE = 0,
	MENU_SHOWCONSOLE,
	MENU_HIDECONSOLE,
	MENU_EXIT
};

static LRESULT CALLBACK handlemessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
		case WM_APP:
			SetForegroundWindow(hWnd);
			switch(lParam)
			{
				//case WM_MOUSEMOVE:
				//	break;
				case WM_LBUTTONUP:
				case WM_RBUTTONUP:
				{
					POINT pos;
					GetCursorPos(&pos);
					TrackPopupMenu(appmenu, TPM_CENTERALIGN|TPM_BOTTOMALIGN|TPM_RIGHTBUTTON, pos.x, pos.y, 0, hWnd, NULL);
					PostMessage(hWnd, WM_NULL, 0, 0);
					break;
				}
			}
			return 0;
		case WM_COMMAND:
			switch(LOWORD(wParam))
			{
                case MENU_OPENCONSOLE:
					setupconsole();
					if(conwindow) ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_HIDECONSOLE, "Hide Console");
                    break;
				case MENU_SHOWCONSOLE:
					ShowWindow(conwindow, SW_SHOWNORMAL);
					ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_HIDECONSOLE, "Hide Console"); 
					break;
				case MENU_HIDECONSOLE:
					ShowWindow(conwindow, SW_HIDE);
					ModifyMenu(appmenu, 0, MF_BYPOSITION|MF_STRING, MENU_SHOWCONSOLE, "Show Console");
					break;
				case MENU_EXIT:
					PostMessage(hWnd, WM_CLOSE, 0, 0);
					break;
			}
			return 0;
		case WM_CLOSE:
			PostQuitMessage(0);
			return 0;
	}
	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

static void setupwindow(const char *title)
{
	copystring(apptip, title);
	//appinstance = GetModuleHandle(NULL);
	if(!appinstance) fatal("failed getting application instance");
	appicon = LoadIcon(appinstance, MAKEINTRESOURCE(IDI_ICON1));//(HICON)LoadImage(appinstance, MAKEINTRESOURCE(IDI_ICON1), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE);
	if(!appicon) fatal("failed loading icon");

	appmenu = CreatePopupMenu();
	if(!appmenu) fatal("failed creating popup menu");
    AppendMenu(appmenu, MF_STRING, MENU_OPENCONSOLE, "Open Console");
    AppendMenu(appmenu, MF_SEPARATOR, 0, NULL);
	AppendMenu(appmenu, MF_STRING, MENU_EXIT, "Exit");
	//SetMenuDefaultItem(appmenu, 0, FALSE);

	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.hCursor = NULL; //LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = appicon;
	wc.lpszMenuName = NULL;
	wc.lpszClassName = title;
	wc.style = 0;
	wc.hInstance = appinstance;
	wc.lpfnWndProc = handlemessages;
	wc.cbWndExtra = 0;
	wc.cbClsExtra = 0;
	wndclass = RegisterClass(&wc);
	if(!wndclass) fatal("failed registering window class");
	
	appwindow = CreateWindow(MAKEINTATOM(wndclass), title, 0, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, HWND_MESSAGE, NULL, appinstance, NULL);
	if(!appwindow) fatal("failed creating window");

	atexit(cleanupwindow);

    if(!setupsystemtray(WM_APP)) fatal("failed adding to system tray");
}

static char *parsecommandline(const char *src, vector<char *> &args)
{
    char *buf = new char[strlen(src) + 1], *dst = buf;
    for(;;)
    {
        while(isspace(*src)) src++;
        if(!*src) break;
        args.add(dst);
		for(bool quoted = false; *src && (quoted || !isspace(*src)); src++)
        {
            if(*src != '"') *dst++ = *src;
			else if(dst > buf && src[-1] == '\\') dst[-1] = '"';
			else quoted = !quoted;
		}
		*dst++ = '\0';
    }
    args.add(NULL);
    return buf;
}
                

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
{
    vector<char *> args;
    char *buf = parsecommandline(GetCommandLine(), args);
	appinstance = hInst;
#ifdef STANDALONE
    int standalonemain(int argc, char **argv);
    int status = standalonemain(args.length()-1, args.getbuf());
    #define main standalonemain
#else
    SDL_SetModuleHandle(hInst);
    int status = SDL_main(args.length()-1, args.getbuf());
#endif
    delete[] buf;
    exit(status);
    return 0;
}

void logoutfv(const char *fmt, va_list args)
{
    if(appwindow)
    {
        logline &line = loglines.add();
        vformatstring(line.buf, fmt, args, sizeof(line.buf));
        if(logfile) writelog(logfile, line.buf);
        line.len = min(strlen(line.buf), sizeof(line.buf)-2);
        line.buf[line.len++] = '\n';
        line.buf[line.len] = '\0';
        if(outhandle) writeline(line);
    }
    else if(logfile) writelogv(logfile, fmt, args);
}

#else

void logoutfv(const char *fmt, va_list args)
{
    FILE *f = getlogfile();
    if(f) writelogv(f, fmt, args);
}

#endif

static bool dedicatedserver = false;

bool isdedicatedserver() { return dedicatedserver; }

void rundedicatedserver()
{
    dedicatedserver = true;
    logoutf("dedicated server started, waiting for clients...");
#ifdef WIN32
    SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	while(!spaghetti::quit)
	{
		MSG msg;
		while(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{
			    spaghetti::quit = true;
			    return;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		serverslice(true, 5);
	}
#else
    while(!spaghetti::quit) serverslice(true, 5);
#endif
    dedicatedserver = false;
}

bool servererror(bool dedicated, const char *desc)
{
#ifndef STANDALONE
    if(!dedicated)
    {
        conoutf(CON_ERROR, "%s", desc);
        cleanupserver();
    }
    else
#endif
    spaghetti::fini(true);
        fatal("%s", desc);
    return false;
}
  
bool setuplistenserver(bool dedicated)
{
    ENetAddress address = { ENET_HOST_ANY, enet_uint16(serverport <= 0 ? server::serverport() : serverport) };
    if(*serverip)
    {
        if(enet_address_set_host(&address, serverip)<0) conoutf(CON_WARN, "WARNING: server ip not resolved");
        else serveraddress.host = address.host;
    }
    serverhost = enet_host_create(&address, ENET_PROTOCOL_MAXIMUM_PEER_ID, server::numchannels(), 0, serveruprate);
    if(!serverhost) return servererror(dedicated, "could not create server host");
    serverhost->duplicatePeers = maxdupclients ? maxdupclients : MAXCLIENTS;
    address.port = server::serverinfoport(serverport > 0 ? serverport : -1);
    pongsock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(pongsock != ENET_SOCKET_NULL && enet_socket_bind(pongsock, &address) < 0)
    {
        enet_socket_destroy(pongsock);
        pongsock = ENET_SOCKET_NULL;
    }
    if(pongsock == ENET_SOCKET_NULL) return servererror(dedicated, "could not create server info socket");
    else enet_socket_set_option(pongsock, ENET_SOCKOPT_NONBLOCK, 1);
    address.port = server::laninfoport();
    lansock = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if(lansock != ENET_SOCKET_NULL && (enet_socket_set_option(lansock, ENET_SOCKOPT_REUSEADDR, 1) < 0 || enet_socket_bind(lansock, &address) < 0))
    {
        enet_socket_destroy(lansock);
        lansock = ENET_SOCKET_NULL;
    }
    if(lansock == ENET_SOCKET_NULL) conoutf(CON_WARN, "WARNING: could not create LAN server info socket");
    else enet_socket_set_option(lansock, ENET_SOCKOPT_NONBLOCK, 1);
    return true;
}

void initserver(bool listen, bool dedicated)
{
    if(dedicated) 
    {
#ifdef WIN32
        setupwindow("Cube 2: Sauerbraten server");
#endif
    }
    
    spaghetti::init();

    if(listen) setuplistenserver(dedicated);

    server::serverinit();

    if(listen)
    {
        dedicatedserver = dedicated;
        updatemasterserver();
        if(dedicated){
            rundedicatedserver();
            spaghetti::fini(false);
            cleanupserver();
            closelogfile();
            spaghetti::quit = false;
        }
#ifndef STANDALONE
        else conoutf("listen server started");
#endif
    }
}

#ifndef STANDALONE
void startlistenserver(int *usemaster)
{
    if(serverhost) { conoutf(CON_ERROR, "listen server is already running"); return; }

    allowupdatemaster = *usemaster>0 ? 1 : 0;
 
    if(!setuplistenserver(false)) return;
    
    updatemasterserver();
   
    conoutf("listen server started for %d clients%s", maxclients, allowupdatemaster ? " and listed with master server" : ""); 
}
COMMAND(startlistenserver, "i");

void stoplistenserver()
{
    if(!serverhost) { conoutf(CON_ERROR, "listen server is not running"); return; }

    kicknonlocalclients();
    enet_host_flush(serverhost);
    cleanupserver();

    conoutf("listen server stopped");
}
COMMAND(stoplistenserver, "");
#endif

bool serveroption(char *opt)
{
    switch(opt[1])
    {
        case 'u': setvar("serveruprate", atoi(opt+2)); return true;
        case 'c': setvar("maxclients", atoi(opt+2)); return true;
        case 'i': setsvar("serverip", opt+2); return true;
        case 'j': setvar("serverport", atoi(opt+2)); return true; 
        case 'm': setsvar("mastername", opt+2); setvar("updatemaster", mastername[0] ? 1 : 0); return true;
#ifdef STANDALONE
        case 'q': logoutf("Using home directory: %s", opt); sethomedir(opt+2); return true;
        case 'k': logoutf("Adding package directory: %s", opt); addpackagedir(opt+2); return true;
        case 'g': logoutf("Setting log file: %s", opt); setlogfile(opt+2); return true;
#endif
        default: return false;
    }
}

vector<const char *> gameargs;

#ifdef STANDALONE
int main(int argc, char **argv)
{   
    setlogfile(NULL);
    if(enet_initialize()<0) fatal("Unable to initialise network module");
    atexit(enet_deinitialize);
    enet_time_set(0);
    for(int i = 1; i<argc; i++) if(argv[i][0]!='-' || !serveroption(argv[i])) gameargs.add(argv[i]);
    game::parseoptions(gameargs);
    initserver(true, true);
    return EXIT_SUCCESS;
}
#endif

namespace luabridge{
enumStack(ENetSocketType);
enumStack(ENetSocketWait);
enumStack(ENetSocketOption);
enumStack(ENetSocketShutdown);
enumStack(ENetPacketFlag);
enumStack(ENetPeerState);
enumStack(ENetEventType);
enumStack(ENetProtocolCommand);
enumStack(ENetProtocolFlag);
}

template<> ucharbuf& ucharbuf::putint(int n){ ::putint(*this, n); return *this; }
packetbuf& packetbuf::putint(int n){ ::putint(*this, n); return *this; }
template<> vector<uchar>& vector<uchar>::putint(int n){ ::putint(*this, n); return *this; }
template<> ucharbuf& ucharbuf::putuint(int n){ ::putuint(*this, n); return *this; }
packetbuf& packetbuf::putuint(int n){ ::putuint(*this, n); return *this; }
template<> vector<uchar>& vector<uchar>::putuint(int n){ ::putuint(*this, n); return *this; }
template<> ucharbuf& ucharbuf::putfloat(float f){ ::putfloat(*this, f); return *this; }
packetbuf& packetbuf::putfloat(float f){ ::putfloat(*this, f); return *this; }
template<> vector<uchar>& vector<uchar>::putfloat(float f){ ::putfloat(*this, f); return *this; }
template<> ucharbuf& ucharbuf::sendstring(const char *t){ ::sendstring(t, *this); return *this; }
packetbuf& packetbuf::sendstring(const char *t){ ::sendstring(t, *this); return *this; }
template<> vector<uchar>& vector<uchar>::sendstring(const char *t){ ::sendstring(t, *this); return *this; }

template<> int ucharbuf::getint(){ return ::getint(*this); }
template<> int ucharbuf::getuint(){ return ::getuint(*this); }
template<> float ucharbuf::getfloat(){ return ::getfloat(*this); }
template<> std::string ucharbuf::getstring(){
    std::string ret;
    while(char ch = getint()){
        if(overread()) break;
        ret.push_back(ch);
    }
    return ret;
}

namespace spaghetti{

using namespace luabridge;

void bindengine(){
    //enet
    using eunseqwnd = lua_arrayproxy<decltype(ENetPeer().unsequencedWindow)>;
    using epeers = lua_arrayproxy<decltype(ENetHost().peers)>;
#define epacket lua_buff_type(&ENetPacket::data, &ENetPacket::dataLength, false)
#define ebuff lua_buff_type(&ENetBuffer::data, &ENetBuffer::dataLength)
    bindArrayProxy<eunseqwnd::type>("engine");
    bindArrayProxy<epeers::type>("engine");
    getGlobalNamespace(L).beginNamespace("engine")
        .beginClass<ENetAddress>("ENetAddress")
            .template addConstructor<void(*)()>()
            .addData("host", &ENetAddress::host)
            .addData("port", &ENetAddress::port)
        .endClass()
        .beginClass<ENetPacket>("ENetPacket")
            .addData("referenceCount", &ENetPacket::referenceCount)
            .addData("flags", &ENetPacket::flags)
            .addProperty("data", &epacket::getBuffer)
            .addProperty("dataLength", &epacket::getLength)
        .endClass()
        .beginClass<ENetBuffer>("ENetBuffer")
            .template addConstructor<void(*)()>()
            .addProperty("data", &ebuff::getBuffer, &ebuff::setBuffer)
            .addProperty("dataLength", &ebuff::getLength, &ebuff::setLength)
        .endClass()
        .beginClass<ENetSocketSet>("ENetSocketSet")
            .template addConstructor<void(*)()>()
        .endClass()
        .beginClass<ENetPeer>("ENetPeer")
            .addData("host", &ENetPeer::host)
            .addData("outgoingPeerID", &ENetPeer::outgoingPeerID)
            .addData("incomingPeerID", &ENetPeer::incomingPeerID)
            .addData("connectID", &ENetPeer::connectID)
            .addData("outgoingSessionID", &ENetPeer::outgoingSessionID)
            .addData("incomingSessionID", &ENetPeer::incomingSessionID)
            .addData("address", &ENetPeer::address)
            .addData("state", &ENetPeer::state)
            .addData("channelCount", &ENetPeer::channelCount)
            .addData("incomingBandwidth", &ENetPeer::incomingBandwidth)
            .addData("outgoingBandwidth", &ENetPeer::outgoingBandwidth)
            .addData("incomingDataTotal", &ENetPeer::incomingDataTotal)
            .addData("outgoingDataTotal", &ENetPeer::outgoingDataTotal)
            .addData("lastSendTime", &ENetPeer::lastSendTime)
            .addData("lastReceiveTime", &ENetPeer::lastReceiveTime)
            .addData("packetsSent", &ENetPeer::packetsSent)
            .addData("packetsLost", &ENetPeer::packetsLost)
            .addData("packetLoss", &ENetPeer::packetLoss)
            .addData("packetLossVariance", &ENetPeer::packetLossVariance)
            .addData("packetThrottle", &ENetPeer::packetThrottle)
            .addData("packetThrottleLimit", &ENetPeer::packetThrottleLimit)
            .addData("packetThrottleCounter", &ENetPeer::packetThrottleCounter)
            .addData("packetThrottleEpoch", &ENetPeer::packetThrottleEpoch)
            .addData("packetThrottleAcceleration", &ENetPeer::packetThrottleAcceleration)
            .addData("packetThrottleDeceleration", &ENetPeer::packetThrottleDeceleration)
            .addData("packetThrottleInterval", &ENetPeer::packetThrottleInterval)
            .addData("pingInterval", &ENetPeer::pingInterval)
            .addData("timeoutLimit", &ENetPeer::timeoutLimit)
            .addData("timeoutMinimum", &ENetPeer::timeoutMinimum)
            .addData("timeoutMaximum", &ENetPeer::timeoutMaximum)
            .addData("lastRoundTripTime", &ENetPeer::lastRoundTripTime)
            .addData("lowestRoundTripTime", &ENetPeer::lowestRoundTripTime)
            .addData("lastRoundTripTimeVariance", &ENetPeer::lastRoundTripTimeVariance)
            .addData("highestRoundTripTimeVariance", &ENetPeer::highestRoundTripTimeVariance)
            .addData("roundTripTime", &ENetPeer::roundTripTime)
            .addData("roundTripTimeVariance", &ENetPeer::roundTripTimeVariance)
            .addData("mtu", &ENetPeer::mtu)
            .addData("windowSize", &ENetPeer::windowSize)
            .addData("reliableDataInTransit", &ENetPeer::reliableDataInTransit)
            .addData("outgoingReliableSequenceNumber", &ENetPeer::outgoingReliableSequenceNumber)
            .addData("incomingUnsequencedGroup", &ENetPeer::incomingUnsequencedGroup)
            .addData("outgoingUnsequencedGroup", &ENetPeer::outgoingUnsequencedGroup)
            .addProperty("unsequencedWindow", &eunseqwnd::getter<ENetPeer, &ENetPeer::unsequencedWindow>)
            .addData("totalWaitingData", &ENetPeer::totalWaitingData)
            .addData("localAddress", &ENetPeer::localAddress)
        .endClass()
        .beginClass<ENetHost>("ENetHost")
            .addData("socket", &ENetHost::socket)
            .addData("address", &ENetHost::address)
            .addData("incomingBandwidth", &ENetHost::incomingBandwidth)
            .addData("outgoingBandwidth", &ENetHost::outgoingBandwidth)
            .addData("mtu", &ENetHost::mtu)
            .addData("randomSeed", &ENetHost::randomSeed)
            .addProperty("peers", &epeers::getter<ENetHost, &ENetHost::peers>)
            .addData("peerCount", &ENetHost::peerCount)
            .addData("channelLimit", &ENetHost::channelLimit)
            .addData("serviceTime", &ENetHost::serviceTime)
            .addData("totalSentData", &ENetHost::totalSentData)
            .addData("totalSentPackets", &ENetHost::totalSentPackets)
            .addData("totalReceivedData", &ENetHost::totalReceivedData)
            .addData("totalReceivedPackets", &ENetHost::totalReceivedPackets)
            .addData("connectedPeers", &ENetHost::connectedPeers)
            .addData("bandwidthLimitedPeers", &ENetHost::bandwidthLimitedPeers)
            .addData("duplicatePeers", &ENetHost::duplicatePeers)
            .addData("maximumPacketSize", &ENetHost::maximumPacketSize)
            .addData("maximumWaitingData", &ENetHost::maximumWaitingData)
            .addData("noTimeouts", &ENetHost::noTimeouts)
        .endClass()
        .beginClass<ENetEvent>("ENetEvent")
            .template addConstructor<void(*)()>()
            .addData("type", &ENetEvent::type)
            .addData("peer", &ENetEvent::peer)
            .addData("channelID", &ENetEvent::channelID)
            .addData("data", &ENetEvent::data)
            .addData("packet", &ENetEvent::packet)
        .endClass()
        .addFunction("ENET_HOST_TO_NET_16", +[](uint16_t v){
            return ENET_HOST_TO_NET_16(v);
        })
        .addFunction("ENET_NET_TO_HOST_16", +[](uint16_t v){
            return ENET_NET_TO_HOST_16(v);
        })
        .addFunction("ENET_HOST_TO_NET_32", +[](uint32_t v){
            return ENET_HOST_TO_NET_32(v);
        })
        .addFunction("ENET_NET_TO_HOST_32", +[](uint32_t v){
            return ENET_NET_TO_HOST_32(v);
        })
        .addFunction("ENET_SOCKETSET_EMPTY", +[](ENetSocketSet& v){
            ENET_SOCKETSET_EMPTY(v);
        })
        .addFunction("ENET_SOCKETSET_ADD", +[](ENetSocketSet& v, ENetSocket s){
            ENET_SOCKETSET_ADD(v, s);
        })
        .addFunction("ENET_SOCKETSET_REMOVE", +[](ENetSocketSet& v, ENetSocket s){
            ENET_SOCKETSET_REMOVE(v, s);
        })
        .addFunction("ENET_SOCKETSET_CHECK", +[](ENetSocketSet& v, ENetSocket s){
            return ENET_SOCKETSET_CHECK(v, s);
        })
        .addFunction("enet_linked_version", enet_linked_version)
        .addFunction("enet_time_get", enet_time_get)
        .addFunction("enet_time_set", enet_time_set)
        .addFunction("enet_socket_create", enet_socket_create)
        .addFunction("enet_socket_bind", enet_socket_bind)
        .addFunction("enet_socket_get_address", enet_socket_get_address)
        .addFunction("enet_socket_listen", enet_socket_listen)
        .addFunction("enet_socket_accept", enet_socket_accept)
        .addFunction("enet_socket_connect", enet_socket_connect)
        .addFunction("enet_socket_send", enet_socket_send)
        .addFunction("enet_socket_send_local", enet_socket_send_local)
        .addFunction("enet_socket_receive", enet_socket_receive)
        .addFunction("enet_socket_receive_local", enet_socket_receive_local)
        .addFunction("enet_socket_wait", enet_socket_wait)
        .addFunction("enet_socket_set_option", enet_socket_set_option)
        .addFunction("enet_socket_get_option", enet_socket_get_option)
        .addFunction("enet_socket_shutdown", enet_socket_shutdown)
        .addFunction("enet_socket_destroy", enet_socket_destroy)
        .addFunction("enet_socketset_select", enet_socketset_select)
        .addFunction("enet_address_set_host", enet_address_set_host)
        .addFunction("enet_address_get_host_ip", enet_address_get_host_ip)
        .addFunction("enet_packet_create", +[](std::string data, enet_uint32 flags){
            return enet_packet_create(data.data(), data.size(), flags);
        })
        .addFunction("enet_packet_destroy", enet_packet_destroy)
        .addFunction("enet_packet_resize", enet_packet_resize)
        .addFunction("enet_crc32", enet_crc32)
        .addFunction("enet_host_create", enet_host_create)
        .addFunction("enet_host_destroy", enet_host_destroy)
        .addFunction("enet_host_connect", enet_host_connect)
        .addFunction("enet_host_check_events", enet_host_check_events)
        .addFunction("enet_host_service", enet_host_service)
        .addFunction("enet_host_flush", enet_host_flush)
        .addFunction("enet_host_broadcast", enet_host_broadcast)
        .addFunction("enet_host_compress_with_range_coder", enet_host_compress_with_range_coder)
        .addFunction("enet_host_channel_limit", enet_host_channel_limit)
        .addFunction("enet_host_bandwidth_limit", enet_host_bandwidth_limit)
        .addFunction("enet_peer_send", enet_peer_send)
        .addFunction("enet_peer_receive", enet_peer_receive)
        .addFunction("enet_peer_ping", enet_peer_ping)
        .addFunction("enet_peer_ping_interval", enet_peer_ping_interval)
        .addFunction("enet_peer_timeout", enet_peer_timeout)
        .addFunction("enet_peer_reset", enet_peer_reset)
        .addFunction("enet_peer_disconnect", enet_peer_disconnect)
        .addFunction("enet_peer_disconnect_now", enet_peer_disconnect_now)
        .addFunction("enet_peer_disconnect_later", enet_peer_disconnect_later)
        .addFunction("enet_peer_throttle_configure", enet_peer_throttle_configure)
    .endNamespace();

    //engine
    bindVectorOf<client*>("engine");
    bindVectorOf<uchar>("engine");
#define ucharbufbinary lua_buff_type(&ucharbuf::buf, &ucharbuf::maxlen, false)
    getGlobalNamespace(L).beginNamespace("engine")
        //tools.h
        .beginClass<ucharbuf>("ucharbuf")
            .addData("len", &ucharbuf::len)
            .addData("flags", &ucharbuf::flags)
            .addProperty("buf", &ucharbufbinary::getBuffer)
            .addProperty("maxlen", &ucharbufbinary::getLength)
            .addFunction("get", (const uchar&(ucharbuf::*)())&ucharbuf::get)
            .addFunction("getbuf", &ucharbuf::getbuf)
            .addFunction("subbuf", &ucharbuf::subbuf)
            .addFunction("put", (void(ucharbuf::*)(const uchar&))&ucharbuf::put)
            .addFunction("putbuf", &ucharbuf::putbuf)
            .addFunction("offset", &ucharbuf::offset)
            .addFunction("empty", &ucharbuf::empty)
            .addFunction("length", &ucharbuf::length)
            .addFunction("remaining", &ucharbuf::remaining)
            .addFunction("overread", &ucharbuf::overread)
            .addFunction("overwrote", &ucharbuf::overwrote)
            .addFunction("forceoverread", &ucharbuf::forceoverread)
            .addFunction("putint", &ucharbuf::putint)
            .addFunction("getint", &ucharbuf::getint)
            .addFunction("putuint", &ucharbuf::putuint)
            .addFunction("getuint", &ucharbuf::getuint)
            .addFunction("sendstring", &ucharbuf::sendstring)
            .addFunction("getstring", &ucharbuf::getstring)
            .addFunction("putfloat", &ucharbuf::putfloat)
            .addFunction("getfloat", &ucharbuf::getfloat)
        .endClass()
        .deriveClass<packetbuf, ucharbuf>("packetbuf")
            .template addConstructor<void(*)(int, int)>()
            .addData("packet", &packetbuf::packet)
            .addData("growth", &packetbuf::growth)
            .addFunction("reliable", &packetbuf::reliable)
            .addFunction("resize", &packetbuf::resize)
            .addFunction("checkspace", &packetbuf::checkspace)
            .addFunction("subbuf", &packetbuf::subbuf)
            .addFunction("put", (void(packetbuf::*)(const uchar&))&packetbuf::put)
            .addFunction("putbuf", &packetbuf::putbuf)
            .addFunction("finalize", &packetbuf::lua_finalize)
            .addFunction("cleanup", &packetbuf::cleanup)
            .addFunction("putint", &packetbuf::putint)
            .addFunction("putuint", &packetbuf::putuint)
            .addFunction("sendstring", &packetbuf::sendstring)
        .endClass()
        .beginClass<vector<uchar>>(("vector<" + classname<vector<uchar>>() + ">").c_str())
            .addFunction("putbuf", &vector<uchar>::putbuf<>)
            .addFunction("putint", &vector<uchar>::putint)
            .addFunction("putuint", &vector<uchar>::putuint)
            .addFunction("sendstring", &vector<uchar>::sendstring)
        .endClass()
        .addFunction("iscubeprint", iscubeprint)
        .addFunction("iscubespace", iscubespace)
        .addFunction("iscubealpha", iscubealpha)
        .addFunction("iscubealnum", iscubealnum)
        .addFunction("iscubelower", iscubelower)
        .addFunction("iscubeupper", iscubeupper)
        .addFunction("cube2uni", cube2uni)
        .addFunction("uni2cube", uni2cube)
        .addFunction("cubelower", cubelower)
        .addFunction("cubeupper", cubeupper)
        .addFunction("decodeutf8", +[](std::string utf8){
            char* cube = new char[utf8.length() + 1];
            int written = decodeutf8((uchar*)cube, utf8.length() + 1, (const uchar*)utf8.data(), utf8.length());
            std::string ret(cube, written);
            delete[] cube;
            return ret;
        })
        .addFunction("encodeutf8", +[](const char* cube){
            std::string ret;
            static uchar ubuf[512];
            size_t len = strlen(cube), carry = 0;
            while(carry < len)
            {
                size_t numu = encodeutf8(ubuf, sizeof(ubuf)-1, &((const uchar *)cube)[carry], len - carry, &carry);
                ret.append((const char*)ubuf, numu);
            }
            return ret;
        })
        .addFunction("path", +[](const char* p){
            return std::string(path(p, true));
        })
        .addCFunction("listdir", [](lua_State* L){
            static vector<char*> dirs;
            dirs.deletearrays();
            if(!listdir(luaL_tolstring(L, 1, 0), true, 0, dirs)) return 0;
            lua_newtable(L);
            loopv(dirs){
                lua_pushstring(L, dirs[i]);
                lua_rawseti(L, -2, i + 1);
            }
            dirs.deletearrays();
            return 1;
        })
        .addCFunction("listfiles", [](lua_State* L){
            static vector<char*> files;
            files.deletearrays();
            int numdirs = listfiles(luaL_tolstring(L, 1, 0), 0, files);
            lua_newtable(L);
            loopv(files){
                lua_pushstring(L, files[i]);
                lua_rawseti(L, -2, i + 1);
            }
            files.deletearrays();
            lua_pushinteger(L, numdirs);
            return 2;
        })
        .addFunction("filtertext", +[](const char* src, bool whitespace){
            char* buff = newstring(src);
            filtertext(buff, buff, whitespace, strlen(src));
            std::string ret = buff;
            delete[] buff;
            return ret;
        })
        //geom.h
        .beginClass<vec>("vec")
            .template addConstructor<void(*)()>()
            .addData("x", &vec::x)
            .addData("y", &vec::y)
            .addData("z", &vec::z)
            .addFunction("__arrayindex", &vec::__arrayindex)
            .addFunction("__arraynewindex", &vec::__arraynewindex)
        .endClass()
        .beginClass<ivec>("ivec")
            .template addConstructor<void(*)()>()
            .addData("x", &ivec::x)
            .addData("y", &ivec::y)
            .addData("z", &ivec::z)
            .addFunction("__arrayindex", &ivec::__arrayindex)
            .addFunction("__arraynewindex", &ivec::__arraynewindex)
        .endClass()
        //ents.h
        .beginClass<entity>("entity")
            .template addConstructor<void(*)()>()
            .addData("o", &entity::o)
            .addData("attr1", &entity::attr1)
            .addData("attr2", &entity::attr2)
            .addData("attr3", &entity::attr3)
            .addData("attr4", &entity::attr4)
            .addData("attr5", &entity::attr5)
            .addData("type", &entity::type)
            .addData("reserved", &entity::reserved)
        .endClass()
        //iengine.h
        .addFunction("sendpacket", sendpacket)
        .addFunction("flushserver", flushserver)
        .addFunction("getclientinfo", getclientinfo)
        .addFunction("getclientpeer", getclientpeer)
        .addFunction("getservermtu", getservermtu)
        .addFunction("getnumclients", getnumclients)
        .addFunction("getclientip", getclientip)
        .addFunction("disconnectreason", disconnectreason)
        .addFunction("disconnect_client", disconnect_client)
        .addFunction("kicknonlocalclients", kicknonlocalclients)
        .addFunction("hasnonlocalclients", hasnonlocalclients)
        .addFunction("haslocalclients", haslocalclients)
        .addFunction("sendserverinforeply", sendserverinforeply)
        .addFunction("requestmaster", requestmaster)
        .addFunction("flushmasteroutput", flushmasteroutput)
        //server.cpp
        .beginClass<client>("client")
            .template addConstructor<void(*)()>()
            .addData("type", &client::type)
            .addData("num", &client::num)
            .addData("peer", &client::peer)
            .addData("hostname", &client::hostname)
            .addData("info", &client::info)
        .endClass()
        .addFunction("writelog", +[](const char* out){
            FILE *f = getlogfile();
            if(f) writelog(f, out);
        })
        .addFunction("setlogfile", setlogfile)
        .addFunction("addclient", addclient)
        .addFunction("delclient", delclient)
        .addFunction("process", process)
        .addVariable("serverhost", &serverhost)
        .addVariable("pongsock", &pongsock)
        .addVariable("lansock", &lansock)
        .addVariable("localclients", &localclients)
        .addVariable("nonlocalclients", &nonlocalclients)
        .addVariable("mastersock", &mastersock)
        .addVariable("masteraddress", &masteraddress)
        .addVariable("curtime", &curtime)
        .addVariable("lastmillis", &lastmillis)
        .addVariable("elapsedtime", &elapsedtime)
        .addVariable("totalmillis", &totalmillis)
        .addVariable("totalsecs", &totalsecs)
    .endNamespace();
    bindVectorOf<entity>("server");
    //export constants as simple variables, don't try to enforce read only.
#define addEnum(n)    lua_pushstring(L, #n); lua_pushnumber(L, n); lua_rawset(L, -3)
    lua_getglobal(L, "engine");
    //enet
    addEnum(ENET_SOCKET_NULL);
    addEnum(ENET_PROTOCOL_MINIMUM_MTU);
    addEnum(ENET_PROTOCOL_MAXIMUM_MTU);
    addEnum(ENET_PROTOCOL_MAXIMUM_PACKET_COMMANDS);
    addEnum(ENET_PROTOCOL_MINIMUM_WINDOW_SIZE);
    addEnum(ENET_PROTOCOL_MAXIMUM_WINDOW_SIZE);
    addEnum(ENET_PROTOCOL_MINIMUM_CHANNEL_COUNT);
    addEnum(ENET_PROTOCOL_MAXIMUM_CHANNEL_COUNT);
    addEnum(ENET_PROTOCOL_MAXIMUM_PEER_ID);
    addEnum(ENET_PROTOCOL_MAXIMUM_FRAGMENT_COUNT);
    addEnum(ENET_PROTOCOL_COMMAND_NONE);
    addEnum(ENET_PROTOCOL_COMMAND_ACKNOWLEDGE);
    addEnum(ENET_PROTOCOL_COMMAND_CONNECT);
    addEnum(ENET_PROTOCOL_COMMAND_VERIFY_CONNECT);
    addEnum(ENET_PROTOCOL_COMMAND_DISCONNECT);
    addEnum(ENET_PROTOCOL_COMMAND_PING);
    addEnum(ENET_PROTOCOL_COMMAND_SEND_RELIABLE);
    addEnum(ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE);
    addEnum(ENET_PROTOCOL_COMMAND_SEND_FRAGMENT);
    addEnum(ENET_PROTOCOL_COMMAND_SEND_UNSEQUENCED);
    addEnum(ENET_PROTOCOL_COMMAND_BANDWIDTH_LIMIT);
    addEnum(ENET_PROTOCOL_COMMAND_THROTTLE_CONFIGURE);
    addEnum(ENET_PROTOCOL_COMMAND_SEND_UNRELIABLE_FRAGMENT);
    addEnum(ENET_PROTOCOL_COMMAND_COUNT);
    addEnum(ENET_PROTOCOL_COMMAND_MASK);
    addEnum(ENET_PROTOCOL_COMMAND_FLAG_ACKNOWLEDGE);
    addEnum(ENET_PROTOCOL_COMMAND_FLAG_UNSEQUENCED);
    addEnum(ENET_PROTOCOL_HEADER_FLAG_COMPRESSED);
    addEnum(ENET_PROTOCOL_HEADER_FLAG_SENT_TIME);
    addEnum(ENET_PROTOCOL_HEADER_SESSION_MASK);
    addEnum(ENET_PROTOCOL_HEADER_SESSION_SHIFT);
    addEnum(ENET_VERSION_MAJOR);
    addEnum(ENET_VERSION_MINOR);
    addEnum(ENET_VERSION_PATCH);
    addEnum(ENET_SOCKET_TYPE_STREAM);
    addEnum(ENET_SOCKET_TYPE_DATAGRAM);
    addEnum(ENET_SOCKET_WAIT_NONE);
    addEnum(ENET_SOCKET_WAIT_SEND);
    addEnum(ENET_SOCKET_WAIT_RECEIVE);
    addEnum(ENET_SOCKET_WAIT_INTERRUPT);
    addEnum(ENET_SOCKOPT_NONBLOCK);
    addEnum(ENET_SOCKOPT_BROADCAST);
    addEnum(ENET_SOCKOPT_RCVBUF);
    addEnum(ENET_SOCKOPT_SNDBUF);
    addEnum(ENET_SOCKOPT_REUSEADDR);
    addEnum(ENET_SOCKOPT_RCVTIMEO);
    addEnum(ENET_SOCKOPT_SNDTIMEO);
    addEnum(ENET_SOCKOPT_PKTINFO);
    addEnum(ENET_SOCKOPT_ERROR);
    addEnum(ENET_SOCKOPT_NODELAY);
    addEnum(ENET_SOCKET_SHUTDOWN_READ);
    addEnum(ENET_SOCKET_SHUTDOWN_WRITE);
    addEnum(ENET_SOCKET_SHUTDOWN_READ_WRITE);
    addEnum(ENET_HOST_ANY);
    addEnum(ENET_HOST_BROADCAST);
    addEnum(ENET_PORT_ANY);
    addEnum(ENET_PACKET_FLAG_RELIABLE);
    addEnum(ENET_PACKET_FLAG_UNSEQUENCED);
    addEnum(ENET_PACKET_FLAG_NO_ALLOCATE);
    addEnum(ENET_PACKET_FLAG_UNRELIABLE_FRAGMENT);
    addEnum(ENET_PACKET_FLAG_SENT);
    addEnum(ENET_PEER_STATE_DISCONNECTED);
    addEnum(ENET_PEER_STATE_CONNECTING);
    addEnum(ENET_PEER_STATE_ACKNOWLEDGING_CONNECT);
    addEnum(ENET_PEER_STATE_CONNECTION_PENDING);
    addEnum(ENET_PEER_STATE_CONNECTION_SUCCEEDED);
    addEnum(ENET_PEER_STATE_CONNECTED);
    addEnum(ENET_PEER_STATE_DISCONNECT_LATER);
    addEnum(ENET_PEER_STATE_DISCONNECTING);
    addEnum(ENET_PEER_STATE_ACKNOWLEDGING_DISCONNECT);
    addEnum(ENET_PEER_STATE_ZOMBIE);
    addEnum(ENET_BUFFER_MAXIMUM);
    addEnum(ENET_HOST_RECEIVE_BUFFER_SIZE);
    addEnum(ENET_HOST_SEND_BUFFER_SIZE);
    addEnum(ENET_HOST_BANDWIDTH_THROTTLE_INTERVAL);
    addEnum(ENET_HOST_DEFAULT_MTU);
    addEnum(ENET_HOST_DEFAULT_MAXIMUM_PACKET_SIZE);
    addEnum(ENET_HOST_DEFAULT_MAXIMUM_WAITING_DATA);
    addEnum(ENET_PEER_DEFAULT_ROUND_TRIP_TIME);
    addEnum(ENET_PEER_DEFAULT_PACKET_THROTTLE);
    addEnum(ENET_PEER_PACKET_THROTTLE_SCALE);
    addEnum(ENET_PEER_PACKET_THROTTLE_COUNTER);
    addEnum(ENET_PEER_PACKET_THROTTLE_ACCELERATION);
    addEnum(ENET_PEER_PACKET_THROTTLE_DECELERATION);
    addEnum(ENET_PEER_PACKET_THROTTLE_INTERVAL);
    addEnum(ENET_PEER_PACKET_LOSS_SCALE);
    addEnum(ENET_PEER_PACKET_LOSS_INTERVAL);
    addEnum(ENET_PEER_WINDOW_SIZE_SCALE);
    addEnum(ENET_PEER_TIMEOUT_LIMIT);
    addEnum(ENET_PEER_TIMEOUT_MINIMUM);
    addEnum(ENET_PEER_TIMEOUT_MAXIMUM);
    addEnum(ENET_PEER_PING_INTERVAL);
    addEnum(ENET_PEER_UNSEQUENCED_WINDOWS);
    addEnum(ENET_PEER_UNSEQUENCED_WINDOW_SIZE);
    addEnum(ENET_PEER_FREE_UNSEQUENCED_WINDOWS);
    addEnum(ENET_PEER_RELIABLE_WINDOWS);
    addEnum(ENET_PEER_RELIABLE_WINDOW_SIZE);
    addEnum(ENET_PEER_FREE_RELIABLE_WINDOWS);
    addEnum(ENET_EVENT_TYPE_NONE);
    addEnum(ENET_EVENT_TYPE_CONNECT);
    addEnum(ENET_EVENT_TYPE_DISCONNECT);
    addEnum(ENET_EVENT_TYPE_RECEIVE);
    //engine
    addEnum(ID_VAR);
    addEnum(ID_FVAR);
    addEnum(ID_SVAR);
    addEnum(ET_EMPTY);
    addEnum(ET_LIGHT);
    addEnum(ET_MAPMODEL);
    addEnum(ET_PLAYERSTART);
    addEnum(ET_ENVMAP);
    addEnum(ET_PARTICLES);
    addEnum(ET_SOUND);
    addEnum(ET_SPOTLIGHT);
    addEnum(ET_GAMESPECIFIC);
    addEnum(MAXENTS);
    addEnum(CS_ALIVE);
    addEnum(CS_DEAD);
    addEnum(CS_SPAWNING);
    addEnum(CS_LAGGED);
    addEnum(CS_EDITING);
    addEnum(CS_SPECTATOR);
    addEnum(PHYS_FLOAT);
    addEnum(PHYS_FALL);
    addEnum(PHYS_SLIDE);
    addEnum(PHYS_SLOPE);
    addEnum(PHYS_FLOOR);
    addEnum(PHYS_STEP_UP);
    addEnum(PHYS_STEP_DOWN);
    addEnum(PHYS_BOUNCE);
    addEnum(MAXCLIENTS);
    addEnum(MAXTRANS);
    addEnum(DISC_NONE);
    addEnum(DISC_EOP);
    addEnum(DISC_LOCAL);
    addEnum(DISC_KICK);
    addEnum(DISC_MSGERR);
    addEnum(DISC_IPBAN);
    addEnum(DISC_PRIVATE);
    addEnum(DISC_MAXCLIENTS);
    addEnum(DISC_TIMEOUT);
    addEnum(DISC_OVERFLOW);
    addEnum(DISC_PASSWORD);
    addEnum(DISC_NUM);
    addEnum(DEFAULTCLIENTS);
    addEnum(ST_EMPTY);
    addEnum(ST_LOCAL);
    addEnum(ST_TCPIP);
    addEnum(MAXPINGDATA);
#define addPtr(n) lua_pushstring(L, #n); push(L, &n); lua_rawset(L, -3)
    addPtr(clients);
    addPtr(pongaddr);
    addPtr(localpongaddr);
    addPtr(serveraddress);
#undef addPtr
    #undef addEnum
    lua_pop(L, 1);
}

}

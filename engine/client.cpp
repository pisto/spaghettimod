// client.cpp, mostly network related client game code

#include "pch.h"
#include "engine.h"

ENetHost *clienthost = NULL;
ENetPeer *curpeer = NULL, *connpeer = NULL;
int connmillis = 0, connattempts = 0, discmillis = 0;

bool multiplayer(bool msg)
{
    // check not correct on listen server?
    if(curpeer && msg) conoutf(CON_ERROR, "operation not available in multiplayer");
    return curpeer!=NULL;
}

void setrate(int rate)
{
   if(!curpeer) return;
   enet_host_bandwidth_limit(clienthost, rate, rate);
}

VARF(rate, 0, 0, 25000, setrate(rate));

void throttle();

VARF(throttle_interval, 0, 5, 30, throttle());
VARF(throttle_accel,    0, 2, 32, throttle());
VARF(throttle_decel,    0, 2, 32, throttle());

void throttle()
{
    if(!curpeer) return;
    ASSERT(ENET_PEER_PACKET_THROTTLE_SCALE==32);
    enet_peer_throttle_configure(curpeer, throttle_interval*1000, throttle_accel, throttle_decel);
}

void abortconnect()
{
    if(!connpeer) return;
    if(connpeer->state!=ENET_PEER_STATE_DISCONNECTED) enet_peer_reset(connpeer);
    connpeer = NULL;
    if(curpeer) return;
    enet_host_destroy(clienthost);
    clienthost = NULL;
}

void connects(char *servername)
{   
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
    }

    ENetAddress address;
    address.port = sv->serverport();

    if(servername)
    {
        addserver(servername);
        conoutf("attempting to connect to %s", servername);
        if(!resolverwait(servername, &address))
        {
            conoutf("\f3could not resolve server %s", servername);
            return;
        }
    }
    else
    {
        conoutf("attempting to connect over LAN");
        address.host = ENET_HOST_BROADCAST;
    }

    if(!clienthost) clienthost = enet_host_create(NULL, 2, rate, rate);

    if(clienthost)
    {
        connpeer = enet_host_connect(clienthost, &address, cc->numchannels()); 
        enet_host_flush(clienthost);
        connmillis = totalmillis;
        connattempts = 0;
    }
    else conoutf("\f3could not connect to server");
}

void lanconnect()
{
    connects(0);
}

void disconnect(int onlyclean, int async)
{
    bool cleanup = onlyclean!=0;
    if(curpeer) 
    {
        if(!discmillis)
        {
            enet_peer_disconnect(curpeer, DISC_NONE);
            enet_host_flush(clienthost);
            discmillis = totalmillis;
        }
        if(curpeer->state!=ENET_PEER_STATE_DISCONNECTED)
        {
            if(async) return;
            enet_peer_reset(curpeer);
        }
        curpeer = NULL;
        discmillis = 0;
        conoutf("disconnected");
        cleanup = true;
    }
    if(cleanup)
    {
        cc->gamedisconnect();
        localdisconnect();
    }
    if(!connpeer && clienthost)
    {
        enet_host_destroy(clienthost);
        clienthost = NULL;
    }
    if(!onlyclean) { localconnect(); cc->gameconnect(false); }
}

void trydisconnect()
{
    if(connpeer)
    {
        conoutf("aborting connection attempt");
        abortconnect();
        return;
    }
    if(!curpeer)
    {
        conoutf("not connected");
        return;
    }
    conoutf("attempting to disconnect...");
    disconnect(0, !discmillis);
}

COMMANDN(connect, connects, "s");
COMMAND(lanconnect, "");
COMMANDN(disconnect, trydisconnect, "");

int lastupdate = -1000;

void sendpackettoserv(ENetPacket *packet, int chan)
{
    if(curpeer) enet_peer_send(curpeer, chan, packet);
    else localclienttoserver(chan, packet);
}

void c2sinfo(dynent *d, int rate)                     // send update to the server
{
    if(totalmillis-lastupdate<rate) return;    // don't update faster than 30fps
    lastupdate = totalmillis;
    ENetPacket *packet = enet_packet_create(NULL, MAXTRANS, 0);
    ucharbuf p(packet->data, packet->dataLength);
    bool reliable = false;
    int chan = cc->sendpacketclient(p, reliable, d);
    if(!p.length()) { enet_packet_destroy(packet); return; }
    if(reliable) packet->flags = ENET_PACKET_FLAG_RELIABLE;
    enet_packet_resize(packet, p.length());
    sendpackettoserv(packet, chan);
    if(clienthost) enet_host_flush(clienthost);
}

void neterr(const char *s)
{
    conoutf(CON_ERROR, "\f3illegal network message (%s)", s);
    disconnect();
}

void localservertoclient(int chan, uchar *buf, int len)   // processes any updates from the server
{
    ucharbuf p(buf, len);
    cc->parsepacketclient(chan, p);
}

void clientkeepalive() { if(clienthost) enet_host_service(clienthost, NULL, 0); }

void gets2c()           // get updates from the server
{
    ENetEvent event;
    if(!clienthost) return;
    if(connpeer && totalmillis/3000 > connmillis/3000)
    {
        conoutf("attempting to connect...");
        connmillis = totalmillis;
        ++connattempts; 
        if(connattempts > 3)
        {
            conoutf("\f3could not connect to server");
            abortconnect();
            return;
        }
    }
    while(clienthost && enet_host_service(clienthost, &event, 0)>0)
    switch(event.type)
    {
        case ENET_EVENT_TYPE_CONNECT:
            disconnect(1); 
            curpeer = connpeer;
            connpeer = NULL;
            conoutf("connected to server");
            throttle();
            if(rate) setrate(rate);
            cc->gameconnect(true);
            break;
         
        case ENET_EVENT_TYPE_RECEIVE:
            if(discmillis) conoutf("attempting to disconnect...");
            else localservertoclient(event.channelID, event.packet->data, (int)event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;

        case ENET_EVENT_TYPE_DISCONNECT:
            extern const char *disc_reasons[];
            if(event.data>=DISC_NUM) event.data = DISC_NONE;
            if(event.peer==connpeer)
            {
                conoutf("\f3could not connect to server");
                abortconnect();
            }
            else
            {
                if(!discmillis || event.data) conoutf("\f3server network error, disconnecting (%s) ...", disc_reasons[event.data]);
                disconnect();
            }
            return;

        default:
            break;
    }
}


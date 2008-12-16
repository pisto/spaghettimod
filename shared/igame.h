// the interface the engine uses to run the gameplay module

struct icliententities
{
    virtual ~icliententities() {}

    virtual void editent(int i) = 0;
    virtual const char *entnameinfo(entity &e) = 0;
    virtual const char *entname(int i) = 0;
    virtual int extraentinfosize() = 0;
    virtual void writeent(entity &e, char *buf) = 0;
    virtual void readent(entity &e, char *buf) = 0;
    virtual float dropheight(entity &e) = 0;
    virtual void rumble(const extentity &e) = 0;
    virtual void trigger(extentity &e) = 0;
    virtual void fixentity(extentity &e) = 0;
    virtual void entradius(extentity &e, float &radius, float &angle, vec &dir) {}
    virtual bool mayattach(extentity &e) { return false; }
    virtual bool attachent(extentity &e, extentity &a) { return false; }
    virtual extentity *newentity() = 0;
    virtual void deleteentity(extentity *e) = 0;
    virtual vector<extentity *> &getents() = 0;
};

struct iclientcom
{
    virtual ~iclientcom() {}

    virtual void gamedisconnect() = 0;
    virtual void parsepacketclient(int chan, ucharbuf &p) = 0;
    virtual int sendpacketclient(ucharbuf &p, bool &reliable, dynent *d) = 0;
    virtual void gameconnect(bool _remote) = 0;
    virtual bool allowedittoggle() = 0;
    virtual void edittoggled(bool on) {}
    virtual void writeclientinfo(FILE *f) = 0;
    virtual void toserver(char *text) = 0;
    virtual void changemap(const char *name) = 0;
    virtual int numchannels() { return 1; }
};

struct igameclient
{
    virtual ~igameclient() {}

    virtual const char *gameident() = 0;
    virtual const char *defaultmap() = 0;
    virtual const char *savedconfig() = 0;
    virtual const char *defaultconfig() = 0;
    virtual const char *autoexec() = 0;
    virtual const char *savedservers() { return NULL; }

    virtual icliententities *getents() = 0;
    virtual iclientcom *getcom() = 0;

    virtual bool clientoption(char *arg) { return false; }
    virtual void updateworld() = 0;
    virtual void initclient() = 0;
    virtual void physicstrigger(physent *d, bool local, int floorlevel, int waterlevel, int material = 0) = 0;
    virtual void edittrigger(const selinfo &sel, int op, int arg1 = 0, int arg2 = 0, int arg3 = 0) = 0;
    virtual char *getclientmap() = 0;
    virtual void resetgamestate() = 0;
    virtual void suicide(physent *d) = 0;
    virtual void newmap(int size) = 0;
    virtual void startmap(const char *name) = 0;
    virtual void preload() {}
    virtual float abovegameplayhud() { return 1.0f; }
    virtual void gameplayhud(int w, int h) = 0;
    virtual void drawhudgun() = 0;
    virtual bool canjump() = 0;
    virtual bool allowmove(physent *d) { return true; }
    virtual void doattack(bool on) = 0;
    virtual dynent *iterdynents(int i) = 0;
    virtual int numdynents() = 0;
    virtual void rendergame() = 0;
    virtual void writegamedata(vector<char> &extras) = 0;
    virtual void readgamedata(vector<char> &extras) = 0;
    virtual void g3d_gamemenus() = 0;
    virtual const char *defaultcrosshair(int index) { return NULL; }
    virtual int selectcrosshair(float &r, float &g, float &b) { return 0; }
    virtual void lighteffects(dynent *d, vec &color, vec &dir) {}
    virtual void setupcamera() {}
    virtual bool detachcamera() { return false; }
    virtual void adddynlights() {}
    virtual void particletrack(physent *owner, vec &o, vec &d) {}
    virtual bool serverinfostartcolumn(g3d_gui *g, int i) { return false; }
    virtual void serverinfoendcolumn(g3d_gui *g, int i) {}
    virtual bool serverinfoentry(g3d_gui *g, int i, const char *name, const char *desc, const char *map, int ping, const vector<int> &attr, int np) { return false; };
}; 
 
struct igameserver
{
    virtual ~igameserver() {}

    virtual bool serveroption(char *arg) { return false; }
    virtual void *newinfo() = 0;
    virtual void deleteinfo(void *ci) = 0;
    virtual void serverinit() = 0;
    virtual void clientdisconnect(int n) = 0;
    virtual int clientconnect(int n, uint ip) = 0;
    virtual void localdisconnect(int n) = 0;
    virtual void localconnect(int n) = 0;
    virtual const char *servername() = 0;
    virtual void recordpacket(int chan, void *data, int len) {}
    virtual void parsepacket(int sender, int chan, bool reliable, ucharbuf &p) = 0;
    virtual bool sendpackets() = 0;
    virtual int welcomepacket(ucharbuf &p, int n, ENetPacket *packet) = 0;
    virtual void serverinforeply(ucharbuf &req, ucharbuf &p) = 0;
    virtual void serverupdate(int lastmillis, int totalmillis) = 0;
    virtual bool servercompatible(char *name, char *sdec, char *map, int ping, const vector<int> &attr, int np) = 0;
    virtual int serverinfoport() = 0;
    virtual int serverport() = 0;
    virtual const char *getdefaultmaster() = 0;
    virtual void sendservmsg(const char *s) = 0;
};

struct igame
{
    virtual ~igame() {}

    virtual igameclient *newclient() = 0;
    virtual igameserver *newserver() = 0;
};

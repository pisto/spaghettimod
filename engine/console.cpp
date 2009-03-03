// console.cpp: the console buffer, its display, and command line control

#include "cube.h"
#include "engine.h"

struct cline { char *line; int type, outtime; };
vector<cline> conlines;

bool saycommandon = false;
string commandbuf;
char *commandaction = NULL, *commandprompt = NULL;
int commandpos = -1;

VARFP(maxcon, 10, 200, 1000, { while(conlines.length() > maxcon) delete[] conlines.pop().line; });

void conline(int type, const char *sf)        // add a line to the console buffer
{
    cline cl;
    cl.line = conlines.length()>maxcon ? conlines.pop().line : newstringbuf("");   // constrain the buffer size
    cl.type = type;
    cl.outtime = totalmillis;                       // for how long to keep line on screen
    conlines.insert(0, cl);
    s_strcpy(cl.line, sf);
}

#define CONSPAD (FONTH/3)

void conoutfv(int type, const char *fmt, va_list args)
{
    string sf, sp;
    formatstring(sf, fmt, args);
    filtertext(sp, sf);
    puts(sp);
    conline(type, sf);
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

bool fullconsole = false;
void toggleconsole() { fullconsole = !fullconsole; }
COMMAND(toggleconsole, "");

int rendercommand(int x, int y, int w)
{
    if(!saycommandon) return 0;

    s_sprintfd(s)("%s %s", commandprompt ? commandprompt : ">", commandbuf);
    int width, height;
    text_bounds(s, width, height, w);
    y-= height-FONTH;
    draw_text(s, x, y, 0xFF, 0xFF, 0xFF, 0xFF, (commandpos>=0) ? (commandpos+1+(commandprompt?strlen(commandprompt):1)) : strlen(s), w);
    return height;
}

void blendbox(int x1, int y1, int x2, int y2, bool border)
{
    notextureshader->set();

    glDepthMask(GL_FALSE);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_COLOR);
    glBegin(GL_QUADS);
    if(border) glColor3d(0.5, 0.3, 0.4);
    else glColor3d(1.0, 1.0, 1.0);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
    glDisable(GL_BLEND);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glBegin(GL_QUADS);
    glColor3d(0.2, 0.7, 0.4);
    glVertex2f(x1, y1);
    glVertex2f(x2, y1);
    glVertex2f(x2, y2);
    glVertex2f(x1, y2);
    glEnd();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    xtraverts += 8;
    glEnable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glDepthMask(GL_TRUE);

    defaultshader->set();
}

VARP(consize, 0, 5, 100);
VARP(confade, 0, 30, 60);
VARP(fullconsize, 0, 75, 100);
VARP(confilter, 0, 0xFFFFFF, 0xFFFFFF);
VARP(fullconfilter, 0, 0xFFFFFF, 0xFFFFFF);

int conskip = 0;

void setconskip(int *n)
{
    int filter = fullconsole ? fullconfilter : confilter,
        skipped = abs(*n),
        dir = *n < 0 ? -1 : 1;
    conskip = clamp(conskip, 0, conlines.length()-1);
    while(skipped)
    {
        conskip += dir;
        if(!conlines.inrange(conskip))
        {
            conskip = clamp(conskip, 0, conlines.length()-1);
            return;
        }
        if(conlines[conskip].type&filter) --skipped;
    }
}

COMMANDN(conskip, setconskip, "i");

int renderconsole(int w, int h)                   // render buffer taking into account time & scrolling
{
    int conheight = min(fullconsole ? ((h*3*fullconsize/100)/FONTH)*FONTH : (FONTH*consize), h*3 - 2*CONSPAD - 2*FONTH/3),
        conwidth = w*3 - 2*CONSPAD - 2*FONTH/3,
        filter = fullconsole ? fullconfilter : confilter;
    
    if(fullconsole) blendbox(CONSPAD, CONSPAD, conwidth+CONSPAD+2*FONTH/3, conheight+CONSPAD+2*FONTH/3, true);
    
    int numl = conlines.length(), offset = min(conskip, numl);
    
    if(!fullconsole && confade)
    {
        if(!conskip) 
        {
            numl = 0;
            loopvrev(conlines) if(totalmillis-conlines[i].outtime < confade*1000) { numl = i+1; break; }
        } 
        else offset--;
    }
   
    int y = 0;
    loopi(numl) //determine visible height
    {
        // shuffle backwards to fill if necessary
        int idx = offset+i < numl ? offset+i : --offset;
        if(!(conlines[idx].type&filter)) continue;
        char *line = conlines[idx].line;
        int width, height;
        text_bounds(line, width, height, conwidth);
        y += height;
        if(y > conheight) { numl = i; if(offset == idx) ++offset; break; }
    }
    y = CONSPAD+FONTH/3;
    loopi(numl)
    {
        int idx = offset + numl-i-1;
        if(!(conlines[idx].type&filter)) continue;
        char *line = conlines[idx].line;
        draw_text(line, CONSPAD+FONTH/3, y, 0xFF, 0xFF, 0xFF, 0xFF, -1, conwidth);
        int width, height;
        text_bounds(line, width, height, conwidth);
        y += height;
    }
    return fullconsole ? (2*CONSPAD+conheight+2*FONTH/3) : (y+CONSPAD+FONTH/3);
}

// keymap is defined externally in keymap.cfg

struct keym
{
    enum
    {
        ACTION_DEFAULT = 0,
        ACTION_SPECTATOR,
        ACTION_EDITING,
        NUMACTIONS
    };
    
    int code;
    char *name;
    char *actions[NUMACTIONS];
    bool pressed;

    keym() : code(-1), name(NULL), pressed(false) { loopi(NUMACTIONS) actions[i] = newstring(""); }
    ~keym() { DELETEA(name); loopi(NUMACTIONS) DELETEA(actions[i]); }
};

hashtable<int, keym> keyms(128);

void keymap(int *code, char *key)
{
    if(overrideidents) { conoutf(CON_ERROR, "cannot override keymap %s", code); return; }
    keym &km = keyms[*code];
    km.code = *code;
    DELETEA(km.name);
    km.name = newstring(key);
}
    
COMMAND(keymap, "is");

keym *keypressed = NULL;
char *keyaction = NULL;

const char *getkeyname(int code)
{
    keym *km = keyms.access(code);
    return km ? km->name : NULL;
}

void searchbinds(char *action, int type)
{
    vector<char> names;
    enumerate(keyms, keym, km,
    {
        if(!strcmp(km.actions[type], action))
        {
            if(names.length()) names.add(' ');
            names.put(km.name, strlen(km.name));
        }
    });
    names.add('\0');
    result(names.getbuf());
}

keym *findbind(char *key)
{
    enumerate(keyms, keym, km,
    {
        if(!strcasecmp(km.name, key)) return &km;
    });
    return NULL;
}   
    
void getbind(char *key, int type)
{
    keym *km = findbind(key);
    result(km ? km->actions[type] : "");
}   

void bindkey(char *key, char *action, int state, const char *cmd)
{
    if(overrideidents) { conoutf(CON_ERROR, "cannot override %s \"%s\"", cmd, key); return; }
    keym *km = findbind(key);
    if(!km) { conoutf(CON_ERROR, "unknown key \"%s\"", key); return; }
    char *&binding = km->actions[state];
    if(!keypressed || keyaction!=binding) delete[] binding;
    // trim white-space to make searchbinds more reliable
    while(isspace(*action)) action++;
    int len = strlen(action);
    while(len>0 && isspace(action[len-1])) len--;
    binding = newstring(action, len);
}

ICOMMAND(bind,     "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_DEFAULT, "bind"));
ICOMMAND(specbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_SPECTATOR, "specbind"));
ICOMMAND(editbind, "ss", (char *key, char *action), bindkey(key, action, keym::ACTION_EDITING, "editbind"));
ICOMMAND(getbind,     "s", (char *key), getbind(key, keym::ACTION_DEFAULT));
ICOMMAND(getspecbind, "s", (char *key), getbind(key, keym::ACTION_SPECTATOR));
ICOMMAND(geteditbind, "s", (char *key), getbind(key, keym::ACTION_EDITING));
ICOMMAND(searchbinds,     "s", (char *action), searchbinds(action, keym::ACTION_DEFAULT));
ICOMMAND(searchspecbinds, "s", (char *action), searchbinds(action, keym::ACTION_SPECTATOR));
ICOMMAND(searcheditbinds, "s", (char *action), searchbinds(action, keym::ACTION_EDITING));

void saycommand(char *init)                         // turns input to the command line on or off
{
    SDL_EnableUNICODE(saycommandon = (init!=NULL));
    if(!editmode) keyrepeat(saycommandon);
    s_strcpy(commandbuf, init ? init : "");
    DELETEA(commandaction);
    DELETEA(commandprompt);
    commandpos = -1;
}

void inputcommand(char *init, char *action, char *prompt)
{
    saycommand(init);
    if(action[0]) commandaction = newstring(action);
    if(prompt[0]) commandprompt = newstring(prompt);
}

void mapmsg(char *s) { s_strncpy(hdr.maptitle, s, 128); }

COMMAND(saycommand, "C");
COMMAND(inputcommand, "sss");
COMMAND(mapmsg, "s");

#if !defined(WIN32) && !defined(__APPLE__)
#include <X11/Xlib.h>
#include <SDL_syswm.h>
#endif

void pasteconsole()
{
    #ifdef WIN32
    if(!IsClipboardFormatAvailable(CF_TEXT)) return; 
    if(!OpenClipboard(NULL)) return;
    char *cb = (char *)GlobalLock(GetClipboardData(CF_TEXT));
    s_strcat(commandbuf, cb);
    GlobalUnlock(cb);
    CloseClipboard();
    #elif defined(__APPLE__)
	extern void mac_pasteconsole(char *commandbuf);

	mac_pasteconsole(commandbuf);
	#else
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version); 
    wminfo.subsystem = SDL_SYSWM_X11;
    if(!SDL_GetWMInfo(&wminfo)) return;
    int cbsize;
    char *cb = XFetchBytes(wminfo.info.x11.display, &cbsize);
    if(!cb || !cbsize) return;
    size_t commandlen = strlen(commandbuf);
    for(char *cbline = cb, *cbend; commandlen + 1 < sizeof(commandbuf) && cbline < &cb[cbsize]; cbline = cbend + 1)
    {
        cbend = (char *)memchr(cbline, '\0', &cb[cbsize] - cbline);
        if(!cbend) cbend = &cb[cbsize];
        if(size_t(commandlen + cbend - cbline + 1) > sizeof(commandbuf)) cbend = cbline + sizeof(commandbuf) - commandlen - 1;
        memcpy(&commandbuf[commandlen], cbline, cbend - cbline);
        commandlen += cbend - cbline;
        commandbuf[commandlen] = '\n';
        if(commandlen + 1 < sizeof(commandbuf) && cbend < &cb[cbsize]) ++commandlen;
        commandbuf[commandlen] = '\0';
    }
    XFree(cb);
    #endif
}

struct hline
{
    char *buf, *action, *prompt;

    hline() : buf(NULL), action(NULL), prompt(NULL) {}
    ~hline()
    {
        DELETEA(buf);
        DELETEA(action);
        DELETEA(prompt);
    }

    void restore()
    {
        s_strcpy(commandbuf, buf);
        if(commandpos >= (int)strlen(commandbuf)) commandpos = -1;
        DELETEA(commandaction);
        DELETEA(commandprompt);
        if(action) commandaction = newstring(action);
        if(prompt) commandprompt = newstring(prompt);
    }

    bool shouldsave()
    {
        return strcmp(commandbuf, buf) ||
               (commandaction ? !action || strcmp(commandaction, action) : action!=NULL) ||
               (commandprompt ? !prompt || strcmp(commandprompt, prompt) : prompt!=NULL);
    }
    
    void save()
    {
        buf = newstring(commandbuf);
        if(commandaction) action = newstring(commandaction);
        if(commandprompt) prompt = newstring(commandprompt);
    }

    void run()
    {
        if(action)
        {
            alias("commandbuf", buf);
            execute(action);
        }
        else if(buf[0]=='/') execute(buf+1);
        else cc->toserver(buf);
    }
};
vector<hline *> history;
int histpos = 0;

void history_(int *n)
{
    static bool inhistory = true;
    if(!inhistory && history.inrange(*n))
    {
        inhistory = true;
        history[history.length()-*n-1]->run();
        inhistory = false;
    }
}

COMMANDN(history, history_, "i");

struct releaseaction
{
    keym *key;
    char *action;
};
vector<releaseaction> releaseactions;

const char *addreleaseaction(const char *s)
{
    if(!keypressed) return NULL;
    releaseaction &ra = releaseactions.add();
    ra.key = keypressed;
    ra.action = newstring(s);
    return keypressed->name;
}

void onrelease(char *s)
{
    addreleaseaction(s);
}

COMMAND(onrelease, "s");

void execbind(keym &k, bool isdown)
{
    loopv(releaseactions)
    {
        releaseaction &ra = releaseactions[i];
        if(ra.key==&k)
        {
            if(!isdown) execute(ra.action);
            delete[] ra.action;
            releaseactions.remove(i--);
        }
    }
    if(isdown)
    {
        int state = editmode ? keym::ACTION_EDITING : (player->state==CS_SPECTATOR ? keym::ACTION_SPECTATOR : keym::ACTION_DEFAULT);
        char *&action = k.actions[state][0] ? k.actions[state] : k.actions[keym::ACTION_DEFAULT];
        keyaction = action;
        keypressed = &k;
        execute(keyaction);
        keypressed = NULL;
        if(keyaction!=action) delete[] keyaction;
    }
    k.pressed = isdown;
}

void consolekey(int code, bool isdown, int cooked)
{
    #ifdef __APPLE__
        #define MOD_KEYS (KMOD_LMETA|KMOD_RMETA) 
    #else
        #define MOD_KEYS (KMOD_LCTRL|KMOD_RCTRL)
    #endif

    if(isdown)
    {
        switch(code)
        {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                break;

            case SDLK_HOME:
                if(strlen(commandbuf)) commandpos = 0;
                break;

            case SDLK_END:
                commandpos = -1;
                break;

            case SDLK_DELETE:
            {
                int len = (int)strlen(commandbuf);
                if(commandpos<0) break;
                memmove(&commandbuf[commandpos], &commandbuf[commandpos+1], len - commandpos);
                resetcomplete();
                if(commandpos>=len-1) commandpos = -1;
                break;
            }

            case SDLK_BACKSPACE:
            {
                int len = (int)strlen(commandbuf), i = commandpos>=0 ? commandpos : len;
                if(i<1) break;
                memmove(&commandbuf[i-1], &commandbuf[i], len - i + 1);
                resetcomplete();
                if(commandpos>0) commandpos--;
                else if(!commandpos && len<=1) commandpos = -1;
                break;
            }

            case SDLK_LEFT:
                if(commandpos>0) commandpos--;
                else if(commandpos<0) commandpos = (int)strlen(commandbuf)-1;
                break;

            case SDLK_RIGHT:
                if(commandpos>=0 && ++commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                break;

            case SDLK_UP:
                if(histpos>0) history[--histpos]->restore(); 
                break;

            case SDLK_DOWN:
                if(histpos+1<history.length()) history[++histpos]->restore();
                break;

            case SDLK_TAB:
                if(!commandaction)
                {
                    complete(commandbuf);
                    if(commandpos>=0 && commandpos>=(int)strlen(commandbuf)) commandpos = -1;
                }
                break;

            case SDLK_v:
                if(SDL_GetModState()&MOD_KEYS) { pasteconsole(); return; }
                // fall through

            default:
                resetcomplete();
                if(cooked)
                {
                    size_t len = (int)strlen(commandbuf);
                    if(len+1<sizeof(commandbuf))
                    {
                        if(commandpos<0) commandbuf[len] = cooked;
                        else
                        {
                            memmove(&commandbuf[commandpos+1], &commandbuf[commandpos], len - commandpos);
                            commandbuf[commandpos++] = cooked;
                        }
                        commandbuf[len+1] = '\0';
                    }
                }
                break;
        }
    }
    else
    {
        if(code==SDLK_RETURN || code==SDLK_KP_ENTER)
        {
            hline *h = NULL;
            if(commandbuf[0])
            {
                if(history.empty() || history.last()->shouldsave())
                    history.add(h = new hline)->save(); // cap this?
                else h = history.last();
            }
            histpos = history.length();
            saycommand(NULL);
            if(h) h->run();
        }
        else if(code==SDLK_ESCAPE)
        {
            histpos = history.length();
            saycommand(NULL);
        }
    }
}

extern bool menukey(int code, bool isdown, int cooked);

void keypress(int code, bool isdown, int cooked)
{
    keym *haskey = keyms.access(code);
    if(haskey && haskey->pressed) execbind(*haskey, isdown); // allow pressed keys to release
    else if(!menukey(code, isdown, cooked)) // 3D GUI mouse button intercept   
    {
        if(saycommandon) consolekey(code, isdown, cooked);
        else if(haskey) execbind(*haskey, isdown);
    }
}

void clear_console()
{
    keyms.clear();
}

static int sortbinds(keym **x, keym **y)
{
    return strcmp((*x)->name, (*y)->name);
}

void writebinds(FILE *f)
{
    static const char *cmds[3] = { "bind", "specbind", "editbind" };
    vector<keym *> binds;
    enumerate(keyms, keym, km, binds.add(&km));
    binds.sort(sortbinds);
    loopj(3)
    {
        loopv(binds)
        {
            keym &km = *binds[i];
            if(*km.actions[j]) fprintf(f, "%s \"%s\" [%s]\n", cmds[j], km.name, km.actions[j]);
        }
    }
}

// tab-completion of all idents and base maps

enum { FILES_DIR = 0, FILES_LIST };

struct fileskey
{
    int type;
    const char *dir, *ext;

    fileskey() {}
    fileskey(int type, const char *dir, const char *ext) : type(type), dir(dir), ext(ext) {}
};

struct filesval
{
    int type;
    char *dir, *ext;
    vector<char *> files;
    
    filesval(int type, const char *dir, const char *ext) : type(type), dir(newstring(dir)), ext(ext && ext[0] ? newstring(ext) : NULL) {}
    ~filesval() { DELETEA(dir); DELETEA(ext); loopv(files) DELETEA(files[i]); files.setsize(0); }
};

static inline bool htcmp(const fileskey &x, const fileskey &y)
{
    return x.type==y.type && !strcmp(x.dir, y.dir) && (x.ext == y.ext || (x.ext && y.ext && !strcmp(x.ext, y.ext)));
}

static inline uint hthash(const fileskey &k)
{
    return hthash(k.dir);
}

static hashtable<fileskey, filesval *> completefiles;
static hashtable<char *, filesval *> completions;

int completesize = 0;
string lastcomplete;

void resetcomplete() { completesize = 0; }

void addcomplete(char *command, int type, char *dir, char *ext)
{
    if(overrideidents)
    {
        conoutf(CON_ERROR, "cannot override complete %s", command);
        return;
    }
    if(!dir[0])
    {
        filesval **hasfiles = completions.access(command);
        if(hasfiles) *hasfiles = NULL;
        return;
    }
    if(type==FILES_DIR)
    {
        int dirlen = (int)strlen(dir);
        while(dirlen > 0 && (dir[dirlen-1] == '/' || dir[dirlen-1] == '\\'))
            dir[--dirlen] = '\0';
        if(ext)
        {
            if(strchr(ext, '*')) ext[0] = '\0';
            if(!ext[0]) ext = NULL;
        }
    }
    fileskey key(type, dir, ext);
    filesval **val = completefiles.access(key);
    if(!val)
    {
        filesval *f = new filesval(type, dir, ext);
        if(type==FILES_LIST) explodelist(dir, f->files); 
        val = &completefiles[fileskey(type, f->dir, f->ext)];
        *val = f;
    }
    filesval **hasfiles = completions.access(command);
    if(hasfiles) *hasfiles = *val;
    else completions[newstring(command)] = *val;
}

void addfilecomplete(char *command, char *dir, char *ext)
{
    addcomplete(command, FILES_DIR, dir, ext);
}

void addlistcomplete(char *command, char *list)
{
    addcomplete(command, FILES_LIST, list, NULL);
}

COMMANDN(complete, addfilecomplete, "sss");
COMMANDN(listcomplete, addlistcomplete, "ss");

void complete(char *s)
{
    if(*s!='/')
    {
        string t;
        s_strcpy(t, s);
        s_strcpy(s, "/");
        s_strcat(s, t);
    }
    if(!s[1]) return;
    if(!completesize) { completesize = (int)strlen(s)-1; lastcomplete[0] = '\0'; }

    filesval *f = NULL;
    if(completesize)
    {
        char *end = strchr(s, ' ');
        if(end)
        {
            string command;
            s_strncpy(command, s+1, min(size_t(end-s), sizeof(command)));
            filesval **hasfiles = completions.access(command);
            if(hasfiles) f = *hasfiles;
        }
    }

    const char *nextcomplete = NULL;
    string prefix;
    s_strcpy(prefix, "/");
    if(f) // complete using filenames
    {
        int commandsize = strchr(s, ' ')+1-s;
        s_strncpy(prefix, s, min(size_t(commandsize+1), sizeof(prefix)));
        if(f->type==FILES_DIR && f->files.empty()) listfiles(f->dir, f->ext, f->files);
        loopi(f->files.length())
        {
            if(strncmp(f->files[i], s+commandsize, completesize+1-commandsize)==0 &&
               strcmp(f->files[i], lastcomplete) > 0 && (!nextcomplete || strcmp(f->files[i], nextcomplete) < 0))
                nextcomplete = f->files[i];
        }
    }
    else // complete using command names
    {
        extern hashtable<const char *, ident> *idents;
        enumerate(*idents, ident, id,
            if(strncmp(id.name, s+1, completesize)==0 &&
               strcmp(id.name, lastcomplete) > 0 && (!nextcomplete || strcmp(id.name, nextcomplete) < 0))
                nextcomplete = id.name;
        );
    }
    if(nextcomplete)
    {
        s_strcpy(s, prefix);
        s_strcat(s, nextcomplete);
        s_strcpy(lastcomplete, nextcomplete);
    }
    else lastcomplete[0] = '\0';
}

static int sortcompletions(char **x, char **y)
{
    return strcmp(*x, *y);
}

void writecompletions(FILE *f)
{
    vector<char *> cmds;
    enumeratekt(completions, char *, k, filesval *, v, { if(v) cmds.add(k); });
    cmds.sort(sortcompletions);
    loopv(cmds)
    {
        char *k = cmds[i];
        filesval *v = completions[k];
        if(v->type==FILES_LIST) fprintf(f, "listcomplete \"%s\" [%s]\n", k, v->dir);
        else fprintf(f, "complete \"%s\" \"%s\" \"%s\"\n", k, v->dir, v->ext ? v->ext : "*");
    }
}


// records video to uncompressed avi files (will split across multiple files once size exceeds 1Gb)
// - people should post process the files because they will get large very rapidly


// Feedback on playing videos:
// quicktime - good
// vlc - ok
// xine - ok
// mplayer - ok
// totem - 2Apr09-RockKeyman:"Failed to create output image buffer of 640x480 pixels"
// avidemux - 2Apr09-RockKeyman:"Impossible to open the file"
// kino - 2Apr09-RockKeyman:imports, but only plays a half second of the 15second movie

#include "engine.h"
#include "SDL_mixer.h"

extern void getfps(int &fps, int &bestdiff, int &worstdiff);

struct aviwriter
{
    stream *f;
    uchar *yuv;
    uint videoframes;
    uint filesequence;    
    const uint videow, videoh, videofps;
    string filename;
    
    vector<long>index;
    
    enum { MAX_CHUNK_DEPTH = 16 };
    long chunkoffsets[MAX_CHUNK_DEPTH];
    int chunkdepth;
    
    void startchunk(const char *fcc)
    {
        f->write(fcc, 4);
        const uint size = 0;
        f->write(&size, 4);
        chunkoffsets[++chunkdepth] = f->tell();
    }
    
    void listchunk(const char *fcc, const char *lfcc)
    {
        startchunk(fcc);
        f->write(lfcc, 4);
    }
    
    void endchunk()
    {
        assert(chunkdepth >= 0);
        uint size = f->tell() - chunkoffsets[chunkdepth];
        f->seek(chunkoffsets[chunkdepth] - 4, SEEK_SET);
        f->putlil(size);
        f->seek(0, SEEK_END);
        --chunkdepth;
    }
    
    void close()
    {
        if(!f) return;
        assert(chunkdepth == 1);
        endchunk(); // LIST movi
        
        startchunk("idx1");
        loopv(index)
        {
            f->write("00dc", 4); // chunkid
            f->putlil<uint>(0x10); // flags - KEYFRAME
            f->putlil<uint>(index[i]); // offset (relative to movi)
            f->putlil<uint>(videow*videoh*2); // size
        }
        endchunk();

        endchunk(); // RIFF AVI

        DELETEP(f);
    }
    
    aviwriter(const char *name, uint w, uint h, uint fps) : f(NULL), yuv(NULL), videoframes(0), filesequence(0), videow(w&~1), videoh(h&~1), videofps(fps)
    {
        s_strcpy(filename, name);
        path(filename);
        if(!strrchr(filename, '.')) s_strcat(filename, ".avi");
    }
    
    ~aviwriter()
    {
        close();
        if(yuv) delete [] yuv;
    }
    
    bool open()
    {
        close();
        string seqfilename;
        if(filesequence == 0) s_strcpy(seqfilename, filename);
        else
        {
            if(filesequence >= 999) return false;
            char *ext = strrchr(filename, '.');
            if(filesequence == 1) 
            {
                string oldfilename;
                s_strcpy(oldfilename, findfile(filename, "wb"));
                *ext = '\0';
                conoutf("movie now recording to multiple: %s_XXX.%s files", filename, ext+1);
                s_sprintf(seqfilename)("%s_%03d.%s", filename, 0, ext+1);
                rename(oldfilename, findfile(seqfilename, "wb"));
            }
            *ext = '\0';
            s_sprintf(seqfilename)("%s_%03d.%s", filename, filesequence, ext+1);
            *ext = '.';
        }
        filesequence++;
        f = openfile(seqfilename, "wb");
        if(!f) return false;
        
        index.setsize(0);
        chunkdepth = -1;
        
        listchunk("RIFF", "AVI ");
        
        listchunk("LIST", "hdrl");
        
        startchunk("avih");
        f->putlil<uint>(1000000 / videofps); // microsecsperframe
        f->putlil<uint>(0); // maxbytespersec
        f->putlil<uint>(0); // reserved
        f->putlil<uint>(0x10 | 0x20); // flags - hasindex|mustuseindex
        f->putlil<uint>(0); // totalframes <-- necessary to fill ??
        f->putlil<uint>(0); // initialframes
        f->putlil<uint>(1); // streams
        f->putlil<uint>(0); // buffersize
        f->putlil<uint>(videow); // width
        f->putlil<uint>(videoh); // height
        f->putlil<uint>(1); // scale
        f->putlil<uint>(videofps); // rate
        f->putlil<uint>(0); // start
        f->putlil<uint>(0); // length
        endchunk(); // avih
        
        listchunk("LIST", "strl");
        
        startchunk("strh");
        f->write("vids", 4); // fcctype
        f->write("I420", 4); // fcchandler
        f->putlil<uint>(0x10 | 0x20); // flags - hasindex|mustuseindex
        f->putlil<uint>(0); // reserved
        f->putlil<uint>(0); // initialframes
        f->putlil<uint>(1); // scale
        f->putlil<uint>(videofps); // rate
        f->putlil<uint>(0); // start
        f->putlil<uint>(0); // length <-- necessary to fill ??
        f->putlil<uint>(videow*videoh*2); // buffersize
        f->putlil<uint>(0); // quality
        f->putlil<uint>(0); // samplesize
        /* not in spec - but seen in example code...
        f->putlil<ushort>(0); // left
        f->putlil<ushort>(0); // top
        f->putlil<ushort>(videow); // right
        f->putlil<ushort>(videoh); // bottom
        */
        endchunk(); // strh
        
        startchunk("strf");
        f->putlil<uint>(40); //headersize
        f->putlil<uint>(videow); // width
        f->putlil<uint>(videoh); // height
        f->putlil<ushort>(3); // planes
        f->putlil<ushort>(12); // bitcount
        f->write("I420", 4); // compression
        f->putlil<uint>(videow*videoh*2); // imagesize
        f->putlil<uint>(0); // xres
        f->putlil<uint>(0); // yres;
        f->putlil<uint>(0); // colorsused
        f->putlil<uint>(0); // colorsrequired
        endchunk(); // strf
        
        /* keep it simple
        uint aw = videow, ah = videoh;
        while(!(aw%5) && !(ah%5)) { aw /= 5; ah /= 5; }
        while(!(aw%3) && !(ah%3)) { aw /= 3; ah /= 3; }
        while(!(aw%2) && !(ah%2)) { aw /= 2; ah /= 2; }
        startchunk("vprp");
        f->putlil<uint>(0); // vidformat
        f->putlil<uint>(0); // vidstandard
        f->putlil<uint>(videofps); // vertrefresh
        f->putlil<uint>(videow); // htotal
        f->putlil<uint>(videoh); // vtotal
        f->putlil<uint>((aw<<16)|ah); // frameaspect
        f->putlil<uint>(videow); // framewidth
        f->putlil<uint>(videoh); // frameheight
        f->putlil<uint>(1); // fieldsperframe
        // for each field - one in the case
        f->putlil<uint>(videoh); // compressheight
        f->putlil<uint>(videow); // compresswidth
        f->putlil<uint>(videoh); // validheight
        f->putlil<uint>(videow); // validwidth
        f->putlil<uint>(0); // validxoffset
        f->putlil<uint>(0); // validyoffset
        f->putlil<uint>(0); // videoxoffset
        f->putlil<uint>(0); // videoyoffset
        endchunk(); // vprp
        */
        
        endchunk(); // LIST strl
        
        listchunk("LIST", "INFO");
        
        const char *software = "Cube 2: Sauerbraten";
        startchunk("ISFT");
        f->write(software, strlen(software)+1);
        endchunk(); // ISFT
        
        endchunk(); // LIST INFO
        
        endchunk(); // LIST hdrl
        
        listchunk("LIST", "movi");
        
        return true;
    }
  
    static inline void boxsample(const uchar *src, const uint stride,
                                 const uint w, const uint iw, const uint h, const uint ih, 
                                 const uint xlow, const uint xhigh, const uint ylow, const uint yhigh,
                                 uint &bdst, uint &gdst, uint &rdst)
    {
        const uchar *xend = &src[max(iw, 1U)<<2];
        uint bt = 0, gt = 0, rt = 0;
        for(const uchar *xcur = &src[4]; xcur < xend; xcur += 4)
        {
            bt += xcur[0];
            gt += xcur[1];
            rt += xcur[2];
        }
        bt = ylow*(bt + ((src[0]*xlow + xend[0]*xhigh)>>12));
        gt = ylow*(gt + ((src[1]*xlow + xend[1]*xhigh)>>12));
        rt = ylow*(rt + ((src[2]*xlow + xend[2]*xhigh)>>12));
        if(ih)
        {
            const uchar *ycur = &src[stride], *yend = &src[stride*ih];
            xend += stride;
            if(ycur < yend) do
            {
                uint b = 0, g = 0, r = 0;
                for(const uchar *xcur = &ycur[4]; xcur < xend; xcur += 4)
                {
                    b += xcur[0];
                    g += xcur[1];
                    r += xcur[2];
                }
                bt += (b<<12) + ycur[0]*xlow + xend[0]*xhigh;
                gt += (g<<12) + ycur[1]*xlow + xend[1]*xhigh;
                rt += (r<<12) + ycur[2]*xlow + xend[2]*xhigh;
                ycur += stride;
                xend += stride;
            } while(ycur < yend);
            if(yhigh)
            {
                uint b = 0, g = 0, r = 0;
                for(const uchar *xcur = &ycur[4]; xcur < xend; xcur += 4)
                {
                    b += xcur[0];
                    g += xcur[1];
                    r += xcur[2];
                }
                bt += yhigh*(b + ((ycur[0]*xlow + xend[0]*xhigh)>>12));
                gt += yhigh*(g + ((ycur[1]*xlow + xend[1]*xhigh)>>12));
                rt += yhigh*(r + ((ycur[2]*xlow + xend[2]*xhigh)>>12));
            }
        }
        uint area = (1<<24) / ((w*h + 0xFFF)>>12);
        bdst = (bt*area)>>24;
        gdst = (gt*area)>>24;
        rdst = (rt*area)>>24;
    }
 
    void encodeyuv(const uchar *pixels, uint srcw, uint srch)
    {
        const int flip = -1;
        const uint planesize = videow * videoh;
        if(!yuv) yuv = new uchar[planesize*2];
        uchar *yplane = yuv, *uplane = yuv + planesize, *vplane = yuv + planesize + planesize/4;
        const int ystride = flip*int(videow), uvstride = flip*int(videow)/2;
        if(flip < 0) { yplane -= int(videoh-1)*ystride; uplane -= int(videoh/2-1)*uvstride; vplane -= int(videoh/2-1)*uvstride; }

        const uchar *src = pixels;
        const uint stride = srcw<<2, wfrac = ((srcw&~1)<<12)/videow, hfrac = ((srch&~1)<<12)/videoh;
        for(uint dy = 0, y = 0, yi = 0; dy < videoh; dy += 2)
        {
            uint yn = y + hfrac, yin = yn>>12, h = yn - y, ih = yin - yi, ylow, yhigh;
            if(yi < yin) { ylow = 0x1000U - (y&0xFFFU); yhigh = yn&0xFFFU; }
            else { ylow = yn - y; yhigh = 0; }

            uint y2n = yn + hfrac, y2in = y2n>>12, h2 = y2n - yn, ih2 = y2in - yin, y2low, y2high;
            if(yin < y2in) { y2low = 0x1000U - (yn&0xFFFU); y2high = y2n&0xFFFU; }
            else { y2low = y2n - yn; y2high = 0; }
            y = y2n;
            yi = y2in;

            const uchar *src2 = src + ih*stride;
            uchar *ydst = yplane, *ydst2 = yplane + ystride, *udst = uplane, *vdst = vplane;
            for(uint dx = 0, x = 0, xi = 0; dx < videow; dx += 2)
            {
                uint xn = x + wfrac, xin = xn>>12, w = xn - x, iw = xin - xi, xlow, xhigh;
                if(xi < xin) { xlow = 0x1000U - (x&0xFFFU); xhigh = xn&0xFFFU; }
                else { xlow = xn - x; xhigh = 0; }

                uint x2n = xn + wfrac, x2in = x2n>>12, w2 = x2n - xn, iw2 = x2in - xin, x2low, x2high;
                if(xin < x2in) { x2low = 0x1000U - (xn&0xFFFU); x2high = x2n&0xFFFU; }
                else { x2low = x2n - xn; x2high = 0; }

                uint b1, g1, r1, b2, g2, r2, b3, g3, r3, b4, g4, r4;
                boxsample(&src[xi<<2], stride, w, iw, h, ih, xlow, xhigh, ylow, yhigh, b1, g1, r1);
                boxsample(&src[xin<<2], stride, w2, iw2, h, ih, x2low, x2high, ylow, yhigh, b2, g2, r2);
                boxsample(&src2[xi<<2], stride, w, iw, h2, ih2, xlow, xhigh, y2low, y2high, b3, g3, r3);
                boxsample(&src2[xin<<2], stride, w2, iw2, h2, ih2, x2low, x2high, y2low, y2high, b4, g4, r4);

                // 0.299*R + 0.587*G + 0.114*B
                *ydst++ = (1225*r1 + 2404*g1 + 467*b1)>>12;
                *ydst++ = (1225*r2 + 2404*g2 + 467*b2)>>12;
                *ydst2++ = (1225*r3 + 2404*g3 + 467*b3)>>12;
                *ydst2++ = (1225*r4 + 2404*g4 + 467*b4)>>12;

                uint b = b1 + b2 + b3 + b4,
                     g = g1 + g2 + g3 + g4,
                     r = r1 + r2 + r3 + r4;
                // U = 0.500*B - 0.169*R - 0.331*G
                // V = 0.500*R - 0.419*G - 0.081*B
                // note: weights here are scaled by 1<<10, as opposed to 1<<12, since r/g/b are already *4
                *udst++ = ((128<<12) + 512*b - 173*r - 339*g)>>12;
                *vdst++ = ((128<<12) + 512*r - 429*g - 83*b)>>12;

                x = x2n;
                xi = x2in;
            }

            src = src2 + ih2*stride;
            yplane += 2*ystride;
            uplane += uvstride;
            vplane += uvstride;
        }
    }

    bool writevideoframe(const uchar *pixels, uint srcw, uint srch, int duplicates)
    {
        if(duplicates <= 0) return true;
      
        encodeyuv(pixels, srcw, srch);
 
        long offset = f->tell() - chunkoffsets[chunkdepth]; // as its relative to movi
        loopi(duplicates) index.add(offset);
        videoframes += duplicates;

        const uint planesize = videow * videoh;
        if(f->tell() + planesize*2 > 1000*1000*1000 && !open()) return false; // check for overflow of 1Gb limit
        startchunk("00dc");
        f->write(yuv, planesize*2);
        endchunk(); // 00dc
        
        return true;
    }
    
};

VAR(moviesync, 0, 0, 1);

namespace recorder 
{
    static enum { REC_OK = 0, REC_USERHALT, REC_TOOSLOW, REC_FILERROR } state = REC_OK;
    
    static aviwriter *file = NULL;
    static int starttime = 0;
   
    enum { MAXBUFFERS = 2 };
 
    struct moviebuffer 
    {
        uchar *video;
        uint videow, videoh, videobpp;
        uchar *sound;
        uint soundmax, soundlength;
        int totalmillis;

        moviebuffer() : video(NULL), sound(NULL) {}
        ~moviebuffer() { cleanup(); }

        void initvideo(int w, int h, int bpp)
        {
            DELETEA(video);
            videow = w;
            videoh = h;
            videobpp = bpp;
            video = new uchar[w*h*bpp];
        }
         
        void initsound(int bufsize)
        {
            DELETEA(sound);
            soundmax = bufsize;
            sound = new uchar[bufsize];
            soundlength = 0;
        }
            
        void cleanup()
        {
            DELETEA(video);
            DELETEA(sound);
        }
    };
    static queue<moviebuffer, MAXBUFFERS> buffers;

    static SDL_Thread *thread = NULL;
    static SDL_mutex *lock = NULL;
    static SDL_cond *shouldencode = NULL, *shouldread = NULL;

    bool isrecording() { return file != NULL; }
    
    int videoencoder(void *data) // runs on a separate thread
    {
        for(bool encoded = false;; encoded = true)
        {   
            SDL_LockMutex(lock);
            if(encoded) buffers.remove();
            SDL_CondSignal(shouldread);
            while(buffers.empty() && state == REC_OK) SDL_CondWait(shouldencode, lock);
            if(state != REC_OK) { SDL_UnlockMutex(lock); break; }
            moviebuffer &m = buffers.removing();
            SDL_UnlockMutex(lock);
            
            uint nextframenum = ((m.totalmillis - starttime)*file->videofps)/1000;
            //printf("frame %d->%d: sound = %d bytes\n", file->videoframes, nextframenum, m.soundlength);
            if(nextframenum > min((uint)10, file->videofps) + file->videoframes) state = REC_TOOSLOW;
            else if(!file->writevideoframe(m.video, m.videow, m.videoh, nextframenum-file->videoframes)) state = REC_FILERROR;
            
            m.soundlength = 0; // flush buffer and prepare for more sound
        }
        
        return 0;
    }
    
    void soundencoder(void *udata, Uint8 *stream, int len) // callback occurs on a separate thread
    {
        SDL_LockMutex(lock);
        moviebuffer &m = buffers.full() ? buffers.added() : buffers.adding(); // add sound to current movie frame
        if(m.soundlength + len > m.soundmax) 
        {    
            while(m.soundlength + len > m.soundmax) m.soundmax *= 2;
            uchar *newbuff = new uchar[m.soundmax];
            memcpy(newbuff, m.sound, m.soundlength);
            delete [] m.sound;
            m.sound = newbuff;
        }
        memcpy(m.sound+m.soundlength, stream, len);
        m.soundlength += len;
        SDL_UnlockMutex(lock);
    }
    
    void start(const char *filename, int videofps, int videow, int videoh) 
    {
        if(file) return;
        
        int fps, bestdiff, worstdiff;
        getfps(fps, bestdiff, worstdiff);
        if(videofps > fps) conoutf(CON_WARN, "frame rate may be too low to capture at %d fps", videofps);
        
        if(videow%2) videow += 1;
        if(videoh%2) videoh += 1;

        file = new aviwriter(filename, videow, videoh, videofps);
        if(!file->open()) 
        { 
            conoutf("unable to create file %s", filename);
            DELETEP(file);
            return;
        }
        conoutf("movie recording to: %s %dx%d @ %dfps", file->filename, file->videow, file->videoh, file->videofps);
        
        starttime = totalmillis;
        
        buffers.clear();
        loopi(MAXBUFFERS)
        {
            buffers.data[i].initvideo(max(file->videow, (uint)screen->w), max(file->videoh, (uint)screen->h), 4);
            buffers.data[i].initsound(4096);
            buffers.data[i].totalmillis = 0;
        }
        lock = SDL_CreateMutex();
        shouldencode = SDL_CreateCond();
        shouldread = SDL_CreateCond();
        thread = SDL_CreateThread(videoencoder, NULL); 
        Mix_SetPostMix(soundencoder, NULL);
    }
    
    void stop()
    {
        if(!file) return;
        if(state == REC_OK) state = REC_USERHALT;
        Mix_SetPostMix(NULL, NULL);
        
        SDL_LockMutex(lock); // wakeup thread enough to kill it
        SDL_CondSignal(shouldencode);
        SDL_UnlockMutex(lock);
        
        SDL_WaitThread(thread, NULL); // block until thread is finished
        
        loopi(MAXBUFFERS) buffers.data[i].cleanup();

        SDL_DestroyMutex(lock);
        SDL_DestroyCond(shouldencode);
        SDL_DestroyCond(shouldread);

        lock = NULL;
        shouldencode = shouldread = NULL;
        thread = NULL;
 
        static const char *mesgs[] = { "ok", "stopped", "computer too slow", "file error"};
        conoutf("movie recording halted: %s, %d frames", mesgs[state], file->videoframes);
        
        DELETEP(file);
        state = REC_OK;
    }
    
    bool readbuffer()
    {
        if(!file) return false;
        if(state != REC_OK)
        {
            stop();
            return false;
        }
        SDL_LockMutex(lock);
        if(moviesync && buffers.full()) SDL_CondWait(shouldread, lock);
        if(!buffers.full())
        {
            moviebuffer &m = buffers.adding();
            SDL_UnlockMutex(lock);

            if((uint)screen->w != m.videow || (uint)screen->h != m.videoh) m.initvideo(screen->w, screen->h, 4);
            m.totalmillis = totalmillis;
            
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            glReadPixels(0, 0, screen->w, screen->h, GL_BGRA, GL_UNSIGNED_BYTE, m.video);
            
            SDL_LockMutex(lock);
            buffers.add();
            SDL_CondSignal(shouldencode);
        }
        SDL_UnlockMutex(lock);
        return true;
    }

    void drawhud()
    {
        int w = screen->w, h = screen->h;

        gettextres(w, h);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, w, h, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        defaultshader->set();

        glPushMatrix();
        glScalef(1/3.0f, 1/3.0f, 1);
    
        double totalsize = double(file->videoframes) * double(file->videow * file->videoh * 4);
        const char *unit = "KB";
        if(totalsize >= 1e9) { totalsize /= 1e9; unit = "GB"; }
        else if(totalsize >= 1e6) { totalsize /= 1e6; unit = "MB"; }
        else totalsize /= 1e3;

        draw_textf("recorded %.1f%s", w*3-7*FONTH-FONTH/2, h*3-FONTH-FONTH*3/2, totalsize, unit);

        glPopMatrix();

        glDisable(GL_TEXTURE_2D);
        glDisable(GL_BLEND);
    }

    void capture()
    {
        if(readbuffer()) drawhud();
    }
}

VARP(moview, 0, 320, 10000);
VARP(movieh, 0, 240, 10000);
VARP(moviefps, 1, 24, 1000);

void movie(char *name)
{
    if(name[0] == '\0') recorder::stop();
    else if(!recorder::isrecording()) recorder::start(name, moviefps, moview ? moview : screen->w, movieh ? movieh : screen->h);
}

COMMAND(movie, "s");


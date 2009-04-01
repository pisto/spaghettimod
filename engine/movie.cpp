// records video to uncompressed avi files (will split across multiple files once size exceeds 1Gb)
// - people should post process the files because they will get large very rapidly

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
        endchunk(); // RIFF AVI

        delete f;
        f = NULL;
    }
    
    aviwriter(const char *name, uint w, uint h, uint fps) : f(NULL), yuv(NULL), videoframes(0), filesequence(0), videow(w), videoh(h), videofps(fps)
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
        
        chunkdepth = -1;
        
        listchunk("RIFF", "AVI ");
        
        listchunk("LIST", "hdrl");
        
        startchunk("avih");
        f->putlil<uint>(1000000 / videofps); // microsecsperframe
        f->putlil<uint>(0); // maxbytespersec
        f->putlil<uint>(0); // reserved
        f->putlil<uint>(0); // flags
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
        f->putlil<uint>(0); // flags
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
    
    bool writevideoframe(uchar *pixels, int duplicates)
    {
        if(duplicates <= 0) return true;
        
        const uint planesize = videow * videoh;
        if(!yuv) yuv = new uchar[planesize*2];
        uchar *yplane = yuv;
        uchar *uplane = yuv + planesize;
        uchar *vplane = yuv + planesize + (planesize >> 2);
        
        const int bpp = 3;
        const bool flip = true;
        const uint pitch = videow*bpp;
        const uchar *rgb = pixels;
        
        for(uint y = 0; y < videoh; y++)
        {
            if(flip) rgb = pixels + ((videoh-1-y)*pitch);
            for(uint x = 0; x < videow; x++) // y at full resolution
            {
                *(yplane++) = (uchar)(((int)(30*rgb[0]) + (int)(59*rgb[1]) + (int)(11*rgb[2]))/100);
                rgb += bpp;
            }
            if((y&1) == 0) // uv at half resolution
            {
                for(uint x = 0; x < videow; x+=2)
                {
                    uchar r = (rgb[0] + rgb[bpp] + rgb[pitch] + rgb[pitch+bpp]) >> 2;
                    uchar g = (rgb[1] + rgb[bpp+1] + rgb[pitch+1] + rgb[pitch+bpp+1]) >> 2;
                    uchar b = (rgb[2] + rgb[bpp+2] + rgb[pitch+2] + rgb[pitch+bpp+2]) >> 2;
                    
                    *(uplane++) = (uchar)(((int)(-17*r) - (int)(33*g) + (int)(50*b)+12800)/100);
                    *(vplane++) = (uchar)(((int)(50*r) - (int)(42*g) - (int)(8*b)+12800)/100);
                    rgb += bpp*2;
                }
            }
        }
        
        if(f->tell() + planesize*2*duplicates > 1000000000 && !open()) return false; // check for overflow of 1Gb limit
                
        loopi(duplicates)
        {
            startchunk("00dc");
            f->write(yuv, planesize*2);
            endchunk(); // 00dc
            videoframes++;
        }
        
        return true;
    }
    
};

enum { REC_OK = 0, REC_USERHALT, REC_GAMEHALT, REC_TOOSLOW, REC_FILERROR };
    
namespace recorder {
    uint state = REC_OK;
    
    aviwriter *file;
    int starttime;
    
    SDL_Thread *thread;   
    
    enum { QLEN = 2 };
    struct movie_t 
    {
        uchar *video;
        uchar *sound;
        uint soundmax;
        uint soundlength;
        int totalmillis;
    } buffer[QLEN];

    SDL_mutex *lock;
    SDL_cond *notfull, *notempty;
    bool empty, full;
    int head, tail;
    
    bool isrecording() { return (file != NULL); }
    
    int videoencoder(void * data) // runs on a separate thread
    {
        while(true)
        {   
            SDL_LockMutex(lock);
            while(empty) SDL_CondWait(notempty, lock);
            movie_t &m = buffer[tail]; // dequeue
            tail = (tail+1)%QLEN; 
            empty = (head==tail);
            full = false;
            SDL_UnlockMutex(lock);
            SDL_CondSignal(notfull);
            
            if(state != REC_OK) break;
            
            int nextframenum = ((m.totalmillis - starttime)*file->videofps)/1000;
            //printf("frame %d->%d: sound = %d bytes\n", file->videoframes, nextframenum, m.soundlength);
            if(nextframenum > (file->videofps+file->videoframes)) state = REC_TOOSLOW;
            else if(!file->writevideoframe(m.video, nextframenum-file->videoframes)) state = REC_FILERROR;
            
            m.soundlength = 0; // flush buffer and prepare for more sound
        }
        
        return 0;
    }
    
    void soundencoder(void *udata, Uint8 *stream, int len) // callback occurs on a separate thread
    {
        SDL_LockMutex(lock);
        movie_t &m = buffer[head]; // add sound to current movie frame
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
    
    void start(const char *filename, int videofps) {
        if(file) return;
        
        int fps, bestdiff, worstdiff;
        getfps(fps, bestdiff, worstdiff);
        if(videofps > fps) { conoutf("unable to capture at %d fps", videofps); return; }
        
        file = new aviwriter(filename, screen->w, screen->h, videofps);
        if(!file->open()) 
        { 
            conoutf("unable to create file %s", filename);
            delete file;
            file = NULL;
            return;
        }
        conoutf("movie recording to: %s %dx%d @ %dfps", file->filename, file->videow, file->videoh, file->videofps);
        
        starttime = totalmillis;
        
        loopi(QLEN)
        {
            movie_t &m = buffer[i];
            m.video = new uchar[file->videow*file->videoh*3];
            m.soundmax = 4096;
            m.sound = new uchar[m.soundmax];
            m.soundlength = 0;
        }
        head = tail = 0;
        empty = true;
        full = false;
        lock = SDL_CreateMutex();
        notfull = SDL_CreateCond();
        notempty = SDL_CreateCond();
        thread =  SDL_CreateThread(videoencoder, NULL); 
        Mix_SetPostMix(soundencoder, NULL);
    }
    
    void stop()
    {
        if(!file) return;
        if(state == REC_OK) state = REC_USERHALT;
        Mix_SetPostMix(NULL, NULL);
        
        SDL_LockMutex(lock); // wakeup thread enough to kill it
        empty = false;
        SDL_UnlockMutex(lock);
        SDL_CondSignal(notempty);
        
        SDL_WaitThread(thread, NULL); // block until thread is finished
        
        loopi(QLEN)
        {
            movie_t &m = buffer[i];
            delete [] m.sound;
            delete [] m.video;
        }
        SDL_DestroyMutex(lock);
        SDL_DestroyCond(notfull);
        SDL_DestroyCond(notempty);
        
        const char *mesgs[] = { "ok", "stopped", "game state change", "computer too slow", "file error"};
        conoutf("movie recording halted: %s, %d frames", mesgs[state], file->videoframes);
        
        delete file;
        file = NULL;
        state = REC_OK;
    }
    
    bool readbuffer()
    {
        if(!file) return false;
        if(screen->w != (int)file->videow || screen->h != (int)file->videoh) state = REC_GAMEHALT;
        if(state != REC_OK)
        {
            stop();
            return false;
        }
        movie_t &m = buffer[head];
        m.totalmillis = totalmillis;
        // note: Apple's guidelines suggest reading XRGBA to match the raw framebuffer format - is this valid on intel or elsewhere?
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, file->videow, file->videoh, GL_RGB, GL_UNSIGNED_BYTE, m.video);
        return true;
    }
    
    void sendbuffer()
    {
        SDL_LockMutex(lock);
        while(full) SDL_CondWait(notfull, lock);
        head = (head+1)%QLEN; // enqueue
        full = (head==tail);
        empty = false;
        SDL_UnlockMutex(lock);
        SDL_CondSignal(notempty);
    }
}

void movie(char *name, int *fps)
{
    if(name[0] == '\0') recorder::stop();
    else if(!recorder::isrecording()) recorder::start(name, (*fps > 0)?(*fps):30);
}

COMMAND(movie, "si");

void glswapbuffers()
{
    bool recording = recorder::readbuffer();
    SDL_GL_SwapBuffers();
    if(recording) recorder::sendbuffer();
}


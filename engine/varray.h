struct varray
{
    enum
    {
        ATTRIB_VERTEX    = 1<<0,
        ATTRIB_COLOR     = 1<<1,
        ATTRIB_NORMAL    = 1<<2,
        ATTRIB_TEXCOORD0 = 1<<3,
        ATTRIB_TEXCOORD1 = 1<<4,
        MAXATTRIBS       = 5
    };
    struct attribinfo
    {
        int type, size, formatsize;
        GLenum format;

        attribinfo() : type(0), size(0), formatsize(0), format(GL_FALSE) {}

        bool operator==(const attribinfo &a) const
        {
            return type == a.type && size == a.size && format == a.format;
        }
        bool operator!=(const attribinfo &a) const
        {
            return type != a.type || size != a.size || format != a.format;
        }
    } attribs[MAXATTRIBS], lastattribs[MAXATTRIBS];
    vector<uchar> data;
    int enabled, numattribs, attribmask, numlastattribs, lastattribmask, vertexsize;
    GLenum primtype;
    uchar *lastbuf;
    bool changedattribs;

    varray() : enabled(0), numattribs(0), attribmask(0), numlastattribs(0), lastattribmask(0), vertexsize(0), primtype(GL_TRIANGLES), lastbuf(NULL), changedattribs(false) 
    {
    }

    void enable()
    {
        enabled = 0;
        numlastattribs = lastattribmask = 0;
        lastbuf = NULL;
    }

    void begin(GLenum mode)
    {
        primtype = mode;
    }

    void defattrib(int type, int size, GLenum format)
    {
        if(type == ATTRIB_VERTEX)
        {
            numattribs = 0;
            vertexsize = 0;
        }
        changedattribs = true;
        attribmask |= type;
        attribinfo &a = attribs[numattribs++];
        a.type = type;
        a.size = size;
        a.format = format;
        switch(format)
        {
            case GL_UNSIGNED_BYTE:  a.formatsize = 1; break;
            case GL_BYTE:           a.formatsize = 1; break;
            case GL_UNSIGNED_SHORT: a.formatsize = 2; break;
            case GL_SHORT:          a.formatsize = 2; break;
            case GL_UNSIGNED_INT:   a.formatsize = 4; break;
            case GL_INT:            a.formatsize = 4; break;
            case GL_FLOAT:          a.formatsize = 4; break;
            case GL_DOUBLE:         a.formatsize = 8; break;
            default:                a.formatsize = 0; break;
        }
        a.formatsize *= size;
        vertexsize += a.formatsize;
    }

    template<class T>
    void attrib(T x)
    {
        size_t len = sizeof(T);
        T *buf = (T *)data.reserve(len).buf;
        buf[0] = x;
        data.advance(len);
    }

    template<class T>
    void attrib(T x, T y)
    {
        size_t len = 2*sizeof(T);
        T *buf = (T *)data.reserve(len).buf;
        buf[0] = x;
        buf[1] = y;
        data.advance(len);
    }

    template<class T>
    void attrib(T x, T y, T z)
    {
        size_t len = 3*sizeof(T);
        T *buf = (T *)data.reserve(len).buf;
        buf[0] = x;
        buf[1] = y;
        buf[2] = z;
        data.advance(len);
    }

    template<class T>
    void attrib(T x, T y, T z, T w)
    {
        size_t len = 4*sizeof(T);
        T *buf = (T *)data.reserve(len).buf;
        buf[0] = x;
        buf[1] = y;
        buf[2] = z;
        buf[3] = w;
        data.advance(len);
    }

    template<size_t N, class T>
    void attribv(const T *v)
    {
        size_t len = N*sizeof(T);
        memcpy(data.reserve(len).buf, v, len);
        data.advance(len);
    }

    void setattrib(const attribinfo &a, uchar *buf)
    {
        switch(a.type)
        {
            case ATTRIB_VERTEX:
                if(!(enabled&a.type)) glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(a.size, a.format, vertexsize, buf);
                break;
            case ATTRIB_COLOR:
                if(!(enabled&a.type)) glEnableClientState(GL_COLOR_ARRAY);
                glColorPointer(a.size, a.format, vertexsize, buf);
                break;
            case ATTRIB_NORMAL:
                if(!(enabled&a.type)) glEnableClientState(GL_NORMAL_ARRAY);
                glNormalPointer(a.format, vertexsize, buf);
                break;
            case ATTRIB_TEXCOORD0:
                if(!(enabled&a.type)) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(a.size, a.format, vertexsize, buf);
                break;
            case ATTRIB_TEXCOORD1:
                glClientActiveTexture_(GL_TEXTURE1_ARB);
                if(!(enabled&a.type)) glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glTexCoordPointer(a.size, a.format, vertexsize, buf);
                glClientActiveTexture_(GL_TEXTURE0_ARB);
                break;
        }
        enabled |= a.type;
    }

    void unsetattrib(const attribinfo &a)
    {
        switch(a.type)
        {
            case ATTRIB_VERTEX:
                glDisableClientState(GL_VERTEX_ARRAY);
                break;
            case ATTRIB_COLOR:
                glDisableClientState(GL_COLOR_ARRAY);
                break;
            case ATTRIB_NORMAL:
                glDisableClientState(GL_NORMAL_ARRAY);
                break;
            case ATTRIB_TEXCOORD0:
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                break;
            case ATTRIB_TEXCOORD1:
                glClientActiveTexture_(GL_TEXTURE1_ARB);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glClientActiveTexture_(GL_TEXTURE0_ARB);
                break;
        }
    }

    int end()
    {
        if(data.empty()) return 0;
        uchar *buf = data.getbuf();
        bool forceattribs = numattribs != numlastattribs || buf != lastbuf;
        if(forceattribs || changedattribs) 
        {
            int diffmask = enabled & lastattribmask & ~attribmask;
            if(diffmask) loopi(numlastattribs)
            {
                const attribinfo &a = lastattribs[i];
                if(diffmask & a.type) unsetattrib(a);
            }
            loopi(numattribs)
            {
                const attribinfo &a = attribs[i];
                if(forceattribs || a != lastattribs[i])
                {
                    setattrib(a, buf);
                    lastattribs[i] = a;
                }
                buf += a.formatsize;
            }
            lastbuf = buf;
            numlastattribs = numattribs;
            lastattribmask = attribmask;
            changedattribs = false;
        }
        int numvertexes = data.length()/vertexsize;
        glDrawArrays(primtype, 0, numvertexes);
        data.setsizenodelete(0);
        return numvertexes;
    }

    void disable()
    {
        if(enabled&ATTRIB_VERTEX) glDisableClientState(GL_VERTEX_ARRAY);
        if(enabled&ATTRIB_COLOR) glDisableClientState(GL_COLOR_ARRAY);
        if(enabled&ATTRIB_NORMAL) glDisableClientState(GL_NORMAL_ARRAY);
        if(enabled&ATTRIB_TEXCOORD0) glDisableClientState(GL_TEXTURE_COORD_ARRAY);
        if(enabled&ATTRIB_TEXCOORD1)
        {
            glClientActiveTexture_(GL_TEXTURE1_ARB);
            glDisableClientState(GL_TEXTURE_COORD_ARRAY);
            glClientActiveTexture_(GL_TEXTURE0_ARB);
        }
        enabled = 0;
    }
};


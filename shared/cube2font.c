#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <png.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_STROKER_H
#include FT_GLYPH_H

typedef unsigned char uchar; 
typedef unsigned short ushort;
typedef unsigned int uint;

void fatal(const char *fmt, ...)    // failure exit
{
    va_list v;
    va_start(v, fmt);
    vfprintf(stderr, fmt, v);
    va_end(v);
    fputc('\n', stderr);

    exit(EXIT_FAILURE);
}

void savepng(uchar *pixels, int w, int h, const char *name)
{
    FILE *f = fopen(name, "wb");
    int y;
    png_structp p;
    png_infop i;
    if(!f) goto failed;
    p = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if(!p) goto failed;
    i = png_create_info_struct(p);
    if(!i) goto failed;
    if(!setjmp(png_jmpbuf(p)))
    {
        png_init_io(p, f);
        png_set_compression_level(p, 9);
        png_set_IHDR(p, i, w, h, 8, PNG_COLOR_TYPE_GRAY_ALPHA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
        png_write_info(p, i);
        for(y = 0; y < h; y++)
        {
            png_write_row(p, &pixels[y*w*2]);
        }
        png_write_end(p, NULL);
        png_destroy_write_struct(&p, &i);
        fclose(f);
        return;
    }
failed:
    fatal("cube2font: failed writing %s", name);
}

int iswinprint(int c)
{
    static const char flags[256] =
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
        1, 0, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 1, 0,
        0, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 0, 1, 1,
        0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
    };
    return flags[c];
}
        
int win2uni(int c)
{
    static const int conv[256] =
    {
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
        16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
        32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
        48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
        64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
        80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
        96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111,
        112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127,
        0x20AC, 129, 0x201A, 0x192, 0x201E, 0x2026, 0x2020, 0x2021, 
        0x2C6, 0x2030, 0x160, 0x2039, 0x152, 142, 0x17D, 143,
        144, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
        0x2DC, 0x2122, 0x161, 0x203A, 0x153, 157, 0x17E, 0x178,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
    };
    return conv[c];
}

int uni2win(int c)
{
    switch(c)
    {
    case 0x20AC: return 128;
    case 0x201A: return 130;
    case 0x192: return 131;
    case 0x201E: return 132;
    case 0x2026: return 133;
    case 0x2020: return 134;
    case 0x2021: return 135;

    case 0x2C6: return 136;
    case 0x2030: return 137;
    case 0x160: return 138;
    case 0x2039: return 139;
    case 0x152: return 140;
    case 0x17D: return 142;

    case 0x2018: return 145;
    case 0x2019: return 146;
    case 0x201C: return 147;
    case 0x201D: return 148;
    case 0x2022: return 149;
    case 0x2013: return 150;
    case 0x2014: return 151;

    case 0x2DC: return 152;
    case 0x2122: return 153;
    case 0x161: return 154;
    case 0x203A: return 155;
    case 0x153: return 156;
    case 0x17E: return 158;
    case 0x178: return 159;

    default: 
        return c < 128 || (c >= 160 && c <= 255) ? c : 0x1A;
    }
}

struct fontchar { int code, tex, x, y, w, h, offx, offy, advance; FT_BitmapGlyph color, alpha; };

char *texname(const char *name, int texnum)
{
    static char file[256];
    snprintf(file, sizeof(file), "%s%d.png", name, texnum);
    return file;
}

void writetexs(const char *name, struct fontchar *chars, int numchars, int tw, int th)
{
    int i, x, y, start = 0;
    uchar *pixels = (uchar *)calloc(tw*th, 2), *dst, *src;
    if(!pixels) fatal("cube2font: failed allocating textures");
    for(i = 0; i < numchars; i++)
    {
        struct fontchar *c = &chars[i];
        if(c->tex != chars[start].tex)
        {
            const char *file = texname(name, chars[start].tex);
            printf("cube2font: writing %d chars to %s\n", i - start, file);
            savepng(pixels, tw, th, file);
            memset(pixels, 0, tw*th*2);
            start = i;
        }
        dst = &pixels[2*((c->y + c->alpha->top - c->color->top)*tw + c->x + c->color->left - c->alpha->left)];
        src = (uchar *)c->color->bitmap.buffer;
        for(y = 0; y < c->color->bitmap.rows; y++)
        {
            for(x = 0; x < c->color->bitmap.width; x++)
                dst[2*x] = src[x];
            src += c->color->bitmap.pitch;
            dst += 2*tw;
        }
        dst = &pixels[2*(c->y*tw + c->x)];
        src = (uchar *)c->alpha->bitmap.buffer;
        for(y = 0; y < c->alpha->bitmap.rows; y++)
        {
            for(x = 0; x < c->alpha->bitmap.width; x++)
                dst[2*x+1] = src[x];
            src += c->alpha->bitmap.pitch;
            dst += 2*tw;
        }
    }
    {
        const char *file = texname(name, chars[start].tex);
        printf("cube2font: writing %d chars to %s\n", i - start, file);
        savepng(pixels, tw, th, file);
    }
    free(pixels);
}

void writecfg(const char *name, struct fontchar *chars, int numchars, int x1, int y1, int x2, int y2, int space)
{
    FILE *f;
    char file[256];
    int i, lastcode = 33, lasttex = 0;
    snprintf(file, sizeof(file), "%s.cfg", name);
    f = fopen(file, "w");
    if(!f) fatal("cube2font: failed writing %s", file);
    printf("cube2font: writing %d chars to %s\n", numchars, file);
    fprintf(f, "font \"%s\" \"%s\" %d %d\n", name, texname(name, 0), space, y2-y1);
    for(i = 0; i < numchars; i++)
    {
        struct fontchar *c = &chars[i];
        while(lastcode < c->code)
        {
            fprintf(f, "fontchar    // 0x%02X\n", lastcode);
            lastcode++;
        }    
        if(lasttex != c->tex)
        {
            fprintf(f, "\nfonttex \"%s\"\n", texname(name, c->tex));
            lasttex = c->tex;
        }
        fprintf(f, c->code < 128 ? "fontchar %d %d %d %d %d %d %d    // %c\n" : "fontchar %d %d %d %d %d %d %d    // 0x%02X\n", c->x, c->y, c->w, c->h, c->offx, y2-c->offy, c->advance, c->code);
        lastcode++;
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    FT_Library l;
    FT_Face f;
    FT_Stroker s;
    int i, pad, offset, advance, w, h, tw, th, c, rw = 0, rh = 0, ry = 0, x1 = INT_MAX, x2 = INT_MIN, y1 = INT_MAX, y2 = INT_MIN, w2 = 0, h2 = 0;
    float border;
    struct fontchar chars[256];
    int numchars = 0, numtex = 0;
    if(argc < 11)
        fatal("Usage: cube2font infile outfile border pad offset advance charwidth charheight texwidth texheight");
    border = atof(argv[3]);
    pad = atoi(argv[4]);
    offset = atoi(argv[5]);
    advance = atoi(argv[6]);
    w = atoi(argv[7]);
    h = atoi(argv[8]);
    tw = atoi(argv[9]);
    th = atoi(argv[10]);
    if(FT_Init_FreeType(&l))
        fatal("cube2font: failed initing freetype");
    if(FT_New_Face(l, argv[1], 0, &f) ||
       FT_Set_Charmap(f, f->charmaps[0]) ||
       FT_Set_Pixel_Sizes(f, w, h) ||
       FT_Stroker_New(l, &s))
        fatal("cube2font: failed loading font %s", argv[1]);
    FT_Stroker_Set(s, (FT_Fixed)(border * 64), FT_STROKER_LINECAP_ROUND, FT_STROKER_LINEJOIN_ROUND, 0);
    for(c = 33; c < 256; c++)
    {
        int uni;
        FT_Glyph p, p2;
        FT_BitmapGlyph b, b2;
        struct fontchar *dst;
        if(!iswinprint(c)) continue;
        uni = win2uni(c);
        if(FT_Load_Char(f, uni, FT_LOAD_DEFAULT))
            fatal("cube2font: failed loading character %d", c);
        FT_Get_Glyph(f->glyph, &p);
        p2 = p; 
        FT_Glyph_StrokeBorder(&p, s, 0, 0);
        FT_Glyph_To_Bitmap(&p, FT_RENDER_MODE_NORMAL, 0, 1);
        FT_Glyph_To_Bitmap(&p2, FT_RENDER_MODE_NORMAL, 0, 1);
        b = (FT_BitmapGlyph)p;
        b2 = (FT_BitmapGlyph)p2;
        dst = &chars[numchars++];
        dst->code = c;
        if(rw + b->bitmap.width > tw)
        {
            ry += rh + pad;
            rw = rh = 0;
        } 
        if(ry + b->bitmap.rows > th)
        {
            ry = rw = rh = 0;
            numtex++;
        }
        dst->tex = numtex;
        dst->offx = b->left + offset;
        dst->offy = b->top;
        dst->advance = offset + ((p->advance.x+0xFFFF)>>16) + advance;
        dst->x = rw;
        dst->y = ry;
        dst->w = b->bitmap.width;
        dst->h = b->bitmap.rows;
        dst->alpha = b;
        dst->color = b2;
        rw += b->bitmap.width + pad;
        if(b->bitmap.rows > rh) rh = b->bitmap.rows;
        if(b->top - b->bitmap.rows < y1) y1 = b->top - b->bitmap.rows;
        if(b->top > y2) y2 = b->top;
        if(b->left < x1) x1 = b->left;
        if(b->left + b->bitmap.width > x2) x2 = b->left + b->bitmap.width;
        if(b->bitmap.width > w2) w2 = b->bitmap.width;
        if(b->bitmap.rows > h2) h2 = b->bitmap.rows;
    }
    if(FT_Load_Char(f, ' ', FT_LOAD_DEFAULT))
        fatal("cube2font: failed loading space character");
    writetexs(argv[2], chars, numchars, tw, th);
    writecfg(argv[2], chars, numchars, x1, y1, x2, y2, (f->glyph->advance.x+0x3F)>>6);
    for(i = 0; i < numchars; i++)
    {
        FT_Done_Glyph((FT_Glyph)chars[i].alpha);
        FT_Done_Glyph((FT_Glyph)chars[i].color);
    }
    FT_Stroker_Done(s);
    FT_Done_FreeType(l);
    printf("cube2font: (%d, %d) .. (%d, %d) = (%d, %d) / (%d, %d), %d texs\n", x1, y1, x2, y2, x2 - x1, y2 - y1, w2, h2, numtex + (rh > 0 ? 1 : 0));
    return EXIT_SUCCESS;
}


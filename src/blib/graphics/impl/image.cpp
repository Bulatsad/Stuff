#include <blib/graphics/image.h>

#include <blib/utilmacro.h>
#include <blib/blibint.h>
#include <blib/inline.h>

#include <blib/core/file.h>

#include <fstream>

#define TGX_TOKEN_TYPE_PIXELSTREAM            0x00
#define TGX_TOKEN_TYPE_REPEATINGPIXELS        0x02
#define TGX_TOKEN_TYPE_TRANSPARENTPIXELSTRING 0x01
#define TGX_TOKEN_TYPE_NEWLINE                0x04

typedef struct
{
    buint16 width;
    buint16 height;
}tgx_header_t;

__blib_private_func __blib_force_inline bool tgx_header_parse_from_memory(_In const void* pdata, _In const size_t size, _Out tgx_header_t* header)
{
    if (size < sizeof(tgx_header_t))
        return false;

    header->width = *((buint16*)pdata);
    header->height = *((buint16*)pdata + 2);

    return true;
}

__blib_private_func __blib_force_inline void parseTGXColors(
    _In const buint16* ptgxPixel,
    _Out buint8* pR,
    _Out buint8* pG,
    _Out buint8* pB
)
{
    *pR = ((*((buint16*)ptgxPixel) >> 10) & 0x1F) << 3;
    *pG = ((*((buint16*)ptgxPixel) >> 5) & 0x1F) << 3;
    *pB = (*((buint16*)ptgxPixel) & 0x1F) << 3;
}

bool blib::graphics::Image::loadTGXPixelData(
    /*_In*/ const buint8* pdata,
    /*_In*/ const size_t pdatasize,
    /*_In*/ buint8* pallete,
    /*_In*/ buint8 color,
    /*_In*/ buint16 nWidth,
    /*_In*/ buint16 nHeight //,
    //std::vector<blib::graphics::Color>& pPixelData
)
{
    buint8 len;
    buint8 type;
    buint16 y = nHeight - 1;
    buint16 x = 0;
    const buint8* pend = pdata + pdatasize;

    while (pdata < pend)
    {
        len = (*pdata & 0x1F) + 1;
        type = (*pdata) >> 5;
        pdata++;

        switch (type)
        {
        case TGX_TOKEN_TYPE_PIXELSTREAM:
        {
            for (uint8_t i = 0; i < len; i++, x++)
            {
                buint8 r = 0;
                buint8 g = 0;
                buint8 b = 0;
                buint8 a = 0xFF;

                if (pallete)
                {
                    parseTGXColors((buint16*)&(pallete[(256 * color + *pdata) << 1]), &r, &g, &b);
                    pdata++;
                }
                else
                {
                    parseTGXColors((const buint16*)pdata, &r, &g, &b);
                    pdata += 2;
                }
                
                this->bitmap[y * nWidth + x] = blib::graphics::Color(r, g, b, a);
            }
        }
        break;

        case TGX_TOKEN_TYPE_TRANSPARENTPIXELSTRING:
        {
            for (buint8 i = 0; i < len; i++, x++);
        }
        break;

        case TGX_TOKEN_TYPE_REPEATINGPIXELS:
        {
            buint8 r = 0;
            buint8 g = 0;
            buint8 b = 0;
            buint8 a = 0xFF;

            if (pallete)
            {
                parseTGXColors((buint16*)&(pallete[(256 * color + *pdata) << 1]), &r, &g, &b);
                pdata++;
            }
            else
            {
                parseTGXColors((const buint16*)pdata, &r, &g, &b);
                pdata += 2;
            }

            for (buint8 i = 0; i < len; i++, x++)
                this->bitmap[y * nWidth + x] = blib::graphics::Color(r, g, b, a);
        }
        break;

        case TGX_TOKEN_TYPE_NEWLINE:
        {
            x = 0;
            if (y == 0x0000)
            {
                // TODO : Logging
                //goto exit_failure;
                //return true;
            }
            else
                y--;
        }
        break;

        default:
            // TODO : Logging
            return false;
        }
    }

    return true;
}

blib::graphics::Image::Image()
{
    this->width = 0;
    this->height = 0;
}

blib::graphics::Image::Image(decltype(blib::graphics::Image::width) aWidth, decltype(blib::graphics::Image::height) aHeight, const Color* pdata)
{
    this->width = aWidth;
    this->height = aHeight;
    this->bitmap.resize(this->width * this->height);
    if (pdata)
    {
        for (decltype(blib::graphics::Image::width) i = 0; i < this->width; ++i)
        {
            for (decltype(blib::graphics::Image::height) j = 0; j < this->height; ++j)
            {
                (*this)[i][j] = pdata[i * this->width + j];
            }
        }
    }
}

blib::graphics::Image::~Image()
{
}

__blib_api std::vector<blib::graphics::Color>& blib::graphics::Image::data()
{
    return this->bitmap;
}

void blib::graphics::Image::create(buint16 a_width, buint16 a_height, blib::graphics::Color color)
{
    this->height = a_height;
    this->width = a_width;
    this->bitmap.resize(a_width * a_height, color);
}

bool blib::graphics::Image::loadFromTgx(const char* path)
{
    std::ifstream tempfin;
    tempfin.open(path, std::ifstream::in | std::ifstream::binary);

    if (!tempfin.is_open())
    {
        // TODO : Logging
        return false;
    }

    blib::core::File fin(std::move(tempfin));
    blib::core::ByteArray filedata = fin.readAll();

    tgx_header_t header;
    if (!tgx_header_parse_from_memory(&filedata[0], filedata.size(), &header))
        return false;

    this->width = header.width;
    this->height = header.height;

    this->bitmap.resize(header.height * header.width, blib::graphics::Color::BlackAlpha);

    if (!loadTGXPixelData(
        &filedata[8],
        (filedata.size() - 8),
        nullptr,
        0,
        header.width,
        header.height)
        )
        return false;

    return true;
}

const void* blib::graphics::Image::getData() const
{
    return &(this->bitmap[0]);
}

void blib::graphics::Image::update(decltype(blib::graphics::Image::width) posX, decltype(blib::graphics::Image::height) posY, const blib::graphics::Image& img)
{
    for (decltype(blib::graphics::Image::width) i = 0; img.width; ++i)
    {
        for (decltype(blib::graphics::Image::height) j = 0; img.height; ++j)
        {
            this->bitmap[(i + posX) * this->height + j + posY] = img.bitmap[i * img.height + j];
        }
    }
}

blib::core::UnsafeSlicer<blib::graphics::Color> blib::graphics::Image::operator[](buint16 index) 
{
    blib::core::UnsafeSlicer<blib::graphics::Color> slicer(this->bitmap.data(), height);
    return blib::core::UnsafeSlicer<blib::graphics::Color>(&(slicer[index]));
}

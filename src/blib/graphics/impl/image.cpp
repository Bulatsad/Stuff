#include <blib/graphics/image.h>

#include <blib/core/console/console.h>
#include <blib/utilmacro.h>
#include <blib/blibint.h>
#include <blib/inline.h>

#include <blib/core/fileStream.h>
#include <blib/core/verifyHelper.h>

#include <cstring>

namespace
{
    // Ключи JSON-объекта изображения (формат сериализации)
    constexpr const char* imageKeyWidth = "width";
    constexpr const char* imageKeyHeight = "height";
    constexpr const char* imageKeyPixels = "pixels";

    // Количество каналов цвета (RGBA)
    constexpr buint32 imageColorChannelCount = 4;
}

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
                // В данных больше строк развёртки, чем заявлено высотой
                // картинки — лишние строки игнорируем
                __blib_log_warning("TGX: image has more scanlines than its declared height, extra rows skipped");
                //goto exit_failure;
                //return true;
            }
            else
                y--;
        }
        break;

        default:
            __blib_log_error("TGX: unknown token type %u", static_cast<unsigned int>(type));
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
        // pdata — row-major (строка за строкой), как отдаёт stb_image.
        // (*this)[x][y] = bitmap[y * width + x] — та же раскладка.
        // Раньше стояло pdata[i * width + j] — транспонированное чтение:
        // квадратные картинки зеркалились, неквадратные давали мусор
        for (decltype(blib::graphics::Image::height) j = 0; j < this->height; ++j)
        {
            for (decltype(blib::graphics::Image::width) i = 0; i < this->width; ++i)
            {
                (*this)[i][j] = pdata[j * this->width + i];
            }
        }
    }
}

blib::graphics::Image::~Image()
{
}

__blib_graphics_api std::vector<blib::graphics::Color>& blib::graphics::Image::data()
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
    // Читаем файл целиком через FileStream (единый файловый поток blib)
    blib::core::FileStream fin;
    blib::core::FileStream::OpenModeFlags mode;
    mode.storage |= static_cast<buint8>(blib::core::OpenMode::Read);
    mode.storage |= static_cast<buint8>(blib::core::OpenMode::Binary);

    if (fin.open(path, mode) != blib::core::FileStatus::OK)
    {
        __blib_log_error("failed to open TGX file '%s'", path);
        return false;
    }

    blib::core::ByteArray filedata = fin.readAll();
    if (filedata.empty())
    {
        __blib_log_error("TGX file '%s' is empty", path);
        return false;
    }

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
    // Row-major: bitmap[y * width + x]. Вписываем img поверх позиции
    // (posX, posY); раньше здесь была транспонированная индексация
    for (decltype(blib::graphics::Image::width) i = 0; i < img.width; ++i)
    {
        for (decltype(blib::graphics::Image::height) j = 0; j < img.height; ++j)
        {
            this->bitmap[(j + posY) * this->width + (i + posX)] = img.bitmap[j * img.width + i];
        }
    }
}

blib::core::UnsafeSlicer<blib::graphics::Color> blib::graphics::Image::operator[](buint16 index) 
{
    // Row-major: image[x][y] = bitmap[y * width + x].
    // Срез стартует в столбце x строки 0 и шагает на width элементов
    // (переход к следующей строке). Раньше срез строился со stride
    // = height (column-major) — расходилось с TGX-загрузчиком
    // (bitmap[y * width + x]) и с загрузкой текстур из stb
    return blib::core::UnsafeSlicer<blib::graphics::Color>(
        &(this->bitmap[static_cast<size_t>(index)]),
        this->width);
}

const blib::core::UnsafeSlicer<blib::graphics::Color> blib::graphics::Image::operator[](buint16 index) const
{
    // Const-вариант для чтения: тот же row-major срез. UnsafeSlicer
    // хранит не-const base (контракт «небезопасного» API без проверок),
    // поэтому константность указателя снимается — доступ предполагается
    // только на чтение (const operator[] среза)
    return blib::core::UnsafeSlicer<blib::graphics::Color>(
        const_cast<blib::graphics::Color*>(&(this->bitmap[static_cast<size_t>(index)])),
        this->width);
}

blib::core::json::JsonValue blib::graphics::Image::toJson() const
{
    blib::core::json::JsonValue doc = blib::core::json::JsonValue::makeObject();

    doc.set(imageKeyWidth, blib::core::json::JsonValue(this->width));
    doc.set(imageKeyHeight, blib::core::json::JsonValue(this->height));

    // Плоский массив байт RGBA (row-major); пустое изображение — []
    blib::core::json::JsonValue& pixels =
        doc.set(imageKeyPixels, blib::core::json::JsonValue::makeArray());
    const size_t pixelBytes = static_cast<size_t>(this->width) *
        static_cast<size_t>(this->height) * imageColorChannelCount;
    if (pixelBytes > 0)
    {
        const buint8* raw = static_cast<const buint8*>(this->getData());
        for (size_t i = 0; i < pixelBytes; ++i)
        {
            pixels.pushBack(blib::core::json::JsonValue(raw[i]));
        }
    }

    return doc;
}

blib::core::LoadStatus blib::graphics::Image::fromJson(_In const blib::core::json::JsonValue& json)
{
    // Валидация формы ДО применения: при ошибке состояние не меняется
    if (!json.isObject() ||
        !json.has(imageKeyWidth) || !json.get(imageKeyWidth).isNumber() ||
        !json.has(imageKeyHeight) || !json.get(imageKeyHeight).isNumber() ||
        !json.has(imageKeyPixels))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    const buint32 loadedWidth = static_cast<buint32>(json.get(imageKeyWidth).asBuint64());
    const buint32 loadedHeight = static_cast<buint32>(json.get(imageKeyHeight).asBuint64());
    // Размеры обязаны влезать в buint16 (тип width/height)
    if (__blib_unlikely(loadedWidth > buint16Max || loadedHeight > buint16Max))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    const blib::core::json::JsonValue& pixelsArr = json.get(imageKeyPixels);
    if (__blib_unlikely(!pixelsArr.isArray() ||
        pixelsArr.size() != loadedWidth * loadedHeight * imageColorChannelCount))
    {
        return blib::core::LoadStatus::InvalidData;
    }

    // Все данные валидны — применить (bitmap собирается во временный
    // буфер, затем swap: аллокация тоже до применения)
    const size_t pixelCount = static_cast<size_t>(loadedWidth) * static_cast<size_t>(loadedHeight);
    std::vector<blib::graphics::Color> loadedBitmap;
    loadedBitmap.resize(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        const buint32 base = static_cast<buint32>(i) * imageColorChannelCount;
        loadedBitmap[i].red = static_cast<buint8>(pixelsArr[base].asBuint64());
        loadedBitmap[i].green = static_cast<buint8>(pixelsArr[base + 1].asBuint64());
        loadedBitmap[i].blue = static_cast<buint8>(pixelsArr[base + 2].asBuint64());
        loadedBitmap[i].alpha = static_cast<buint8>(pixelsArr[base + 3].asBuint64());
    }

    this->width = static_cast<buint16>(loadedWidth);
    this->height = static_cast<buint16>(loadedHeight);
    this->bitmap.swap(loadedBitmap);

    return blib::core::LoadStatus::None;
}

blib::core::SaveStatus blib::graphics::Image::save(_In blib::core::IOutputStream& os) const
{
    const blib::core::json::JsonValue doc = this->toJson();
    if (__blib_unlikely(doc.writeTo(os) != blib::core::json::JsonError::None))
    {
        return blib::core::SaveStatus::WriteFailed;
    }
    return blib::core::SaveStatus::None;
}

blib::core::LoadStatus blib::graphics::Image::load(_In blib::core::IInputStream& is)
{
    blib::core::json::JsonParser parser;
    blib::core::json::JsonValue doc;
    if (__blib_unlikely(parser.parse(is, doc) != blib::core::json::JsonError::None))
    {
        return blib::core::LoadStatus::InvalidData;
    }
    return this->fromJson(doc);
}

bool blib::graphics::Image::strongCompare(_In const blib::core::IStrongComparable& other,
    _In blib::core::CompareSession& session) const
{
    if (!session.enter(this, &other))
    {
        return true;
    }

    const blib::graphics::Image& o = static_cast<const blib::graphics::Image&>(other);

    if (this->width != o.width || this->height != o.height)
    {
        return false;
    }

    // Попиксельное сравнение RGBA (bitmap). Пустые изображения равны
    const size_t bytes = static_cast<size_t>(this->width) *
        static_cast<size_t>(this->height) * imageColorChannelCount;
    if (bytes == 0)
    {
        return true;
    }

    const void* dataThis = this->getData();
    const void* dataOther = o.getData();
    if (dataThis == nullptr || dataOther == nullptr)
    {
        return false;
    }
    return std::memcmp(dataThis, dataOther, bytes) == 0;
}

bool blib::graphics::Image::verify() const
{
    // Round-trip без RTTI (см. blib::core::verifyRoundTrip)
    return blib::core::verifyRoundTrip(*this);
}

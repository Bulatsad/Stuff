#pragma once

#include <vector>
#include <string>
#include <cstdint>

#include <blib/graphics/image.h>


namespace parsers
{
    class GM1File
    {
    public:
        enum class DataType : uint32_t
        {
            Interface = 0x00000001,   // TGX-данные, 16-битный цвет в токенах
            Animation = 0x00000002,   // TGX-данные, индексы палитры (1 байт/пиксель)
            Building = 0x00000003,    // 512 байт raw-тайла + TGX-часть (16 бит)
            Font = 0x00000004,        // TGX-данные, 16-битный цвет
            BitMap = 0x00000005,      // raw 16 бит, размер w x (h - 7)
            ConstSize = 0x00000006,   // TGX-данные, 16-битный цвет
            BitMap1 = 0x00000007      // raw 16 бит, размер w x h
        };

        struct Header
        {
            uint32_t quantity;
            DataType dataType;
            uint32_t dataSize;
        };

        // Заголовок кадра (имена полей — по референс-декомпилу Stronghold
        // Image Toolbox, см. GRAPHICS.md «Форматы»)
        struct ImageHeader
        {
            uint16_t width;
            uint16_t height;
            uint16_t horizontalOffset;
            uint16_t verticalOffset;
            uint8_t part;
            uint8_t subparts;
            uint16_t baseHeight;
            uint8_t direction;
            uint8_t horizontalStartOffset;
            uint8_t widthInGame;
            uint8_t performanceId;
        };

        std::vector<blib::graphics::Image> loadFromFile(const std::string& path);
        std::vector<blib::graphics::Image> parseFromMemory(const std::vector<uint8_t>& data);

        // Заголовки кадров файла (индексы совпадают с индексами images из
        // loadFromFile/parseFromMemory, пока ни один кадр не пропущен как
        // битый — см. .cpp). Потребителю (sc2img) нужны horizontalOffset/
        // verticalOffset для выравнивания кадров и остальные метаданные
        const std::vector<ImageHeader>& getImageHeaders() const;

        // Тайлы кадров типа Building (ромб 30x16, прозрачные углы):
        // индексы совпадают с images; для остальных типов записи пустые
        // (0x0). Тайл рисуется в игре на земле под зданием — композицию
        // «здание + земля» собирает потребитель (sc2img, см. SC2IMG.md)
        const std::vector<blib::graphics::Image>& getImageTiles() const;

        // Разобранный заголовок файла (количество кадров, Data_Type,
        // Data_Size) — потребителю нужен тип данных для пост-обработки
        // (композиция зданий в sc2img)
        const Header& getHeader() const;

        // Переопределение цвета игрока: палитра GM1 сегментирована по
        // игрокам (256 цветов на игрока). false — дефолтная палитра 0
        // (нейтральная); true — применить playerColorOverride ко всем
        // палитровым кадрам (UI-выбор цвета игрока в sc2img)
        bool playerColorOverridden = false;
        uint8_t playerColorOverride = 0;

    private:
        static const size_t PALETTE_SIZE = 5120;
        std::vector<uint8_t> palette;
        Header header;
        std::vector<ImageHeader> imageHeaders;
        std::vector<blib::graphics::Image> imageTiles;
    };
}

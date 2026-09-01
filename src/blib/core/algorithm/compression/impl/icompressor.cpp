#include <blib/core/algorithm/compression/icompressor.h>

#include <utility>

namespace blib
{
    namespace algorithm
    {
        namespace compression
        {
            void ICompressor::setSettings(CompressionSettings&& settings)
            {
                // Move-присваивание: аллокатор переносится без копирования
                this->settings = std::move(settings);
            }

            const CompressionSettings& ICompressor::getSettings() const
            {
                return this->settings;
            }
        }
    }
}

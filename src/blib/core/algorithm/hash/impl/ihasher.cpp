#include <blib/core/algorithm/hash/ihasher.h>

#include <utility>

namespace blib
{
    namespace algorithm
    {
        namespace hash
        {
            void IHasher::setSettings(HashSettings&& settings)
            {
                // Move-присваивание: аллокатор переносится без копирования
                this->settings = std::move(settings);
            }

            const HashSettings& IHasher::getSettings() const
            {
                return this->settings;
            }
        }
    }
}

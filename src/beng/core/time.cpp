#include <beng/core/time.h>
#include <chrono>

namespace beng
{
    namespace
    {
        // Количество наносекунд в одной секунде (steady_clock отдаёт наносекунды)
        constexpr double nanosPerSecond = 1000000000.0;
    }

    Time::Time()
        : deltaTime(0.0f)
        , totalTime(0.0f)
        , lastTick(std::chrono::steady_clock::now().time_since_epoch().count())
    {
    }

    void Time::tick()
    {
        buint64 currentTick = std::chrono::steady_clock::now().time_since_epoch().count();
        buint64 tickDelta = currentTick - lastTick;

        // Конвертировать наносекунды в секунды (steady_clock возвращает наносекунды)
        deltaTime = static_cast<float>(static_cast<double>(tickDelta) / nanosPerSecond);

        totalTime += deltaTime;
        lastTick = currentTick;
    }

    float Time::getFPS() const
    {
        if (deltaTime > 0.0f)
        {
            return 1.0f / deltaTime;
        }
        return 0.0f;
    }

} // namespace beng

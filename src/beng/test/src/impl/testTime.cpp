#include <blib/test/src/test.h>

#include <beng/core/time.h>

#include <thread>
#include <chrono>

BLIB_TEST_CASE("time: delta is zero before first tick")
{
    beng::Time time;

    BLIB_TEST_CHECK_CLOSE(time.getDeltaTime(), 0.0f, 0.0001f);
    BLIB_TEST_CHECK_CLOSE(time.getTotalTime(), 0.0f, 0.0001f);
}

BLIB_TEST_CASE("time: tick produces non-negative delta and growing total")
{
    beng::Time time;

    time.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    time.tick();

    float delta = time.getDeltaTime();
    float total = time.getTotalTime();

    BLIB_TEST_CHECK(delta >= 0.0f);
    BLIB_TEST_CHECK(total > 0.0f);
    // Дельта не может превысить суммарное время
    BLIB_TEST_CHECK(total >= delta);
}

BLIB_TEST_CASE("time: fps is positive after a real tick")
{
    beng::Time time;

    time.tick();
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    time.tick();

    float fps = time.getFPS();
    BLIB_TEST_CHECK(fps > 0.0f);
}

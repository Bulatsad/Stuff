#include <blib/test/src/test.h>

// Циклические очереди: lockless SPSC и MPSC на RWLocker
#include <blib/system/thread/circleQueue.h>
#include <blib/system/thread/multiProducerSingleConsumerCircleQueue.h>

#include <algorithm>
#include <thread>
#include <vector>

// ============================================================
// LocklessProducerConcumerCircleQueue (SPSC)
// Одиночный поток: механика кольца (roundtrip, переполнение,
// wrap-around). Контрактная пара потоков проверяется отдельно.
// ============================================================

BLIB_TEST_CASE("SPSC: push/pop round trip")
{
    blib::LocklessProducerConcumerCircleQueue<int> queue(4);

    BLIB_TEST_CHECK(queue.isEmpty());

    for (int i = 1; i <= 4; ++i)
        BLIB_TEST_CHECK(queue.push(i));

    BLIB_TEST_CHECK(!queue.isEmpty());

    for (int i = 1; i <= 4; ++i)
    {
        int out = 0;
        BLIB_TEST_REQUIRE(queue.pop(out));
        BLIB_TEST_CHECK(out == i);
    }

    BLIB_TEST_CHECK(queue.isEmpty());
}

BLIB_TEST_CASE("SPSC: overflow returns false without overwriting")
{
    blib::LocklessProducerConcumerCircleQueue<int> queue(2);

    BLIB_TEST_CHECK(queue.push(1));
    BLIB_TEST_CHECK(queue.push(2));

    // Буфер полон: push должен вернуть false, элемент не должен пройти
    BLIB_TEST_CHECK(!queue.push(3));

    int out = 0;
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 1);
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 2);
    BLIB_TEST_CHECK(!queue.pop(out));
}

BLIB_TEST_CASE("SPSC: wrap-around preserves order")
{
    blib::LocklessProducerConcumerCircleQueue<int> queue(8);

    // Первый круг
    for (int i = 1; i <= 8; ++i)
        BLIB_TEST_REQUIRE(queue.push(i));
    BLIB_TEST_CHECK(!queue.push(9)); // полон

    for (int i = 1; i <= 8; ++i)
    {
        int out = 0;
        BLIB_TEST_REQUIRE(queue.pop(out));
        BLIB_TEST_CHECK(out == i);
    }
    BLIB_TEST_CHECK(queue.isEmpty());

    // Второй круг — индексы обернулись
    for (int i = 9; i <= 16; ++i)
        BLIB_TEST_REQUIRE(queue.push(i));

    for (int i = 9; i <= 16; ++i)
    {
        int out = 0;
        BLIB_TEST_REQUIRE(queue.pop(out));
        BLIB_TEST_CHECK(out == i);
    }
}

BLIB_TEST_CASE("SPSC: capacity boundary - one element")
{
    blib::LocklessProducerConcumerCircleQueue<int> queue(1);

    BLIB_TEST_CHECK(queue.push(42));
    BLIB_TEST_CHECK(!queue.push(43)); // ёмкость 1: второй push не проходит

    int out = 0;
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 42);
    BLIB_TEST_CHECK(queue.isEmpty());
}

// Контракт SPSC: один поток-продюсер и один поток-консюмер.
// Ёмкость заведомо больше общего числа сообщений — переполнения нет,
// поэтому детерминированно проверяется, что все значения пришли
// ровно один раз и в порядке отправки.
BLIB_TEST_CASE("SPSC: producer/consumer thread exchange")
{
    const int totalMessages = 1000;
    blib::LocklessProducerConcumerCircleQueue<int> queue(1024);

    std::thread producer([&queue, totalMessages]()
        {
            for (int i = 0; i < totalMessages; ++i)
            {
                // Переполнения быть не должно (1024 > 1000), иначе тест зависнет
                if (!queue.push(i))
                    return;
            }
        });

    std::vector<int> received;
    received.reserve(totalMessages);

    while (static_cast<int>(received.size()) < totalMessages)
    {
        int out = 0;
        if (queue.pop(out))
            received.push_back(out);
    }

    producer.join();

    BLIB_TEST_REQUIRE(static_cast<int>(received.size()) == totalMessages);
    for (int i = 0; i < totalMessages; ++i)
    {
        // SPSC сохраняет порядок: i-е сообщение имеет значение i
        if (received[static_cast<size_t>(i)] != i)
        {
            BLIB_TEST_CHECK(false);
            break;
        }
    }
}

// ============================================================
// MultiProducerSingleConsumerCircleQueue (MPSC, RWLocker)
// ============================================================

BLIB_TEST_CASE("MPSC: push/pop round trip")
{
    blib::MultiProducerSingleConsumerCircleQueue<int> queue(4);

    BLIB_TEST_CHECK(queue.isEmpty());

    for (int i = 1; i <= 4; ++i)
        queue.push(i);

    BLIB_TEST_CHECK(!queue.isEmpty());

    for (int i = 1; i <= 4; ++i)
    {
        int out = 0;
        BLIB_TEST_REQUIRE(queue.pop(out));
        BLIB_TEST_CHECK(out == i);
    }

    BLIB_TEST_CHECK(queue.isEmpty());
}

BLIB_TEST_CASE("MPSC: overflow drops oldest")
{
    blib::MultiProducerSingleConsumerCircleQueue<int> queue(3);

    queue.push(1);
    queue.push(2);
    queue.push(3);

    // Буфер полон: push всегда успешен, роняет самое старое (1)
    queue.push(4);

    int out = 0;
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 2);
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 3);
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 4);
    BLIB_TEST_CHECK(!queue.pop(out));
}

BLIB_TEST_CASE("MPSC: wrap-around keeps the last N in order")
{
    blib::MultiProducerSingleConsumerCircleQueue<int> queue(8);

    for (int i = 1; i <= 16; ++i)
        queue.push(i);

    // После 16 push при ёмкости 8 в буфере остаются 9..16
    for (int i = 9; i <= 16; ++i)
    {
        int out = 0;
        BLIB_TEST_REQUIRE(queue.pop(out));
        BLIB_TEST_CHECK(out == i);
    }
    BLIB_TEST_CHECK(queue.isEmpty());
}

BLIB_TEST_CASE("MPSC: capacity boundary - one element")
{
    blib::MultiProducerSingleConsumerCircleQueue<int> queue(1);

    queue.push(42);
    queue.push(43); // роняет 42

    int out = 0;
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 43);
    BLIB_TEST_CHECK(queue.isEmpty());
}

BLIB_TEST_CASE("MPSC: reset reinitializes and frees old buffer")
{
    blib::MultiProducerSingleConsumerCircleQueue<int> queue(4);

    queue.push(1);
    queue.push(2);

    queue.reset(2); // уменьшаем ёмкость — старые элементы уничтожены

    BLIB_TEST_CHECK(queue.isEmpty());

    queue.push(10);
    queue.push(20);

    int out = 0;
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 10);
    BLIB_TEST_REQUIRE(queue.pop(out));
    BLIB_TEST_CHECK(out == 20);
}

// Стресс MPSC: несколько продюсеров одновременно пишут уникальные
// значения, консюмер собирает всё. Ёмкость больше общего числа
// сообщений — потерь нет, детерминированная проверка целостности:
// каждое значение пришло ровно один раз.
BLIB_TEST_CASE("MPSC: multiple producers stress")
{
    const int producersCount = 3;
    const int messagesPerProducer = 400;
    const int totalMessages = producersCount * messagesPerProducer;

    blib::MultiProducerSingleConsumerCircleQueue<int> queue(4096);

    std::vector<std::thread> producers;
    producers.reserve(producersCount);
    for (int p = 0; p < producersCount; ++p)
    {
        producers.emplace_back([&queue, p, messagesPerProducer]()
            {
                const int base = p * 1000;
                for (int i = 0; i < messagesPerProducer; ++i)
                    queue.push(base + i);
            });
    }

    std::vector<int> received;
    received.reserve(totalMessages);
    while (static_cast<int>(received.size()) < totalMessages)
    {
        int out = 0;
        if (queue.pop(out))
            received.push_back(out);
    }

    for (auto& t : producers)
        t.join();

    BLIB_TEST_REQUIRE(static_cast<int>(received.size()) == totalMessages);

    // Ничего не потеряно и не продублировано: после сортировки
    // получаем ровно ожидаемое множество значений
    std::sort(received.begin(), received.end());

    int expected = 0;
    bool ok = true;
    for (int p = 0; p < producersCount; ++p)
    {
        for (int i = 0; i < messagesPerProducer; ++i, ++expected)
        {
            if (received[static_cast<size_t>(expected)] != p * 1000 + i)
            {
                ok = false;
                break;
            }
        }
        if (!ok)
            break;
    }
    BLIB_TEST_CHECK(ok);
}

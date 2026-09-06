#include <blib/core/console/console.h>
#include "LinkedList.h"
#include "ObjectPool.h"

#include <vector>

struct MyStruct
{
    int a;
    MyStruct() { a = 0; }
    MyStruct(int as) { a = as; }
    ~MyStruct()
    {
        __blib_log_info("destructor");
    }
};

int main()
{
    // CLI-приложение: у буфера консоли нет UI-потребителя, поэтому
    // включаем дублирование строк в stdout/stderr
    blib::console::Console::instance().getOutput().setStdoutEcho(true);

    blib::LinkedList<int> ll;

    ll.pushBack(1);
    ll.pushBack(2);
    ll.pushBack(3);

    ll.popFront();
    ll.popFront();
    ll.popFront();

    ll.pushBack(1);
    ll.pushBack(2);
    ll.pushBack(3);

    std::vector<int>v;

    for (size_t i = 0; i < ll.size(); ++i)
        __blib_log_info("ll[%zu] = %d", i, ll[i]);

    blib::StaticObjectPool<MyStruct, 1>pool;

    pool.create(1);
    MyStruct* a = pool.create(2);
    pool.destroy(a);
    pool.create();

    return 0;
}

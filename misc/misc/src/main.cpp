#include <iostream>
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
        std::cout << "destructor" << std::endl;
    }
};

int main()
{
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
        std::cout << ll[i] << std::endl;

    blib::StaticObjectPool<MyStruct, 1>pool;

    pool.create(1);
    MyStruct* a = pool.create(2);
    pool.destroy(a);
    pool.create();

    return 0;
}

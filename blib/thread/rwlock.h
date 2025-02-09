#pragma once

namespace thread
{
    class ReadWriteSyncronizer
    {
    private:
        void* rwsyncer;
    public:
        void readLock();
        void readUnock();

        void writeLock();
        void writeUnlock();

        ReadWriteSyncronizer();
        ~ReadWriteSyncronizer();
        ReadWriteSyncronizer(const ReadWriteSyncronizer&) = delete;
        ReadWriteSyncronizer(ReadWriteSyncronizer&&) = delete;
    };
}
#pragma once

#include<blib/sound/soundFormat.h>
#include<memory>

#include<blib/core/alignedAllocator.h>
#include<blib/align.h>

namespace blib
{
    template<class Allocator = blib::CacheAlignedAllocator<uint8_t> >
    class __blib_cache_aligned SoundBufferTemplate
    {
    public:
        SoundBufferTemplate() = delete;
        SoundBufferTemplate(uint32_t bufferDurationMillisecond, const SoundFormat& fmt);
        SoundBufferTemplate(const SoundBufferTemplate& rhs);
        SoundBufferTemplate(SoundBufferTemplate&& rhs);
        SoundBufferTemplate(const SoundFormat& fmt, size_t _size, size_t _payloadSize, void* _pdata);
        ~SoundBufferTemplate();

        void setSize(size_t _size);
        void* getData();

        const void* getData() const;
        size_t getPayloadSize() const;
        size_t getSize() const;
        SoundFormat getFormat() const;
    private:
        SoundFormat format;
        size_t size;
        size_t payloadSize;
        void* pdata;
        void* platformCtx;
        Allocator allocator;
    };

    typedef SoundBufferTemplate<> SoundBuffer;
    typedef std::vector<SoundBuffer> SoundBuffers;
}

template<class Allocator>
__blib_inline blib::SoundBufferTemplate<Allocator>::SoundBufferTemplate(uint32_t bufferDurationMillisecond, const SoundFormat& fmt)
{
    this->size = ((size_t)(((double)fmt.sampleRate / (double)1000) * bufferDurationMillisecond) * (fmt.bitRate / 8)) * fmt.channel;
    this->pdata = allocator.allocate(this->size);
    this->format = fmt;
    this->payloadSize = 0;
    this->platformCtx = nullptr;
}

template<class Allocator>
__blib_inline blib::SoundBufferTemplate<Allocator>::SoundBufferTemplate(const SoundBufferTemplate& rhs)
{
    this->format = rhs.format;
    this->size = rhs.size;
    this->payloadSize = rhs.payloadSize;
    this->platformCtx = rhs.platformCtx;
    this->pdata = allocator.allocate(this->size);

    memcpy(this->pdata, rhs.pdata, this->size);
}

template<class Allocator>
__blib_inline blib::SoundBufferTemplate<Allocator>::SoundBufferTemplate(SoundBufferTemplate&& rhs)
{
    this->format = rhs.format;
    this->size = rhs.size;
    this->payloadSize = rhs.payloadSize;
    this->pdata = rhs.pdata;
    this->platformCtx = rhs.platformCtx;

    memset(&rhs, 0, sizeof(SoundBufferTemplate<Allocator>));
}

template<class Allocator>
__blib_inline blib::SoundBufferTemplate<Allocator>::SoundBufferTemplate(const SoundFormat& fmt, size_t _size, size_t _payloadSize, void* _pdata)
{
    this->format = fmt;
    this->size = _size;
    this->payloadSize = _payloadSize;
    this->pdata = _pdata;
}

template<class Allocator>
__blib_inline blib::SoundBufferTemplate<Allocator>::~SoundBufferTemplate()
{
    this->allocator.deallocate(reinterpret_cast<typename Allocator::type*>(this->pdata), this->size);
    
    //memset(this, 0, sizeof(SoundBufferTemplate<Allocator>));
}

template<class Allocator>
__blib_inline void blib::SoundBufferTemplate<Allocator>::setSize(size_t _size)
{
    this->size = _size;
}

template<class Allocator>
__blib_inline void* blib::SoundBufferTemplate<Allocator>::getData()
{
    return this->pdata;
}

template<class Allocator>
__blib_inline const void* blib::SoundBufferTemplate<Allocator>::getData() const
{
    return this->pdata;
}

template<class Allocator>
__blib_inline size_t blib::SoundBufferTemplate<Allocator>::getPayloadSize() const
{
    return this->payloadSize;
}

template<class Allocator>
__blib_inline size_t blib::SoundBufferTemplate<Allocator>::getSize() const
{
    return this->size;
}

template<class Allocator>
inline blib::SoundFormat blib::SoundBufferTemplate<Allocator>::getFormat() const
{
    return this->format;
}

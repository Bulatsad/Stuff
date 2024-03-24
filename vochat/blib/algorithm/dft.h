#pragma once

#include <memory.h>

#define _USE_MATH_DEFINES
#include <math.h>

constexpr double PI = M_PI;
constexpr double _2PI = PI * 2;

namespace blib
{
    template<class T, class Allocator>
    struct Spectre
    {
        std::vector<T> a;
        std::vector<T> b;
        size_t size;

        Allocator allocator;

        Spectre(size_t _size);
        ~Spectre();
    };

    template<class InT, class SpectreT = double, class Allocator = std::allocator<SpectreT> >
    Spectre<SpectreT, Allocator> fourierTransform(const void* pdata, size_t size);

    template<class Out, class T, class Allocator = std::allocator<T> >
    std::vector<Out> inverseFourierTransform(const Spectre<T, Allocator>* spectre);
}

template<class T, class Allocator>
inline blib::Spectre<T, Allocator>::Spectre(size_t _size)
{
    //this->size = _size;
    //
    //this->a = this->allocator.allocate(this->size);
    //this->b = this->allocator.allocate(this->size);
    //
    //memset(this->a, 0, this->size);
    //memset(this->b, 0, this->size);

    this->a.resize(_size);
    this->b.resize(_size);
    this->size = _size;
}

template<class T, class Allocator>
inline blib::Spectre<T, Allocator>::~Spectre()
{
    //this->allocator.deallocate(this->a, this->size);
    //this->allocator.deallocate(this->b, this->size);
}

template<class InT, class SpectreT, class Allocator>
blib::Spectre<SpectreT, Allocator> blib::fourierTransform(const void* pdata, size_t a_size)
{
    using Spectre_t = Spectre<SpectreT, Allocator>;
    const InT* wave = reinterpret_cast<const InT*>(pdata);
    size_t size = a_size / sizeof(InT);
    Spectre_t spectre(size);

    for (size_t k = 0; k < size; ++k)
    {
        for (size_t n = 0; n < size ; ++n)
        {
            spectre.b[k] += wave[n] * sin(k * (static_cast<SpectreT>(_2PI) / size) * n);
            spectre.a[k] += wave[n] * cos(k * (static_cast<SpectreT>(_2PI) / size) * n);
        }

        spectre.b[k] = spectre.b[k] / size;
        spectre.a[k] = spectre.a[k] / size;
    }

    return spectre;
}

template<class Out, class SpectreT, class Allocator>
std::vector<Out> blib::inverseFourierTransform(const blib::Spectre<SpectreT, Allocator>* spectre)
{
    std::vector<Out>res;
    res.resize(spectre->size,0);

    for (size_t k = 0; k < spectre->size; ++k)
    {
        SpectreT currentFt = 0;
        for (size_t n = 1; n < spectre->size; ++n)
        {
            currentFt +=
                spectre->a[n] * cos(k * (static_cast<SpectreT>(_2PI) / spectre->size) * n) +
                spectre->b[n] * sin(k * (static_cast<SpectreT>(_2PI) / spectre->size) * n);
        }
        res[k] = static_cast<Out>(currentFt);
    }

    return res;
}

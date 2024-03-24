#pragma once 

#include <memory>
#include <vector>

#include <complex>
#define _USE_MATH_DEFINES
#include <math.h>
#include <cmath>

#include <blib/inline.h>

namespace blib
{
    template<class CTy, class Allocator = std::allocator<CTy> >
    struct RotateCoef
    {
        CTy* pdata;
        size_t size;

        Allocator allocator;

        RotateCoef();
        RotateCoef(size_t _size);
        ~RotateCoef();
    };

    typedef std::complex<double> base;
    typedef std::vector<base> coefs;

    coefs expFourierTransform(const coefs& wave)
    {
        coefs c(wave.size(), { 0,0 });

        const double w = -(2 * static_cast<double>(M_PI) / wave.size());

        for (size_t k = 0; k < wave.size(); ++k)
        {
            for (size_t n = 0; n < wave.size(); ++n)
            {
                auto angle = (base(cos(k * w * n), sin(k * w * n)));
                c[k] += base(wave[n].real(), 0) * angle;
            }
        }

        return c;
    }

    coefs expFastFourierTransform(const coefs& wave)
    {
        if(wave.size() == 1)
            return { wave[0] };

        coefs a(wave.size() / 2);
        coefs b(wave.size() / 2);

        for (size_t i = 0, j = 0; i < wave.size(); i += 2, ++j)
        {
            a[j] = wave[i];
            b[j] = wave[i + 1];
        }

        a = expFastFourierTransform(a);
        b = expFastFourierTransform(b);

        coefs c(wave.size());
        const double w = (2 * static_cast<double>(M_PI) / wave.size());

        base deltaAngle = base(cos(w), sin(w));
        base angle = base(1, 0);
        for (size_t k = 0; k < wave.size() / 2; ++k)
        {
            c[k] = a[k] + b[k] * angle;
            c[k + wave.size() / 2] = a[k] - b[k] * angle;
            angle *= deltaAngle;
        }

        return c;
    }
    coefs expFastInverseFourierTransform(const coefs& wave)
    {
        if (wave.size() == 1)
            return { wave[0] };

        coefs a(wave.size() / 2);
        coefs b(wave.size() / 2);

        for (size_t i = 0, j = 0; i < wave.size(); i += 2, ++j)
        {
            a[j] = wave[i];
            b[j] = wave[i + 1];
        }

        a = expFastInverseFourierTransform(a);
        b = expFastInverseFourierTransform(b);

        coefs c(wave.size());
        const double w = -(2 * static_cast<double>(M_PI) / wave.size());

        base deltaAngle = base(cos(w), sin(w));
        base angle = base(1, 0);
        for (size_t k = 0; k < wave.size() / 2; ++k)
        {
            c[k] = a[k] + b[k] * angle;
            c[k + wave.size() / 2] = a[k] - b[k] * angle;
            angle *= deltaAngle;

            c[k] /= 2;
            c[k + wave.size() / 2] /= 2;
        }

        return c;
    }

    coefs expInverseFourierTransform(const coefs& c)
    {
        const size_t size = c.size();
        const double w = 2 * static_cast<double>(M_PI) / size;

        coefs wave(size, { 0,0 });

        for (size_t n = 0; n < size; ++n)
        {
            for (size_t k = 0; k < size; ++k)
            {
                auto angle = (base(cos(k * w * n), sin(k * w * n)));
                wave[n] += c[k] * angle;
            }
            wave[n] = base(wave[n].real() / size, wave[n].imag());
        }
        
        return wave;
    }
}

template<class RotateCoefType, class Allocator>
__blib_inline blib::RotateCoef<RotateCoefType, Allocator>::RotateCoef()
{
    this->pdata = nullptr;
    this->size = 0;
}

template<class CTy, class Allocator>
__blib_inline blib::RotateCoef<CTy, Allocator>::RotateCoef(size_t _size)
{
    this->pdata = this->allocator.allocate(_size * sizeof(CTy));
    memset(this->pdata, 0, _size * sizeof(CTy));
    this->size = _size;
}

template<class CTy, class Allocator>
__blib_inline blib::RotateCoef<CTy, Allocator>::~RotateCoef()
{
    //this->allocator.deallocate(this->pdata, this->size * sizeof(CTy));
}

//template<class InDataType, class RotateCoefType = double, class Allocator = std::allocator<RotateCoefType>>
//__blib_inline blib::RotateCoef<RotateCoefType, Allocator> expFourierTransform(const void* pdata, size_t size)



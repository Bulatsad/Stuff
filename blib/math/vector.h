#pragma once

namespace blib
{
    namespace math
    {
        typedef size_t VectorSizeT;
        template<class Type, VectorSizeT size>
        class Vector
        {
        public:
            Type data[size];
            
            Vector operator+(const Vector& rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] += rhs.data[i];
                return res;
            }
            Vector operator-(const Vector& rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] -= rhs.data[i];
                return res;
            }
            Vector operator+=(const Vector& rhs)
            {
                *this = *this + rhs;
            }
            Vector operator-=(const Vector& rhs)
            {
                *this = *this - rhs;
            }
            Vector operator*(Type rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] *= rhs.data[i];
                return res;
            }
            Vector operator*=(Type rhs)
            {
                *this = *this * rhs;
            }
            Vector operator/(Type rhs) const
            {
                Vector res = *this;
                for (size_t i = 0; i < size; ++i)
                    res.data[i] /= rhs.data[i];
                return res;
            }
            Vector operator/=(Type rhs)
            {
                *this = *this / rhs;
            }
            Vector operator ==(const Vector& rhs) const
            {
                for (size_t i = 0; i < size; ++i)
                {
                    if (this->data[i] != rhs.data[i])
                        return false;
                }
                return true;
            }
            Vector operator !=(const Vector& rhs) const
            {
                for (size_t i = 0; i < size; ++i)
                {
                    if (this->data[i] != rhs.data[i])
                        return true;
                }
                return false;
            }

            const Type& operator[](VectorSizeT index) const
            {
                return this->data[index];
            }
            Type& operator[](VectorSizeT index)
            {
                return this->data[index];
            }

            //Vector operator*(T left, const Vector& rhs);

        };
    }
}
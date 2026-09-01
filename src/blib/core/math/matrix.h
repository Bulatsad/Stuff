#pragma once

#include <blib/core/math/vector.h>

#include<stdexcept>

namespace blib
{
    namespace math
    {
        // stores matrix as array of lines(rows)
        typedef unsigned int matrixSizeT;
        template<class Type, matrixSizeT tmplWidth, matrixSizeT tmplHeight >
        class Matrix
        {
        private:
        public:
            blib::math::Vector<Type, tmplHeight> data[tmplWidth];

            Matrix()
            {
                this->loadIdentity();
            }
            Matrix(std::initializer_list<Type> args)
            {
                //check
                if (args.size() != tmplWidth * tmplHeight)
                {
                    throw std::runtime_error("Invalid matrix argument count");
                }

                //construct
                matrixSizeT i = 0;
                matrixSizeT j = 0;
                for (const auto& arg : args)
                {
                    if (i >= tmplWidth)
                    {
                        i = 0;
                        j++;
                    }
                    if (j >= tmplHeight)
                    {
                        j = 0;
                    }

                    this->data[i][j] = arg;
                    i++;
                }
            }

            Matrix operator+(const Matrix& rhs) const
            {
                // check
                //if (tmplWidth != rhs.columnsCount || tmplHeight != rhs.rowsCount)
                //{
                //    throw new std::runtime_error("Additin matrix with differnt sizes");
                //}

                //calculate
                Matrix res;
                for (matrixSizeT i = 0; i < tmplWidth; ++i)
                {
                    for (matrixSizeT j = 0; j < tmplHeight; ++j)
                    {
                        res.data[i][j] = this->data[i][j] + rhs.data[i][j];
                    }
                }
                return res;
            }
            Matrix operator-(const Matrix& rhs) const
            {
                //calculate
                Matrix res;
                for (matrixSizeT i = 0; i < tmplWidth; ++i)
                {
                    for (matrixSizeT j = 0; j < tmplHeight; ++j)
                    {
                        res.data[i][j] = this->data[i][j] - rhs.data[i][j];
                    }
                }
                return res;
            }

            Matrix<Type, tmplHeight, tmplWidth> Transpose() const
            {
                Matrix<Type, tmplHeight, tmplWidth> res;
                for (matrixSizeT i = 0; i < tmplHeight; ++i)
                {
                    for (matrixSizeT j = 0; j < tmplWidth; ++j)
                    {
                        res.data[i][j] = this->data[j][i];
                    }
                }
                return res;
            }
        
            template<class TypeRhs, matrixSizeT tmplWidthRhs, matrixSizeT tmplHeightRhs>
            Matrix<Type, tmplWidthRhs, tmplHeight> operator*(const Matrix<TypeRhs, tmplWidthRhs, tmplHeightRhs>& rhs) const
            {
                if (tmplWidth != tmplHeightRhs)
                {
                    throw std::runtime_error("The number of columns in matrix A must match the number of rows in matrix B for matrix multiplying");
                }

                Matrix<Type, tmplWidthRhs, tmplHeight> res;
                for (matrixSizeT i = 0; i < tmplWidthRhs; ++i)
                {
                    for (matrixSizeT j = 0; j < tmplHeight; ++j)
                    {
                        res.data[i][j] = Type(0);
                        for (matrixSizeT k = 0; k < tmplWidth; ++k)
                        {
                            res.data[i][j] += this->data[k][j] * rhs.data[i][k];
                        }
                    }
                }
                return res;
            }

            Matrix& operator=(const Matrix& rhs)
            {
                for (matrixSizeT i = 0 ; i < tmplWidth; ++i)
                {
                    this->data[i] = rhs.data[i];
                }

                return *this;
            }

            void loadIdentity()
            {
                matrixSizeT pos = 0;
                memset(this, 0, sizeof(Matrix<Type, tmplWidth, tmplHeight>));
                while (true)
                {
                    if (tmplWidth > pos && tmplHeight > pos)
                    {
                        this->data[pos][pos] = Type(1.0);
                        ++pos;
                    }
                    else
                        break;
                }
            }
        };
    }
}

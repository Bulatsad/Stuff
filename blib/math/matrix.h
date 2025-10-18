#pragma once

#include <blib/math/vector.h>

#include <exception>

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
            matrixSizeT columnsCount = tmplWidth;
            matrixSizeT rowsCount = tmplHeight;
            blib::math::Vector<Type, tmplHeight> data[tmplWidth];

            Matrix()
            {
                this->loadIdentity();
            }
            Matrix(std::initializer_list<Type> args)
            {
                //check
                if (args.size() != this->columnsCount * this->rowsCount)
                {
                    throw std::exception("Invalid matrix argument count");
                }

                //construct
                matrixSizeT i = 0;
                matrixSizeT j = 0;
                for (const auto& arg : args)
                {
                    if (i >= this->columnsCount)
                    {
                        i = 0;
                        j++;
                    }
                    if (j >= this->rowsCount)
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
                if (this->columnsCount != rhs.columnsCount || this->rowsCount != rhs.rowsCount)
                {
                    throw new std::exception("Additin matrix with differnt sizes");
                }

                //calculate
                Matrix res;
                for (matrixSizeT i = 0; i < this->columnsCount; ++i)
                {
                    for (matrixSizeT j = 0; j < this->rowsCount; ++j)
                    {
                        res.data[i][j] = this->data[i][j] + rhs.data[i][j];
                    }
                }
                return res;
            }
            Matrix operator-(const Matrix& rhs) const
            {
                // check
                if (this->columnsCount != rhs.columnsCount || this->rowsCount != rhs.rowsCount)
                {
                    throw new std::exception("Subtraction matrix with differnt sizes");
                }

                //calculate
                Matrix res;
                for (matrixSizeT i = 0; i < this->rowsCount; ++i)
                {
                    for (matrixSizeT j = 0; j < this->columnsCount; ++j)
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
                // check // The number of columns in matrix A must match the number of rows in matrix B
                if (tmplWidth != tmplHeightRhs)
                {
                    throw new std::exception("The number of columns in matrix A must match the number of rows in matrix B for matrix multiplying");
                }

                // calculating
                Matrix<Type, tmplWidthRhs, tmplHeight> res;
                for (matrixSizeT i = 0; i < tmplWidthRhs; ++i)
                {
                    for (matrixSizeT j = 0; j < tmplHeight; ++j)
                    {
                        res.data[i][j] = 0;
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

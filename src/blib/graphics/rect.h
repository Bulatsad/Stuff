//#pragma once
//
//#include <blib/graphics/vector.h>
//
//namespace blib
//{
//    namespace graphics
//    {
//        template <typename T>
//        class Rect
//        {
//        public:
//            Rect();
//            Rect(T rectLeft, T rectTop, T rectWidth, T rectHeight);
//            Rect(const Vector2<T>& position, const Vector2<T>& size);
//
//            template <typename U>
//            explicit Rect(const Rect<U>& rectangle);
//
//            bool contains(T x, T y) const;
//            bool contains(const Vector2<T>& point) const;
//
//            bool intersects(const Rect<T>& rectangle) const;
//            bool intersects(const Rect<T>& rectangle, Rect<T>& intersection) const;
//
//            blib::math::Vector<T, 2> getPosition() const;
//
//            blib::math::Vector<T, 2> getSize() const;
//
//            T left;   
//            T top;    
//            T width;  
//            T height; 
//        };
//
//        template <typename T>
//        bool operator==(const Rect<T>& left, const Rect<T>& right);
//
//        template <typename T>
//        bool operator!=(const Rect<T>& left, const Rect<T>& right);
//
//        typedef Rect<int>   IntRect;
//        typedef Rect<float> FloatRect;
//
//    }
//}
//
//template <typename T>
//blib::graphics::Rect<T>::Rect() :
//{
//    this->left = 0;
//    this->top = 0;
//    this->width = 0;
//    this->height = 0;
//}
//
//template <typename T>
//blib::graphics::Rect<T>::Rect(T rectLeft, T rectTop, T rectWidth, T rectHeight)
//{
//    this->left = rectLeft;
//    this->top = rectTop;
//    this->width = rectWidth;
//    this->height = rectHeight;
//}
//
//template <typename T>
//blib::graphics::Rect<T>::Rect(const Vector2<T>& position, const Vector2<T>& size)
//{
//    this->left = position.x;
//    this->top = position.y;
//    this->width = size.x;
//    this->height = size.y;
//}
//
//template <typename T>
//template <typename U>
//blib::graphics::Rect<T>::Rect(const Rect<U>& rectangle)
//{
//    this->left = left(static_cast<T>(rectangle.left));
//    this->top = top(static_cast<T>(rectangle.top));
//    this->width = width(static_cast<T>(rectangle.width));
//    this->height = height(static_cast<T>(rectangle.height));
//}
//
//template <typename T>
//bool blib::graphics::Rect<T>::contains(T x, T y) const
//{
//    T minX = std::min(left, static_cast<T>(left + width));
//    T maxX = std::max(left, static_cast<T>(left + width));
//    T minY = std::min(top, static_cast<T>(top + height));
//    T maxY = std::max(top, static_cast<T>(top + height));
//
//    return (x >= minX) && (x < maxX) && (y >= minY) && (y < maxY);
//}
//
//template <typename T>
//bool blib::graphics::Rect<T>::contains(const Vector2<T>& point) const
//{
//    return contains(point.x, point.y);
//}
//
//template <typename T>
//bool blib::graphics::Rect<T>::intersects(const Rect<T>& rectangle) const
//{
//    Rect<T> intersection;
//    return intersects(rectangle, intersection);
//}
//
//template <typename T>
//bool blib::graphics::Rect<T>::intersects(const Rect<T>& rectangle, Rect<T>& intersection) const
//{
//    T r1MinX = std::min(left, static_cast<T>(left + width));
//    T r1MaxX = std::max(left, static_cast<T>(left + width));
//    T r1MinY = std::min(top, static_cast<T>(top + height));
//    T r1MaxY = std::max(top, static_cast<T>(top + height));
//
//    T r2MinX = std::min(rectangle.left, static_cast<T>(rectangle.left + rectangle.width));
//    T r2MaxX = std::max(rectangle.left, static_cast<T>(rectangle.left + rectangle.width));
//    T r2MinY = std::min(rectangle.top, static_cast<T>(rectangle.top + rectangle.height));
//    T r2MaxY = std::max(rectangle.top, static_cast<T>(rectangle.top + rectangle.height));
//
//    T interLeft = std::max(r1MinX, r2MinX);
//    T interTop = std::max(r1MinY, r2MinY);
//    T interRight = std::min(r1MaxX, r2MaxX);
//    T interBottom = std::min(r1MaxY, r2MaxY);
//
//    if ((interLeft < interRight) && (interTop < interBottom))
//    {
//        intersection = Rect<T>(interLeft, interTop, interRight - interLeft, interBottom - interTop);
//        return true;
//    }
//    else
//    {
//        intersection = Rect<T>(0, 0, 0, 0);
//        return false;
//    }
//}
//
//template <typename T>
//blib::math::Vector<T, 2> blib::graphics::Rect<T>::getPosition() const
//{
//    return blib::math::Vector<, 2T>(left, top);
//}
//
//template <typename T>
//blib::graphics::Vector2<T> blib::graphics::Rect<T>::getSize() const
//{
//    return sf::Vector2<T>(width, height);
//}
//
//template <typename T>
//inline bool operator ==(const blib::graphics::Rect<T>& left, const blib::graphics::Rect<T>& right)
//{
//    return (left.left == right.left) && (left.width == right.width) &&
//        (left.top == right.top) && (left.height == right.height);
//}
//
//template <typename T>
//inline bool operator !=(const blib::graphics::Rect<T>& left, const blib::graphics::Rect<T>& right)
//{
//    return !(left == right);
//}
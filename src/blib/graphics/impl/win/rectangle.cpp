//#include <blib/graphics/rectangle.h>
//
//#include <Windows.h>
//#include <gl/GL.h>
//
//void blib::graphics::Rectangle::setWidth(bint32 _width)
//{
//    this->width = _width;
//}
//
//void blib::graphics::Rectangle::setHeight(bint32 _height)
//{
//    this->height = _height;
//}
//
//void blib::graphics::Rectangle::setColor(const Color color)
//{
//    this->color = color;
//}
//
//void blib::graphics::Rectangle::setPosition(const Vector3f& _position)
//{
//    this->transform.position = _position;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getPosition() const
//{
//    return this->transform.position;
//}
//
//void blib::graphics::Rectangle::setRotation(const Vector3f& _rotation)
//{
//    this->transform.rotation = _rotation;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getRotation() const
//{
//    return this->transform.rotation;
//}
//
//void blib::graphics::Rectangle::setScale(const Vector3f& _scale)
//{
//    this->transform.scale.x = _scale.x;
//    this->transform.scale.x = _scale.y;
//    this->transform.scale.x = _scale.z;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getScale() const
//{
//    return this->transform.scale;
//}
//
//void blib::graphics::Rectangle::setOrigin(const Vector3f& _origin)
//{
//    this->transform.origin.x = _origin.x;
//    this->transform.origin.y = _origin.y;
//    this->transform.origin.z = _origin.z;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getOrigin() const
//{
//    return this->transform.origin;
//}
//
//void blib::graphics::Rectangle::Move(const Vector3f& _position)
//{
//    this->transform.position += _position;
//}
//
//void blib::graphics::Rectangle::Rotate(const Vector3f& _rotatation)
//{
//    this->transform.rotation += _rotatation;
//}
//
//void blib::graphics::Rectangle::draw(RenderWindow& wnd)
//{
//    glPushMatrix();
//    {
//        glTranslatef(
//            this->transform.position.x,
//            this->transform.position.y,
//            this->transform.position.z
//        );
//        
//        glRotatef(this->transform.rotation.x, 1, 0, 0);
//        glRotatef(this->transform.rotation.y, 0, 1, 0);
//        glRotatef(this->transform.rotation.z, 0, 0, 1);
//        
//        glScalef(this->transform.scale.x, this->transform.scale.y, this->transform.scale.z);
//
//        glBegin(GL_LINE_LOOP);
//        {
//            float x0 = this->transform.position.x - this->transform.origin.x;
//            float x1 = this->transform.position.x - this->transform.origin.x + this->width;
//            float y0 = this->transform.position.y - this->transform.origin.y;
//            float y1 = this->transform.position.y - this->transform.origin.y + this->height;
//
//            glVertex3f(x0, y0, 0);
//            glVertex3f(x1, y0, 0);
//            glVertex3f(x1, y1, 0);
//            glVertex3f(x0, y1, 0);
//        }
//        glEnd();
//    }
//   glPopMatrix();
//}
#include <blib/graphics/rectangle.h>
//
//#include <Windows.h>
//#include <gl/GL.h>
//
//void blib::graphics::Rectangle::setWidth(bint32 _width)
//{
//    this->width = _width;
//}
//
//void blib::graphics::Rectangle::setHeight(bint32 _height)
//{
//    this->height = _height;
//}
//
//void blib::graphics::Rectangle::setColor(const Color color)
//{
//    this->color = color;
//}
//
//void blib::graphics::Rectangle::setPosition(const Vector3f& _position)
//{
//    this->transform.position = _position;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getPosition() const
//{
//    return this->transform.position;
//}
//
//void blib::graphics::Rectangle::setRotation(const Vector3f& _rotation)
//{
//    this->transform.rotation = _rotation;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getRotation() const
//{
//    return this->transform.rotation;
//}
//
//void blib::graphics::Rectangle::setScale(const Vector3f& _scale)
//{
//    this->transform.scale.x = _scale.x;
//    this->transform.scale.x = _scale.y;
//    this->transform.scale.x = _scale.z;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getScale() const
//{
//    return this->transform.scale;
//}
//
//void blib::graphics::Rectangle::setOrigin(const Vector3f& _origin)
//{
//    this->transform.origin.x = _origin.x;
//    this->transform.origin.y = _origin.y;
//    this->transform.origin.z = _origin.z;
//}
//
//blib::graphics::Vector3f blib::graphics::Rectangle::getOrigin() const
//{
//    return this->transform.origin;
//}
//
//void blib::graphics::Rectangle::Move(const Vector3f& _position)
//{
//    this->transform.position += _position;
//}
//
//void blib::graphics::Rectangle::Rotate(const Vector3f& _rotatation)
//{
//    this->transform.rotation += _rotatation;
//}
//
//void blib::graphics::Rectangle::draw(RenderWindow& wnd)
//{
//    glPushMatrix();
//    {
//        glTranslatef(
//            this->transform.position.x,
//            this->transform.position.y,
//            this->transform.position.z
//        );
//        
//        glRotatef(this->transform.rotation.x, 1, 0, 0);
//        glRotatef(this->transform.rotation.y, 0, 1, 0);
//        glRotatef(this->transform.rotation.z, 0, 0, 1);
//        
//        glScalef(this->transform.scale.x, this->transform.scale.y, this->transform.scale.z);
//
//        glBegin(GL_LINE_LOOP);
//        {
//            float x0 = this->transform.position.x - this->transform.origin.x;
//            float x1 = this->transform.position.x - this->transform.origin.x + this->width;
//            float y0 = this->transform.position.y - this->transform.origin.y;
//            float y1 = this->transform.position.y - this->transform.origin.y + this->height;
//
//            glVertex3f(x0, y0, 0);
//            glVertex3f(x1, y0, 0);
//            glVertex3f(x1, y1, 0);
//            glVertex3f(x0, y1, 0);
//        }
//        glEnd();
//    }
//   glPopMatrix();
//}

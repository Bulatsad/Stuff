//#include <blib/graphics/romb.h>
//
//#include <Windows.h>
//#include <gl/GL.h>
//
//void blib::graphics::Romb::setWidth(bint32 _width)
//{
//    this->width = _width;
//}
//
//void blib::graphics::Romb::setHeight(bint32 _height)
//{
//    this->height = _height;
//}
//
//void blib::graphics::Romb::setColor(const Color color)
//{
//    this->color = color;
//}
//
//void blib::graphics::Romb::draw(RenderWindow& wnd)
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
//            float halfW = this->width / 2;
//            float halfH = this->height / 2;
//            float x0 = this->transform.position.x /*- this->transform.origin.x*/ - halfW;
//            float x1 = this->transform.position.x /*- this->transform.origin.x*/ + halfW;
//            float y0 = this->transform.position.y /*- this->transform.origin.y*/ - halfH;
//            float y1 = this->transform.position.y /*- this->transform.origin.y*/ + halfH;
//
//            glVertex3f(x0, this->transform.position.y, 0);
//            glVertex3f(this->transform.position.x, y0, 0);
//            glVertex3f(x1, this->transform.position.y, 0);
//            glVertex3f(this->transform.position.x, y1, 0);
//        }
//        glEnd();
//    }
//    glPopMatrix();
//}

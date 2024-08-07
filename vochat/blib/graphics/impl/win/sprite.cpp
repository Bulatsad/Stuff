#include <blib/graphics/sprite.h>

#include <Windows.h>
#include <gl/GL.h>

blib::graphics::Sprite::Sprite()
{
    memset(&(this->transform), 0, sizeof(this->transform));

    this->pTexture = NULL;

    this->transform.scale.x = 1.f;
    this->transform.scale.y = 1.f;

    this->verties[0].textureCoord.x = 0;
    this->verties[0].textureCoord.y = 0;

    this->verties[1].textureCoord.x = 1;
    this->verties[1].textureCoord.y = 0;

    this->verties[2].textureCoord.x = 1;
    this->verties[2].textureCoord.y = 1;

    this->verties[3].textureCoord.x = 0;
    this->verties[3].textureCoord.y = 1;

    this->updateCache = true;
    this->pTexture = nullptr;
}

blib::graphics::Sprite::~Sprite()
{
}

void blib::graphics::Sprite::setTexture(const Texture& texture)
{
    this->pTexture = &texture;
}

void blib::graphics::Sprite::setPosition(float x, float y, float z)
{
    this->transform.position.x = x;
    this->transform.position.y = y;
    this->transform.position.z = z;
    this->updateCache = true;
}

void blib::graphics::Sprite::setOrigin(float x, float y)
{
    this->transform.origin.x = x;
    this->transform.origin.y = y;
    this->updateCache = true;
}

void blib::graphics::Sprite::draw(RenderWindow& window)
{

    ////prolog
    //// Set up ortographic projection
    //glMatrixMode(GL_PROJECTION);
    //glPushMatrix();
    //glLoadIdentity();
    //glOrtho(0, window.getHeight(), 0, window.getWight(), -1, 1);

    //wglMakeCurrent(wnd->hdc, wnd->context);

    if (this->updateCache)
    {
        float angle = -this->transform.rotation * 3.141592654f / 180.f;
        float cosine = (float)cos(angle);
        float sine = (float)sin(angle);
        float sxc = this->transform.scale.x * cosine;
        float syc = this->transform.scale.y * cosine;
        float sxs = this->transform.scale.x * sine;
        float sys = this->transform.scale.y * sine;
        float tx = -this->transform.origin.x * sxc - this->transform.origin.y * sys + this->transform.position.x;
        float ty = this->transform.origin.x * sxs - this->transform.origin.y * syc + this->transform.position.y;

        this->matrixTransform[0] = sxc; this->matrixTransform[4] = sys; this->matrixTransform[8] = 0.f; this->matrixTransform[12] = tx;
        this->matrixTransform[1] = -sxs; this->matrixTransform[5] = syc; this->matrixTransform[9] = 0.f; this->matrixTransform[13] = ty;
        this->matrixTransform[2] = 0.f; this->matrixTransform[6] = 0.f; this->matrixTransform[10] = 1.f; this->matrixTransform[14] = 0.f;
        this->matrixTransform[3] = 0.f; this->matrixTransform[7] = 0.f; this->matrixTransform[11] = -0.f; this->matrixTransform[15] = 1.f;

        this->verties[0].screenCoord.x = this->matrixTransform[0] * 0 + this->matrixTransform[4] * 0 + this->matrixTransform[12];
        this->verties[0].screenCoord.y = this->matrixTransform[1] * 0 + this->matrixTransform[5] * 0 + this->matrixTransform[13];

        this->verties[1].screenCoord.x = this->matrixTransform[0] * (this->pTexture->width) + this->matrixTransform[4] * 0 + this->matrixTransform[12];
        this->verties[1].screenCoord.y = this->matrixTransform[1] * (this->pTexture->width) + this->matrixTransform[5] * 0 + this->matrixTransform[13];

        this->verties[2].screenCoord.x = this->matrixTransform[0] * (this->pTexture->width) + this->matrixTransform[4] * (this->pTexture->height) + this->matrixTransform[12];
        this->verties[2].screenCoord.y = this->matrixTransform[1] * (this->pTexture->width) + this->matrixTransform[5] * (this->pTexture->height) + this->matrixTransform[13];

        this->verties[3].screenCoord.x = this->matrixTransform[0] * 0 + this->matrixTransform[4] * (this->pTexture->height) + this->matrixTransform[12];
        this->verties[3].screenCoord.y = this->matrixTransform[1] * 0 + this->matrixTransform[5] * (this->pTexture->height) + this->matrixTransform[13];

        this->updateCache = FALSE;
    }



    //glEnable(GL_TEXTURE_2D);
    //glBindTexture(GL_TEXTURE_2D, *static_cast<GLuint*>(this->pTexture->getContext()));
    //
    //glEnable(GL_TEXTURE_2D);
    //
    //
    //glDisable(GL_LIGHTING);
    //
    //glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    //glEnable(GL_BLEND);
    //
    //glBegin(GL_QUADS);
    //
    //glTexCoord3f(0, 0, 0); glVertex3f(this->verties[0].screenCoord.x, this->verties[0].screenCoord.y,0);
    //glTexCoord3f(1, 0, 0); glVertex3f(this->verties[1].screenCoord.x, this->verties[1].screenCoord.y,0);
    //glTexCoord3f(1, 1, 0); glVertex3f(this->verties[2].screenCoord.x, this->verties[2].screenCoord.y,0);
    //glTexCoord3f(0, 1, 0); glVertex3f(this->verties[3].screenCoord.x, this->verties[3].screenCoord.y,0);
    //
    //glEnd();
    //
    ////glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    ////glEnableClientState(GL_VERTEX_ARRAY);
    ////
    ////glVertexPointer(2, GL_FLOAT, sizeof(verties_2f_t), &(this->verties[0].screenCoord));
    ////glTexCoordPointer(2, GL_FLOAT, sizeof(verties_2f_t), &(this->verties[0].textureCoord));
    ////
    ////glDrawArrays(GL_QUADS, 0, 4);
    //
    //
    //glDisable(GL_TEXTURE_2D);
    //glEnable(GL_LIGHTING);
    //
    ////epilog
    //// Reset Projection Matrix
    ////glPopMatrix();

    //glPushMatrix();


    /////////////////////////////////



    glEnable(GL_TEXTURE_2D);

    //glRotatef(0.8, 1, 0, 1);

    //glBindTexture(GL_TEXTURE_2D, *static_cast<GLuint*>(this->pTexture->getContext()));
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glPushMatrix();
    {
        //glLoadIdentity();
        
        glTranslatef(this->transform.position.x, this->transform.position.y, this->transform.position.z);
     
        glBegin(GL_TRIANGLE_STRIP);
        {
            //glTexCoord3f(0, 0, 0); glVertex3f(1.f / window.getHeight() * this->verties[0].screenCoord.x, 1.f / window.getWight() * this->verties[0].screenCoord.y, 0);
            //glTexCoord3f(0, 1, 0); glVertex3f(1.f / window.getHeight() * this->verties[3].screenCoord.x, 1.f / window.getWight() * this->verties[3].screenCoord.y, 0);
            //glTexCoord3f(1, 0, 0); glVertex3f(1.f / window.getHeight() * this->verties[1].screenCoord.x, 1.f / window.getWight() * this->verties[1].screenCoord.y, 0);
            //glTexCoord3f(1, 1, 0); glVertex3f(1.f / window.getHeight() * this->verties[2].screenCoord.x, 1.f / window.getWight() * this->verties[2].screenCoord.y, 0);


            float x0 = this->transform.position.x;
            float x1 = this->transform.position.x + this->pTexture->width;
            float y0 = this->transform.position.y;
            float y1 = this->transform.position.y + this->pTexture->height;

            glTexCoord3f(0, 0, 0); glVertex3f(x0, y0, 0);
            glTexCoord3f(0, 1, 0); glVertex3f(x0, y1, 0);
            glTexCoord3f(1, 0, 0); glVertex3f(x1, y0, 0);
            glTexCoord3f(1, 1, 0); glVertex3f(x1, y1, 0);
        }
        glEnd();

        GLfloat m[16];
        glGetFloatv(GL_PROJECTION_MATRIX, m);
        printf("spritePROJECTION\n");
        for (int i = 0; i < 4; ++i)
        {
            for (int j = 0; j < 4; ++j)
            {
                printf("%f ", m[i * 4 + j]);
            }
            printf("\n");
        }
        printf("\n");
        printf("\n");
        printf("\n");
    }
    glPopMatrix();

}
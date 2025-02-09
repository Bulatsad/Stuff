#include <blib/graphics/model.h>

#include <fstream>
#include <vector>
#include <utility>

#include <blib/core/string.h>
#include <blib/inline.h>
#include <blib/graphics/vertex.h>

const float DefaultVertexW = 1.0;

struct ModelCtx
{
    std::vector<blib::graphics::Vertex4f>verchies;
    std::vector<std::vector<int> >faces;
};

#define __blib_this_context(_this) reinterpret_cast<ModelCtx*>(_this->ctx)

blib::graphics::ObjModel::ObjModel()
{
    this->ctx = new ModelCtx();
}

blib::graphics::ObjModel::~ObjModel()
{
    delete this->ctx;
}

__blib_private_func __blib_inline blib::graphics::ModelParsingStatus loadVertex(
    ModelCtx& model, 
    const blib::core::StringList& splited
)
{
    if (splited.size() >= 6 || splited.size() <= 3)
    {
        // TODO : Logging
        blib::graphics::ModelParsingStatus::UnsupportedFormat;
    }

    model.verchies.push_back(blib::graphics::Vertex4f());

    model.verchies.back().x = atof(splited[1].c_str());
    model.verchies.back().y = atof(splited[2].c_str());
    model.verchies.back().z = atof(splited[3].c_str());

    if (splited.size() > 4)
        model.verchies.back().w = atof(splited[4].c_str());
    else
        model.verchies.back().w = DefaultVertexW;
    
    return blib::graphics::ModelParsingStatus::OK;
}

__blib_private_func __blib_inline blib::graphics::ModelParsingStatus loadFace(
    ModelCtx& model,
    const blib::core::StringList& splited
)
{
    std::vector<int>tempFace;
    for (int i = 1; i < splited.size(); ++i)
    {
        auto spliedFace = blib::core::split(splited[i], "/");

        if (spliedFace.size() == 0)
        {
            return blib::graphics::ModelParsingStatus::UnsupportedFormat;
        }

        int vertexIndex = atoi(spliedFace[0].c_str());

        if (vertexIndex < 0)
            vertexIndex = model.verchies.size() + vertexIndex;
        else
            --vertexIndex;

        tempFace.push_back(vertexIndex);
    }

    model.faces.emplace_back(std::move(tempFace));

    return blib::graphics::ModelParsingStatus::OK;
}

blib::graphics::ModelParsingStatus blib::graphics::ObjModel::loadFromFile(const std::string& file)
{
    std::ifstream fin;
    fin.open(file, std::ios::in);
    if (!fin.is_open())
    {
        // TODO : Logging
        return ModelParsingStatus::CannotOpenFile;
    }

    while (!fin.eof())
    {
        std::string line;
        std::getline(fin, line);

        auto splited = blib::core::split(line, " ");
        
        if (splited.size() == 0)
            continue;

        if (splited[0] == "v")
        {
            auto status = loadVertex(*__blib_this_context(this), splited);
            if (status != ModelParsingStatus::OK)
            {
                // TODO : Logging
                return status;
            }
        }

        if (splited[0] == "f")
        {
            auto status = loadFace(*__blib_this_context(this), splited);
            if (status != ModelParsingStatus::OK)
            {
                // TODO : Logging
                return status;
            }
        }
    }

    return ModelParsingStatus::OK;
}

#include <Windows.h>
#include <gl/GL.h>

void blib::graphics::ObjModel::testDraw()
{
    glPushMatrix();

    for (const auto& face : __blib_this_context(this)->faces)
    {
        glBegin(GL_TRIANGLE_STRIP);

        for (const auto& index : face)
        {
            auto x = __blib_this_context(this)->verchies[index].x / 51;
            auto y = __blib_this_context(this)->verchies[index].y / 51;
            auto z = __blib_this_context(this)->verchies[index].z / 51;

            if (x - 1 > .0001 || y - 1 > .0001 || z - 1 > 0.0001)
                printf("alarm\n");

            glColor3f(1., 1., .9);
            glVertex3f(
                x,
                y - 1,
                z
            );
        }

        glEnd();
    }

    glPopMatrix();
    glFlush();

    /*
    glBegin(GL_POLYGON);
    glColor3f(1.0, 1.0, 1.0);
    glVertex3f(0.5, -0.5, 0.5);
    glVertex3f(0.5, 0.5, 0.5);
    glVertex3f(-0.5, 0.5, 0.5);
    glVertex3f(-0.5, -0.5, 0.5);
    glEnd();

    // Purple side - RIGHT
    glBegin(GL_POLYGON);
    glColor3f(1.0, 0.0, 1.0);
    glVertex3f(0.5, -0.5, -0.5);
    glVertex3f(0.5, 0.5, -0.5);
    glVertex3f(0.5, 0.5, 0.5);
    glVertex3f(0.5, -0.5, 0.5);
    glEnd();

    // Green side - LEFT
    glBegin(GL_POLYGON);
    glColor3f(0.0, 1.0, 0.0);
    glVertex3f(-0.5, -0.5, 0.5);
    glVertex3f(-0.5, 0.5, 0.5);
    glVertex3f(-0.5, 0.5, -0.5);
    glVertex3f(-0.5, -0.5, -0.5);
    glEnd();

    // Blue side - TOP
    glBegin(GL_POLYGON);
    glColor3f(0.0, 0.0, 1.0);
    glVertex3f(0.5, 0.5, 0.5);
    glVertex3f(0.5, 0.5, -0.5);
    glVertex3f(-0.5, 0.5, -0.5);
    glVertex3f(-0.5, 0.5, 0.5);
    glEnd();

    // Red side - BOTTOM
    glBegin(GL_POLYGON);
    glColor3f(1.0, 0.0, 0.0);
    glVertex3f(0.5, -0.5, -0.5);
    glVertex3f(0.5, -0.5, 0.5);
    glVertex3f(-0.5, -0.5, 0.5);
    glVertex3f(-0.5, -0.5, -0.5);
    glEnd();

    glFlush();*/
}

#undef __blib_this_context

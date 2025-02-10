#include <iostream>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/camera.h>
#include <blib/graphics/model.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/sprite.h>
#include <blib/graphics/keyboard.h>

#include <assimp/config.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <beng/graphics/mesh.h>

int main()
{
    blib::graphics::ObjModel obj;
    if (obj.loadFromFile("M:\\Stuff\\obj_spider\\triangle.txt") != blib::graphics::ModelParsingStatus::OK)
    {
        std::cerr << "err";
        return 1;
    }

    Assimp::Importer importer;

    const aiScene* pscene = importer.ReadFile("M:\\Stuff\\obj_spider\\FinalBaseMesh.obj",
        aiPostProcessSteps::aiProcess_CalcTangentSpace      |
        aiPostProcessSteps::aiProcess_Triangulate           |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
        aiPostProcessSteps::aiProcess_SortByPType
    );
    if (!pscene)
    {
        std::cerr << "error on loading object file" << std::endl;
        return EXIT_FAILURE;
    }

    beng::graphics::Mesh mesh;
    mesh.loadFromAssimpMesh(pscene->mMeshes[0]);

    blib::graphics::RenderWindow window(800, 600, "beng");
    blib::graphics::Camera camera;
    camera.setProjectionMode(blib::graphics::Camera::ProjectionMode::Perspective, window);

    camera.setPosition(blib::graphics::Vector3f(0, 0, 0));
    camera.setRotation(blib::graphics::Vector3f(0, 0, 0));
    obj.transform.position.z += 3;

    clock_t endframe = clock();

    blib::graphics::Image img;
    img.loadFromTgx("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Stronghold Crusader Extreme\\gfx\\frontend_loading_ex.tgx");

    blib::graphics::Texture txr;
    txr.create(img);

    blib::graphics::Sprite spr;
    spr.setTexture(txr);
    spr.setPosition({ 0, 0, -1000 });
    spr.setOrigin({ 0, 0, 0 });


    while (window.isOpen())
    {
        float deltatime = (clock() - endframe) / 1000.f;
        std::cout << deltatime;
        endframe = clock();

        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Escape))
        {
            window.close();
        }
        //obj.transform.position.z += 3;

        window.clear();
        window.update();
        camera.display(window);
        //obj.testDraw();
        spr.draw(window);
        mesh.draw(window);
        //obj.transform.rotation.y += 0.5;
        camera.controlUpdate(deltatime, window);

        window.display();
    }

    return 0;
}

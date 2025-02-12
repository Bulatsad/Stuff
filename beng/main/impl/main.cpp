#include <iostream>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/camera.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/sprite.h>
#include <blib/graphics/keyboard.h>

#include <assimp/config.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <beng/graphics/mesh.h>
#include <beng/graphics/model.h>

int main()
{
    Assimp::Importer importer;

    std::string objectfilename = "M:\\Stuff\\obj_spider\\boblampclean.md5mesh";

    const aiScene* pscene = importer.ReadFile(objectfilename,
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
    blib::graphics::RenderWindow window(800, 600, "beng");
    blib::graphics::Camera camera;
    camera.setProjectionMode(blib::graphics::Camera::ProjectionMode::Perspective, window);

    beng::graphics::Model model;
    model.parseFromAssimpScene(pscene, objectfilename);
    


    clock_t endframe = clock();

    blib::graphics::Image img;
    img.loadFromTgx("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Stronghold Crusader Extreme\\gfx\\frontend_loading_ex.tgx");

    blib::graphics::Texture txr;
    txr.create(img);

    blib::graphics::Sprite spr;
    spr.setTexture(txr);
    spr.setPosition(0,0,1000);

    model.setPosition(0, 0, 100);

    while (window.isOpen())
    {
        float deltatime = (clock() - endframe) / 1000.f;
        //std::cout << deltatime << std::endl;
        endframe = clock();

        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Escape))
        {
            window.close();
        }
        //obj.transform.position.z += 3;

        window.clear();
        window.update();
        camera.draw(window,blib::graphics::RenderContext());
        //obj.testDraw();
        spr.draw(window, blib::graphics::RenderContext());
        model.draw(window, blib::graphics::RenderContext());
        //obj.transform.rotation.y += 0.5;
        camera.controlUpdate(deltatime, window);

        window.display();
    }

    return 0;
}

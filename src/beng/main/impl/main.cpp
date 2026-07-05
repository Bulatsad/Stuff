#include <iostream>

#include <assimp/config.h>
#include <assimp/Importer.hpp>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <blib/graphics/mesh.h>
#include <blib/math/matrix.h>
#include <blib/graphics/renderWindow.h>
#include <blib/graphics/camera.h>
#include <blib/graphics/vertex.h>
#include <blib/graphics/sprite.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/animator.h>

#include <beng/graphics/model.h>
#include <beng/graphics/skinmesh.h>
//int main()
//{
//    Assimp::Importer importer;
//
//    std::string objectfilename = "M:\\Stuff\\obj_spider\\boblampclean.md5mesh";
//
//    const aiScene* pscene = importer.ReadFile(objectfilename,
//        aiPostProcessSteps::aiProcess_CalcTangentSpace      |
//        aiPostProcessSteps::aiProcess_Triangulate           |
//        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
//        aiPostProcessSteps::aiProcess_SortByPType
//    );
//    if (!pscene)
//    {
//        std::cerr << "error on loading object file" << std::endl;
//        return EXIT_FAILURE;
//    }
//    blib::graphics::RenderWindow window(1800, 600, "beng");
//    blib::graphics::Camera camera;
//    window.setCamera(&camera);
//    camera.setPerpective(75, static_cast<float>(window.getWight()) / static_cast<float>(window.getHeight()), 0.1, 1000000);
//    //camera.setProjectionMode(blib::graphics::Camera::ProjectionMode::Perspective, window);
//
//    beng::graphics::Model model;
//    model.parseFromAssimpScene(pscene, objectfilename);
//    
//
//
//    clock_t endframe = clock();
//
//    blib::graphics::Image img;
//    img.loadFromTgx("C:\\Program Files (x86)\\Steam\\steamapps\\common\\Stronghold Crusader Extreme\\gfx\\frontend_loading_ex.tgx");
//
//    blib::graphics::Texture txr;
//    txr.create(img);
//
//    blib::graphics::Sprite spr;
//    spr.setTexture(txr);
//    spr.setPosition(0,0,1000);
//
//    model.setPosition(0, 0, 100);
//
//    while (window.isOpen())
//    {
//        float deltatime = (clock() - endframe) / 1000.f;
//        //std::cout << deltatime << std::endl;
//        endframe = clock();
//
//        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Escape))
//        {
//            window.close();
//        }
//        //obj.transform.position.z += 3;
//
//        window.clear();
//        window.update();
//
//        //window.draw(camera);
//        //camera.draw(window,blib::graphics::RenderContext());
//        
//        //window.draw(spr);
//        //spr.draw(window, blib::graphics::RenderContext());
//
//        window.draw(model);
//        //model.draw(window, blib::graphics::RenderContext());
//
//        //camera.controlUpdate(deltatime, window);
//
//        window.display();
//    }
//
//    return 0;
//}
#include <imgui/imgui.h>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_opengl3.h>
#include <blib/graphics/impl/win/winRenderWindowUtil.h>
int main()
{
    blib::graphics::RenderWindow wnd(800, 600, "beng");
    blib::graphics::Camera camera;

    blib::graphics::IRenderTarget rt(800,600);
    rt.rc.setCamera(&camera);
    camera.setPerpective(60, static_cast<float>(wnd.getWight()) / static_cast<float>(wnd.getHeight()), 0.1, 1000);
    camera.setPosition(0, 0, 0);

    blib::graphics::Mesh mesh;

    //mesh.vertices.push_back({ -0.5f, -0.5f, 0.0f });
    //mesh.vertices.push_back({ 0.5f, -0.5f, 0.0f });
    //mesh.vertices.push_back({ 0.0f,  0.5f, 0.0f });
    //mesh.colors.push_back(blib::graphics::Color(255, 0, 255, 0));
    //mesh.colors.push_back(blib::graphics::Color(255, 0, 255, 0));
    //mesh.colors.push_back(blib::graphics::Color(255, 0, 255, 0));
    //mesh.setPosition(0, 0, -1);
    ////mesh.setScale( 0.5, 0.5, 0.5 );

    //blib::graphics::Image img;
    //blib::graphics::Texture txr;
    //blib::graphics::Sprite spr;
    //spr.setPosition(0, 0, -1);
    //img.loadFromTgx("C:/Program Files (x86)/Steam/steamapps/common/Stronghold Crusader Extreme/gfx/frontend_loading_ex.tgx");
    //txr.create(img, rt.rc);
    //spr.setTexture(txr);

    Assimp::Importer importer;

    std::string objectfilename = "M:\\Stuff\\obj_spider\\boblampclean.md5mesh";

    const aiScene* pscene = importer.ReadFile(objectfilename,
        aiPostProcessSteps::aiProcess_CalcTangentSpace |
        aiPostProcessSteps::aiProcess_Triangulate |
        aiPostProcessSteps::aiProcess_JoinIdenticalVertices |
        aiPostProcessSteps::aiProcess_SortByPType |
        aiPostProcessSteps::aiProcess_PopulateArmatureData
    );
    if (!pscene)
    {
        std::cerr << "error on loading object file" << std::endl;
        return EXIT_FAILURE;
    }

    blib::graphics::Animator animator;
    animator.loadFromAssimp(pscene);

    beng::graphics::SkinMesh skinMesh;
    skinMesh.loadFromAssimp(pscene);

    blib::graphics::Image t(512, 256);
    t[456][0] = blib::graphics::Color(255, 255, 255, 255);

    beng::graphics::Model model;
    model.parseFromAssimpScene(pscene, objectfilename);
    model.setPosition(0, -30, -50);
    model.setRotation(90, 0, 0);

    model.meshes[0].move(0, -10, 0);

    //model.meshes = { beng::graphics::Model::bakeMeshes(model.meshes) };
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(__blib_render_window_context(wnd.__getCtx())->hwnd);
    ImGui_ImplOpenGL3_Init();
        
    float endframe = clock();
    while (wnd.isOpen())
    {
        float deltatime = (clock() - endframe) / 1000.f;
        endframe = clock();
        if (blib::graphics::Keyboard::isKeyPressed(blib::graphics::Keyboard::Key::Escape))
        {
            wnd.close();
        }

        rt.clear();
        wnd.update();

        camera.controlUpdate(deltatime, wnd);

        model.meshes[0].rotateZ(deltatime * 10);

        std::cout << camera.getPosition().x << " " << camera.getPosition().y << " " << camera.getPosition().z << std::endl;

        //wnd.draw(mesh);
        rt.draw(model);
        //wnd.draw(spr);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();             // ��-��-��-��-��-��- ��-��-��-��-��-��-��-��-��-��- UI

        // 3. ��-��-��- UI ��-��-��-
        ImGui::Begin("Window");
        ImGui::Button("Click me");
        ImGui::End();

        // 4. ��-��-��-��-��-��-��-��-��- ��-��-��-��-
        ImGui::Render();


        // 6. ��-��-��-��-��-��-��-��- ImGui ��-��-��-��-��-��- ��-��-��-��-��-
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        wnd.display(rt/*,100,100*/);
    }

    //{
    //    {
    //        blib::math::Matrix<int, 3, 2> a({ 1, -2, 4,
    //                                          2,  0,-1 });
    //
    //        blib::math::Matrix<int, 3, 2> b({ 5, 2, 3,
    //                                          4, 6, 2 });
    //        auto c = a + b;
    //        auto d = a + b;
    //        /*
    //        6 0 7
    //        6 6 1
    //        */
    //    }
    //    {
    //        blib::math::Matrix<int, 3, 2> a({ 1, 2, 0,
    //                                          3, 1,-1 });
    //
    //        blib::math::Matrix<int, 1, 3> b({ 1,
    //                                          2,
    //                                          3 });
    //
    //        auto c = a * b;
    //        auto d = a * b;
    //        /*
    //        5
    //        2
    //        */
    //    }
    //    {
    //        blib::math::Matrix<int, 3, 2> a({ 1, 3, 7,
    //                                          2, 4,-1 });
    //
    //        auto t = a.Transpose();
    //        auto t1 = a.Transpose();
    //        /*
    //        1  2
    //        3  4
    //        7 -1
    //        */
    //    }
    //}
    return 0;
}
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
#include <blib/graphics/mouse.h>

#include <blib/graphics/skinmodel.h>
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

static WNDPROC s_engineWndProc = nullptr;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

static LRESULT CALLBACK bengImguiWndProc(HWND hwnd, UINT uMsg, WPARAM wparam, LPARAM lparam)
{
    if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wparam, lparam))
        return true;

    return CallWindowProc(s_engineWndProc, hwnd, uMsg, wparam, lparam);
}

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

    blib::graphics::SkinModel skinModel;
    skinModel.loadFromAssimp(pscene, objectfilename);
    skinModel.playAnimation();
    skinModel.setPosition(0, -30, -50);
    skinModel.setRotation(90, 0, 0);

    // TEMP DEBUG
    {
        std::cout << "animations count: " << pscene->mNumAnimations << std::endl;
        const auto& animations = skinModel.getAnimator().getAnimations();
        for (const auto& animation : animations)
            std::cout << "animation: " << animation.name << " durationMs=" << animation.durationMs << " channels=" << animation.channels.size() << std::endl;

        const auto& bones = skinModel.getSkelet().getBoneStorage();
        std::cout << "bones count: " << bones.size() << " root=" << (skinModel.getSkelet().root ? skinModel.getSkelet().root->name : "NULL") << std::endl;
        for (size_t i = 0; i < bones.size() && i < 3; ++i)
            std::cout << "bone[" << i << "] " << bones[i].name << " offsetPos=" << bones[i].offsetMatrix.data[0][3] << " " << bones[i].offsetMatrix.data[1][3] << " " << bones[i].offsetMatrix.data[2][3] << std::endl;

        const auto& modelTransform = skinModel.getTransform();
        std::cout << "modelTransform pos=" << modelTransform.data[0][3] << " " << modelTransform.data[1][3] << " " << modelTransform.data[2][3] << std::endl;
        std::cout << "modelTransform row0=" << modelTransform.data[0][0] << " " << modelTransform.data[0][1] << " " << modelTransform.data[0][2] << " " << modelTransform.data[0][3] << std::endl;
    }

    blib::graphics::Image t(512, 256);
    t[456][0] = blib::graphics::Color(255, 255, 255, 255);
    
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    HWND hwnd = __blib_render_window_context(wnd.__getCtx())->hwnd;
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplOpenGL3_Init();
    s_engineWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(bengImguiWndProc)));
        
    float endframe = clock();

    bool isFocused = false;
    bool printedFinalMatrices = false;
    blib::graphics::Mouse::setVisible(isFocused);

    while (wnd.isOpen())
    {
        float deltatime = (clock() - endframe) / 1000.f;
        endframe = clock();
        blib::graphics::Keyboard::update();
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
        {
            //wnd.close();
            isFocused = !isFocused;
            blib::graphics::Mouse::setVisible(isFocused);
        }

        rt.clear();
        wnd.update();

        if (!isFocused)
        {
            camera.controlUpdate(deltatime, wnd, isFocused);

            // TEMP: T-pose test — animation disabled, finalMatrices must stay identity
            //skinModel.update(deltatime);

            // TEMP DEBUG
            if (!printedFinalMatrices)
            {
                printedFinalMatrices = true;
                const auto& finalMatrices = skinModel.getSkelet().getFinalMatrices();
                std::cout << "finalMatrices count: " << finalMatrices.size() << std::endl;
                for (size_t i = 0; i < finalMatrices.size() && i < 3; ++i)
                    std::cout << "final[" << i << "] row0=" << finalMatrices[i].data[0][0] << " " << finalMatrices[i].data[0][1] << " " << finalMatrices[i].data[0][2] << " pos=" << finalMatrices[i].data[0][3] << " " << finalMatrices[i].data[1][3] << " " << finalMatrices[i].data[2][3] << std::endl;
            }

            std::cout << camera.getPosition().x << " " << camera.getPosition().y << " " << camera.getPosition().z << std::endl;
        }

        //wnd.draw(mesh);
        rt.draw(skinModel);
        //wnd.draw(spr);

        if (isFocused)
        {
            // UI
            {   
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();             // ��-��-��-��-��-��- ��-��-��-��-��-��-��-��-��-��- UI

                // 3. ��-��-��- UI ��-��-��-
                ImGui::SetNextWindowSize(ImVec2(350, 300), ImGuiCond_FirstUseEver);
                ImGui::Begin("Menu");

                if (ImGui::Button("Continue"))
                {
                    isFocused = false;
                    blib::graphics::Mouse::setVisible(isFocused);
                }

                const auto& animations = skinModel.getAnimator().getAnimations();
                for (const auto& animation : animations)
                {
                    std::string buttonName = animation.name.empty() ? "Animation name is not defined" : animation.name;
                    if (ImGui::Button(buttonName.c_str()))
                    {
                        skinModel.selectAnimation(animation.name);
                        skinModel.playAnimation();
                    }
                }

                if (ImGui::Button("EXIT"))
                {
                    wnd.close();
                }

                ImGui::End();

                // 4. ��-��-��-��-��-��-��-��-��- ��-��-��-��-
                ImGui::Render();


                // 6. ��-��-��-��-��-��-��-��- ImGui ��-��-��-��-��-��- ��-��-��-��-��-
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }
        }

        wnd.display(rt/*,100,100*/);
    }

    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(s_engineWndProc));

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
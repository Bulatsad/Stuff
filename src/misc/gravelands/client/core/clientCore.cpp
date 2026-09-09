#include <gravelands/client/core/clientCore.h>
#include <gravelands/client/core/isometricTileset.h>

#include <beng/core/time.h>

#include <blib/core/console/console.h>
#include <blib/graphics/camera.h>
#include <blib/graphics/color.h>
#include <blib/graphics/keyboard.h>
#include <blib/graphics/rendertarget.h>
#include <blib/graphics/renderWindow.h>
#include <blib/system/memory/globalAllocator.h>

namespace gravelands
{
    namespace
    {
        // Параметры перспективной камеры по умолчанию
        constexpr float cameraFovDegrees = 60.0f;
        constexpr float cameraNearDistance = 0.1f;
        constexpr float cameraFarDistance = 1000.0f;
    }

    // Внутренности клиента: окно, рендер-таргет, камера, тайлы, таймер.
    // Полное определение скрыто в .cpp (pimpl) — заголовок не тянет
    // графические типы blib в потребителей.
    struct ClientCore::ClientCoreImpl
    {
        blib::graphics::RenderWindow window;
        blib::graphics::IRenderTarget renderTarget;
        blib::graphics::Camera camera;
        gravelands::IsometricTileset tileset;
        beng::Time time;

        ClientCoreImpl()
            : window(static_cast<uint16_t>(windowWidth), static_cast<uint16_t>(windowHeight), gameTitle)
            , renderTarget(windowWidth, windowHeight)
            , camera()
            , tileset()
            , time()
        {
        }
    };

    ClientCore::ClientCore()
        : impl(nullptr)
    {
    }

    ClientCore::~ClientCore()
    {
        // Страховка: если владелец не вызвал shutdown явно
        shutdown();
    }

    bool ClientCore::initialize()
    {
        auto& globalAllocator = blib::memory::GlobalAllocator::instance();

        // Аллокация через GlobalAllocator + placement new (проектное правило:
        // выделяющие new/delete запрещены, placement new разрешён)
        impl = static_cast<ClientCoreImpl*>(globalAllocator.allocate(sizeof(ClientCoreImpl)));
        new (impl) ClientCoreImpl();

        // Перспективная камера; вид-матрица обновляется в controlUpdate
        // каждый кадр (WASD/Space/LShift — перемещение)
        impl->camera.setPerpective(
            blib::math::AngleDegreef(cameraFovDegrees),
            static_cast<float>(impl->window.getWight()) / static_cast<float>(impl->window.getHeight()),
            cameraNearDistance,
            cameraFarDistance);
        impl->camera.setPosition(0.0f, 0.0f, 0.0f);

        impl->renderTarget.rc.setCamera(&impl->camera);

        __blib_log_info("%s client core initialized (%ux%u window)",
            gameTitle, windowWidth, windowHeight);

        return true;
    }

    void ClientCore::tick()
    {
        if (__blib_unlikely(impl == nullptr))
        {
            return;
        }

        // Переменный dt кадра (клиентская сторона гибридного таймстепа;
        // фиксированный тикрейт живёт на сервере — см. ARCHITECTURE.md)
        impl->time.tick();
        const float deltaTime = impl->time.getDeltaTime();

        // Прокачка оконных сообщений (закрытие по X, перерисовка) —
        // как в model_viewer, иначе окно не живёт
        impl->window.update();

        // Обновление состояния клавиатуры перед опросом (для камеры и выхода)
        blib::graphics::Keyboard::update();

        // Escape закрывает окно
        if (blib::graphics::Keyboard::isKeyJustPressed(blib::graphics::Keyboard::Key::Escape))
        {
            impl->window.close();
            return;
        }

        // Камера: WASD/Space/LShift, мышь не захватывается (isFocused = false —
        // курсор возвращается в центр, дельта не накапливается)
        impl->camera.controlUpdate(deltaTime, impl->window, false);

        impl->renderTarget.clear(blib::graphics::Color::Black);
        impl->tileset.draw(impl->renderTarget);
        impl->window.display(impl->renderTarget);
    }

    bool ClientCore::isRunning() const
    {
        return impl != nullptr && impl->window.isOpen();
    }

    void ClientCore::shutdown()
    {
        if (impl == nullptr)
        {
            return;
        }

        // Явный вызов деструктора + возврат памяти глобальному аллокатору
        impl->~ClientCoreImpl();
        blib::memory::GlobalAllocator::instance().deallocate(impl, sizeof(ClientCoreImpl));
        impl = nullptr;

        __blib_log_info("%s client core shut down", gameTitle);
    }

} // namespace gravelands

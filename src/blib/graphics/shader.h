#pragma once

#include <string>
#include <vector>

#include <blib/config.h>
#include <blib/blibint.h>
#include <blib/graphics/material.h>
#include <blib/graphics/opengl.h>

namespace blib
{
    namespace graphics
    {
        class __blib_graphics_api Shader;
        class __blib_graphics_api ShaderProgram;

        // Коды ошибок компиляции/линковки шейдеров (см. AGENTS.md,
        // раздел "Обработка ошибок": None = 0 — всегда успех)
        enum class ShaderError : buint32
        {
            None = 0,
            InvalidType,
            FileNotFound,
            CompilationFailed,
            LinkFailed
        };

        class __blib_graphics_api Shader
        {
        private:
            void* ctx;
            std::string shaderPath;
        public:
            friend class __blib_graphics_api ShaderProgram;

            enum class Type
            {
                None,
                vertex,
                fragment
            }type;

            Shader();
            Shader(const std::string& path);
            ~Shader();

            // Владение GL-объектом: копирование дало бы двойное
            // освобождение в деструкторе (см. GRAPHICS.md)
            Shader(const Shader&) = delete;
            Shader& operator=(const Shader&) = delete;

            // Перемещение передаёт владение ctx (источник обнуляется —
            // его деструктор ничего не освобождает)
            Shader(Shader&& other) noexcept;
            Shader& operator=(Shader&& other) = delete;

            void setPath(const std::string& path);
            const std::string& getPath() const;
            void setType(const Type type);
            void setRenderApi(RenderApi* pRenderApi);
            ShaderError compile();
        };

        class __blib_graphics_api ShaderProgram
        {
        private:
            void* ctx;
        public:
            ShaderProgram();
            ~ShaderProgram();

            // Владение GL-объектом: копирование дало бы двойное
            // освобождение в деструкторе (см. GRAPHICS.md)
            ShaderProgram(const ShaderProgram&) = delete;
            ShaderProgram& operator=(const ShaderProgram&) = delete;

            // Перемещение передаёт владение ctx и перерегистрирует
            // программу в реестре hotreload (источник обнуляется)
            ShaderProgram(ShaderProgram&& other) noexcept;
            ShaderProgram& operator=(ShaderProgram&& other) = delete;

            void AttachShader(Shader& shader);
            int create();
            ShaderError compile();
            GLuint getContext();
            void setRenderApi(RenderApi* pRenderApi);
            void use();
            void unuse();

            // Перекомпилирует прикреплённые шейдеры с диска и перелинковывает
            // программу. При любой ошибке старая (рабочая) программа остаётся
            // нетронутой — рендер продолжает работать. Возвращает код ошибки
            // (None — перезагрузка успешна). Используется консольной командой
            // "hotreload" (см. reloadAllShaderPrograms)
            ShaderError reload();

            // Локация uniform'а с кешированием: первый запрос идёт в GL,
            // далее — из кеша. Кеш сбрасывается при reload(). Линейный
            // поиск по фиксированному массиву — горячий путь без аллокаций.
            GLint getUniformLocation(_In const char* name);
        };

        // Перезагружает все живые шейдерные программы (реестр пополняется
        // в ShaderProgram::create, убирается в деструкторе)
        void __blib_graphics_api reloadAllShaderPrograms();

        // Регистрирует консольные команды графики (Console из blib-core):
        //   hotreload      — перекомпиляция всех шейдеров с диска;
        //   reload_shaders — то же (алиас).
        // Вызывать один раз при инициализации приложения
        void __blib_graphics_api registerGraphicsConsoleCommands();

    }
}

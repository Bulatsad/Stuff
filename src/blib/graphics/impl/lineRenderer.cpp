#include <blib/graphics/lineRenderer.h>

#include <blib/core/console/console.h>
#include <blib/system/memory/globalAllocator.h>
#include <blib/graphics/shaderPaths.h>

#include <Windows.h>
#include <gl/GL.h>

namespace blib
{
    namespace graphics
    {
        namespace
        {
            // Локации вершинных атрибутов шейдера линий
            // (должны совпадать с LineVertexShader.glsl)
            enum class LineAttributeNames : GLuint
            {
                position = 0,
                color = 1,
                vbCount = 2
            };

            constexpr GLsizei lineVertexStride = static_cast<GLsizei>(sizeof(LineRenderer::LineVertex));
            constexpr size_t positionAttributeOffset = offsetof(LineRenderer::LineVertex, position);
            constexpr size_t colorAttributeOffset = offsetof(LineRenderer::LineVertex, color);
        }

        struct oglLineRendererContext
        {
            GLuint vao;
            GLuint vbo;
        };

        #define __blib_this_context(_this) (static_cast<oglLineRendererContext*>(_this->ctx))

        LineRenderer::LineRenderer()
        {
            // Выделяющая форма new запрещена проектом: контекст
            // аллоцируется через GlobalAllocator (см. AGENTS.md)
            this->ctx = blib::memory::GlobalAllocator::instance().allocate(sizeof(oglLineRendererContext));
            memset(this->ctx, 0, sizeof(oglLineRendererContext));
        }

        LineRenderer::~LineRenderer()
        {
            if (this->ctx)
            {
                blib::memory::GlobalAllocator::instance().deallocate(this->ctx, sizeof(oglLineRendererContext));
                this->ctx = nullptr;
            }
        }

        void LineRenderer::addLine(_In const blib::graphics::Vector3f& start, _In const blib::graphics::Vector3f& end, _In const blib::graphics::Color& color)
        {
            // Каждый отрезок — две вершины с общим цветом
            LineVertex startVertex;
            startVertex.position = start;
            startVertex.color = color;

            LineVertex endVertex;
            endVertex.position = end;
            endVertex.color = color;

            this->vertices.push_back(startVertex);
            this->vertices.push_back(endVertex);
        }

        void LineRenderer::clear()
        {
            this->vertices.clear();
        }

        buint32 LineRenderer::getSegmentCount() const
        {
            return static_cast<buint32>(this->vertices.size() / 2);
        }

        void LineRenderer::bake(blib::graphics::RenderContext& ctx) const
        {
            // VAO + VBO для динамической геометрии
            ctx.api.ogl.ext.__blib_glGenVertexArrays(1, &(__blib_this_context(this)->vao));
            ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);

            ctx.api.ogl.ext.__blib_glGenBuffers(1, &(__blib_this_context(this)->vbo));
            ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbo);

            // Позиция: 3 float
            ctx.api.ogl.ext.__blib_glEnableVertexAttribArray(static_cast<GLuint>(LineAttributeNames::position));
            ctx.api.ogl.ext.__blib_glVertexAttribPointer(
                static_cast<GLuint>(LineAttributeNames::position),
                3, GL_FLOAT, GL_FALSE,
                lineVertexStride,
                reinterpret_cast<const void*>(positionAttributeOffset));

            // Цвет: 4 байта RGBA, нормализуется в [0,1]
            ctx.api.ogl.ext.__blib_glEnableVertexAttribArray(static_cast<GLuint>(LineAttributeNames::color));
            ctx.api.ogl.ext.__blib_glVertexAttribPointer(
                static_cast<GLuint>(LineAttributeNames::color),
                4, GL_UNSIGNED_BYTE, GL_TRUE,
                lineVertexStride,
                reinterpret_cast<const void*>(colorAttributeOffset));

            // Шейдеры линий: MVP + повершинный цвет, без текстур.
            // Ошибки компиляции не фатальны: логируем предупреждение,
            // линии просто не отрисуются (как в Mesh::bake)
            this->vertexShader.setPath(std::string(blib::graphics::lineShaderBasePath) + blib::graphics::lineVertexShaderName);
            this->vertexShader.setType(blib::graphics::Shader::Type::vertex);
            this->vertexShader.setRenderApi(&(ctx.api));
            {
                blib::graphics::ShaderError err = this->vertexShader.compile();
                if (__blib_unlikely(err != blib::graphics::ShaderError::None))
                {
                    __blib_log_warning("line renderer bake: vertex shader compile failed (error %u)", static_cast<buint32>(err));
                }
            }

            this->fragmentShader.setPath(std::string(blib::graphics::lineShaderBasePath) + blib::graphics::lineFragmentShaderName);
            this->fragmentShader.setType(blib::graphics::Shader::Type::fragment);
            this->fragmentShader.setRenderApi(&(ctx.api));
            {
                blib::graphics::ShaderError err = this->fragmentShader.compile();
                if (__blib_unlikely(err != blib::graphics::ShaderError::None))
                {
                    __blib_log_warning("line renderer bake: fragment shader compile failed (error %u)", static_cast<buint32>(err));
                }
            }

            this->program.setRenderApi(&(ctx.api));
            this->program.create();
            this->program.AttachShader(this->vertexShader);
            this->program.AttachShader(this->fragmentShader);
            {
                blib::graphics::ShaderError err = this->program.compile();
                if (__blib_unlikely(err != blib::graphics::ShaderError::None))
                {
                    __blib_log_warning("line renderer bake: shader program link failed (error %u)", static_cast<buint32>(err));
                }
            }

            ctx.api.ogl.ext.__blib_glBindVertexArray(0);
        }

        void LineRenderer::draw(blib::graphics::RenderContext& ctx) const
        {
            if (this->vertices.empty())
            {
                return;
            }

            if (!(this->baked))
            {
                this->bake(ctx);
                this->baked = true;
            }

            ctx.setShaderProgram(&(this->program));
            ctx.sendVievMatrixToShaderProgram();
            ctx.sendProjectionMatrixToShaderProgram();
            ctx.sendModelMatrixToShaderProgram(this->getTransform());

            // Перезаливаем геометрию: поза скелета меняется каждый кадр
            ctx.api.ogl.ext.__blib_glBindVertexArray(__blib_this_context(this)->vao);
            ctx.api.ogl.ext.__blib_glBindBuffer(GL_ARRAY_BUFFER, __blib_this_context(this)->vbo);
            ctx.api.ogl.ext.__blib_glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(this->vertices[0]) * this->vertices.size(),
                this->vertices.data(),
                GL_DYNAMIC_DRAW);

            const GLsizei vertexCount = static_cast<GLsizei>(this->vertices.size());
            ctx.api.ogl.ext.__blib_gl_glDrawArrays(GL_LINES, 0, vertexCount);

            ctx.api.ogl.ext.__blib_glBindVertexArray(0);
        }

    }
}

#pragma once

#include "PerformanceTestCore.h"

#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "MaterialOwnerFiles.h"
#include "StudioMaterialProviders.h"
#include "graphics/render/RendererState.h"

namespace render
{
    class StudioDiagnosticsTestAccess
    {
    public:
        static int Count(const StudioDiagnostics& diagnostics)
        {
            return diagnostics.count_;
        }
        static const char* Model(const StudioDiagnostics& diagnostics, size_t index)
        {
            return diagnostics.records_[index].model;
        }
    };
    class TestQuadTestAccess
    {
    public:
        static unsigned int Vao(const TestQuad& quad)
        {
            return quad.vao_;
        }
    };
    class StudioRendererTestAccess
    {
    public:
        static void SetPendingImage(StudioRenderer& renderer, const StudioMaterialBindings& bindings)
        {
            renderer.pending_ = StudioRenderer::PendingModel{};
            renderer.pending_.active = true;
            renderer.pending_.mesh_count = 1;
            renderer.pending_.meshes.emplace_back();
            StudioQueuedMesh& mesh = renderer.pending_.meshes.back();
            mesh.material.emplace();
            mesh.material->program = "fixture";
            ncl_material::Param parameter;
            parameter.name = "uImage";
            parameter.value.type = ncl_material::ParamType::TextureRef;
            parameter.value.texture_ref = "fixtures/pending.png";
            mesh.material->params.push_back(parameter);
            mesh.material_bindings = &bindings;
        }

        static const StudioPose& get_cached_pose(const StudioRenderer& renderer)
        {
            return renderer.model_draw_.bones;
        }

        static size_t QueuedCount(const StudioRenderer& renderer)
        {
            return renderer.queue_.packets_.size();
        }

        static std::vector<StudioDrawPacket>& get_queued_packets(StudioRenderer& renderer)
        {
            return renderer.queue_.packets_;
        }

        static void DrawOrdinaryPackets(StudioRenderer& renderer)
        {
            std::vector<StudioDrawPacket>& packets = renderer.queue_.packets_;

            if (packets.empty())
            {
                return;
            }

            renderer.submission_sink_.Begin();

            for (const StudioDrawPacket& packet : packets)
            {
                renderer.submission_sink_.Draw(packet);
            }

            renderer.submission_sink_.End();
            packets.clear();
        }

        static StudioDrawQueue& Queue(StudioRenderer& renderer)
        {
            return renderer.queue_;
        }
        static const std::vector<StudioQueuedMesh>& PendingMeshes(const StudioRenderer& renderer)
        {
            return renderer.pending_.meshes;
        }
        static size_t PendingCount(const StudioRenderer& renderer)
        {
            return renderer.pending_.mesh_count;
        }
        static bool IsPending(const StudioRenderer& renderer)
        {
            return renderer.pending_.active;
        }
    };
} // namespace render

namespace render::preview
{
    class ScenePreviewTestAccess
    {
    public:
        static PreviewInteraction& Interaction(ScenePreview& scene)
        {
            return scene.interaction_;
        }

        static bool IsTargetActive(const ScenePreview& scene)
        {
            return scene.target_.active_;
        }
    };
} // namespace render::preview

namespace renderer_test
{
    class LifecycleBackend : public render::RendererLifecycleBackend
    {
    public:
        std::vector<std::string> events;
        std::function<void(std::string_view)> on_event;
        void Record(std::string_view event)
        {
            events.emplace_back(event);
            if (on_event)
            {
                on_event(event);
            }
        }
        void MapChanged() override
        {
            Record("map");
        }
        void ReleaseContext() override
        {
            Record("release");
        }
        void AbandonContext() override
        {
            Record("abandon");
        }
        void Shutdown() override
        {
            Record("shutdown");
        }
        void UninstallTextSurface() override
        {
            Record("uninstall");
        }
        void ReleaseText() override
        {
            Record("text.release");
        }
        void ForgetText() override
        {
            Record("text.forget");
        }
    };

    class CaptureBackend : public render::RenderCaptureBackend
    {
    public:
        std::map<goldsrc::cvar_t*, std::string> values;
        std::vector<std::pair<goldsrc::cvar_t*, std::string>> changes;
        std::vector<std::string> captures;
        unsigned int reports{};
        bool capture_result = true;
        std::function<void()> on_set;
        std::function<void()> on_capture;

        void SetCvar(goldsrc::cvar_t* cvar, const char* value) override
        {
            changes.emplace_back(cvar, value);
            values[cvar] = value;
            cvar->string = values[cvar].data();
            if (on_set)
            {
                on_set();
            }
        }
        bool Capture(const char* name, const char* suffix, std::filesystem::path& path) override
        {
            captures.push_back(std::string(name) + suffix);
            path = std::filesystem::path("capture") / captures.back();
            if (on_capture)
            {
                on_capture();
            }
            return capture_result;
        }
        void ReportCompleted(const char*, const std::filesystem::path&) override
        {
            ++reports;
        }
    };

    class ResourceBackend : public StudioMaterialSourceFixture
    {
    public:
        std::function<void(unsigned int)> on_delete_texture;
        unsigned int builds{};
        std::function<GLuint(const char*, tex::TextureLoadGuard)> on_load;
        unsigned int Load(const char* path, tex::TextureLoadGuard guard) override
        {
            return on_load ? on_load(path, guard) : StudioMaterialSourceFixture::Load(path, guard);
        }
        bool Build(const render::ProgramDesc& descriptor, ncl_gl::Program& program) override
        {
            ++builds;
            const char* vertex = "#version 330\nlayout(location=0) in vec2 position; void main(){gl_Position=vec4(position,0,1);}";
            const char* fragment = "#version 330\nuniform float uTime; out vec4 color; void main(){color=vec4(uTime,0,0,1);}";
            std::string error;
            ncl_gl::LoadProgram(
                program,
                vertex,
                fragment,
                descriptor.uniform_names,
                descriptor.uniform_count,
                descriptor.attrib_names,
                descriptor.attrib_count,
                &error
            );
            return program.ok;
        }
        void DeleteProgram(unsigned int program) override
        {
            glDeleteProgram(program);
        }
        void DeleteTexture(unsigned int texture) override
        {
            if (on_delete_texture)
            {
                on_delete_texture(texture);
            }
            glDeleteTextures(1, &texture);
        }
    };

    struct Owners
    {
        performance_test::Core performance;
        ResourceBackend backend;
        render::RendererResources resources{backend, backend, backend};
        MaterialOwnerFiles files;
        LifecycleBackend external;
        CaptureBackend capture;
        render::RendererState state{resources, files, external, capture, performance.monitor, performance.mapper};
    };
} // namespace renderer_test

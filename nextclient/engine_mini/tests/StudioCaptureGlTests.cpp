#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include <glad/glad.h>
#include <goldsrc/engine/video.h>
#include <gtest/gtest.h>
#include <ncl_material/material_parse.h>
#include <ncl_perf/profiler.h>
#include <ncl_studio/StudioMesh.h>
#include <ncl_utils/scope_exit.h>
#include <next_engine_mini/MaterialEditInterface.h>
#include <next_engine_mini/render_cvars.h>

#include "PreviewTestOwners.h"
#include "RadarGlContext.h"
#include "RendererOwnerTestSupport.h"
#include "StudioModelTestSupport.h"
#include "StudioScopeTrace.h"
#include "StudioTestPose.h"
#include "StudioTextureUploadFixture.h"
#include "common/common.h"
#include "common/cvar.h"
#include "common/filesystem.h"
#include "common/model.h"
#include "console/console.h"
#include "engine.h"
#include "graphics/gl_local.h"
#include "graphics/preview/PreviewInteraction.h"
#include "graphics/preview/PreviewSceneTarget.h"
#include "graphics/render/RenderConfig.h"
#include "graphics/render/RendererLifecycle.h"
#include "graphics/render/RendererState.h"
#include "graphics/render/ScopedGlState.h"
#include "graphics/render/StudioDraw.h"
#include "graphics/render/StudioIntercept.h"
#include "graphics/render/StudioPreviewContext.h"
#include "graphics/render/StudioSubmit.h"
#include "graphics/render/effects/studio_effects.h"
#include "graphics/render/material/ServerMaterials.h"
#include "graphics/render/material/material_edit.h"
#include "graphics/render/material/material_manifests.h"
#include "graphics/render/material/material_textures.h"
#include "graphics/render/model/StudioGeometry.h"
#include "graphics/render/model/studio_model_cache.h"
#include "graphics/render/render_device.h"
#include "graphics/render/shader_manager.h"
#include "performance/PerformanceMetrics.h"
#include "performance/PerformancePanelLayout.h"
#include "resource/texture_manager/TextureManager.h"

void R_DrawTEntitiesOnList(goldsrc::qboolean client_only);

namespace
{
    constexpr int kImageWidth = 32;
    constexpr int kImageHeight = 16;

    class CaptureModel
    {
    private:
        std::vector<uint8_t> bytes_;

    public:
        goldsrc::model_t model{};

        CaptureModel(const char* name, const std::vector<int>& mesh_counts, int rejected_part = -1)
        {
            bytes_.resize(sizeof(goldsrc::studiohdr_t));
            std::strcpy(model.name, name);

            const int bodyparts = Append(nullptr, mesh_counts.size() * sizeof(goldsrc::mstudiobodyparts_t));
            const int submodels = Append(nullptr, mesh_counts.size() * sizeof(goldsrc::mstudiomodel_t));
            const float positions[][3] = {{-0.3f, -0.6f, 0}, {0, 0.6f, 0}, {0.3f, -0.6f, 0}};
            const float normal[][3] = {{0, 0, 1}};
            const uint8_t bones[] = {0, 0, 0};
            const short commands[] = {3, 0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 0};
            const int positions_offset = Append(positions, sizeof(positions));
            const int normals_offset = Append(normal, sizeof(normal));
            const int bones_offset = Append(bones, sizeof(bones));
            const int commands_offset = Append(commands, sizeof(commands));

            for (size_t part = 0; part < mesh_counts.size(); part++)
            {
                const int mesh_offset = Append(nullptr, mesh_counts[part] * sizeof(goldsrc::mstudiomesh_t));
                goldsrc::mstudiobodyparts_t& bodypart = At<goldsrc::mstudiobodyparts_t>(bodyparts)[part];
                bodypart.nummodels = 1;
                bodypart.base = 1;
                bodypart.modelindex = submodels + static_cast<int>(part * sizeof(goldsrc::mstudiomodel_t));

                goldsrc::mstudiomodel_t& submodel = At<goldsrc::mstudiomodel_t>(submodels)[part];
                submodel.nummesh = mesh_counts[part];
                submodel.meshindex = mesh_offset;
                submodel.numverts = 3;
                submodel.vertindex = positions_offset;
                submodel.vertinfoindex = bones_offset;
                submodel.numnorms = 1;
                submodel.normindex = normals_offset;
                submodel.norminfoindex = bones_offset;

                for (int mesh_index = 0; mesh_index < mesh_counts[part]; mesh_index++)
                {
                    goldsrc::mstudiomesh_t& mesh = At<goldsrc::mstudiomesh_t>(mesh_offset)[mesh_index];
                    mesh.numtris = 1;
                    mesh.triindex = commands_offset;
                    mesh.skinref = static_cast<int>(part) == rejected_part && mesh_index == mesh_counts[part] - 1 ? 1 : 0;
                }
            }

            const int textures = Append(nullptr, 2 * sizeof(goldsrc::mstudiotexture_t));
            const short skins[] = {0, 1};
            const int skin_offset = Append(skins, sizeof(skins));

            for (int texture = 0; texture < 2; texture++)
            {
                goldsrc::mstudiotexture_t& entry = At<goldsrc::mstudiotexture_t>(textures)[texture];
                std::strcpy(entry.name, texture == 0 ? "red" : "green");
                entry.width = 1;
                entry.height = 1;
                entry.flags = goldsrc::STUDIO_NF_FULLBRIGHT;
            }

            goldsrc::studiohdr_t* header = get_header();
            header->version = ncl_studio::kStudioVersion;
            header->numbones = 1;
            header->numbodyparts = static_cast<int>(mesh_counts.size());
            header->bodypartindex = bodyparts;
            header->textureindex = textures;
            header->numtextures = 2;
            header->numskinref = 1;
            header->numskinfamilies = 2;
            header->skinindex = skin_offset;
            header->texturedataindex = static_cast<int>(bytes_.size());
            header->length = static_cast<int>(bytes_.size());
            std::strcpy(header->name, name);
        }

        void SetBodyVariants()
        {
            goldsrc::studiohdr_t* header = get_header();
            const int submodels = At<goldsrc::mstudiobodyparts_t>(header->bodypartindex)[0].modelindex;

            At<goldsrc::mstudiobodyparts_t>(header->bodypartindex)[0].nummodels = 3;
            header->numbodyparts = 1;

            for (int variant = 0; variant < 3; ++variant)
            {
                const float half_width = 0.15f + variant * 0.07f;
                const float positions[][3] = {
                    {-half_width, -0.55f, 0},
                    {0, 0.25f + variant * 0.15f, 0},
                    {half_width, -0.55f, 0}
                };
                const int offset = Append(positions, sizeof(positions));
                At<goldsrc::mstudiomodel_t>(submodels)[variant].vertindex = offset;
            }

            get_header()->length = static_cast<int>(bytes_.size());
            get_header()->texturedataindex = get_header()->length;
        }

        void SetVertexBones(bool mixed)
        {
            get_header()->numbones = 2;

            const goldsrc::mstudiomodel_t* submodel = static_cast<const goldsrc::mstudiomodel_t*>(get_submodel(0));
            uint8_t* bones = bytes_.data() + submodel->vertinfoindex;

            bones[0] = 1;
            bones[1] = mixed ? 0 : 1;
            bones[2] = 1;
        }

        void SetSubdividedBoxGeometry()
        {
            std::vector<std::array<float, 3>> positions;
            std::vector<std::array<short, 3>> triangles;
            const float cuts[3][4] = {{-12.3f, -1.7f, 2.1f, 12.3f}, {-12.8f, -2, 1.8f, 12.8f}, {0, 9.1f, 12.6f, 21.8f}};
            for (int axis = 0; axis < 3; axis++)
            {
                const int u = (axis + 1) % 3;
                const int v = (axis + 2) % 3;
                for (int side = 0; side < 2; side++)
                {
                    for (int row = 0; row < 3; row++)
                    {
                        for (int column = 0; column < 3; column++)
                        {
                            const short first = static_cast<short>(positions.size());
                            for (int corner = 0; corner < 4; corner++)
                            {
                                std::array<float, 3> point{};
                                point[axis] = cuts[axis][side * 3];
                                point[u] = cuts[u][column + (corner == 1 || corner == 2 ? 1 : 0)];
                                point[v] = cuts[v][row + (corner >= 2 ? 1 : 0)];
                                positions.push_back(point);
                            }
                            triangles.push_back(
                                {first, static_cast<short>(first + (side ? 2 : 1)), static_cast<short>(first + (side ? 1 : 2))}
                            );
                            triangles.push_back(
                                {first, static_cast<short>(first + (side ? 3 : 2)), static_cast<short>(first + (side ? 2 : 3))}
                            );
                        }
                    }
                }
            }
            const std::vector<unsigned char> bones(positions.size());
            std::vector<short> commands;
            for (const auto& triangle : triangles)
            {
                commands.push_back(3);
                for (short index : triangle)
                {
                    commands.insert(commands.end(), {index, 0, 0, 0});
                }
            }
            commands.push_back(0);
            const int vertices = Append(positions.data(), positions.size() * sizeof(positions[0]));
            const int bone_indices = Append(bones.data(), bones.size());
            const int triangles_offset = Append(commands.data(), commands.size() * sizeof(short));
            auto* submodel = static_cast<goldsrc::mstudiomodel_t*>(get_submodel(0));
            submodel->numverts = static_cast<int>(positions.size());
            submodel->vertindex = vertices;
            submodel->vertinfoindex = bone_indices;
            auto* mesh = At<goldsrc::mstudiomesh_t>(submodel->meshindex);
            mesh->numtris = static_cast<int>(triangles.size());
            mesh->triindex = triangles_offset;
            get_header()->length = static_cast<int>(bytes_.size());
            get_header()->texturedataindex = get_header()->length;
        }

        void SetTriangle(const std::array<float, 9>& positions)
        {
            const goldsrc::mstudiomodel_t* submodel = static_cast<goldsrc::mstudiomodel_t*>(get_submodel(0));
            std::memcpy(bytes_.data() + submodel->vertindex, positions.data(), sizeof(positions));
        }

        goldsrc::studiohdr_t* get_header()
        {
            return reinterpret_cast<goldsrc::studiohdr_t*>(bytes_.data());
        }

        void* get_submodel(int part)
        {
            const goldsrc::mstudiobodyparts_t& bodypart = At<goldsrc::mstudiobodyparts_t>(get_header()->bodypartindex)[part];
            return bytes_.data() + bodypart.modelindex;
        }

        void SetTextureCoordinates()
        {
            goldsrc::mstudiomodel_t* submodel = static_cast<goldsrc::mstudiomodel_t*>(get_submodel(0));
            short* commands = At<short>(At<goldsrc::mstudiomesh_t>(submodel->meshindex)->triindex);
            commands[8] = 64;
            commands[11] = 64;
            for (int index = 0; index < get_header()->numtextures; index++)
            {
                goldsrc::mstudiotexture_t& texture = At<goldsrc::mstudiotexture_t>(get_header()->textureindex)[index];
                texture.width = texture.height = 64;
            }
        }

    private:
        template <class T>
        T* At(int offset)
        {
            return reinterpret_cast<T*>(bytes_.data() + offset);
        }

        int Append(const void* source, size_t size)
        {
            while (bytes_.size() % 4 != 0)
            {
                bytes_.push_back(0);
            }

            int offset = static_cast<int>(bytes_.size());
            bytes_.resize(bytes_.size() + size, 0);
            if (source != nullptr)
            {
                std::memcpy(bytes_.data() + offset, source, size);
            }

            return offset;
        }
    };

    PFNGLGENBUFFERSPROC g_ResourceGenBuffers{};
    PFNGLDELETEBUFFERSPROC g_ResourceDeleteBuffers{};
    PFNGLGENVERTEXARRAYSPROC g_ResourceGenArrays{};
    PFNGLDELETEVERTEXARRAYSPROC g_ResourceDeleteArrays{};
    PFNGLBUFFERDATAPROC g_ResourceBufferData{};
    bool g_FailGeometryBuffers{};
    int g_GeometryUploads{};
    int g_GeometryCreations{};
    int g_GeometryBufferDeletes{};
    int g_GeometryOperations{};
    std::vector<size_t> g_GeometryDeletesAfterDraws;
    std::vector<GLuint> g_GeometryDeletedArrays;

    void APIENTRY ObserveGeometryBuffers(GLsizei count, GLuint* names)
    {
        g_GeometryOperations++;
        if (g_FailGeometryBuffers)
        {
            std::fill_n(names, count, 0u);
            return;
        }

        g_ResourceGenBuffers(count, names);
    }

    void APIENTRY ObserveGeometryArrays(GLsizei count, GLuint* names)
    {
        g_GeometryOperations++;
        g_GeometryCreations += count;
        g_ResourceGenArrays(count, names);
    }

    void APIENTRY ObserveGeometryUpload(GLenum target, GLsizeiptr size, const void* bytes, GLenum usage)
    {
        g_GeometryOperations++;
        if (target == GL_ARRAY_BUFFER)
        {
            g_GeometryUploads++;
        }

        g_ResourceBufferData(target, size, bytes, usage);
    }

    class GeometryTrace
    {
    public:
        GeometryTrace()
        {
            g_GeometryUploads = 0;
            g_GeometryCreations = 0;
            g_GeometryBufferDeletes = 0;
            g_GeometryOperations = 0;
            g_GeometryDeletesAfterDraws.clear();
            g_GeometryDeletedArrays.clear();
            Attach();
        }

        ~GeometryTrace()
        {
            g_FailGeometryBuffers = false;
            glad_glGenBuffers = g_ResourceGenBuffers;
            glad_glDeleteBuffers = g_ResourceDeleteBuffers;
            glad_glGenVertexArrays = g_ResourceGenArrays;
            glad_glDeleteVertexArrays = g_ResourceDeleteArrays;
            glad_glBufferData = g_ResourceBufferData;
        }

        void Attach();
    };

    template <class Result, class... Args>
    class CaptureChain : public nitroapi::HandlersChainInterface<Result, Args...>
    {
    public:
        void DetachHook() override {}
        bool IsHookAttached() override
        {
            return false;
        }
        uint32_t GetSubscribersCount() const override
        {
            return 0;
        }
        uint32_t GetHandlersCount() const override
        {
            return 0;
        }
        void AttachHook(nitroapi::GenericHookInterface<Result, Args...>*) override {}
        nitroapi::GenericHookInterface<Result, Args...>* GetHook() const override
        {
            return nullptr;
        }
        bool AddHandler(nitroapi::HandlerInterface<Result, Args...>*) override
        {
            return false;
        }
        bool RemoveHandler(nitroapi::HandlerInterface<Result, Args...>*) override
        {
            return false;
        }
        bool Subscribe(nitroapi::SubscriberInterface<Result, Args...>*) override
        {
            return false;
        }
        bool Unsubscribe(nitroapi::SubscriberInterface<Result, Args...>*) override
        {
            return false;
        }
    };

    class CaptureClock : public nitroapi::GenericHookInterface<double>
    {
    public:
        void UnsetCallbackAndDisableHook() override {}
        uint32_t GetTrampolineAddress() const override
        {
            return 0;
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return 0;
        }
        bool IsHookActive() const override
        {
            return false;
        }
        void SetCallbackAndEnableHook(std::function<double()>&&) override {}
        double InvokeOriginal() override
        {
            return 0;
        }
        double Invoke() override
        {
            return 0;
        }
    };

    std::string* g_TextOnlyConsole{};

    char* CapturePrint(const char* format, ...)
    {
        if (g_TextOnlyConsole != nullptr)
        {
            if (std::strcmp(format, "%s") != 0)
            {
                ADD_FAILURE() << "The engine console boundary accepts preformatted text only";
                return nullptr;
            }

            va_list arguments;
            va_start(arguments, format);
            g_TextOnlyConsole->append(va_arg(arguments, const char*));
            va_end(arguments);

            return nullptr;
        }

        va_list arguments;
        va_start(arguments, format);
        std::vfprintf(stderr, format, arguments);
        va_end(arguments);
        return nullptr;
    }

    class CaptureConsole : public nitroapi::GenericHookInterface<char*, const char*>
    {
    public:
        void UnsetCallbackAndDisableHook() override {}
        uint32_t GetTrampolineAddress() const override
        {
            return GetOriginalFunctionAddress();
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return reinterpret_cast<uint32_t>(CapturePrint);
        }
        bool IsHookActive() const override
        {
            return false;
        }
        void SetCallbackAndEnableHook(std::function<char*(const char*)>&&) override {}
        char* InvokeOriginal(const char* text) override
        {
            return CapturePrint("%s", text);
        }
        char* Invoke(const char* text) override
        {
            return InvokeOriginal(text);
        }
    };

    class CaptureEngine : public nitroapi::NitroApiInterface
    {
    private:
        nitroapi::EngineData data_;
        CaptureChain<double> clock_chain_;
        CaptureChain<char*, const char*> console_chain_;
        CaptureClock clock_;
        CaptureConsole console_;
        studio_test::Chain<void*, goldsrc::cache_user_t*, int, char*> allocation_chain_;
        studio_test::Chain<void*, goldsrc::cache_user_t*> cache_chain_;
        studio_test::Chain<void, goldsrc::cache_user_t*> free_chain_;

    public:
        studio_test::Hook<void*, goldsrc::cache_user_t*, int, char*> allocation;
        studio_test::Hook<void*, goldsrc::cache_user_t*> cache;
        studio_test::Hook<void, goldsrc::cache_user_t*> free;

        CaptureEngine()
        {
            data_.Sys_FloatTime.SetChain(&clock_chain_);
            data_.Sys_FloatTime.SetHook(&clock_);
            data_.Con_Printf.SetChain(&console_chain_);
            data_.Con_Printf.SetHook(&console_);
            data_.Cache_Alloc.SetChain(&allocation_chain_);
            data_.Cache_Alloc.SetHook(&allocation);
            data_.Cache_Check.SetChain(&cache_chain_);
            data_.Cache_Check.SetHook(&cache);
            data_.Cache_Free.SetChain(&free_chain_);
            data_.Cache_Free.SetHook(&free);
            cache.callback = [](goldsrc::cache_user_t* value) { return value->data; };
            free.callback = [](goldsrc::cache_user_t* value) { value->data = nullptr; };
        }

        void WriteLog(const char*, const char*) override {}
        void SetSEHCallback(GlobalExceptionHandlerFunc) override {}
        GlobalExceptionHandlerFunc GetSEHCallback() override
        {
            return nullptr;
        }
        bool Initialize(ICommandLine*, IFileSystem*, IRegistry*) override
        {
            return false;
        }
        bool UnInitialize() override
        {
            return true;
        }
        bool IsInitialized() override
        {
            return false;
        }
        nitroapi::ClientData* GetClientData() override
        {
            return nullptr;
        }
        nitroapi::EngineData* GetEngineData() override
        {
            return &data_;
        }
        nitroapi::WindowsData* GetWindowsData() override
        {
            return nullptr;
        }
        nitroapi::SDL2Data* GetSDL2Data() override
        {
            return nullptr;
        }
        nitroapi::BuildVersion* GetBuildVersion() override
        {
            return nullptr;
        }
        void GetVersion(char*, int) override {}
    };

    struct CapturedDraw
    {
        GLint program{};
        GLint texture{};
        float bone_x{};
        float glow_scale{};
        std::array<float, 3> shell_color{};
        std::array<float, 3> chrome_origin{};
        std::array<float, 3> view_right{};
        GLint entity_lights{};
        int meshes{};
        int indices{};
    };

    struct CaptureState
    {
        goldsrc::cl_entity_t* entity{};
        CaptureModel* model{};
        CaptureModel* attachment{};
        goldsrc::model_t* texture_model{};
        const goldsrc::studiohdr_t* texture_header{};
        std::function<goldsrc::model_t*(const char*, int)> find_model;
        bool model_prepared{};
        int late_attachment_loads{};
        std::array<float, render::kStudioBoneFloats> bones{};
        GLuint textures[2]{};
        int skin_calls{};
        int bone_calls{};
        int entity_calls{};
        int stock_calls{};
        int forced_flags{};
        bool late{};
        std::vector<CapturedDraw> draws;
        std::vector<GLsizeiptr> bone_uploads;
    };

    CaptureState g_Capture;
    std::function<int(int)> g_TransparentModelDraw;

    int DrawTransparentCaptureModel(int flags)
    {
        return g_TransparentModelDraw(flags);
    }
    PFNGLMULTIDRAWELEMENTSPROC g_OriginalMultiDraw{};
    PFNGLBUFFERSUBDATAPROC g_OriginalBoneUpload{};
    StudioScopeTrace* g_RigidTrace{};
    int g_RigidDraws{};
    bool g_CheckInstanceLayout{};
    PFNGLDRAWELEMENTSINSTANCEDPROC g_OriginalInstancedDraw{};

    void APIENTRY ObserveInstancedDraw(GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instances)
    {
        ++g_RigidDraws;

        if (g_CheckInstanceLayout)
        {
            GLint value = 0;
            glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_SIZE, &value);
            EXPECT_EQ(value, 3);
            glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
            EXPECT_EQ(value, 64);
            glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &value);
            EXPECT_EQ(value, GL_TRUE);
            glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &value);
            EXPECT_EQ(value, 1);
            glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_TYPE, &value);
            EXPECT_EQ(value, GLint(GL_UNSIGNED_INT));
        }

        g_OriginalInstancedDraw(mode, count, type, indices, instances);
    }

    void APIENTRY ObserveCapturedBoneUpload(GLenum target, GLintptr offset, GLsizeiptr size, const void* data)
    {
        if (g_RigidTrace != nullptr)
        {
            EXPECT_EQ(
                g_RigidTrace->current(),
                target == GL_ARRAY_BUFFER ? performance::PerfStage::StudioInstanceUpload : performance::PerfStage::StudioBoneUpload
            );
        }

        g_Capture.bone_uploads.push_back(size);
        g_OriginalBoneUpload(target, offset, size, data);
    }

    void APIENTRY ObserveGeometryDelete(GLsizei count, const GLuint* names)
    {
        g_GeometryOperations++;
        for (GLsizei index = 0; index < count; index++)
        {
            g_GeometryDeletesAfterDraws.push_back(g_Capture.draws.size());
            g_GeometryDeletedArrays.push_back(names[index]);
        }

        g_ResourceDeleteArrays(count, names);
    }

    void APIENTRY ObserveGeometryBufferDelete(GLsizei count, const GLuint* names)
    {
        g_GeometryOperations++;
        g_GeometryBufferDeletes += count;
        g_ResourceDeleteBuffers(count, names);
    }

    void GeometryTrace::Attach()
    {
        g_ResourceGenBuffers = glad_glGenBuffers;
        g_ResourceDeleteBuffers = glad_glDeleteBuffers;
        g_ResourceGenArrays = glad_glGenVertexArrays;
        g_ResourceDeleteArrays = glad_glDeleteVertexArrays;
        g_ResourceBufferData = glad_glBufferData;
        glad_glGenBuffers = ObserveGeometryBuffers;
        glad_glDeleteBuffers = ObserveGeometryBufferDelete;
        glad_glGenVertexArrays = ObserveGeometryArrays;
        glad_glDeleteVertexArrays = ObserveGeometryDelete;
        glad_glBufferData = ObserveGeometryUpload;
    }

    void APIENTRY ObserveCaptureDraw(GLenum mode, const GLsizei* counts, GLenum type, const void* const* offsets, GLsizei count)
    {
        CapturedDraw draw;
        glGetIntegerv(GL_CURRENT_PROGRAM, &draw.program);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &draw.texture);
        glGetBufferSubData(GL_UNIFORM_BUFFER, 3 * sizeof(float), sizeof(draw.bone_x), &draw.bone_x);
        glGetUniformfv(draw.program, glGetUniformLocation(draw.program, "uGlowScale"), &draw.glow_scale);
        GLint shell_color = glGetUniformLocation(draw.program, "uShellColor");

        if (shell_color >= 0)
        {
            glGetUniformfv(draw.program, shell_color, draw.shell_color.data());
        }
        glGetUniformfv(draw.program, glGetUniformLocation(draw.program, "uChromeOrigin"), draw.chrome_origin.data());
        glGetUniformfv(draw.program, glGetUniformLocation(draw.program, "uViewRight"), draw.view_right.data());
        glGetUniformiv(draw.program, glGetUniformLocation(draw.program, "uEntityLightCount"), &draw.entity_lights);
        draw.meshes = count;
        for (GLsizei index = 0; index < count; index++)
        {
            draw.indices += counts[index];
        }

        g_Capture.draws.push_back(draw);
        g_OriginalMultiDraw(mode, counts, type, offsets, count);
    }

    goldsrc::cl_entity_t* CurrentCaptureEntity()
    {
        EXPECT_FALSE(g_Capture.late);
        g_Capture.entity_calls++;
        return g_Capture.entity;
    }

    float**** CaptureBones()
    {
        EXPECT_FALSE(g_Capture.late);
        g_Capture.bone_calls++;
        return reinterpret_cast<float****>(g_Capture.bones.data());
    }

    void CaptureSkin(void* header, int skin)
    {
        EXPECT_FALSE(g_Capture.late);
        EXPECT_EQ(header, g_Capture.texture_header != nullptr ? g_Capture.texture_header : g_Capture.model->get_header());
        ASSERT_NE(g_Capture.entity, nullptr);
        EXPECT_EQ(skin, g_Capture.entity->curstate.skin);
        ASSERT_GE(skin, 0);
        ASSERT_LT(skin, 2);
        g_Capture.skin_calls++;
        glBindTexture(GL_TEXTURE_2D, g_Capture.textures[skin]);
    }

    void* CaptureExtradata(goldsrc::model_t* model)
    {
        EXPECT_FALSE(g_Capture.late);
        if (model == g_Capture.texture_model)
        {
            return model->cache.data;
        }
        if (g_Capture.attachment != nullptr && model == &g_Capture.attachment->model)
        {
            if (g_Capture.model_prepared && g_Capture.model != g_Capture.attachment)
            {
                g_Capture.late_attachment_loads++;
            }
            return g_Capture.attachment->get_header();
        }
        EXPECT_EQ(model, &g_Capture.model->model);
        return g_Capture.model->get_header();
    }

    goldsrc::model_t* CaptureForName(const char* name, int crash)
    {
        return g_Capture.find_model ? g_Capture.find_model(name, crash) : nullptr;
    }

    struct CompanionFile
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiotexture_t textures[2]{};
        short skins[4] = {0, 1, 1, 0};
        uint8_t pixels[2][4 + 256 * 3]{};

        CompanionFile()
        {
            header.version = ncl_studio::kStudioVersion;
            header.length = sizeof(*this);
            header.numtextures = 2;
            header.textureindex = offsetof(CompanionFile, textures);
            header.texturedataindex = offsetof(CompanionFile, pixels);
            header.numskinref = 2;
            header.numskinfamilies = 2;
            header.skinindex = offsetof(CompanionFile, skins);

            for (int index = 0; index < 2; index++)
            {
                std::strcpy(textures[index].name, index == 0 ? "red" : "green");
                textures[index].width = 2;
                textures[index].height = 2;
                textures[index].flags = goldsrc::STUDIO_NF_NOMIPS | goldsrc::STUDIO_NF_FULLBRIGHT;
                textures[index].index = offsetof(CompanionFile, pixels) + index * sizeof(pixels[0]);
                std::fill_n(pixels[index] + 4, 256 * 3, uint8_t{255});
            }
        }
    };

    class CompanionLoader
    {
    private:
        CaptureEngine& engine_;
        std::vector<uint8_t> cache_;
        goldsrc::qboolean initialized_{};
        goldsrc::qboolean* old_initialized_ = p_host_initialized;
        std::function<void*(goldsrc::cache_user_t*, int, char*)> old_allocation_;
        studio_test::TextureUploadCallbacks old_upload_;

    public:
        CompanionFile file;
        goldsrc::model_t model{};
        int loads{};
        std::function<void()> before_allocate;

        CompanionLoader(CaptureEngine& engine, const char* name, bool cached) :
            engine_(engine),
            cache_(offsetof(CompanionFile, pixels)),
            old_allocation_(std::move(engine.allocation.callback)),
            old_upload_(std::move(studio_test::g_TextureUploadCallbacks))
        {
            std::strcpy(model.name, name);
            model.type = goldsrc::mod_studio;
            p_host_initialized = &initialized_;
            studio_test::g_TextureUploadCallbacks.load = [](tex::Texture& texture,
                                                            const tex::TexIdentifierStr& identifier,
                                                            TextureFormat format,
                                                            const uint8_t* data,
                                                            int width,
                                                            int height,
                                                            bool mipmap,
                                                            const uint8_t* palette,
                                                            int) {
                EXPECT_EQ(format, TextureFormat::Opaque);
                std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);

                for (int index = 0; index < width * height; index++)
                {
                    std::copy_n(palette + data[index] * 3, 3, pixels.data() + index * 4);
                    pixels[index * 4 + 3] = 255;
                }

                GLuint image = 0;
                glGenTextures(1, &image);
                GL_Bind(static_cast<int>(image));
                *p_currenttexture = -1;
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                texture.identifier = identifier;
                texture.texnum = static_cast<int>(image);
                texture.width = width;
                texture.height = height;
                texture.mipmap = mipmap;
                texture.palette = tex::PaletteHandle::Invalid();
                return image != 0;
            };
            studio_test::g_TextureUploadCallbacks.release = [](tex::Texture& texture) {
                GLuint image = static_cast<GLuint>(texture.texnum);
                glDeleteTextures(1, &image);
                texture.texnum = 0;
            };
            engine_.allocation.callback = [this](goldsrc::cache_user_t* cache, int size, char*) -> void* {
                EXPECT_EQ(size, cache_.size());

                if (before_allocate)
                {
                    before_allocate();
                }

                cache->data = cache_.data();
                return cache->data;
            };
            g_Capture.texture_model = &model;
            g_Capture.texture_header = get_header();
            g_Capture.find_model = [this](const char* path, int) {
                EXPECT_STREQ(path, model.name);

                if (model.cache.data == nullptr)
                {
                    Reload();
                }

                return &model;
            };

            if (cached)
            {
                std::memcpy(cache_.data(), &file, cache_.size());
                model.cache.data = cache_.data();
            }
        }

        ~CompanionLoader()
        {
            g_Capture.find_model = {};
            g_Capture.texture_model = nullptr;
            g_Capture.texture_header = nullptr;
            engine_.allocation.callback = std::move(old_allocation_);
            tex::g_TextureManagerGlob.ForEach([this](tex::Texture* texture, tex::TextureHandle handle, tex::TextureLifetime) {
                if (std::string_view(texture->identifier.data(), texture->identifier.size()).starts_with(model.name))
                {
                    tex::g_TextureManagerGlob.Destroy(handle);
                }
            });
            studio_test::g_TextureUploadCallbacks = std::move(old_upload_);
            p_host_initialized = old_initialized_;
        }

        goldsrc::studiohdr_t* get_header()
        {
            return reinterpret_cast<goldsrc::studiohdr_t*>(cache_.data());
        }

        void Reload()
        {
            loads++;
            Mod_LoadStudioModel(&model, &file, sizeof(file));
        }
    };

    goldsrc::model_t* CaptureModelByIndex(int index)
    {
        EXPECT_EQ(index, 7);
        return g_Capture.attachment != nullptr ? &g_Capture.attachment->model : nullptr;
    }

    void CaptureSetupModel(int part, void** bodypart, void** submodel)
    {
        *bodypart = nullptr;
        *submodel = g_Capture.model->get_submodel(part);

        const goldsrc::studiohdr_t* header = g_Capture.model->get_header();
        const goldsrc::mstudiobodyparts_t* parts =
            reinterpret_cast<const goldsrc::mstudiobodyparts_t*>(reinterpret_cast<const uint8_t*>(header) + header->bodypartindex);

        if (parts[part].nummodels > 1)
        {
            *submodel = static_cast<goldsrc::mstudiomodel_t*>(*submodel) + g_Capture.entity->curstate.body % parts[part].nummodels;
        }
    }

    void CaptureNoop() {}
    int CaptureForcedFlags()
    {
        return g_Capture.forced_flags;
    }
    void CaptureHeader(void*) {}
    void CaptureModelPointer(goldsrc::model_t*) {}
    void CaptureLighting(goldsrc::alight_t*) {}
    void CaptureCvar(const char**) {}
    void CaptureStock()
    {
        g_Capture.stock_calls++;
    }

    class StudioCaptureGlTest : public testing::Test
    {
    private:
        RadarGlContext context_;
        CaptureEngine engine_;
        std::unique_ptr<goldsrc::client_state_t> client_;
        goldsrc::engine_studio_api_t api_{};
        goldsrc::refdef_t refdef_{};
        goldsrc::cl_enginefunc_dst_t destination_{};
        goldsrc::cvar_t* cvars_{};
        goldsrc::cvar_t enabled_{};
        goldsrc::cvar_t disabled_{};
        int current_texture_ = -1;
        GLuint framebuffer_{};
        GLuint image_{};
        bool initialized_{};

    protected:
        CaptureEngine& get_engine()
        {
            return engine_;
        }

        virtual int WindowSamples() const
        {
            return 0;
        }

        void CheckMultisampleVisibility(bool window);

        void SetUp() override
        {
            std::string error;
            ASSERT_TRUE(context_.Initialize(error, true, WindowSamples())) << error;
            ASSERT_TRUE(GLAD_GL_VERSION_3_3);

            g_Capture = CaptureState{};
            studio_test::g_Files.clear();
            client_ = std::make_unique<goldsrc::client_state_t>();
            cl = client_.get();
            enabled_.value = 1;
            developer = &disabled_;
            g_NitroApi = &engine_;
            p_currenttexture = &current_texture_;
            p_cvar_vars = &cvars_;
            destination_.pfnGetCvarPointer = CaptureCvar;
            p_g_engdstAddrs = &destination_;
            r_refdef = &refdef_;
            p_cl_elights = nullptr;
            render::g_RenderCfg = {};
            render::g_RenderCfg.studio_shader = &enabled_;
            render::g_RenderCfg.studio_queue = &enabled_;
            render::g_RenderCfg.studio_materials = &disabled_;
            for (int index = 0; index < 1024; index++)
            {
                lightgammatable[index] = index;
            }
            lightgammatable[1020] = 1023;

            render::Device_OnContextCreated();
            initialized_ = true;
            qglBindTexture = glad_glBindTexture;
            qglColorTableEXT = nullptr;
            g_OriginalMultiDraw = glad_glMultiDrawElements;
            glad_glMultiDrawElements = ObserveCaptureDraw;
            g_OriginalBoneUpload = glad_glBufferSubData;
            glad_glBufferSubData = ObserveCapturedBoneUpload;

            glGenTextures(2, g_Capture.textures);
            const uint8_t colors[][4] = {{255, 0, 0, 255}, {0, 255, 0, 255}};
            for (int skin = 0; skin < 2; skin++)
            {
                glBindTexture(GL_TEXTURE_2D, g_Capture.textures[skin]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, colors[skin]);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            }

            glGenTextures(1, &image_);
            glBindTexture(GL_TEXTURE_2D, image_);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kImageWidth, kImageHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glGenFramebuffers(1, &framebuffer_);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, image_, 0);
            ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
            glViewport(0, 0, kImageWidth, kImageHeight);
            glDisable(GL_DEPTH_TEST);
            glDisable(GL_BLEND);
            glDisable(GL_SCISSOR_TEST);
            glClearColor(0, 0, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glMatrixMode(GL_PROJECTION);
            glLoadIdentity();
            glMatrixMode(GL_MODELVIEW);
            glLoadIdentity();

            api_.Mod_Extradata = CaptureExtradata;
            api_.Mod_ForName = CaptureForName;
            api_.GetModelByIndex = CaptureModelByIndex;
            api_.GetCurrentEntity = CurrentCaptureEntity;
            api_.StudioGetBoneTransform = CaptureBones;
            api_.StudioSetHeader = CaptureHeader;
            api_.SetRenderModel = CaptureModelPointer;
            api_.StudioSetupModel = CaptureSetupModel;
            api_.StudioSetupLighting = CaptureLighting;
            api_.StudioSetupSkin = CaptureSkin;
            api_.StudioDrawPoints = CaptureStock;
            api_.GetForceFaceFlags = CaptureForcedFlags;
            api_.RestoreRenderer = CaptureNoop;
            render::Studio_PatchApi(&api_);
        }

        void TearDown() override
        {
            if (!initialized_)
            {
                return;
            }

            studio_test::g_Files.clear();
            render::MaterialEdit_Get()->End();
            render::Studio_Shutdown();
            render::Models_Clear();
            glad_glMultiDrawElements = g_OriginalMultiDraw;
            glad_glBufferSubData = g_OriginalBoneUpload;
            glDeleteFramebuffers(1, &framebuffer_);
            glDeleteTextures(1, &image_);
            glDeleteTextures(2, g_Capture.textures);
            render::Device_OnContextDestroyed();
            render::g_RenderCfg = {};
            developer = nullptr;
            g_NitroApi = nullptr;
            p_currenttexture = nullptr;
            p_cvar_vars = nullptr;
            p_g_engdstAddrs = nullptr;
            r_refdef = nullptr;
            cl = nullptr;
            cls = nullptr;
            render::ServerMaterials_Clear();
        }

        void BeginModel(CaptureModel& model, goldsrc::cl_entity_t& entity, float bone_x)
        {
            g_Capture.model = &model;
            g_Capture.entity = &entity;
            entity.model = &model.model;
            g_Capture.bones.fill(0);
            g_Capture.bones[0] = 1;
            g_Capture.bones[5] = 1;
            g_Capture.bones[10] = 1;
            g_Capture.bones[3] = bone_x;
            api_.SetRenderModel(&model.model);
            api_.StudioSetHeader(model.get_header());

            float light_direction[] = {0, 0, -1};
            goldsrc::alight_t light{};
            light.ambientlight = 255;
            light.color[0] = light.color[1] = light.color[2] = 1;
            light.plightvec = light_direction;
            api_.StudioSetupLighting(&light);
            g_Capture.model_prepared = true;
        }

        void DrawPart(int part)
        {
            void* bodypart = nullptr;
            void* submodel = nullptr;
            api_.StudioSetupModel(part, &bodypart, &submodel);
            api_.StudioDrawPoints();
        }

        void EndModel()
        {
            api_.RestoreRenderer();
        }

        void SetLighting(int ambient, int shade)
        {
            float direction[] = {0, 0, -1};
            goldsrc::alight_t light{};
            light.ambientlight = ambient;
            light.shadelight = shade;
            light.color[0] = light.color[1] = light.color[2] = 1;
            light.plightvec = direction;
            api_.StudioSetupLighting(&light);
        }

        std::array<uint8_t, 4> ReadPixel(int x)
        {
            std::array<uint8_t, 4> pixel{};
            glReadPixels(x, kImageHeight / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            return pixel;
        }

        std::array<uint8_t, kImageWidth * kImageHeight * 4> DrawSequence(
            CaptureModel& model,
            bool ordinary,
            const std::vector<int>& bodies,
            const std::function<void(size_t)>& prepare = {},
            bool transparent = false,
            const std::function<void(std::span<render::StudioDrawPacket>)>& prepare_packets = {}
        )
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            g_Capture.draws.clear();
            g_Capture.bone_uploads.clear();

            render::GetRendererState().studio.ResetSubmissionStats();
            g_RigidDraws = 0;
            g_OriginalInstancedDraw = std::exchange(glad_glDrawElementsInstanced, ObserveInstancedDraw);
            ncl_utils::ScopeExit restore([] { glad_glDrawElementsInstanced = g_OriginalInstancedDraw; });

            if (transparent)
            {
                render::Studio_BeginTransparentPass();
            }
            else
            {
                render::Studio_BeginOpaquePass();
            }

            goldsrc::cl_entity_t entity{};

            for (size_t index = 0; index < bodies.size(); ++index)
            {
                entity.curstate.body = bodies[index];
                BeginModel(model, entity, bodies.size() < 8 ? -0.4f + static_cast<float>(index) * 0.2f : 0.0f);

                if (prepare)
                {
                    prepare(index);
                }

                DrawPart(0);
                EndModel();

                if (ordinary && !prepare_packets)
                {
                    render::StudioRendererTestAccess::DrawOrdinaryPackets(render::GetRendererState().studio);
                }
            }

            if (prepare_packets)
            {
                prepare_packets(render::StudioRendererTestAccess::get_queued_packets(render::GetRendererState().studio));
            }

            if (ordinary)
            {
                render::StudioRendererTestAccess::DrawOrdinaryPackets(render::GetRendererState().studio);
            }

            if (transparent)
            {
                render::Studio_EndTransparentPass();
            }
            else
            {
                render::Studio_EndOpaquePass();
            }

            std::array<uint8_t, kImageWidth * kImageHeight * 4> pixels{};
            glReadPixels(0, 0, kImageWidth, kImageHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

            return pixels;
        }

        std::array<uint8_t, kImageWidth * kImageHeight * 4> DrawRigidSequence(
            CaptureModel& model,
            const std::vector<int>& bodies,
            const std::function<void(size_t)>& prepare = {},
            bool transparent = false,
            const std::function<void(std::span<render::StudioDrawPacket>)>& prepare_packets = {}
        )
        {
            return DrawSequence(model, false, bodies, prepare, transparent, prepare_packets);
        }

        std::array<uint8_t, kImageWidth * kImageHeight * 4> DrawOrdinarySequence(
            CaptureModel& model,
            const std::vector<int>& bodies,
            const std::function<void(size_t)>& prepare = {},
            bool transparent = false,
            const std::function<void(std::span<render::StudioDrawPacket>)>& prepare_packets = {}
        )
        {
            return DrawSequence(model, true, bodies, prepare, transparent, prepare_packets);
        }
    };

#ifdef _WIN32
    class StudioCaptureWindowGlTest : public StudioCaptureGlTest
    {
    protected:
        int WindowSamples() const override
        {
            return 4;
        }
    };
#endif
} // namespace

TEST_F(StudioCaptureGlTest, TransparentTraversalDrainsBeforeExternalDrawsAndClientCleanup)
{
    CaptureModel first("models/transparent_first.mdl", {1});
    CaptureModel second("models/transparent_second.mdl", {1});
    first.model.type = goldsrc::mod_studio;
    second.model.type = goldsrc::mod_studio;
    goldsrc::cl_entity_t entities[5]{};
    goldsrc::model_t external_models[3]{};
    const goldsrc::modtype_t external_types[] = {goldsrc::mod_sprite, goldsrc::mod_brush, goldsrc::mod_alias};
    entities[0].model = &first.model;
    entities[1].model = &second.model;
    for (int index = 0; index < 5; index++)
    {
        entities[index].index = index + 1;
        entities[index].curstate.renderamt = 255;
    }
    for (int index = 0; index < 3; index++)
    {
        external_models[index].type = external_types[index];
        entities[index + 2].model = &external_models[index];
    }
    const int order[] = {0, 1, 2, 0, 1, 3, 0, 1, 4, 0, 1};
    goldsrc::transObjRef objects[11]{};
    for (size_t index = 0; index < std::size(order); index++)
    {
        objects[index].pEnt = &entities[order[index]];
    }

    studio_test::Chain<int, goldsrc::cl_entity_t*> blend_chain;
    studio_test::Hook<int, goldsrc::cl_entity_t*> blend_hook;
    blend_hook.callback = [](goldsrc::cl_entity_t*) { return 255; };
    studio_test::Chain<void, goldsrc::cl_entity_t*> external_chain;
    studio_test::Hook<void, goldsrc::cl_entity_t*> external_hook;
    int external_draws = 0;
    external_hook.callback = [&](goldsrc::cl_entity_t* entity) {
        external_draws++;
        EXPECT_EQ(entity, &entities[external_draws + 1]);
        EXPECT_EQ(g_Capture.draws.size(), static_cast<size_t>(external_draws * 2));
        GLint program = -1;
        glGetIntegerv(GL_CURRENT_PROGRAM, &program);
        EXPECT_EQ(program, 0);
    };
    studio_test::Chain<void> triangles_chain;
    studio_test::Hook<void> triangles_hook;
    bool client_called = false;
    triangles_hook.callback = [&] {
        client_called = true;
        EXPECT_EQ(g_Capture.draws.size(), 8u);
        EXPECT_TRUE(glIsEnabled(GL_ALPHA_TEST));
        GLint environment = 0;
        glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &environment);
        EXPECT_EQ(environment, GL_MODULATE);
    };
    nitroapi::EngineData* engine = get_engine().GetEngineData();
    engine->CL_FxBlend.SetChain(&blend_chain);
    engine->CL_FxBlend.SetHook(&blend_hook);
    engine->R_DrawSpriteModel.SetChain(&external_chain);
    engine->R_DrawSpriteModel.SetHook(&external_hook);
    engine->R_DrawBrushModel.SetChain(&external_chain);
    engine->R_DrawBrushModel.SetHook(&external_hook);
    engine->R_DrawAliasModel.SetChain(&external_chain);
    engine->R_DrawAliasModel.SetHook(&external_hook);
    engine->ClientDLL_DrawTransparentTriangles.SetChain(&triangles_chain);
    engine->ClientDLL_DrawTransparentTriangles.SetHook(&triangles_hook);

    goldsrc::r_studio_interface_t studio{1, DrawTransparentCaptureModel, nullptr};
    goldsrc::cl_entity_t* current = nullptr;
    goldsrc::transObjRef* object_list = objects;
    int object_count = 11;
    float blend = 1;
    vec3_t origin{};
    goldsrc::qboolean fog = false;
    goldsrc::qboolean multitexture = false;
    goldsrc::cvar_t draw_entities{};
    draw_entities.value = 1;
    goldsrc::r_studio_interface_t* old_studio = pStudioAPI;
    goldsrc::cl_entity_t** old_current = p_currententity;
    goldsrc::transObjRef** old_objects = p_transObjects;
    int* old_count = p_numTransObjs;
    float* old_blend = p_r_blend;
    vec3_t* old_origin = p_r_entorigin;
    goldsrc::qboolean* old_fog = p_g_bUserFogOn;
    goldsrc::qboolean* old_multitexture = p_mtexenabled;
    goldsrc::cvar_t* old_draw_entities = r_drawentities;
    PFNGLDISABLEPROC old_disable = qglDisable;
    PFNGLENABLEPROC old_enable = qglEnable;
    PFNGLTEXENVFPROC old_texture_environment = qglTexEnvf;
    auto restore = ncl_utils::MakeScopeExit([&] {
        pStudioAPI = old_studio;
        p_currententity = old_current;
        p_transObjects = old_objects;
        p_numTransObjs = old_count;
        p_r_blend = old_blend;
        p_r_entorigin = old_origin;
        p_g_bUserFogOn = old_fog;
        p_mtexenabled = old_multitexture;
        r_drawentities = old_draw_entities;
        qglDisable = old_disable;
        qglEnable = old_enable;
        qglTexEnvf = old_texture_environment;
        g_TransparentModelDraw = {};
    });
    pStudioAPI = &studio;
    p_currententity = &current;
    p_transObjects = &object_list;
    p_numTransObjs = &object_count;
    p_r_blend = &blend;
    p_r_entorigin = &origin;
    p_g_bUserFogOn = &fog;
    p_mtexenabled = &multitexture;
    r_drawentities = &draw_entities;
    qglDisable = glad_glDisable;
    qglEnable = glad_glEnable;
    qglTexEnvf = glad_glTexEnvf;
    g_TransparentModelDraw = [&](int flags) {
        EXPECT_EQ(flags, goldsrc::STUDIO_RENDER | goldsrc::STUDIO_EVENTS);
        EXPECT_EQ(g_Capture.draws.size(), static_cast<size_t>(external_draws * 2));
        BeginModel(current == &entities[0] ? first : second, *current, current == &entities[0] ? -0.5f : 0.5f);
        DrawPart(0);
        EndModel();
        return 1;
    };

    R_DrawTEntitiesOnList(false);

    EXPECT_EQ(external_draws, 3);
    EXPECT_TRUE(client_called);
    EXPECT_EQ(object_count, 0);
    EXPECT_EQ(blend, 1.0f);
    ASSERT_EQ(g_Capture.draws.size(), 8u);
    for (size_t index = 0; index < g_Capture.draws.size(); index++)
    {
        EXPECT_EQ(g_Capture.draws[index].bone_x, index % 2 == 0 ? -0.5f : 0.5f);
    }
}

TEST_F(StudioCaptureGlTest, CapturesTwoModelsBeforeTheirEntitySkinAndPoseChange)
{
    StudioScopeTrace trace(render::GetRendererState().studio.get_monitor());
    CaptureModel first("models/capture_first.mdl", {1});
    CaptureModel second("models/capture_second.mdl", {1});
    goldsrc::cl_entity_t first_entity{};
    goldsrc::cl_entity_t second_entity{};
    first_entity.index = 1;
    second_entity.index = 2;
    second_entity.curstate.skin = 1;
    render::Studio_BeginOpaquePass();

    BeginModel(first, first_entity, -0.5f);
    DrawPart(0);
    EndModel();
    EXPECT_EQ(g_Capture.skin_calls, 1);
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_TRUE(g_Capture.draws.empty());

    BeginModel(second, second_entity, 0.5f);
    DrawPart(0);
    EndModel();
    EXPECT_EQ(g_Capture.skin_calls, 2);
    EXPECT_EQ(g_Capture.bone_calls, 2);
    ASSERT_TRUE(g_Capture.draws.empty());

    const int entity_calls = g_Capture.entity_calls;
    first_entity.curstate.skin = 1;
    second_entity.curstate.skin = 0;
    g_Capture.bones.fill(100);
    second.get_header()->numbones = 128;
    g_Capture.entity = nullptr;
    g_Capture.late = true;
    glMatrixMode(GL_MODELVIEW);
    glTranslatef(100, 100, 100);
    glBindTexture(GL_TEXTURE_2D, 0);
    render::Studio_EndOpaquePass();

    EXPECT_EQ(g_Capture.entity_calls, entity_calls);
    EXPECT_EQ(g_Capture.skin_calls, 2);
    EXPECT_EQ(g_Capture.bone_calls, 2);
    EXPECT_EQ(g_Capture.stock_calls, 0);
    ASSERT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_EQ(g_Capture.bone_uploads, (std::vector<GLsizeiptr>{48, 48}));
    EXPECT_EQ(g_Capture.draws[0].program, g_Capture.draws[1].program);
    EXPECT_NE(g_Capture.draws[0].program, 0);
    EXPECT_EQ(g_Capture.draws[0].texture, static_cast<GLint>(g_Capture.textures[0]));
    EXPECT_EQ(g_Capture.draws[1].texture, static_cast<GLint>(g_Capture.textures[1]));
    EXPECT_FLOAT_EQ(g_Capture.draws[0].bone_x, -0.5f);
    EXPECT_FLOAT_EQ(g_Capture.draws[1].bone_x, 0.5f);
    EXPECT_EQ(ReadPixel(8), (std::array<uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ(ReadPixel(24), (std::array<uint8_t, 4>{0, 255, 0, 255}));
    EXPECT_GT(trace.calls[performance::PerfStage::StudioDataPrepare], 0u);
    EXPECT_EQ(trace.parents[performance::PerfStage::StudioDataPrepare], performance::PerfStage::Studio);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, MaterialImageLoadingRebindsCapturedRemappedSkinAndEngineBookkeeping)
{
    CaptureModel model("models/capture_remapped_material.mdl", {1});
    goldsrc::mstudiotexture_t* textures =
        reinterpret_cast<goldsrc::mstudiotexture_t*>(reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex);
    std::strcpy(textures[0].name, "RemapFixture");
    studio_test::g_Files["models/capture_remapped_material.mat"] = R"({
        "version": 1, "materials": {"RemapFixture": {"program": "weapon_finish", "params": {
            "uPattern": {"texture": "fixtures/runtime-color.png"}
        }}}
    })";
    render::g_RenderCfg.studio_materials = nullptr;
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(g_Capture.draws[0].texture, static_cast<GLint>(g_Capture.textures[0]));
    EXPECT_EQ(*p_currenttexture, static_cast<int>(g_Capture.textures[0]));
    EXPECT_EQ(ReadPixel(16), (std::array<uint8_t, 4>{255, 0, 0, 255}));

    const uint8_t remapped[] = {0, 255, 0, 255};
    glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, remapped);
    glClear(GL_COLOR_BUFFER_BIT);
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_EQ(g_Capture.draws[1].texture, static_cast<GLint>(g_Capture.textures[0]));
    EXPECT_EQ(ReadPixel(16), (std::array<uint8_t, 4>{0, 255, 0, 255}));
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    render::Studio_EndOpaquePass();
    render::MaterialTextures_Clear();
}

TEST_F(StudioCaptureGlTest, GammaAdapterOwnsNormalizedStrideFourValuesBeforeFirstDraw)
{
    CaptureModel model("models/capture_gamma.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;

    for (int index = 0; index < 1024; ++index)
    {
        lightgammatable[index] = (index * 7) % 1024;
    }

    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    GLuint program = static_cast<GLuint>(g_Capture.draws.back().program);

    for (int index : {0, 1, 17, 255})
    {
        std::string name = "uLightGamma[" + std::to_string(index) + "]";
        GLint location = glGetUniformLocation(program, name.c_str());
        ASSERT_GE(location, 0);
        float value = -1;
        glGetUniformfv(program, location, &value);
        EXPECT_FLOAT_EQ(value, static_cast<float>((index * 4 * 7) % 1024) / 1023.0f);
    }

    lightgammatable[17 * 4] = 1023;
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    float value = -1;
    GLint location = glGetUniformLocation(program, "uLightGamma[17]");
    glGetUniformfv(program, location, &value);
    EXPECT_FLOAT_EQ(value, static_cast<float>((17 * 4 * 7) % 1024) / 1023.0f);
    render::Studio_OnGammaChanged();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    glGetUniformfv(program, location, &value);
    EXPECT_FLOAT_EQ(value, 1.0f);
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
}

TEST_F(StudioCaptureGlTest, FogMatchesFixedFunctionModesDepthAndTextureAlphaAcrossPrograms)
{
    CaptureModel model("models/capture_fog.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    const uint8_t skin[] = {255, 0, 0, 128};
    glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, skin);
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);
    glEnable(GL_FOG);
    const float fog_color[] = {0, 0, 1, 0};
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_DENSITY, 0.2f);
    glFogf(GL_FOG_START, 2);
    glFogf(GL_FOG_END, 6);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, 0.1, 10);

    for (const char* program : {"", "studio", "weapon_finish"})
    {
        SCOPED_TRACE(program);
        studio_test::g_Files.clear();
        if (*program != '\0')
        {
            studio_test::g_Files["models/capture_fog.mat"] =
                std::string(R"({"version":1,"materials":{"red":{"program":")") + program + R"(","params":{}}}})";
        }
        render::Models_ReloadDocuments();

        for (GLenum mode : {GL_LINEAR, GL_EXP, GL_EXP2})
        {
            SCOPED_TRACE(mode);
            glFogi(GL_FOG_MODE, mode);

            for (float depth : {0.5f, 4.0f, 8.0f})
            {
                SCOPED_TRACE(depth);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glTranslatef(0, 0, -depth);
                glClear(GL_COLOR_BUFFER_BIT);
                glUseProgram(0);
                glDisable(GL_CULL_FACE);
                glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
                glBegin(GL_TRIANGLES);
                glTexCoord2f(0, 0);
                glVertex3f(-0.3f, -0.6f, 0);
                glVertex3f(0, 0.6f, 0);
                glVertex3f(0.3f, -0.6f, 0);
                glEnd();
                const std::array<uint8_t, 4> expected = ReadPixel(16);
                EXPECT_EQ(expected[3], 128);

                glClear(GL_COLOR_BUFFER_BIT);
                BeginModel(model, entity, 0);
                DrawPart(0);
                EndModel();
                const std::array<uint8_t, 4> actual = ReadPixel(16);

                for (size_t channel = 0; channel < actual.size(); channel++)
                {
                    EXPECT_NEAR(actual[channel], expected[channel], 1) << channel;
                }
            }
        }
    }

    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, QueuedPacketsKeepIndependentFogAndLeaveLiveFogUnchanged)
{
    CaptureModel model("models/capture_queued_fog.mdl", {1});
    goldsrc::cl_entity_t entities[3]{};
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -4);
    render::Studio_BeginOpaquePass();

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    const float first_color[] = {0, 0, 1, 0};
    glFogfv(GL_FOG_COLOR, first_color);
    glFogf(GL_FOG_START, 2);
    glFogf(GL_FOG_END, 6);
    BeginModel(model, entities[0], -0.55f);
    DrawPart(0);
    EndModel();

    glFogi(GL_FOG_MODE, GL_EXP);
    const float second_color[] = {0, 1, 0, 0};
    glFogfv(GL_FOG_COLOR, second_color);
    glFogf(GL_FOG_DENSITY, 0.34657359f);
    BeginModel(model, entities[1], 0);
    DrawPart(0);
    EndModel();

    glDisable(GL_FOG);
    BeginModel(model, entities[2], 0.55f);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());

    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_EXP2);
    const float live_color[] = {1, 1, 0, 1};
    glFogfv(GL_FOG_COLOR, live_color);
    glFogf(GL_FOG_DENSITY, 0.75f);
    glFogf(GL_FOG_START, 7);
    glFogf(GL_FOG_END, 9);
    glLoadIdentity();
    glTranslatef(100, 100, 100);
    render::Studio_EndOpaquePass();

    const std::array<uint8_t, 4> expected[] = {{128, 0, 128, 255}, {64, 191, 0, 255}, {255, 0, 0, 255}};
    const int columns[] = {7, 16, 25};
    for (size_t index = 0; index < std::size(columns); index++)
    {
        const std::array<uint8_t, 4> actual = ReadPixel(columns[index]);
        for (size_t channel = 0; channel < actual.size(); channel++)
        {
            EXPECT_NEAR(actual[channel], expected[index][channel], 1) << index << ":" << channel;
        }
    }

    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 3u);
    EXPECT_EQ(g_Capture.draws[0].program, g_Capture.draws[1].program);
    EXPECT_EQ(g_Capture.draws[1].program, g_Capture.draws[2].program);
    EXPECT_TRUE(glIsEnabled(GL_FOG));
    GLint mode = 0;
    glGetIntegerv(GL_FOG_MODE, &mode);
    EXPECT_EQ(mode, GL_EXP2);
    float value = 0;
    glGetFloatv(GL_FOG_DENSITY, &value);
    EXPECT_FLOAT_EQ(value, 0.75f);
    glGetFloatv(GL_FOG_START, &value);
    EXPECT_FLOAT_EQ(value, 7);
    glGetFloatv(GL_FOG_END, &value);
    EXPECT_FLOAT_EQ(value, 9);
    std::array<float, 4> color{};
    glGetFloatv(GL_FOG_COLOR, color.data());
    EXPECT_EQ(color, (std::array<float, 4>{1, 1, 0, 1}));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, FogPreservesAlphaBlendingAdditiveAndMaskedMeshesAcrossPrograms)
{
    CaptureModel model("models/capture_blended_fog.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(1, 1, 1, 1);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);
    const float fog_color[] = {0, 0, 1, 0};
    glFogfv(GL_FOG_COLOR, fog_color);
    glFogf(GL_FOG_START, 2);
    glFogf(GL_FOG_END, 6);
    glClearColor(0.1f, 0.2f, 0.3f, 0.25f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, 1, 10);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glTranslatef(0, 0, -4);

    for (const char* program : {"", "studio", "weapon_finish"})
    {
        SCOPED_TRACE(program);
        studio_test::g_Files.clear();
        if (*program != '\0')
        {
            studio_test::g_Files["models/capture_blended_fog.mat"] =
                std::string(R"({"version":1,"materials":{"red":{"program":")") + program + R"(","params":{}}}})";
        }
        render::Models_ReloadDocuments();

        for (int flags : {0, goldsrc::STUDIO_NF_ADDITIVE, goldsrc::STUDIO_NF_MASKED})
        {
            SCOPED_TRACE(flags);
            goldsrc::mstudiotexture_t* textures = reinterpret_cast<goldsrc::mstudiotexture_t*>(
                reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex
            );
            textures[0].flags = goldsrc::STUDIO_NF_FULLBRIGHT | flags;

            for (uint8_t alpha : {uint8_t{64}, uint8_t{192}})
            {
                SCOPED_TRACE(alpha);
                const uint8_t skin[] = {255, 0, 0, alpha};
                glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, skin);
                glEnable(GL_BLEND);
                glBlendFunc(
                    flags == goldsrc::STUDIO_NF_ADDITIVE ? GL_ONE : GL_SRC_ALPHA,
                    flags == goldsrc::STUDIO_NF_ADDITIVE ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA
                );
                if (flags == goldsrc::STUDIO_NF_MASKED)
                {
                    glEnable(GL_ALPHA_TEST);
                    glAlphaFunc(GL_GREATER, 0.5f);
                }
                else
                {
                    glDisable(GL_ALPHA_TEST);
                }
                glClear(GL_COLOR_BUFFER_BIT);
                glUseProgram(0);
                glDisable(GL_CULL_FACE);
                glBegin(GL_TRIANGLES);
                glTexCoord2f(0, 0);
                glVertex3f(-0.3f, -0.6f, 0);
                glVertex3f(0, 0.6f, 0);
                glVertex3f(0.3f, -0.6f, 0);
                glEnd();
                const std::array<uint8_t, 4> expected = ReadPixel(16);

                glClear(GL_COLOR_BUFFER_BIT);
                BeginModel(model, entity, 0);
                DrawPart(0);
                EndModel();
                const std::array<uint8_t, 4> actual = ReadPixel(16);
                for (size_t channel = 0; channel < actual.size(); channel++)
                {
                    EXPECT_NEAR(actual[channel], expected[channel], 1) << channel;
                }
            }
        }
    }

    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RollsBackOnlyRejectedBodypartAfterMoreThan256Meshes)
{
    StudioScopeTrace trace(render::GetRendererState().studio.get_monitor());
    CaptureModel model("models/capture_rollback.mdl", {1, 258, 1}, 1);
    goldsrc::cl_entity_t entity{};
    entity.index = 3;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);

    DrawPart(0);
    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_TRUE(g_Capture.draws.empty());
    DrawPart(1);
    EXPECT_EQ(g_Capture.stock_calls, 1);
    EXPECT_STREQ(render::Studio_LastRejection(), "mesh skin reference does not resolve to a texture");
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(g_Capture.draws[0].meshes, 1);
    EXPECT_EQ(g_Capture.draws[0].indices, 3);
    EXPECT_EQ(g_Capture.skin_calls, 1);

    DrawPart(2);
    EXPECT_EQ(g_Capture.stock_calls, 2);
    EXPECT_EQ(g_Capture.skin_calls, 1);
    EXPECT_EQ(g_Capture.bone_calls, 1);
    EndModel();
    render::Studio_EndOpaquePass();

    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(g_Capture.draws[0].meshes, 1);
    EXPECT_EQ(g_Capture.draws[0].indices, 3);
    EXPECT_EQ(ReadPixel(16), (std::array<uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_GT(trace.calls[performance::PerfStage::StudioDataPrepare], 0u);
    EXPECT_EQ(trace.parents[performance::PerfStage::StudioDataPrepare], performance::PerfStage::Studio);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, ReusedCaptureStorageDoesNotRetainShellInputs)
{
    CaptureModel model("models/capture_shell_reset.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 4;
    entity.curstate.renderfx = goldsrc::kRenderFxGlowShell;
    entity.curstate.renderamt = 20;
    entity.curstate.rendercolor.r = 255;
    g_Capture.forced_flags = goldsrc::STUDIO_NF_CHROME;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, -0.5f);
    DrawPart(0);
    EndModel();

    entity.curstate.renderfx = 0;
    g_Capture.forced_flags = 0;
    BeginModel(model, entity, 0.5f);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();

    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_FLOAT_EQ(g_Capture.draws[0].glow_scale, 1.0f);
    EXPECT_EQ(g_Capture.draws[0].shell_color, (std::array<float, 3>{1, 0, 0}));
    EXPECT_FLOAT_EQ(g_Capture.draws[1].glow_scale, 0.0f);
    EXPECT_EQ(g_Capture.draws[1].shell_color, (std::array<float, 3>{0, 0, 0}));
    EXPECT_EQ(ReadPixel(24), (std::array<uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, GeometryLookupRetainsIdentityAcrossHeaderReplacementAndClear)
{
    GeometryTrace trace;
    CaptureModel first("models/capture_replaced.mdl", {2});
    CaptureModel replacement("models/capture_replaced.mdl", {3});
    goldsrc::cl_entity_t entity{};
    entity.index = 5;
    render::Studio_BeginOpaquePass();
    BeginModel(first, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());
    ASSERT_TRUE(render::Models_PrepareMetadata(&replacement.model, replacement.get_header(), replacement.get_header(), &replacement.model));
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    ASSERT_EQ(g_GeometryDeletesAfterDraws.size(), 1u);
    EXPECT_EQ(g_GeometryDeletesAfterDraws[0], 1u);
    BeginModel(replacement, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();

    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_EQ(g_Capture.draws[0].meshes, 2);
    EXPECT_EQ(g_Capture.draws[1].meshes, 3);
    EXPECT_EQ(render::Models_ModelCount(), 1);

    render::Models_Clear();
    EXPECT_EQ(render::Models_ModelCount(), 0);
    render::Studio_BeginOpaquePass();
    BeginModel(first, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();

    ASSERT_EQ(g_Capture.draws.size(), 3u);
    EXPECT_EQ(g_Capture.draws[2].meshes, 2);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, GeometryLookupIncludesNameWhenHeaderAddressIsReused)
{
    CaptureModel model("models/capture_old_name.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 6;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    std::strcpy(model.model.name, "models/capture_new_name.mdl");
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();

    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_EQ(render::Models_ModelCount(), 2);
    EXPECT_STREQ(render::Models_ModelName(0), "models/capture_new_name.mdl");
    EXPECT_STREQ(render::Models_ModelName(1), "models/capture_old_name.mdl");
    EXPECT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, InvalidMetadataIsExcludedFromEditorLookups)
{
    CaptureModel model("models/capture_invalid_bones.mdl", {1});
    model.get_header()->numbones = 129;
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    for (int draw = 0; draw < 2; ++draw)
    {
        BeginModel(model, entity, 0);
        DrawPart(0);
        EndModel();
    }
    render::Studio_EndOpaquePass();

    EXPECT_EQ(g_Capture.stock_calls, 2);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(render::Models_ModelCount(), 0);
    EXPECT_EQ(render::Models_TextureCount(model.model.name), 0);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, GeometryOnlyFailurePreservesValidEditorMetadata)
{
    CaptureModel model("models/capture_invalid_geometry.mdl", {1});
    goldsrc::mstudiomodel_t* submodel = static_cast<goldsrc::mstudiomodel_t*>(model.get_submodel(0));
    submodel->vertindex = model.get_header()->texturedataindex + 1;
    ASSERT_TRUE(render::Models_PrepareMetadata(&model.model, model.get_header(), model.get_header(), &model.model));
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();

    for (int draw = 0; draw < 2; draw++)
    {
        BeginModel(model, entity, 0);
        DrawPart(0);
        EndModel();
    }

    render::Studio_EndOpaquePass();
    EXPECT_EQ(g_Capture.stock_calls, 2);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_EQ(render::Models_TextureCount(model.model.name), 2);
    EXPECT_NE(render::Models_ResolveMaterialByName(model.model.name, "red", nullptr), nullptr);
}

TEST_F(StudioCaptureGlTest, UploadFailurePreservesMetadataAndMaterials)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_upload_failed.mdl", {1});
    ASSERT_TRUE(render::Models_PrepareMetadata(&model.model, model.get_header(), model.get_header(), &model.model));
    g_FailGeometryBuffers = true;
    EXPECT_EQ(render::Models_PrepareGeometry(&model.model, model.get_header()), nullptr);
    g_FailGeometryBuffers = false;
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_EQ(render::Models_TextureCount(model.model.name), 2);
    EXPECT_NE(render::Models_ResolveMaterialByName(model.model.name, "red", nullptr), nullptr);
    EXPECT_EQ(g_GeometryCreations, 0);
}

TEST_F(StudioCaptureGlTest, SessionResetRebuildsSameNameAndHeaderStorageWithChangedMesh)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_session_reused.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    ASSERT_EQ(g_GeometryUploads, 2);
    ASSERT_EQ(g_GeometryCreations, 2);
    const render::StudioGpuModel* first = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(first, nullptr);
    ASSERT_FALSE(first->bounds.empty());
    EXPECT_FLOAT_EQ(first->bounds[0].minimum.x, -0.3f);
    ASSERT_EQ(render::Models_ModelCount(), 1);
    int operations = g_GeometryOperations;
    render::Studio_DiscardPending();
    render::Models_ForgetSessionMetadata();
    EXPECT_EQ(render::Models_ModelName(0), nullptr);
    EXPECT_EQ(render::Models_Metadata(model.model.name), nullptr);
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_EQ(g_GeometryCreations, 2);
    EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
    EXPECT_EQ(g_GeometryOperations, operations);

    model.SetTriangle({-0.8f, -0.6f, 0, 0, 0.6f, 0, 0.8f, -0.6f, 0});
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    const render::StudioGpuModel* replacement = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(replacement, nullptr);
    ASSERT_FALSE(replacement->bounds.empty());
    EXPECT_FLOAT_EQ(replacement->bounds[0].minimum.x, -0.8f);
    EXPECT_FLOAT_EQ(replacement->bounds[0].maximum.x, 0.8f);
    EXPECT_EQ(g_GeometryUploads, 3);
    EXPECT_EQ(g_GeometryCreations, 4);
    EXPECT_EQ(g_GeometryDeletesAfterDraws.size(), 2u);
    EXPECT_EQ(g_Capture.stock_calls, 0);
}

TEST_F(StudioCaptureGlTest, StudioLoadDrainsQueuedWorkBeforeCacheAllocationAndSelectiveRetirement)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_reload_boundary.mdl", {1});
    CaptureModel retained("models/capture_reload_retained.mdl", {1});
    ASSERT_TRUE(render::Models_PrepareMetadata(&retained.model, retained.get_header(), retained.get_header(), &retained.model));
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());
    goldsrc::studiohdr_t replacement{};
    replacement.version = ncl_studio::kStudioVersion;
    replacement.length = sizeof(replacement);
    goldsrc::studiohdr_t allocated{};
    int allocations = 0;
    get_engine().allocation.callback = [&](goldsrc::cache_user_t* cache, int size, char*) -> void* {
        allocations++;
        EXPECT_EQ(size, sizeof(allocated));
        EXPECT_EQ(g_Capture.draws.size(), 1u);
        EXPECT_EQ(render::Models_Metadata(model.model.name), nullptr);
        EXPECT_NE(render::Models_Metadata(retained.model.name), nullptr);
        EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
        cache->data = &allocated;
        return cache->data;
    };

    Mod_LoadStudioModel(&model.model, &replacement, sizeof(replacement));

    EXPECT_EQ(allocations, 1);
    EXPECT_EQ(model.model.cache.data, &allocated);
    EXPECT_EQ(allocated.version, ncl_studio::kStudioVersion);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_STREQ(render::Models_ModelName(0), retained.model.name);
    render::Studio_EndOpaquePass();
    get_engine().allocation.callback = {};
}

TEST_F(StudioCaptureGlTest, ExistingSlotReuseDrainsQueuedWorkBeforeFreeAndPreservesOtherMetadata)
{
    ASSERT_EQ(mod_numknown, 0);
    struct ResetModels
    {
        ~ResetModels()
        {
            std::memset(mod_known, 0, sizeof(mod_known));
            mod_numknown = 0;
        }
    } reset;
    GeometryTrace trace;
    CaptureModel model("models/capture_slot_reused.mdl", {1});
    CaptureModel retained("models/capture_slot_retained.mdl", {1});
    ASSERT_TRUE(render::Models_PrepareMetadata(&retained.model, retained.get_header(), retained.get_header(), &retained.model));
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());
    mod_numknown = goldsrc::MAX_KNOWN_MODELS;

    for (int index = 0; index < mod_numknown; index++)
    {
        std::snprintf(mod_known[index].name, sizeof(mod_known[index].name), "models/occupied_%d.mdl", index);
        mod_known[index].needload = NL_PRESENT;
    }

    mod_known[0] = model.model;
    mod_known[0].type = goldsrc::mod_studio;
    mod_known[0].needload = NL_UNREFERENCED;
    mod_known[0].cache.data = model.get_header();
    int frees = 0;
    get_engine().free.callback = [&](goldsrc::cache_user_t* cache) {
        frees++;
        EXPECT_EQ(g_Capture.draws.size(), 1u);
        EXPECT_EQ(render::Models_Metadata(model.model.name), nullptr);
        EXPECT_NE(render::Models_Metadata(retained.model.name), nullptr);
        cache->data = nullptr;
    };

    EXPECT_EQ(Mod_FindName(false, "models/capture_new_slot.mdl"), &mod_known[0]);

    EXPECT_EQ(frees, 1);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_STREQ(render::Models_ModelName(0), retained.model.name);
    render::Studio_EndOpaquePass();
    get_engine().free.callback = [](goldsrc::cache_user_t* cache) { cache->data = nullptr; };
}

TEST_F(StudioCaptureGlTest, TextureHeaderReplacementDoesNotRebuildGeometry)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_texture_replaced.mdl", {1});
    CaptureModel companion("models/capture_texture_companion.mdl", {1});
    ASSERT_TRUE(render::Models_PrepareMetadata(&model.model, model.get_header(), model.get_header(), &model.model));
    const render::StudioGpuModel* geometry = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(geometry, nullptr);
    goldsrc::mstudiotexture_t* textures = reinterpret_cast<goldsrc::mstudiotexture_t*>(
        reinterpret_cast<uint8_t*>(companion.get_header()) + companion.get_header()->textureindex
    );
    std::strcpy(textures[0].name, "new_texture");
    ASSERT_TRUE(render::Models_PrepareMetadata(&model.model, model.get_header(), companion.get_header(), &companion.model));
    EXPECT_STREQ(render::Models_TextureName(model.model.name, 0), "new_texture");
    EXPECT_NE(render::Models_ResolveMaterialByName(model.model.name, "new_texture", nullptr), nullptr);
    EXPECT_EQ(render::Models_PrepareGeometry(&model.model, model.get_header()), geometry);
    EXPECT_EQ(g_GeometryUploads, 1);
    EXPECT_EQ(g_GeometryCreations, 2);
    EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
}

TEST_F(StudioCaptureGlTest, CompanionReloadAtSameAddressRefreshesOwnedTextureStateWithoutGeometryRebuild)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_companion_reload.mdl", {1});
    CaptureModel retained("models/capture_companion_retained.mdl", {1});
    model.get_header()->textureindex = 0;
    model.get_header()->numtextures = 0;
    ASSERT_TRUE(render::Models_PrepareMetadata(&retained.model, retained.get_header(), retained.get_header(), &retained.model));
    CompanionLoader companion(get_engine(), "models/capture_companion_reloadT.mdl", true);
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_STREQ(render::Models_TextureName(model.model.name, 0), "red");
    ASSERT_EQ(render::Models_SkinFamilyCount(model.model.name), 2);
    ASSERT_EQ(g_GeometryUploads, 2);
    const render::StudioGpuModel* geometry = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(geometry, nullptr);
    const goldsrc::studiohdr_t* storage = companion.get_header();
    companion.model.cache.data = nullptr;
    std::strcpy(companion.file.textures[0].name, "blue");
    companion.file.textures[0].flags = goldsrc::STUDIO_NF_NOMIPS;
    companion.file.textures[0].width = 1;
    companion.file.textures[0].height = 4;
    companion.file.header.numskinfamilies = 1;
    companion.before_allocate = [&]() {
        EXPECT_EQ(render::Models_TextureCount(model.model.name), 0);
        EXPECT_EQ(render::Models_ResolveMaterialByName(model.model.name, "red", nullptr), nullptr);
        EXPECT_NE(render::Models_Metadata(retained.model.name), nullptr);
        EXPECT_EQ(render::Models_PrepareGeometry(&model.model, model.get_header()), geometry);
    };

    companion.Reload();

    EXPECT_EQ(companion.model.cache.data, storage);
    EXPECT_EQ(companion.loads, 1);
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_STREQ(render::Models_TextureName(model.model.name, 0), "blue");
    EXPECT_EQ(render::Models_SkinFamilyCount(model.model.name), 1);
    EXPECT_EQ(render::Models_ResolveMaterialByName(model.model.name, "red", nullptr), nullptr);
    const ncl_material::Material* material = render::Models_ResolveMaterialByName(model.model.name, "blue", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("fullbright"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("fullbright")->scalar, 0);
    EXPECT_NE(render::Models_Metadata(retained.model.name), nullptr);
    EXPECT_EQ(render::Models_PrepareGeometry(&model.model, model.get_header()), geometry);
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_EQ(g_GeometryCreations, 2);
    EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, NestedCompanionLoadPreservesCurrentStudioCapture)
{
    GeometryTrace trace;
    CaptureModel queued("models/capture_companion_queued.mdl", {1});
    CaptureModel model("models/capture_companion_nested.mdl", {1, 1});
    model.get_header()->textureindex = 0;
    model.get_header()->numtextures = 0;
    goldsrc::cl_entity_t queued_entity{};
    queued_entity.index = 6;
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(queued, queued_entity, -0.25f);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());
    CompanionLoader companion(get_engine(), "models/capture_companion_nestedT.mdl", false);
    companion.before_allocate = [&]() {
        EXPECT_EQ(g_Capture.draws.size(), 1u);
        EXPECT_TRUE(render::Studio_Lighting().valid);
        EXPECT_FLOAT_EQ(render::Studio_Lighting().ambient, 255);
        EXPECT_NE(render::Models_Metadata(queued.model.name), nullptr);
    };
    BeginModel(model, entity, 0.25f);
    ASSERT_TRUE(render::Studio_Lighting().valid);

    DrawPart(0);
    DrawPart(1);
    EndModel();
    render::Studio_EndOpaquePass();

    EXPECT_EQ(companion.loads, 1);
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 2u);
    EXPECT_EQ(g_Capture.draws[0].meshes, 1);
    EXPECT_EQ(g_Capture.draws[1].meshes, 2);
    EXPECT_FLOAT_EQ(g_Capture.draws[1].bone_x, 0.25f);
    EXPECT_TRUE(render::Studio_Lighting().valid);
    EXPECT_EQ(render::Models_TextureCount(model.model.name), 2);
    EXPECT_EQ(g_GeometryUploads, 3);
    EXPECT_EQ(g_GeometryCreations, 3);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, DeviceLossRetainsMetadataAndRecreatesGeometryOnNextDraw)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_device_loss.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    ASSERT_EQ(g_GeometryUploads, 2);
    const ncl_studio::StudioMetadata* metadata = render::Models_Metadata(model.model.name);
    ASSERT_NE(metadata, nullptr);
    render::Device_OnContextDestroyed();
    EXPECT_EQ(render::Models_Metadata(model.model.name), metadata);
    EXPECT_EQ(render::Models_TextureCount(model.model.name), 2);
    EXPECT_NE(render::Models_ResolveMaterialByName(model.model.name, "red", nullptr), nullptr);
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
    EXPECT_EQ(g_GeometryBufferDeletes, 0);
    render::Device_OnContextCreated();
    trace.Attach();
    glad_glMultiDrawElements = ObserveCaptureDraw;
    glad_glBufferSubData = ObserveCapturedBoneUpload;
    EXPECT_EQ(g_GeometryUploads, 2);
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    EXPECT_EQ(g_GeometryUploads, 4);
    EXPECT_EQ(g_GeometryCreations, 4);
    EXPECT_TRUE(g_GeometryDeletesAfterDraws.empty());
    EXPECT_EQ(g_Capture.stock_calls, 0);
    EXPECT_EQ(g_Capture.draws.size(), 2u);
}

TEST_F(StudioCaptureGlTest, ByNameRefreshesShaderAndServerGenerationDefaultsWithoutAnotherDraw)
{
    CaptureModel model("models/capture_default_generation.mdl", {1});
    studio_test::g_Files["models/capture_default_generation.mat"] = R"({"version":1,"materials":{"red":{"program":"studio"}}})";
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    const ncl_material::Material* material = render::Models_ResolveMaterialByName(model.model.name, "red", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uBaseTint"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uBaseTint")->vec4[0], 1);
    ncl_material::ProgramManifest* manifest = const_cast<ncl_material::ProgramManifest*>(render::Manifest_Get("studio"));
    ASSERT_NE(manifest, nullptr);
    ncl_material::ParamSchema* tint = nullptr;

    for (ncl_material::ParamSchema& parameter : manifest->parameters)
    {
        if (parameter.name == "uBaseTint")
        {
            tint = &parameter;
        }
    }

    ASSERT_NE(tint, nullptr);
    struct RestoreDefault
    {
        ncl_material::ParamSchema& parameter;
        ncl_material::ParamValue value;

        ~RestoreDefault()
        {
            parameter.default_value = value;
        }
    } restore{*tint, tint->default_value};
    tint->default_value.vec4[0] = 0.5f;
    studio_test::g_Files["generation_fixture.mat"] =
        R"({"version":1,"materials":[{"name":"generation","texture":"red","material":{"program":"studio"}}]})";
    render::ServerMaterials_SetBinding(render::ServerMaterialBindingOwner{99, 1}, "generation_fixture.mat", "generation");
    ASSERT_NE(render::ServerMaterials_BindingFor({99, "red"}), nullptr);
    unsigned int server_generation = render::ServerMaterials_Generation();
    render::ServerMaterials_Clear();
    ASSERT_NE(render::ServerMaterials_Generation(), server_generation);
    material = render::Models_ResolveMaterialByName(model.model.name, "red", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uBaseTint"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uBaseTint")->vec4[0], 0.5f);
    tint->default_value.vec4[0] = 0.25f;
    uint32_t generation = render::Shaders_Generation();
    render::Device_OnContextDestroyed();
    ASSERT_NE(render::Shaders_Generation(), generation);
    material = render::Models_ResolveMaterialByName(model.model.name, "red", nullptr);
    ASSERT_NE(material, nullptr);
    ASSERT_NE(material->FindParam("uBaseTint"), nullptr);
    EXPECT_FLOAT_EQ(material->FindParam("uBaseTint")->vec4[0], 0.25f);
    EXPECT_EQ(g_Capture.draws.size(), 1u);
}

TEST_F(StudioCaptureGlTest, OrdinaryEntityKeepsItsCapturedScale)
{
    CaptureModel model("models/capture_ordinary_scale.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 8;
    entity.curstate.scale = 2.0f;

    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0.25f);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();


    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_FLOAT_EQ(g_Capture.draws[0].bone_x, 0.25f);
}

TEST_F(StudioCaptureGlTest, PreviewScopeUsesItsRequestedScale)
{
    CaptureModel model("models/capture_preview_scale.mdl", {1});
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    PreviewModel request{};
    request.scale = 2.0f;
    render::Studio_BeginOpaquePass();

    {
        render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request);
        BeginModel(model, entity, 0.25f);
        DrawPart(0);
        EndModel();
    }

    render::Studio_EndOpaquePass();
    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_FLOAT_EQ(g_Capture.draws[0].bone_x, 0.5f);
}
TEST_F(StudioCaptureGlTest, AuthoringScopeUsesPreviewCameraWithGameplayShadersDisabled)
{
    CaptureModel model("models/capture_authoring.mdl", {1});
    goldsrc::cl_entity_t entity{};
    goldsrc::cvar_t disabled{};
    goldsrc::cvar_t* saved_shader = render::g_RenderCfg.studio_shader;
    goldsrc::cl_entity_t** saved_current = p_currententity;
    render::g_RenderCfg.studio_shader = &disabled;
    p_currententity = &g_Capture.entity;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.camera.origin[0] = 42.0f;
    params.camera.angles[1] = 90.0f;
    PreviewModel request{};

    {
        render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request);
        BeginModel(model, entity, 0.25f);
        DrawPart(0);
        EndModel();
    }

    p_currententity = saved_current;
    render::g_RenderCfg.studio_shader = saved_shader;
    ASSERT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_FLOAT_EQ(g_Capture.draws[0].chrome_origin[0], 42.0f);
    EXPECT_NEAR(g_Capture.draws[0].view_right[0], 1.0f, 0.00001f);
    EXPECT_NEAR(g_Capture.draws[0].view_right[1], 0.0f, 0.00001f);
    EXPECT_EQ(g_Capture.draws[0].entity_lights, 0);
    EXPECT_FLOAT_EQ(disabled.value, 0.0f);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
TEST_F(StudioCaptureGlTest, AuthoredMaterialsApplyCoverageAndThePreviewClockToActualModels)
{
    CaptureModel model("models/capture_coverage.mdl", {1});
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    auto draw = [&]() {
        glClear(GL_COLOR_BUFFER_BIT);

        {
            render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request);
            BeginModel(model, entity, 0);
            DrawPart(0);
            EndModel();
        }

        std::vector<uint8_t> pixels(kImageWidth * kImageHeight * 4);
        glReadPixels(0, 0, kImageWidth, kImageHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };

    draw();
    ASSERT_TRUE(editor->Begin(model.model.name, "red"));

    for (const char* program : {"studio", "weapon_finish"})
    {
        SCOPED_TRACE(program);
        ASSERT_TRUE(editor->SetEditedProgram(program));

        for (const char* amount : {"uDissolve", "uNoise"})
        {
            ASSERT_TRUE(editor->SetScalar("uDissolve", 0));
            ASSERT_TRUE(editor->SetScalar("uNoise", 0));
            draw();
            EXPECT_EQ(ReadPixel(16)[0], 255);
            ASSERT_TRUE(editor->SetScalar(amount, 1));
            draw();
            EXPECT_EQ(ReadPixel(16)[0], 0) << amount;
        }

        ASSERT_TRUE(editor->SetScalar("uDissolve", 0));
        ASSERT_TRUE(editor->SetScalar("uNoise", 0.5f));
        ASSERT_TRUE(editor->SetScalar("uNoiseSpace", 1));
        ASSERT_TRUE(editor->SetScalar("uNoiseSeed", 7));
        ASSERT_TRUE(editor->SetScalar("uNoiseSpeed", 4));
        params.time = 0.25;
        std::vector<uint8_t> first = draw();
        EXPECT_EQ(draw(), first);
        params.time = 0.5;
        EXPECT_NE(draw(), first);
        ASSERT_TRUE(editor->SetScalar("uNoiseSpeed", 0));
        std::vector<uint8_t> frozen = draw();
        params.time = 99;
        EXPECT_EQ(draw(), frozen);
    }

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, LocalAttachmentEffectCapturesItsOrdinaryParentBeforeHeaderSwitch)
{
    CaptureModel parent("models/capture_parent.mdl", {1});
    CaptureModel weapon("models/p_capture_effect.mdl", {1});
    studio_test::g_Files["models/p_capture_effect.mat"] =
        R"({"version":1,"materials":{"red":{"program":"studio","params":{"uOccludedAmount":1,"uOccludedColor":[0,1,0,1]}}}})";
    g_Capture.attachment = &weapon;
    render::g_RenderCfg.studio_materials = nullptr;
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    entity.player = 1;
    entity.curstate.weaponmodel = 7;
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kImageWidth, kImageHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1);
    glClear(GL_DEPTH_BUFFER_BIT);
    render::StudioEffects_BeginView();
    render::StudioPlayerEffectsScope effects(&entity);
    BeginModel(parent, entity, 0);
    DrawPart(0);
    EndModel();
    EXPECT_EQ(render::StudioEffects_TargetCount(), 1u);
    EXPECT_EQ(g_Capture.late_attachment_loads, 0);
    BeginModel(weapon, entity, 0);
    g_Capture.bones[11] = .5f;
    DrawPart(0);
    EndModel();
    render::StudioEffects_EndView();
    EXPECT_EQ(ReadPixel(16), (std::array<uint8_t, 4>{255, 0, 0, 255}));
    EXPECT_EQ(g_Capture.stock_calls, 0);
    glDeleteRenderbuffers(1, &depth);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, GlitchDeformsGeometrySplitsColorAndUsesThePreviewClock)
{
    CaptureModel model("models/capture_glitch.mdl", {1});
    model.SetTextureCoordinates();
    goldsrc::cl_entity_t entity{};
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    std::vector<uint8_t> gradient(64 * 64 * 4);
    for (int y = 0; y < 64; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            const int index = (y * 64 + x) * 4;
            gradient[index] = static_cast<uint8_t>(32 + x * 3);
            gradient[index + 1] = static_cast<uint8_t>(32 + y * 3);
            gradient[index + 2] = static_cast<uint8_t>(224 - x * 3);
            gradient[index + 3] = 255;
        }
    }
    glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, gradient.data());
    glClearColor(0, 0, 0, 0);
    render::preview::PreviewInteraction interaction(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    auto draw = [&]() {
        interaction.BeginFrame();
        interaction.BeginModel(0, request);
        glClear(GL_COLOR_BUFFER_BIT);
        {
            render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request, &interaction);
            BeginModel(model, entity, 0);
            DrawPart(0);
            EndModel();
        }
        std::vector<uint8_t> pixels(kImageWidth * kImageHeight * 4);
        glReadPixels(0, 0, kImageWidth, kImageHeight, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        return pixels;
    };
    draw();
    ASSERT_TRUE(editor->Begin(model.model.name, "red"));
    for (const char* program : {"studio", "weapon_finish"})
    {
        SCOPED_TRACE(program);
        ASSERT_TRUE(editor->SetEditedProgram(program));
        const std::vector<uint8_t> original = draw();
        ASSERT_TRUE(editor->SetScalar("uGlitchAmount", 1));
        ASSERT_TRUE(editor->SetScalar("uGlitchDisplacement", 0));
        ASSERT_TRUE(editor->SetScalar("uGlitchRgbSplit", 4));
        ASSERT_TRUE(editor->SetScalar("uGlitchSpeed", 0));
        const std::vector<uint8_t> split = draw();
        EXPECT_NE(split, original);
        for (size_t index = 3; index < split.size(); index += 4)
        {
            EXPECT_EQ(split[index], original[index]);
        }
        ASSERT_TRUE(editor->SetScalar("uGlitchRgbSplit", 0));
        ASSERT_TRUE(editor->SetScalar("uGlitchDisplacement", 12));
        const std::vector<uint8_t> displaced = draw();
        int changed_coverage = 0;
        for (size_t index = 3; index < displaced.size(); index += 4)
        {
            changed_coverage += displaced[index] != original[index];
        }
        EXPECT_GT(changed_coverage, 0);
        int checked_present = 0;
        int checked_absent = 0;
        for (int y = 0; y < kImageHeight; y++)
        {
            for (int x = 0; x < kImageWidth; x++)
            {
                const bool covered = displaced[(y * kImageWidth + x) * 4 + 3] != 0;
                if ((covered && checked_present >= 8) || (!covered && checked_absent >= 8))
                {
                    continue;
                }
                const PreviewHit hit = interaction.Render({0, 0, kImageWidth, kImageHeight}, {0, 0, kImageWidth, kImageHeight}, x, y);
                EXPECT_EQ(hit.model_index, covered ? 0 : -1) << x << "," << y;
                checked_present += covered;
                checked_absent += !covered;
            }
        }
        EXPECT_GT(checked_present, 0);
        params.time = 15;
        EXPECT_EQ(draw(), displaced);
        ASSERT_TRUE(editor->SetScalar("uGlitchSpeed", 12));
        const std::vector<uint8_t> first = draw();
        params.time += .25;
        EXPECT_NE(draw(), first);
        ASSERT_TRUE(editor->SetScalar("uGlitchAmount", 0));
        EXPECT_EQ(draw(), original);
        ASSERT_TRUE(editor->SetScalar("uGlitchAmount", 1));
        EXPECT_NE(draw(), original);
        ASSERT_TRUE(editor->ClearParam("uGlitchAmount"));
        EXPECT_TRUE(draw() == original) << "clearing restores the declared disabled default";
    }
}

TEST_F(StudioCaptureGlTest, EffectMaskUsesTheMainPassFaceCulling)
{
    CaptureModel model("models/capture_cull.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    studio_test::g_Files["models/capture_cull.mat"] =
        R"({"version":1,"materials":{"red":{"program":"studio","params":{"uOccludedAmount":1,"uOccludedColor":[0,1,0,1],"uOutlineWidth":2,"uOutlineColor":[0,0,1,1]}}}})";
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kImageWidth, kImageHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(.25);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    render::StudioEffects_BeginView();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::StudioEffects_EndView();
    EXPECT_GT(ReadPixel(16)[1], 240);
    glDeleteRenderbuffers(1, &depth);
}

TEST_F(StudioCaptureGlTest, VisibleObliqueModelNeverReceivesItsOwnFillOrOutline)
{
    CaptureModel model("models/capture_oblique.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    GLint image = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 256, 256);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glViewport(17, 11, 220, 210);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearDepth(1);
    std::vector<uint8_t> before(256 * 256 * 4);
    std::vector<uint8_t> after(before.size());
    for (const char* program : {"studio", "weapon_finish"})
    {
        for (float glitch : {0.0f, 1.0f})
        {
            studio_test::g_Files["models/capture_oblique.mat"] =
                std::string(R"({"version":1,"materials":{"red":{"program":")") + program +
                R"(","params":{"uOccludedAmount":1,"uOccludedColor":[0,1,0,1],"uOutlineWidth":4,"uOutlineColor":[0,0,1,1],"uGlitchAmount":)" +
                std::to_string(glitch) + R"(,"uGlitchDisplacement":8,"uGlitchSpeed":8,"uGlitchSeed":3}}}})";
            render::Models_ReloadDocuments();
            for (int frame = 0; frame < 24; frame++)
            {
                SCOPED_TRACE(std::string(program) + " frame " + std::to_string(frame) + " glitch " + std::to_string(glitch));
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glFrustum(-4, 4, -4, 4, 4, 4096);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glRotatef(static_cast<float>(frame) * 1.31f, 1, 0, 0);
                glRotatef(static_cast<float>(frame) * .57f - 7, 0, 1, 0);
                glTranslatef(-1000.125f, -2000.25f, -725.75f);
                render::StudioEffects_BeginView();
                BeginModel(model, entity, 0);
                g_Capture.bones[0] = g_Capture.bones[5] = g_Capture.bones[10] = 256;
                g_Capture.bones[3] = 1000.25f;
                g_Capture.bones[7] = 2000.5f;
                g_Capture.bones[11] = 500.75f;
                cl->time = frame * .03125;
                DrawPart(0);
                EndModel();
                glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
                render::StudioEffects_EndView();
                glReadPixels(0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
                int changed = 0;
                int covered = 0;
                for (size_t index = 0; index < before.size(); index += 4)
                {
                    changed += !std::equal(before.begin() + index, before.begin() + index + 4, after.begin() + index);
                    covered += before[index] > 128;
                }
                EXPECT_GT(covered, 30);
                EXPECT_EQ(changed, 0);
            }
        }
    }
    glDeleteRenderbuffers(1, &depth);
}

TEST_F(StudioCaptureGlTest, ShaderResourcesIgnoreGameFilesAndDoNotExportProgramBinaries)
{
    studio_test::g_Files["shaders/studio.vert"] = "untrusted vertex";
    studio_test::g_Files["shaders/studio.json"] = "untrusted manifest";
    studio_test::g_Files["shaders/material_glitch.glsl"] = "untrusted include";
    studio_test::g_Files["shaders/untrusted.frag"] = "untrusted fragment";
    for (const char* name : {"studio.vert", "studio.json", "material_glitch.glsl"})
    {
        std::string source;
        ASSERT_TRUE(render::Shaders_LoadSource(name, source));
        EXPECT_EQ(source.find("untrusted"), std::string::npos) << name;
    }
    std::string source;
    EXPECT_FALSE(render::Shaders_LoadSource("untrusted.frag", source));
    EXPECT_EQ(render::FindCvar("r_shader_reload").name, nullptr);

    PFNGLGETPROGRAMBINARYPROC original = glad_glGetProgramBinary;
    static int binary_exports;
    binary_exports = 0;
    glad_glGetProgramBinary = [](GLuint, GLsizei, GLsizei* length, GLenum*, void*) {
        binary_exports++;
        *length = 0;
    };
    const render::ProgramDesc desc{"test_quad.vert", "test_quad.frag", "#define EMBEDDED_SOURCE_TEST 1", nullptr, 0, nullptr, 0};
    const ncl_gl::Program* program = render::Shaders_Get(desc);
    glad_glGetProgramBinary = original;
    ASSERT_NE(program, nullptr);
    EXPECT_EQ(binary_exports, 0);
}

TEST_F(StudioCaptureGlTest, SelectionUsesCapturedPoseDepthSkinAndMaskedAlpha)
{
    render::preview::PreviewInteraction interaction(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    GLuint vao = 0;
    GLuint vertex_buffer = 0;
    GLuint bone_buffer = 0;
    GLuint index_buffer = 0;
    GLuint texture = 0;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    const float positions[] = {-0.8f, -0.8f, 0, 0, 0.8f, 0, 0.8f, -0.8f, 0};
    glBufferData(GL_ARRAY_BUFFER, sizeof(positions), positions, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
    glGenBuffers(1, &bone_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, bone_buffer);
    const unsigned char bone_indices[9]{};
    glBufferData(GL_ARRAY_BUFFER, sizeof(bone_indices), bone_indices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(3);
    glVertexAttribIPointer(3, 3, GL_UNSIGNED_BYTE, 0, nullptr);
    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    const unsigned int indices[] = {0, 1, 2};
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char transparent[] = {255, 255, 255, 0};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, transparent);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    render::StudioDrawPacket packet{.bones = CapturePose()};
    packet.vao = vao;
    packet.blend = 1;
    packet.bone_count = 1;
    packet.bones = CapturePose({{0, 1.0f}, {5, 1.0f}, {10, 1.0f}}, packet.bones);
    packet.raster.depth_range[1] = 1;

    for (int index = 0; index < 4; index++)
    {
        packet.modelview[index * 5] = packet.projection[index * 5] = 1;
    }

    render::StudioQueuedMesh mesh{};
    mesh.index_count = 3;
    mesh.skin = 3;
    mesh.texture = texture;
    packet.meshes.push_back(mesh);
    PreviewModel model{};
    interaction.BeginModel(2, model);
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    packet.bones = CapturePose({{11, -0.3f}}, packet.bones);
    packet.meshes[0].skin = 7;
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    PreviewHit hit = interaction.Render({0, 0, 32, 32}, {0, 0, 32, 32}, 16, 16);
    EXPECT_EQ(hit.model_index, 2);
    EXPECT_EQ(hit.texture_index, 7);

    interaction.BeginFrame();
    packet.bones = CapturePose({{11, 0.0f}}, packet.bones);
    packet.meshes[0].skin = 3;
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    packet.bones = CapturePose({{11, -0.3f}}, packet.bones);
    packet.meshes[0].skin = 7;
    packet.meshes[0].masked = true;
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    hit = interaction.Render({0, 0, 32, 32}, {0, 0, 32, 32}, 16, 16);
    EXPECT_EQ(hit.texture_index, 3);
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
    EXPECT_EQ(hit.texture_index, 3);
    glBindTexture(GL_TEXTURE_2D, texture);
    const unsigned char opaque[] = {255, 255, 255, 255};
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, opaque);

    for (const char* program : {"studio", "weapon_finish"})
    {
        for (float alpha : {0.25f, 0.5f, 1.0f})
        {
            interaction.BeginFrame();
            packet.bones = CapturePose({{11, 0.0f}}, packet.bones);
            packet.meshes[0].skin = 3;
            packet.meshes[0].masked = false;
            packet.meshes[0].material.reset();
            PrepareCoveragePacket(render::GetRendererState().coverage, packet);
            interaction.Capture(packet);
            packet.bones = CapturePose({{11, -0.3f}}, packet.bones);
            packet.meshes[0].skin = 7;
            packet.meshes[0].masked = true;
            ncl_material::Material material;
            material.program = program;
            ncl_material::Param tint;
            tint.name = "uBaseTint";
            tint.value.type = ncl_material::ParamType::Vec4;
            tint.value.vec4 = {1, 1, 1, alpha};
            material.params.push_back(tint);
            packet.meshes[0].material = material;
            PrepareCoveragePacket(render::GetRendererState().coverage, packet);
            interaction.Capture(packet);
            hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
            EXPECT_EQ(hit.texture_index, alpha <= 0.5f ? 3 : 7);
        }
    }

    for (const char* program : {"studio", "weapon_finish"})
    {
        for (const char* name : {"uDissolve", "uNoise"})
        {
            for (float amount : {1.0f, 0.0f})
            {
                interaction.BeginFrame();
                packet.bones = CapturePose({{11, 0.0f}}, packet.bones);
                packet.meshes[0].skin = 3;
                packet.meshes[0].masked = false;
                packet.meshes[0].material.reset();
                PrepareCoveragePacket(render::GetRendererState().coverage, packet);
                interaction.Capture(packet);
                packet.bones = CapturePose({{11, -0.3f}}, packet.bones);
                packet.meshes[0].skin = 7;
                packet.meshes[0].masked = true;
                ncl_material::Material material;
                material.program = program;
                ncl_material::Param coverage;
                coverage.name = name;
                coverage.value.type = ncl_material::ParamType::Scalar;
                coverage.value.scalar = amount;
                material.params.push_back(coverage);
                packet.meshes[0].material = material;
                PrepareCoveragePacket(render::GetRendererState().coverage, packet);
                interaction.Capture(packet);
                hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
                EXPECT_EQ(hit.texture_index, amount >= 1.0f ? 3 : 7) << program << "/" << name;
            }
        }
    }

    interaction.BeginFrame();
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
    EXPECT_EQ(hit.model_index, -1);
    EXPECT_EQ(hit.texture_index, -1);
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    render::Device_OnContextDestroyed();
    render::Device_OnContextCreated();
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
    EXPECT_EQ(hit.model_index, -1);
    PrepareCoveragePacket(render::GetRendererState().coverage, packet);
    interaction.Capture(packet);
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
    EXPECT_EQ(hit.texture_index, 7);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 0, 0);
    EXPECT_EQ(hit.model_index, -1);
    GLint binding = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &binding);
    EXPECT_EQ(binding, 0);
    glBindVertexArray(vao);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &binding);
    ASSERT_EQ(static_cast<GLuint>(binding), index_buffer);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &binding);
    ASSERT_EQ(static_cast<GLuint>(binding), vertex_buffer);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &binding);
    ASSERT_EQ(binding, GL_TRUE);
    hit = interaction.Render({0, 0, 16, 16}, {0, 0, 16, 16}, 8, 8);
    EXPECT_EQ(hit.texture_index, 7);
    interaction.Clear();
    glDeleteTextures(1, &texture);
    glDeleteBuffers(1, &vertex_buffer);
    glDeleteBuffers(1, &bone_buffer);
    glDeleteBuffers(1, &index_buffer);
    glDeleteVertexArrays(1, &vao);

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, PreviewCaptureOwnsPacketsAndExcludesOtherEntitiesAndConfiguredDraws)
{
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    using Capture = render::preview::PreviewInteractionTestAccess;
    CaptureModel model("models/preview_packet.mdl", {1});
    goldsrc::cl_entity_t entity{};
    goldsrc::cl_entity_t other{};
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    params.time = 13.5;
    glViewport(3, 7, 24, 20);
    PreviewModel request{};
    request.scale = 2;

    {
        render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(model, entity, 0.25f);
        DrawPart(0);
        EndModel();
        render::GetRendererState().studio.CapturePreviewPacket(&other, render::StudioDrawPacket{});
    }

    ASSERT_EQ(Capture::Count(capture), 1);
    ASSERT_EQ(Capture::Packet(capture, 0).meshes.size(), 1);
    EXPECT_EQ(Capture::Packet(capture, 0).meshes[0].skin, 0);
    EXPECT_EQ(Capture::Packet(capture, 0).meshes[0].index_count, 3);
    EXPECT_FLOAT_EQ(Capture::Packet(capture, 0).bones[3], 0.5f);
    EXPECT_FLOAT_EQ(Capture::Packet(capture, 0).effect_time, 13.5f);
    EXPECT_EQ(Capture::Packet(capture, 0).viewport, (std::array<int, 4>{3, 7, 24, 20}));
    params.time = 99.0;
    glViewport(0, 0, 32, 32);
    EXPECT_FLOAT_EQ(Capture::Packet(capture, 0).effect_time, 13.5f);
    params.mode = PreviewRenderMode::Configured;

    {
        render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(model, entity, 1);
        DrawPart(0);
        EndModel();
    }

    EXPECT_EQ(Capture::Count(capture), 1);
    EXPECT_FLOAT_EQ(Capture::Packet(capture, 0).bones[3], 0.5f);
}

nitroapi::NitroApiInterface* StudioTest_Engine()
{
    static CaptureEngine engine;
    return &engine;
}

TEST_F(StudioCaptureGlTest, LocalMaterialCannotEnableNetworkOcclusionButServerParametersCan)
{
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    using Capture = render::preview::PreviewInteractionTestAccess;
    CaptureModel model("models/capture_authority.mdl", {1});
    studio_test::g_Files["models/capture_authority.mat"] =
        R"({"version":1,"materials":{"red":{"program":"studio","params":{"uOccludedAmount":1,"uOutlineWidth":16,"uOutlineMode":1}}}})";
    studio_test::g_Files["authority.mat"] =
        R"({"version":1,"materials":[{"name":"highlight","texture":"red","material":{"program":"studio","params":{"uOccludedAmount":1,"uOutlineWidth":16,"uOutlineMode":1}}}]})";
    goldsrc::cl_entity_t entity{};
    entity.index = 7;
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    goldsrc::client_static_t client{};
    client.state = goldsrc::ca_disconnected;
    cls = &client;
    render::Studio_BindEngine();
    auto draw = [&](bool enabled) {
        capture.BeginFrame();
        {
            render::StudioPreviewContext preview(render::GetRendererState().studio, &entity, params, request, &capture);
            BeginModel(model, entity, 0);
            DrawPart(0);
            EndModel();
        }
        ASSERT_EQ(Capture::Count(capture), 1u);
        ASSERT_TRUE(Capture::Packet(capture, 0).meshes.front().material.has_value());
        EXPECT_EQ(render::StudioEffects_IsEnabled(Capture::Packet(capture, 0)), enabled);
    };
    draw(true);
    render::ServerMaterials_SetBinding(7, "authority.mat", "highlight");
    client.state = goldsrc::ca_active;
    draw(false);
    const render::ServerMaterialBindingOwner owner{7, 10};
    render::ServerMaterials_SetBinding(owner, "authority.mat", "highlight");
    draw(false);
    ASSERT_TRUE(render::ServerMaterials_SetParam(owner, "uOccludedAmount", 0.5f));
    draw(true);
    ASSERT_TRUE(render::ServerMaterials_SetParam(owner, "uOccludedAmount", 0));
    ASSERT_TRUE(render::ServerMaterials_SetParam(owner, "uOutlineWidth", 2.0f));
    draw(true);
    render::ServerMaterials_ClearBindings(7);
    draw(false);
    client.state = goldsrc::ca_disconnected;
    draw(true);

    studio_test::g_Files["models/capture_authority.mat"] = R"({"version":1,"materials":{"red":{"program":"studio"}}})";
    studio_test::g_Files["authority_omitted.mat"] =
        R"({"version":1,"materials":[{"name":"highlight","texture":"red","material":{"program":"studio"}}]})";
    render::Models_ReloadDocuments();
    draw(false);
    client.state = goldsrc::ca_active;
    draw(false);
    render::ServerMaterials_SetBinding(owner, "authority_omitted.mat", "highlight");
    draw(false);
    const ncl_material::Material* binding = render::ServerMaterials_BindingFor({7, "red"});
    ASSERT_NE(binding, nullptr);
    ASSERT_NE(binding->FindParam("uOccludedAmount"), nullptr);
    ASSERT_NE(binding->FindParam("uOutlineWidth"), nullptr);
    EXPECT_FLOAT_EQ(binding->FindParam("uOccludedAmount")->scalar, 0);
    EXPECT_FLOAT_EQ(binding->FindParam("uOutlineWidth")->scalar, 0);
    ASSERT_TRUE(render::ServerMaterials_SetParam(owner, "uOutlineWidth", 2.0f));
    draw(true);
}

TEST_F(StudioCaptureGlTest, RotatingSubdividedModelKeepsExactDepthWithCroppedEffectMasks)
{
    CaptureModel model("models/capture_subdivided.mdl", {1});
    model.SetSubdividedBoxGeometry();
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    GLint image = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1280, 720, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 1280, 720);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
    glViewport(17, 11, 1200, 650);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    std::vector<uint8_t> before(1280 * 720 * 4);
    std::vector<uint8_t> after(before.size());
    for (const char* program : {"studio", "weapon_finish"})
    {
        studio_test::g_Files["models/capture_subdivided.mat"] =
            std::string(R"({"version":1,"materials":{"red":{"program":")") + program +
            R"(","params":{"uOccludedAmount":1,"uOccludedColor":[0,1,1,1],"uOutlineWidth":4.75,"uGlowRadius":23.75,"uGlowStrength":0.65625}}}})";
        render::Models_ReloadDocuments();
        for (bool reversed : {false, true})
        {
            glDepthRange(reversed ? 1 : 0, reversed ? 0 : 1);
            glDepthFunc(reversed ? GL_GEQUAL : GL_LEQUAL);
            glClearDepth(reversed ? 0 : 1);
            for (int frame = 0; frame < 24; frame++)
            {
                SCOPED_TRACE(std::string(program) + " frame " + std::to_string(frame) + " reversed " + std::to_string(reversed));
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                glMatrixMode(GL_PROJECTION);
                glLoadIdentity();
                glFrustum(-4, 4, -2.2, 2.2, 4, 4096);
                glMatrixMode(GL_MODELVIEW);
                glLoadIdentity();
                glTranslatef(0, 0, -110);
                glRotatef(-65, 1, 0, 0);
                glRotatef(static_cast<float>(frame) * 15, 0, 0, 1);
                glTranslatef(0, 0, -13);
                render::StudioEffects_BeginView();
                BeginModel(model, entity, -.002287f);
                g_Capture.bones[7] = -.111727f;
                DrawPart(0);
                EndModel();
                glReadPixels(0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
                render::StudioEffects_EndView();
                glReadPixels(0, 0, 1280, 720, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
                int covered = 0;
                int changed = 0;
                for (size_t index = 0; index < before.size(); index += 4)
                {
                    covered += before[index] > 128;
                    changed += !std::equal(before.begin() + index, before.begin() + index + 4, after.begin() + index);
                }
                EXPECT_GT(covered, 100);
                EXPECT_EQ(changed, 0);
            }
        }
    }
    glDeleteRenderbuffers(1, &depth);
}

#ifdef _WIN32
TEST_F(StudioCaptureGlTest, AuthoringTargetAvoidsWindowDepthInterpolationArtifacts)
{
    HWND window = WindowFromDC(wglGetCurrentDC());
    ASSERT_TRUE(SetWindowPos(window, nullptr, 0, 0, 2560, 1440, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER));
    CaptureModel model("models/capture_window_depth.mdl", {1});
    model.SetTriangle({-8.75f, -1.96f, 25.17f, -8.75f, 1.81f, 25.17f, -3.67f, -4.72f, 26.73f});
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    const PreviewRect view{0, 169, 2100, 1246};
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    render::ScopedGlState saved;
    std::vector<uint8_t> before(view.wide * view.tall * 4);
    std::vector<uint8_t> after(before.size());
    std::vector<uint8_t> presented(before.size());
    for (const char* program : {"studio", "weapon_finish"})
    {
        SCOPED_TRACE(program);
        studio_test::g_Files["models/capture_window_depth.mat"] =
            std::string(R"({"version":1,"materials":{"red":{"program":")") + program +
            R"(","params":{"uOccludedAmount":1,"uOccludedColor":[0,1,1,1],"uOutlineWidth":4.75,"uGlowRadius":23.75,"uGlowStrength":0.65625}}}})";
        render::Models_ReloadDocuments();
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glDrawBuffer(GL_BACK);
        glViewport(view.x, view.y, view.wide, view.tall);
        ASSERT_TRUE(target.Begin(view, false));
        GLint output = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &output);
        EXPECT_NE(output, 0);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glDepthRange(0, 1);
        glClearDepth(1);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(-2.3094, 2.3094, -1.370244, 1.370244, 4, 4096);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(0, 0, -75);
        glRotatef(-65, 1, 0, 0);
        glRotatef(233.75f, 0, 0, 1);
        glTranslatef(0, 0, -13);
        render::StudioEffects_BeginView();
        BeginModel(model, entity, -.002287f);
        g_Capture.bones[7] = -.111727f;
        DrawPart(0);
        EndModel();
        glReadPixels(0, 0, view.wide, view.tall, GL_RGBA, GL_UNSIGNED_BYTE, before.data());
        render::StudioEffects_EndView();
        glReadPixels(0, 0, view.wide, view.tall, GL_RGBA, GL_UNSIGNED_BYTE, after.data());
        int changed = 0;
        int covered = 0;
        for (size_t index = 0; index < before.size(); index += 4)
        {
            covered += before[index] > 128;
            changed += !std::equal(before.begin() + index, before.begin() + index + 4, after.begin() + index);
        }
        EXPECT_GT(covered, 0);
        EXPECT_EQ(changed, 0);
        target.Present(view, view);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glReadPixels(view.x, view.y, view.wide, view.tall, GL_RGBA, GL_UNSIGNED_BYTE, presented.data());
        EXPECT_EQ(after, presented);
    }
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}
#endif

TEST_F(StudioCaptureGlTest, OutlineModesExposeVisibleOnlyChoice)
{
    render::MaterialEditInterface* editor = render::MaterialEdit_Get();
    ASSERT_NE(editor, nullptr);
    for (const char* program : {"studio", "weapon_finish"})
    {
        EXPECT_EQ(editor->GetChoiceCount(program, "uOutlineMode"), 3);
        EXPECT_STREQ(editor->GetChoiceLabel(program, "uOutlineMode", 2), "#ncl_mat_outline_mode_visible");
    }
}

TEST_F(StudioCaptureGlTest, MultisampleModelsPreserveVisibilityInAllOutlineModes)
{
    CheckMultisampleVisibility(false);
}

#ifdef _WIN32
TEST_F(StudioCaptureWindowGlTest, MultisampleWindowPreservesVisibilityInAllOutlineModes)
{
    ASSERT_TRUE(SetWindowPos(WindowFromDC(wglGetCurrentDC()), nullptr, 0, 0, 800, 500, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER));
    CheckMultisampleVisibility(true);
}
#endif

void StudioCaptureGlTest::CheckMultisampleVisibility(bool window)
{
    constexpr int wide = 640;
    constexpr int tall = 360;
    render::ScopedGlState saved;
    GLint resolve = 0;
    GLint image = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &resolve);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &image);
    glBindTexture(GL_TEXTURE_2D, image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, wide, tall, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    GLuint output = 0;
    GLuint images[2]{};
    if (!window)
    {
        glGenFramebuffers(1, &output);
        glGenRenderbuffers(2, images);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, output);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glDepthRange(0, 1);
    glClearDepth(1);
    CaptureModel model("models/capture_multisample.mdl", {1});
    model.SetSubdividedBoxGeometry();
    goldsrc::cl_entity_t entity{};
    entity.index = 1;
    render::g_RenderCfg.studio_materials = nullptr;
    std::vector<uint8_t> before(wide * tall * 4);
    std::vector<uint8_t> after(before.size());
    GLint render_output = output;
    const auto read = [&](std::vector<uint8_t>& pixels) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, render_output);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolve);
        glDisable(GL_SCISSOR_TEST);
        glBlitFramebuffer(0, 0, wide, tall, 0, 0, wide, tall, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, resolve);
        glReadPixels(0, 0, wide, tall, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindFramebuffer(GL_FRAMEBUFFER, render_output);
    };
    render::preview::PreviewSceneTarget target(render::GetRendererState().resources.device);
    const PreviewRect view{0, 0, wide, tall};
    GLint max_samples = 0;
    glGetIntegerv(GL_MAX_DEPTH_TEXTURE_SAMPLES, &max_samples);
    for (int samples : {4, 8, 0, 4})
    {
        SCOPED_TRACE(samples);
        if (window && samples != 4)
        {
            continue;
        }
        if (samples > max_samples)
        {
            continue;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, output);
        for (int index = 0; !window && index < 2; index++)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, images[index]);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, index ? GL_DEPTH_COMPONENT24 : GL_RGBA8, wide, tall);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, index ? GL_DEPTH_ATTACHMENT : GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, images[index]);
        }
        ASSERT_EQ(glCheckFramebufferStatus(GL_FRAMEBUFFER), GLenum(GL_FRAMEBUFFER_COMPLETE));
        const std::pair<const char*, int> cases[] = {
            {"studio", 0}, {"studio", 1}, {"studio", 2}, {"weapon_finish", 0}, {"weapon_finish", 1}, {"weapon_finish", 2}
        };
        for (const std::pair<const char*, int>& item : cases)
        {
            const char* program = item.first;
            const int outline_mode = item.second;
            SCOPED_TRACE(program);
            SCOPED_TRACE(outline_mode);
            studio_test::g_Files["models/capture_multisample.mat"] =
                std::string(R"({"version":1,"materials":{"red":{"program":")") + program + R"(","params":{"uOutlineMode":)" +
                std::to_string(outline_mode) +
                R"(,"uOccludedAmount":1,"uOccludedColor":[0,1,1,1],"uOutlineColor":[1,0.5,0.1,1],"uOutlineWidth":4.75,"uGlowRadius":23.75,"uGlowStrength":0.65625}}}})";
            render::Models_ReloadDocuments();
            for (bool reversed : {false, true})
            {
                SCOPED_TRACE(reversed);
                for (int frame = 0; frame < 12; frame++)
                {
                    SCOPED_TRACE(frame);
                    const bool authoring = frame >= 6;
                    const bool wall = frame % 3 == 2;
                    glViewport(0, 0, wide, tall);
                    glDepthRange(reversed ? 1 : 0, reversed ? 0 : 1);
                    glDepthFunc(reversed ? GL_GEQUAL : GL_LEQUAL);
                    if (authoring)
                    {
                        ASSERT_TRUE(target.Begin(view, false));
                    }
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &render_output);
                    glDisable(GL_SCISSOR_TEST);
                    glClearColor(0, 0, 0, 1);
                    glClearDepth(reversed ? 0 : 1);
                    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glFrustum(-4, 4, -2.2, 2.2, 4, 4096);
                    glMatrixMode(GL_MODELVIEW);
                    glLoadIdentity();
                    glTranslatef(0, 0, -75);
                    glRotatef(-65, 1, 0, 0);
                    glRotatef(static_cast<float>(frame) * 30, 0, 0, 1);
                    glTranslatef(0, 0, -13);
                    render::StudioEffects_BeginView();
                    BeginModel(model, entity, 0);
                    DrawPart(0);
                    EndModel();
                    if (wall)
                    {
                        glEnable(GL_SCISSOR_TEST);
                        glScissor(0, 0, wide / 2, tall);
                        glClearDepth(reversed ? .99 : .01);
                        glClearColor(.1f, .1f, .1f, 1);
                        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
                        glDisable(GL_SCISSOR_TEST);
                    }
                    read(before);
                    render::StudioEffects_EndView();
                    EXPECT_EQ(render::StudioEffects_LastStats().samples, samples);
                    read(after);
                    int covered = 0;
                    int changed = 0;
                    int visible_changed = 0;
                    int outline = 0;
                    uint8_t surface_red = 0;
                    for (size_t index = 0; index < before.size(); index += 4)
                    {
                        if (before[index + 1] == 0 && before[index + 2] == 0)
                        {
                            surface_red = std::max(surface_red, before[index]);
                        }
                    }
                    int solid_covered = 0;
                    int solid_changed = 0;
                    for (size_t index = 0; index < before.size(); index += 4)
                    {
                        const bool different = !std::equal(before.begin() + index, before.begin() + index + 4, after.begin() + index);
                        const bool visible = before[index] > 128 && before[index + 1] < 64 && before[index + 2] < 64;
                        covered += visible;
                        changed += different;
                        visible_changed += visible && different;
                        const bool solid = before[index] == surface_red && before[index + 1] == 0 && before[index + 2] == 0;
                        solid_covered += solid;
                        solid_changed += solid && different;
                        outline += before[index] < 128 && before[index + 1] < 32 && after[index] > 128 && after[index + 1] > 32;
                    }
                    EXPECT_GT(covered, 100);
                    EXPECT_GT(solid_covered, 100);
                    EXPECT_EQ(solid_changed, 0);
                    if (outline_mode == 0)
                    {
                        EXPECT_EQ(visible_changed, 0);
                    }
                    if (wall || outline_mode != 0)
                    {
                        EXPECT_GT(changed, 0);
                        EXPECT_GT(outline, 0);
                    }
                    else
                    {
                        EXPECT_EQ(changed, 0);
                    }
                    if (authoring)
                    {
                        target.Present(view, view);
                    }
                }
            }
        }
    }
    glDeleteFramebuffers(1, &output);
    glDeleteRenderbuffers(2, images);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, QueuedPacketsKeepIndependentPosesWithoutFog)
{
    CaptureModel model("models/capture_queued_poses.mdl", {2});
    goldsrc::cl_entity_t entities[3]{};
    const float poses[] = {-0.55f, 0, 0.55f};
    glDisable(GL_FOG);
    render::Studio_BeginOpaquePass();

    for (size_t index = 0; index < std::size(poses); ++index)
    {
        BeginModel(model, entities[index], poses[index]);
        DrawPart(0);
        EndModel();
    }

    ASSERT_TRUE(g_Capture.draws.empty());
    render::Studio_EndOpaquePass();
    ASSERT_EQ(g_Capture.draws.size(), std::size(poses));
    const int columns[] = {7, 16, 25};

    for (size_t index = 0; index < std::size(poses); ++index)
    {
        EXPECT_EQ(g_Capture.draws[index].program, g_Capture.draws[0].program);
        EXPECT_FLOAT_EQ(g_Capture.draws[index].bone_x, poses[index]);
        EXPECT_EQ(ReadPixel(columns[index]), (std::array<uint8_t, 4>{255, 0, 0, 255}));
    }

    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, LifecycleLiveReleaseRetiresQueuedGeometryAndRecreatesItsOwners)
{
    GeometryTrace trace;
    CaptureModel model("models/capture_lifecycle_live.mdl", {1});
    goldsrc::cl_entity_t entity{};
    entity.index = 12;
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    ASSERT_TRUE(g_Capture.draws.empty());
    ASSERT_EQ(g_GeometryUploads, 2);
    ASSERT_EQ(g_GeometryCreations, 2);
    const render::StudioGpuModel* geometry = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(geometry, nullptr);
    GLuint retired_vao = geometry->vao;
    ASSERT_NE(retired_vao, 0u);
    ASSERT_TRUE(glIsVertexArray(retired_vao));

    render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
    lifecycle.ReleaseLiveContext();
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(render::Models_ModelCount(), 0);
    EXPECT_EQ(std::count(g_GeometryDeletedArrays.begin(), g_GeometryDeletedArrays.end(), retired_vao), 1);
    EXPECT_FALSE(glIsVertexArray(retired_vao));
    EXPECT_FALSE(render::Device_IsContextAlive());
    lifecycle.ReleaseLiveContext();
    EXPECT_EQ(std::count(g_GeometryDeletedArrays.begin(), g_GeometryDeletedArrays.end(), retired_vao), 1);

    lifecycle.OnContextCreated();
    ASSERT_TRUE(render::Device_IsContextCurrent());
    trace.Attach();
    glad_glMultiDrawElements = ObserveCaptureDraw;
    glad_glBufferSubData = ObserveCapturedBoneUpload;
    render::Studio_EndOpaquePass();
    ASSERT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_EQ(g_GeometryCreations, 2);
    render::Studio_BeginOpaquePass();
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    render::Studio_EndOpaquePass();
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(render::Models_ModelCount(), 1);
    EXPECT_EQ(g_GeometryUploads, 4);
    EXPECT_EQ(g_GeometryCreations, 4);
    const render::StudioGpuModel* recreated = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(recreated, nullptr);
    EXPECT_TRUE(glIsVertexArray(recreated->vao));
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, LivePendingBuilderReusesMaterialCapacitiesAndClearsAbsentMaterials)
{
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/live_pending_reuse.mdl", {1, 0});
    ncl_material::Material material;
    material.program = "weapon_finish";
    ncl_material::Param pattern;
    pattern.name = "uPattern";
    pattern.value.type = ncl_material::ParamType::TextureRef;
    pattern.value.texture_ref = "fixtures/" + std::string(100, 'p') + ".png";
    material.params.push_back(pattern);
    ncl_material::Param wear = pattern;
    wear.name = "uWearMap";
    wear.value.texture_ref = "fixtures/" + std::string(100, 'w') + ".png";
    material.params.push_back(wear);
    ncl_material::OverlaySlot overlay;
    overlay.name.assign(80, 'o');
    material.overlays.push_back(overlay);
    ncl_material::MaterialDoc document;
    document.entries.push_back({"red", material});
    studio_test::g_Files["models/live_pending_reuse.mat"] = ncl_material::WriteMaterialDoc(document);
    goldsrc::cl_entity_t entity{};
    render::StudioRenderer& renderer = render::GetRendererState().studio;
    using Access = render::StudioRendererTestAccess;
    BeginModel(model, entity, 0);
    DrawPart(0);
    ASSERT_EQ(Access::PendingCount(renderer), 1u);
    const auto& first = Access::PendingMeshes(renderer)[0].material;
    ASSERT_TRUE(first);
    ASSERT_FALSE(first->overlays.empty());
    const ncl_material::Param* first_params = first->params.data();
    const char* first_texture = first->FindParam("uPattern")->texture_ref.data();
    const char* first_overlay = first->overlays[0].name.data();
    EndModel();
    BeginModel(model, entity, 0);
    DrawPart(0);
    ASSERT_EQ(Access::PendingCount(renderer), 1u);
    const auto& second = Access::PendingMeshes(renderer)[0].material;
    ASSERT_TRUE(second);
    EXPECT_EQ(second->params.data(), first_params);
    EXPECT_EQ(second->FindParam("uPattern")->texture_ref.data(), first_texture);
    EXPECT_EQ(second->overlays[0].name.data(), first_overlay);
    EXPECT_NE(Access::PendingMeshes(renderer)[0].program, nullptr);
    EXPECT_NE(Access::PendingMeshes(renderer)[0].material_bindings, nullptr);
    EndModel();
    goldsrc::cvar_t disabled{};
    render::g_RenderCfg.studio_materials = &disabled;
    BeginModel(model, entity, 0);
    DrawPart(0);
    ASSERT_EQ(Access::PendingCount(renderer), 1u);
    EXPECT_FALSE(Access::PendingMeshes(renderer)[0].material);
    EndModel();
    EXPECT_EQ(g_Capture.stock_calls, 0) << render::Studio_LastRejection();
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, ReusedTailCannotPublishMeshesOrSupplyCapturedSkin)
{
    CaptureModel warm("models/pending_tail_warm.mdl", {1, 1});
    CaptureModel fewer("models/pending_tail_fewer.mdl", {1});
    goldsrc::cl_entity_t entity{};
    BeginModel(warm, entity, 0);
    DrawPart(0);
    entity.curstate.skin = 1;
    DrawPart(1);
    EndModel();
    ASSERT_EQ(g_Capture.skin_calls, 2);
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    {
        render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(fewer, entity, 0);
        entity.curstate.skin = 1;
        DrawPart(0);
        EndModel();
    }
    using Access = render::preview::PreviewInteractionTestAccess;
    ASSERT_EQ(Access::Count(capture), 1u);
    ASSERT_EQ(Access::Packet(capture, 0).meshes.size(), 1u);
    EXPECT_EQ(Access::Packet(capture, 0).meshes[0].skin, 1);
    EXPECT_EQ(Access::Packet(capture, 0).meshes[0].texture, g_Capture.textures[1]);
    EXPECT_EQ(g_Capture.skin_calls, 3);
    EXPECT_EQ(g_Capture.stock_calls, 0);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, CompanionLoadingInvalidationRejectsTheCapturedEpoch)
{
    CaptureModel model("models/invalidated_companion.mdl", {1});
    model.get_header()->textureindex = 0;
    model.get_header()->numtextures = 0;
    goldsrc::cl_entity_t entity{};
    CompanionLoader companion(get_engine(), "models/invalidated_companionT.mdl", false);
    companion.before_allocate = [&] { render::GetRendererState().intercept.ForgetModel(); };
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(g_Capture.stock_calls, 1);
    EXPECT_STREQ(render::Studio_LastRejection(), "studio model was invalidated during texture preparation");
}

TEST_F(StudioCaptureGlTest, MaterialDocumentLoadingInvalidationRejectsTheCapturedEpoch)
{
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/invalidated_metadata.mdl", {1});
    studio_test::g_Files["models/invalidated_metadata.mat"] = R"({"version":1,"materials":{"red":{"program":"studio"}}})";
    studio_test::FileReadProbe probe;
    probe.path = "models/invalidated_metadata.mat";
    bool invalidated = false;
    probe.after_read = [&] {
        if (!invalidated)
        {
            invalidated = true;
            render::GetRendererState().intercept.ForgetModel();
        }
    };
    studio_test::g_FileReadProbe = &probe;
    goldsrc::cl_entity_t entity{};
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    studio_test::g_FileReadProbe = nullptr;
    EXPECT_TRUE(invalidated);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(g_Capture.stock_calls, 1);
    EXPECT_STREQ(render::Studio_LastRejection(), "studio model was invalidated during material preparation");
}

TEST_F(StudioCaptureGlTest, ImagePreparationCancellationKeepsPayloadOwnedAndPublishesNothing)
{
    const uint64_t packets_before = render::Studio_QueueStats().packets;
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/cancel_image.mdl", {1});
    studio_test::g_Files["models/cancel_image.mat"] =
        R"({"version":1,"materials":{"red":{"program":"weapon_finish","params":{"uPattern":{"texture":"fixtures/runtime-color.png"}}}}})";
    goldsrc::cl_entity_t entity{};
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    bool entered = false;
    studio_test::g_MaterialImageLoadCallback = [&](tex::TextureLoadGuard guard) {
        render::StudioRenderer& renderer = render::GetRendererState().studio;
        const bool preparation = !render::StudioRendererTestAccess::IsPending(renderer) &&
                                 render::StudioRendererTestAccess::QueuedCount(renderer) == 0 &&
                                 render::preview::PreviewInteractionTestAccess::Count(capture) == 0;
        EXPECT_TRUE(preparation);
        if (!preparation)
        {
            return;
        }
        entered = true;
        const float* completed_pose = render::StudioRendererTestAccess::get_cached_pose(renderer).data();
        ASSERT_NE(completed_pose, nullptr);
        const std::array<float, render::kStudioBoneFloats> expected = g_Capture.bones;
        render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
        lifecycle.OnClientStateClearing();
        EXPECT_FALSE(guard.is_epoch_current());
        EXPECT_EQ(render::StudioRendererTestAccess::get_cached_pose(renderer).data(), nullptr);
        EXPECT_TRUE(std::equal(expected.begin(), expected.end(), completed_pose));
    };
    {
        render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(model, entity, 0);
        DrawPart(0);
        EndModel();
        EXPECT_TRUE(context.is_rejected());
    }
    studio_test::g_MaterialImageLoadCallback = {};
    EXPECT_TRUE(entered);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(render::preview::PreviewInteractionTestAccess::Count(capture), 0u);
    EXPECT_EQ(render::Studio_QueueStats().packets, packets_before);
    EXPECT_TRUE(render::Device_IsContextCurrent());
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

namespace
{
    decltype(glad_glPushAttrib) g_PreparationPushAttrib{};
    unsigned int g_PreparationPushes{};
    void APIENTRY ObservePreparationPush(GLbitfield mask)
    {
        ++g_PreparationPushes;
        g_PreparationPushAttrib(mask);
    }
} // namespace

TEST_F(StudioCaptureGlTest, WarmImagePreparationAddsNoBackendLoadOrGlSnapshot)
{
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/warm_images.mdl", {1});
    studio_test::g_Files["models/warm_images.mat"] =
        R"({"version":1,"materials":{"red":{"program":"weapon_finish","params":{"uPattern":{"texture":"fixtures/runtime-color.png"}}}}})";
    goldsrc::cl_entity_t entity{};
    unsigned int loads = 0;
    studio_test::g_MaterialImageLoadCallback = [&](tex::TextureLoadGuard) { ++loads; };
    g_PreparationPushAttrib = glad_glPushAttrib;
    glad_glPushAttrib = ObservePreparationPush;
    g_PreparationPushes = 0;
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(g_PreparationPushes, 2u);
    g_PreparationPushes = 0;
    BeginModel(model, entity, 0);
    DrawPart(0);
    EndModel();
    glad_glPushAttrib = g_PreparationPushAttrib;
    studio_test::g_MaterialImageLoadCallback = {};
    EXPECT_EQ(loads, 1u);
    EXPECT_EQ(g_PreparationPushes, 1u);
    EXPECT_EQ(g_Capture.stock_calls, 0);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, MapResetFromImagePreparationPreservesExternalBindingsAndRetiresOnlyOwnedNames)
{
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/map_reset_images.mdl", {1, 0});
    studio_test::g_Files["models/map_reset_images.mat"] =
        R"({"version":1,"materials":{"red":{"program":"weapon_finish","params":{"uPattern":{"texture":"fixtures/runtime-color.png"}}}}})";
    const GLuint retired_image = render::MaterialTexture_Neutral();
    ASSERT_NE(retired_image, 0u);
    GLuint external_vao = 0;
    GLuint external_buffer = 0;
    GLuint external_image = 0;
    glGenVertexArrays(1, &external_vao);
    glBindVertexArray(external_vao);
    glGenBuffers(1, &external_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, external_buffer);
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_STATIC_DRAW);
    glGenTextures(1, &external_image);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, external_image);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
    goldsrc::cl_entity_t entity{};
    BeginModel(model, entity, 0);
    DrawPart(0);
    const render::StudioGpuModel* geometry = render::Models_PrepareGeometry(&model.model, model.get_header());
    ASSERT_NE(geometry, nullptr);
    const GLuint retired_vao = geometry->vao;
    ASSERT_NE(retired_vao, external_vao);
    bool entered = false;
    studio_test::g_MaterialImageLoadCallback = [&](tex::TextureLoadGuard guard) {
        render::StudioRenderer& renderer = render::GetRendererState().studio;
        const bool phase =
            !render::StudioRendererTestAccess::IsPending(renderer) && render::StudioRendererTestAccess::QueuedCount(renderer) == 0;
        EXPECT_TRUE(phase);
        if (!phase)
        {
            return;
        }
        entered = true;
        GLint binding = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &binding);
        EXPECT_EQ(binding, GLint(external_vao));
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
        EXPECT_EQ(binding, GLint(external_buffer));
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
        EXPECT_EQ(binding, GLint(g_Capture.textures[0]));
        render::RendererLifecycle lifecycle = preview_test::MakeLifecycle(render::GetRendererState());
        lifecycle.OnMapReset();
        EXPECT_FALSE(guard.is_epoch_current());
        EXPECT_TRUE(render::Device_IsContextCurrent());
        EXPECT_FALSE(glIsVertexArray(retired_vao));
        EXPECT_FALSE(glIsTexture(retired_image));
    };
    EndModel();
    studio_test::g_MaterialImageLoadCallback = {};
    EXPECT_TRUE(entered);
    EXPECT_TRUE(g_Capture.draws.empty());
    GLint binding = 0;
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &binding);
    EXPECT_EQ(binding, GLint(external_vao));
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &binding);
    EXPECT_EQ(binding, GLint(external_buffer));
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    EXPECT_EQ(binding, GLint(g_Capture.textures[0]));
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &binding);
    EXPECT_EQ(binding, GLint(external_image));
    glActiveTexture(GL_TEXTURE0);
    EXPECT_FALSE(glIsVertexArray(retired_vao));
    EXPECT_FALSE(glIsTexture(retired_image));
    EXPECT_TRUE(glIsVertexArray(external_vao));
    EXPECT_TRUE(glIsBuffer(external_buffer));
    EXPECT_TRUE(glIsTexture(external_image));
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteVertexArrays(1, &external_vao);
    glDeleteBuffers(1, &external_buffer);
    glDeleteTextures(1, &external_image);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidVariantConsumesInstanceMatrixAndRangeWithoutBonesBlock)
{
    std::string defines = std::string(render::kStudioShadeDefines) + "\n#define STUDIO_RIGID_INSTANCING 1\n";
    const render::ProgramDesc desc{
        "studio.vert",
        "studio_legacy.frag",
        defines.c_str(),
        render::kStudioUniformNames,
        render::kUniformCount,
        nullptr,
        0
    };
    const ncl_gl::Program* program = render::Shaders_Get(desc);

    ASSERT_NE(program, nullptr);
    EXPECT_EQ(glGetAttribLocation(program->prog, "aInstanceRow0"), 5);
    EXPECT_EQ(glGetAttribLocation(program->prog, "aInstanceRow1"), 6);
    EXPECT_EQ(glGetAttribLocation(program->prog, "aInstanceRow2"), 7);
    EXPECT_EQ(glGetAttribLocation(program->prog, "aInstanceRange"), 8);
    EXPECT_EQ(glGetUniformBlockIndex(program->prog, "Bones"), GL_INVALID_INDEX);
}

TEST_F(StudioCaptureGlTest, RigidUnionKeepsBodyAndPosePixelsWithOneDrawAndUpload)
{
    GeometryTrace geometry_trace;
    const int built_before = render::GetRendererState().resources.shaders.get_built_from_source();

    CaptureModel model("rigid_variants", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2};

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);

    ASSERT_EQ(g_Capture.draws.size(), 4u);
    EXPECT_EQ(g_GeometryCreations, 2);
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_EQ(render::GetRendererState().resources.shaders.get_built_from_source() - built_before, 2);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_EQ(g_Capture.bone_uploads, (std::vector<GLsizeiptr>{4 * 64}));
    EXPECT_EQ(g_GeometryCreations, 2);
    EXPECT_EQ(g_GeometryUploads, 2);
    EXPECT_EQ(render::GetRendererState().resources.shaders.get_built_from_source() - built_before, 2);
    EXPECT_GT(std::count(ordinary.begin(), ordinary.end(), uint8_t{255}), kImageWidth * kImageHeight);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidUnionCombinesAdjacentPacketsWithSameAndDifferentBodyRanges)
{
    CaptureModel model("rigid_same_range", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 2, 0, 0, 1, 2};

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(stats.physical_draws, 1u);
    EXPECT_EQ(stats.logical_groups, 6u);
    EXPECT_EQ(stats.logical_triangles, 6u);
    EXPECT_EQ(stats.submitted_triangles, 18u);
    EXPECT_EQ(stats.saved_calls, 5u);
    EXPECT_EQ(stats.instance_uploads, 1u);
    EXPECT_EQ(stats.instance_bytes, 6u * 64);
}

TEST_F(StudioCaptureGlTest, RigidUnionPreservesSharedFogAndSplitsAtCapturedStateChanges)
{
    CaptureModel model("rigid_shared_state", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2, 0};

    const auto prepare = [](size_t index) {
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);

        const float fog_color[] = {0.2f, 0.4f, 0.6f, 1.0f};
        glFogfv(GL_FOG_COLOR, fog_color);
        glFogf(GL_FOG_START, index == 2 ? -0.5f : -1.0f);
        glFogf(GL_FOG_END, 1.0f);
    };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare));

    const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(g_RigidDraws, 2);
    EXPECT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(stats.physical_draws, 3u);
    EXPECT_EQ(stats.logical_triangles, 5u);
    EXPECT_EQ(stats.submitted_triangles, 13u);
    EXPECT_EQ(stats.instance_uploads, 1u);
    EXPECT_EQ(stats.instance_bytes, 5u * 64);

    glDisable(GL_FOG);
}

TEST_F(StudioCaptureGlTest, RigidUnionHandlesCapacityMultipleFlushesRestartAndGeometryRecreation)
{
    CaptureModel model("rigid_capacity", {1, 1, 1});
    model.SetBodyVariants();

    float blend = 1.0f;
    float* previous_blend = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous_blend; });

    const auto prepare = [&](size_t index) { blend = 0.1f + static_cast<float>(index % 9) * 0.1f; };

    for (size_t count : {size_t{127}, size_t{128}, size_t{130}})
    {
        std::vector<int> bodies(count);

        for (size_t index = 0; index < count; ++index)
        {
            bodies[index] = static_cast<int>(index % 3);
        }

        const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare);

        EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare));

        const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

        EXPECT_EQ(stats.physical_draws, count > 128 ? 2u : 1u);
        EXPECT_EQ(stats.instance_uploads, count > 128 ? 2u : 1u);
        EXPECT_EQ(stats.instance_bytes, count * 64);
        EXPECT_EQ(stats.logical_triangles, count);
        EXPECT_EQ(stats.submitted_triangles, count * 3);
    }

    const std::vector<int> bodies{2, 0, 1, 2};
    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(0xffffffff);

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 0);
    EXPECT_EQ(render::GetRendererState().studio.get_submission_stats().instance_uploads, 0u);

    glDisable(GL_PRIMITIVE_RESTART);

    render::Studio_DiscardPending();
    render::GetRendererState().models.ReleaseGeometry();

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidUnionPreservesObservableTransparentOrderAndLitGammaPixels)
{
    CaptureModel model("rigid_alpha_order", {1, 1, 1});
    model.SetBodyVariants();

    goldsrc::mstudiotexture_t* textures =
        reinterpret_cast<goldsrc::mstudiotexture_t*>(reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex);
    textures[0].flags = 0;

    float blend = 0.45f;
    float* previous_blend = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous_blend; });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    std::vector<int> bodies{2, 0, 1, 2};

    const auto prepare = [&](size_t index) {
        SetLighting(32, 192);

        const float angle = 0.9f + bodies[index] * 0.25f;
        g_Capture.bones[0] = std::cos(angle);
        g_Capture.bones[2] = std::sin(angle);
        g_Capture.bones[8] = -std::sin(angle);
        g_Capture.bones[10] = std::cos(angle);
        g_Capture.bones[3] = 0;
    };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare, true);

    EXPECT_GT(ordinary[(kImageWidth * (kImageHeight / 2) + kImageWidth / 2) * 4], 0);
    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare, true));

    EXPECT_EQ(g_RigidDraws, 1);

    std::reverse(bodies.begin(), bodies.end());
    EXPECT_NE(ordinary, DrawOrdinarySequence(model, bodies, prepare, true));

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

TEST_F(StudioCaptureGlTest, RigidUnionRestoresCallerProgramVaoDivisorsBuffersAndUniformRange)
{
    CaptureModel model("rigid_caller_state", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2};

    DrawRigidSequence(model, bodies);

    const ncl_gl::Program* caller_program = render::Shaders_Get(render::kStudioBuiltinProgram);

    ASSERT_NE(caller_program, nullptr);

    GLuint caller_vao = 0;
    GLuint buffers[3]{};
    glGenVertexArrays(1, &caller_vao);
    glGenBuffers(3, buffers);

    GLint alignment = 0;
    glGetIntegerv(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, &alignment);
    const GLintptr start = std::max(alignment, 16);

    glBindVertexArray(caller_vao);
    glBindBuffer(GL_ARRAY_BUFFER, buffers[0]);
    glBufferData(GL_ARRAY_BUFFER, 1024, nullptr, GL_STATIC_DRAW);
    glVertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, 32, reinterpret_cast<const void*>(16));
    glVertexAttribDivisor(6, 7);
    glEnableVertexAttribArray(6);

    glVertexAttribIPointer(8, 3, GL_UNSIGNED_INT, 28, reinterpret_cast<const void*>(4));
    glVertexAttribDivisor(8, 9);
    glEnableVertexAttribArray(8);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 64, nullptr, GL_STATIC_DRAW);

    glBindBuffer(GL_UNIFORM_BUFFER, buffers[2]);
    glBufferData(GL_UNIFORM_BUFFER, start + 256, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferRange(GL_UNIFORM_BUFFER, render::kStudioBonesBinding, buffers[2], start, 256);

    glUseProgram(caller_program->prog);

    for (GLuint slot = 5; slot < 8; ++slot)
    {
        glVertexAttrib4f(slot, static_cast<float>(slot), 0.25f, 0.5f, 0.75f);
    }

    glVertexAttribI4ui(8, 17, 19, 23, 29);
    glEnable(GL_DEPTH_CLAMP);
    g_CheckInstanceLayout = true;
    ncl_utils::ScopeExit reset_layout([] { g_CheckInstanceLayout = false; });

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 1);

    GLint value = 0;
    GLint64 wide_value = 0;

    glGetIntegerv(GL_CURRENT_PROGRAM, &value);
    EXPECT_EQ(value, static_cast<GLint>(caller_program->prog));
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(caller_vao));

    glGetVertexAttribiv(6, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &value);
    EXPECT_EQ(value, 7);
    glGetVertexAttribiv(6, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &value);
    EXPECT_EQ(value, GL_TRUE);

    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_SIZE, &value);
    EXPECT_EQ(value, 3);
    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &value);
    EXPECT_EQ(value, 28);
    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_INTEGER, &value);
    EXPECT_EQ(value, GL_TRUE);

    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &value);
    EXPECT_EQ(value, 9);
    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_TYPE, &value);
    EXPECT_EQ(value, GLint(GL_UNSIGNED_INT));
    glGetVertexAttribiv(8, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &value);
    EXPECT_EQ(value, GLint(buffers[0]));

    void* pointer = nullptr;
    glGetVertexAttribPointerv(8, GL_VERTEX_ATTRIB_ARRAY_POINTER, &pointer);
    EXPECT_EQ(pointer, reinterpret_cast<void*>(4));

    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(buffers[0]));
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(buffers[1]));
    glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &value);
    EXPECT_EQ(value, static_cast<GLint>(buffers[2]));

    glGetInteger64i_v(GL_UNIFORM_BUFFER_START, render::kStudioBonesBinding, &wide_value);
    EXPECT_EQ(wide_value, start);
    glGetInteger64i_v(GL_UNIFORM_BUFFER_SIZE, render::kStudioBonesBinding, &wide_value);
    EXPECT_EQ(wide_value, 256);

    EXPECT_NE(glIsEnabled(GL_DEPTH_CLAMP), GL_FALSE);

    for (GLuint slot = 5; slot < 8; ++slot)
    {
        std::array<float, 4> current{};
        glGetVertexAttribfv(slot, GL_CURRENT_VERTEX_ATTRIB, current.data());
        EXPECT_EQ(current, (std::array<float, 4>{static_cast<float>(slot), 0.25f, 0.5f, 0.75f}));
    }

    std::array<GLuint, 4> integer_current{};
    glGetVertexAttribIuiv(8, GL_CURRENT_VERTEX_ATTRIB, integer_current.data());
    EXPECT_EQ(integer_current, (std::array<GLuint, 4>{17, 19, 23, 29}));

    glDisable(GL_DEPTH_CLAMP);
    glUseProgram(0);
    glBindVertexArray(0);
    glBindBufferBase(GL_UNIFORM_BUFFER, render::kStudioBonesBinding, 0);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDeleteBuffers(3, buffers);
    glDeleteVertexArrays(1, &caller_vao);

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidCountersKeepPhysicalCallsSeparateFromLogicalGeometryAndPhaseCapture)
{
    CaptureModel multi("rigid_ordinary_multimesh", {2});

    DrawOrdinarySequence(multi, {0, 0});

    const render::StudioSubmissionStats ordinary = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(ordinary.physical_draws, 2u);
    EXPECT_EQ(ordinary.logical_groups, 2u);
    EXPECT_EQ(ordinary.logical_triangles, 4u);
    EXPECT_EQ(ordinary.submitted_triangles, 4u);
    EXPECT_EQ(ordinary.instance_uploads, 0u);

    CaptureModel model("rigid_phase_capture", {1, 1, 1});
    model.SetBodyVariants();

    render::StudioPhaseCapture& diagnostics = render::GetRendererState().diagnostics.get_phase_capture();
    ASSERT_TRUE(diagnostics.Start());
    ASSERT_TRUE(diagnostics.BeginView({}, 100));

    DrawRigidSequence(model, {2, 0, 1, 2});
    diagnostics.EndView(200);
    diagnostics.Stop();

    std::ostringstream phases;
    std::ostringstream metadata;

    ASSERT_TRUE(diagnostics.Export(phases, metadata));

    EXPECT_NE(phases.str().find(",physical_draws,submitted_triangles\n"), std::string::npos);
    EXPECT_NE(phases.str().find(",4,4,4,1,1,12\n"), std::string::npos);
    EXPECT_EQ(g_RigidDraws, 1);

    diagnostics.Reset();
}

TEST_F(StudioCaptureGlTest, RigidMetadataUsesTheConsumedBoneAndRejectsMixedBones)
{
    for (bool mixed : {false, true})
    {
        CaptureModel model(mixed ? "rigid_mixed_bones" : "rigid_second_bone", {1, 1, 1});
        model.SetBodyVariants();
        model.SetVertexBones(mixed);

        const auto prepare = [](size_t index) {
            g_Capture.bones[12] = 1;
            g_Capture.bones[17] = 1;
            g_Capture.bones[22] = 1;
            g_Capture.bones[15] = -0.4f + index * 0.2f;
        };

        const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, {2, 0, 1, 2}, prepare);

        EXPECT_EQ(ordinary, DrawRigidSequence(model, {2, 0, 1, 2}, prepare));

        EXPECT_EQ(g_RigidDraws, mixed ? 0 : 1);
        EXPECT_EQ(render::GetRendererState().studio.get_submission_stats().instance_uploads, mixed ? 0u : 1u);
    }
}

TEST_F(StudioCaptureGlTest, RigidUnionKeepsMaskedAndAdditivePixelsWithDepth)
{
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kImageWidth, kImageHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    const int flags[] = {goldsrc::STUDIO_NF_MASKED, goldsrc::STUDIO_NF_ADDITIVE};

    for (int flag : flags)
    {
        CaptureModel model(flag == goldsrc::STUDIO_NF_MASKED ? "rigid_masked" : "rigid_additive", {1, 1, 1});
        model.SetBodyVariants();
        model.SetTextureCoordinates();

        goldsrc::mstudiotexture_t* textures =
            reinterpret_cast<goldsrc::mstudiotexture_t*>(reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex);
        textures[0].flags = flag | goldsrc::STUDIO_NF_FULLBRIGHT;
        glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
        const uint8_t pixels[] = {255, 0, 0, 0, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 0};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, {2, 0, 1, 2});

        EXPECT_EQ(ordinary, DrawRigidSequence(model, {2, 0, 1, 2}));

        EXPECT_EQ(g_RigidDraws, 1);
    }

    glDisable(GL_DEPTH_TEST);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &depth);

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidSubmissionFallsBackForChromeAndEntityLights)
{
    CaptureModel model("rigid_exclusions", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2};
    g_Capture.forced_flags = goldsrc::STUDIO_NF_CHROME;

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> chrome = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(chrome, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 0);
    g_Capture.forced_flags = 0;

    goldsrc::dlight_t lights[kMaxElights]{};
    lights[0].die = 100;
    lights[0].radius = 100;
    lights[0].origin[2] = 10;
    lights[0].color = {255, 255, 255};

    goldsrc::dlight_t(*previous)[kMaxElights] = p_cl_elights;
    p_cl_elights = &lights;
    ncl_utils::ScopeExit restore([&] { p_cl_elights = previous; });

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> lit = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(lit, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 0);
    EXPECT_EQ(render::GetRendererState().studio.get_submission_stats().instance_uploads, 0u);
}

TEST_F(StudioCaptureGlTest, RigidUploadScopeAndDeviceLossPreserveOrdinaryGeometryContract)
{
    CaptureModel model("rigid_device_loss", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2};
    StudioScopeTrace trace(render::GetRendererState().studio.get_monitor());
    g_RigidTrace = &trace;
    ncl_utils::ScopeExit restore([] { g_RigidTrace = nullptr; });

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);

    EXPECT_EQ(trace.calls[performance::PerfStage::StudioInstanceUpload], 0u);
    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(trace.calls[performance::PerfStage::StudioInstanceUpload], 1u);

    const render::StudioGpuModel* gpu = render::GetRendererState().models.PrepareGeometry(&model.model, model.get_header());

    ASSERT_NE(gpu, nullptr);

    glBindVertexArray(gpu->vao);

    for (GLuint slot = 5; slot <= 8; ++slot)
    {
        GLint value = 0;
        glGetVertexAttribiv(slot, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &value);
        EXPECT_EQ(value, 0);
        glGetVertexAttribiv(slot, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &value);
        EXPECT_EQ(value, GL_FALSE);
    }

    glBindVertexArray(0);

    render::Device_OnContextDestroyed();
    render::Device_OnContextCreated();
    g_OriginalMultiDraw = std::exchange(glad_glMultiDrawElements, ObserveCaptureDraw);
    g_OriginalBoneUpload = std::exchange(glad_glBufferSubData, ObserveCapturedBoneUpload);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_EQ(trace.calls[performance::PerfStage::StudioInstanceUpload], 2u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidSubmissionRejectsIncompatibleCapturedPacketsWithoutReordering)
{
    CaptureModel model("rigid_strict_fallback", {1, 1, 1});
    model.SetBodyVariants();

    render::StudioRenderer& renderer = render::GetRendererState().studio;
    render::StudioMaterialBindings stale;
    const std::vector<std::pair<const char*, std::function<void(render::StudioDrawPacket&)>>> changes = {
        {"lighting", [](render::StudioDrawPacket& p) { p.lighting.ambient += 1; }},
        {"gamma", [](render::StudioDrawPacket& p) { p.gamma[0] += 1; }},
        {"fog", [](render::StudioDrawPacket& p) { p.fog.color[0] = 0.25f; }},
        {"texture", [](render::StudioDrawPacket& p) { p.meshes[0].texture = g_Capture.textures[1]; }},
        {"skin", [](render::StudioDrawPacket& p) { p.meshes[0].skin += 1; }},
        {"bindings_generation",
         [&](render::StudioDrawPacket& p) {
             stale = *p.rigid_bindings;
             ++stale.generation;
             p.rigid_bindings = &stale;
         }},
        {"nonfinite_bone",
         [](render::StudioDrawPacket& p) { p.bones = CapturePose({{3, std::numeric_limits<float>::quiet_NaN()}}, p.bones); }},
        {"nonfinite_blend_nan", [](render::StudioDrawPacket& p) { p.blend = std::numeric_limits<float>::quiet_NaN(); }},
        {"nonfinite_blend_positive", [](render::StudioDrawPacket& p) { p.blend = std::numeric_limits<float>::infinity(); }},
        {"nonfinite_blend_negative", [](render::StudioDrawPacket& p) { p.blend = -std::numeric_limits<float>::infinity(); }},
        {"scale", [](render::StudioDrawPacket& p) { p.bone_scale = 2; }},
        {"flipped", [](render::StudioDrawPacket& p) { p.flipped = true; }},
        {"glow", [](render::StudioDrawPacket& p) { p.glow_scale = 1; }},
        {"lights", [](render::StudioDrawPacket& p) { p.light_count = 1; }},
        {"chrome", [](render::StudioDrawPacket& p) { p.meshes[0].chrome = true; }},
        {"authored", [](render::StudioDrawPacket& p) { p.meshes[0].material.emplace(); }},
        {"mesh_count", [](render::StudioDrawPacket& p) { p.meshes.push_back(p.meshes[0]); }},
        {"selected_range", [](render::StudioDrawPacket& p) { p.meshes[0].index_offset = 0; }}
    };
    g_OriginalInstancedDraw = std::exchange(glad_glDrawElementsInstanced, ObserveInstancedDraw);
    ncl_utils::ScopeExit restore([] { glad_glDrawElementsInstanced = g_OriginalInstancedDraw; });

    for (const auto& [name, change] : changes)
    {
        SCOPED_TRACE(name);
        g_RigidDraws = 0;
        g_Capture.draws.clear();
        g_Capture.bone_uploads.clear();
        renderer.ResetSubmissionStats();

        render::Studio_BeginOpaquePass();
        goldsrc::cl_entity_t entity{};

        for (int index = 0; index < 2; ++index)
        {
            entity.curstate.body = index;
            BeginModel(model, entity, index == 0 ? -0.5f : 0.5f);
            DrawPart(0);
            EndModel();
        }

        ASSERT_EQ(render::StudioRendererTestAccess::QueuedCount(renderer), 2u);
        change(render::StudioRendererTestAccess::get_queued_packets(renderer)[1]);

        render::Studio_EndOpaquePass();
        EXPECT_EQ(g_RigidDraws, 0);
        ASSERT_EQ(g_Capture.draws.size(), 2u);
        EXPECT_FLOAT_EQ(g_Capture.draws[0].bone_x, -0.5f);

        if (std::string_view(name) != "nonfinite_bone")
        {
            EXPECT_FLOAT_EQ(g_Capture.draws[1].bone_x, 0.5f);
        }

        EXPECT_EQ(renderer.get_submission_stats().instance_uploads, 0u);

        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    }
}

namespace
{
    struct RigidFloatingField
    {
        const char* name;
        size_t count;
        bool shared;

        std::function<void(render::StudioDrawPacket&, size_t, float)> set;
    };

    const std::vector<RigidFloatingField> kRigidFloatingFields = {
        {"modelview", 16, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.modelview[i] = v; }},
        {"projection", 16, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.projection[i] = v; }},
        {"depth_range", 2, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.raster.depth_range[i] = v; }},
        {"fog_color", 4, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.fog.color[i] = v; }},
        {"fog_density", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.fog.density = v; }},
        {"fog_start", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.fog.start = v; }},
        {"fog_end", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.fog.end = v; }},
        {"ambient", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.lighting.ambient = v; }},
        {"shade", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.lighting.shade = v; }},
        {"color", 3, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.lighting.color[i] = v; }},
        {"light_vec", 3, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.lighting.light_vec[i] = v; }},
        {"chrome_origin", 3, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.chrome_origin[i] = v; }},
        {"view_right", 3, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.view_right[i] = v; }},
        {"glow_scale", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.glow_scale = v; }},
        {"bone_scale", 1, false, [](render::StudioDrawPacket& p, size_t, float v) { p.bone_scale = v; }},
        {"shell_color", 3, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.shell_color[i] = v; }},
        {"gamma", 2, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.gamma[i] = v; }},
        {"effect_time", 1, true, [](render::StudioDrawPacket& p, size_t, float v) { p.effect_time = v; }},
        {"texel_scale", 2, true, [](render::StudioDrawPacket& p, size_t i, float v) { p.meshes[0].texel_scale[i] = v; }},
        {"blend", 1, false, [](render::StudioDrawPacket& p, size_t, float v) { p.blend = v; }},
        {"bone", 12, false, [](render::StudioDrawPacket& p, size_t i, float v) {
             p.bones = CapturePose({{p.rigid_range.bone * 12 + i, v}}, p.bones);
         }}
    };
} // namespace

TEST_F(StudioCaptureGlTest, RigidNonfiniteCapturedFieldsRetainOrdinaryFallbackAndCounters)
{
    CaptureModel model("rigid_nonfinite_fields", {1});
    const float nonfinite[] = {
        std::numeric_limits<float>::quiet_NaN(),
        std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };

    for (const RigidFloatingField& field : kRigidFloatingFields)
    {
        SCOPED_TRACE(field.name);

        for (size_t component = 0; component < field.count; ++component)
        {
            SCOPED_TRACE(component);

            for (float value : nonfinite)
            {
                SCOPED_TRACE(value);
                DrawRigidSequence(model, {0, 0}, {}, false, [&](std::span<render::StudioDrawPacket> packets) {
                    for (render::StudioDrawPacket& packet : packets)
                    {
                        field.set(packet, component, value);
                    }
                });

                const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

                EXPECT_EQ(g_RigidDraws, 0);
                EXPECT_EQ(g_Capture.draws.size(), 2u);
                EXPECT_EQ(stats.physical_draws, 2u);
                EXPECT_EQ(stats.logical_groups, 2u);
                EXPECT_EQ(stats.logical_triangles, 2u);
                EXPECT_EQ(stats.submitted_triangles, 2u);
                EXPECT_EQ(stats.instance_uploads, 0u);
                EXPECT_EQ(stats.instance_bytes, 0u);
                EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::Ineligible)], 2u);
                EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::SharedState)], 0u);
                EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::Singleton)], 0u);
                EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
            }
        }
    }
}

TEST_F(StudioCaptureGlTest, RigidSignedZeroSharedFieldsKeepExactAdjacentRunBoundaries)
{
    CaptureModel model("rigid_signed_zero", {1});

    for (const RigidFloatingField& field : kRigidFloatingFields)
    {
        if (!field.shared)
        {
            continue;
        }

        SCOPED_TRACE(field.name);

        for (size_t component = 0; component < field.count; ++component)
        {
            SCOPED_TRACE(component);

            const auto prepare_packets = [&](std::span<render::StudioDrawPacket> packets) {
                for (size_t index = 0; index < packets.size(); ++index)
                {
                    field.set(packets[index], component, index < 2 ? 0.0f : -0.0f);
                }
            };

            const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary =
                DrawOrdinarySequence(model, {0, 0, 0, 0}, {}, false, prepare_packets);

            EXPECT_EQ(ordinary, DrawRigidSequence(model, {0, 0, 0, 0}, {}, false, prepare_packets));

            const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

            EXPECT_EQ(g_RigidDraws, 2);
            EXPECT_EQ(stats.physical_draws, 2u);
            EXPECT_EQ(stats.logical_groups, 4u);
            EXPECT_EQ(stats.logical_triangles, 4u);
            EXPECT_EQ(stats.submitted_triangles, 4u);
            EXPECT_EQ(stats.instance_batches, 2u);
            EXPECT_EQ(stats.saved_calls, 2u);
            EXPECT_EQ(stats.instance_uploads, 1u);
            EXPECT_EQ(stats.instance_bytes, 4u * 64u);
            EXPECT_EQ(stats.fallbacks, (decltype(stats.fallbacks){}));
            EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
        }
    }
}

TEST_F(StudioCaptureGlTest, RigidIneligibleBoundaryPreservesSeparateRunsAndPacketOrder)
{
    CaptureModel model("rigid_ineligible_boundary", {1});

    const auto prepare_packets = [](std::span<render::StudioDrawPacket> packets) { packets[2].meshes[0].chrome = true; };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary =
        DrawOrdinarySequence(model, {0, 0, 0, 0, 0}, {}, false, prepare_packets);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, {0, 0, 0, 0, 0}, {}, false, prepare_packets));

    const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(g_RigidDraws, 2);
    ASSERT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_FLOAT_EQ(g_Capture.draws[0].bone_x, 0.0f);
    EXPECT_EQ(stats.physical_draws, 3u);
    EXPECT_EQ(stats.logical_groups, 5u);
    EXPECT_EQ(stats.logical_triangles, 5u);
    EXPECT_EQ(stats.submitted_triangles, 5u);
    EXPECT_EQ(stats.instance_batches, 2u);
    EXPECT_EQ(stats.saved_calls, 2u);
    EXPECT_EQ(stats.instance_uploads, 1u);
    EXPECT_EQ(stats.instance_bytes, 5u * 64u);
    EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::Ineligible)], 1u);
    EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::SharedState)], 0u);
    EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::Singleton)], 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST(StudioRigidPanelTest, InstancePlanningAndUploadRemainVisibleWithoutAddingNestedSubmitToOther)
{
    auto snapshot = std::make_unique<performance::PerfSnapshot>();
    snapshot->cpu_frame_count = 1;
    snapshot->cpu_count = 5;
    const ncl_perf::PerfScopeId_t ids[] = {
        performance::PerfStage::Studio,
        performance::PerfStage::StudioInstancePlan,
        performance::PerfStage::StudioInstanceUpload,
        performance::PerfStage::StudioShader,
        performance::PerfStage::StudioInstanceSubmit
    };
    const ncl_perf::PerfDuration_t durations[] = {12000000, 2000000, 3000000, 6000000, 5000000};

    for (uint32_t index = 0; index < snapshot->cpu_count; ++index)
    {
        ncl_perf::PerfCpuSample& sample = snapshot->cpu_tree[index];
        sample.scope_id = ids[index];
        sample.parent_id = index == 4 ? performance::PerfStage::StudioShader : performance::PerfStage::Studio;
        sample.inclusive_ns = durations[index];
        sample.self_ns = index == 0 ? 1000000 : durations[index];
        sample.calls = 1;
        sample.state = ncl_perf::PerfSampleState::Measured;
    }

    performance::PerformancePanelLayout layout;
    layout.Build(*snapshot, 1600, 1200, 0, nullptr, nullptr, 2, snapshot.get());

    std::string text;

    for (const performance::PerfPanelCommand& command : layout.get_commands())
    {
        if (command.kind == performance::PerfPanelCommandKind::Text)
        {
            text += layout.get_text(command);
            text += '\n';
        }
    }

    EXPECT_NE(text.find("Other Studio: 6.000 ms/frame"), std::string::npos);
    EXPECT_NE(text.find("Top detail: Instance submission 5.000 ms/frame"), std::string::npos);
    EXPECT_EQ(performance::Stage(performance::PerfStage::StudioInstanceSubmit)->scope.parent_id, performance::PerfStage::StudioShader);
}

TEST_F(StudioCaptureGlTest, RigidImplicitLegacyMaterialKeepsInstancesEnabledWithMaterialsOn)
{
    goldsrc::cvar_t materials{};
    materials.value = 1;

    render::g_RenderCfg.studio_materials = &materials;
    CaptureModel model("models/rigid_implicit.mdl", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 2, 0, 0};

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);
    ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::ModelDocument;
    const ncl_material::Material* material = render::GetRendererState().models.ResolveMaterialByName(model.model.name, "red", &origin);

    ASSERT_NE(material, nullptr);
    EXPECT_EQ(origin, ncl_material::MaterialOrigin::Implicit);
    EXPECT_EQ(material->program, ncl_material::kLegacyProgram);
    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_TRUE(g_Capture.draws.empty());
    EXPECT_EQ(g_Capture.bone_uploads, (std::vector<GLsizeiptr>{4 * 64}));

    const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(stats.physical_draws, 1u);
    EXPECT_EQ(stats.logical_groups, 4u);
    EXPECT_EQ(stats.instance_batches, 1u);
    EXPECT_EQ(stats.saved_calls, 3u);
    EXPECT_EQ(stats.fallbacks[static_cast<size_t>(render::StudioRigidFallback::Ineligible)], 0u);

    const render::StudioGpuModel* gpu = render::GetRendererState().models.PrepareGeometry(&model.model, model.get_header());

    ASSERT_NE(gpu, nullptr);
    EXPECT_EQ(gpu->rigid.count, 3u);
    EXPECT_NE(gpu->rigid_vao, 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidAuthoredLegacyAndCustomMaterialsKeepOrdinarySubmission)
{
    goldsrc::cvar_t materials{};
    materials.value = 1;

    render::g_RenderCfg.studio_materials = &materials;

    for (const char* program : {"studio_legacy", "studio"})
    {
        SCOPED_TRACE(program);

        const std::string name = std::string("models/rigid_authored_") + program;
        studio_test::g_Files[name + ".mat"] = std::string("{\"version\":1,\"materials\":{\"red\":{\"program\":\"") + program + "\"}}}";

        CaptureModel model((name + ".mdl").c_str(), {1, 1, 1});
        model.SetBodyVariants();

        const std::vector<int> bodies{2, 2, 0, 0};

        const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies);
        ncl_material::MaterialOrigin origin = ncl_material::MaterialOrigin::Implicit;
        ASSERT_NE(render::GetRendererState().models.ResolveMaterialByName(model.model.name, "red", &origin), nullptr);

        EXPECT_EQ(origin, ncl_material::MaterialOrigin::ModelDocument);
        EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies));

        EXPECT_EQ(g_RigidDraws, 0);
        EXPECT_EQ(g_Capture.draws.size(), 4u);
        EXPECT_EQ(render::GetRendererState().studio.get_submission_stats().instance_uploads, 0u);

        EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
    }
}

TEST_F(StudioCaptureGlTest, RigidStatsPassPreformattedFullWidthCountersToTheEngineConsole)
{
    studio_test::Chain<void, const char*, goldsrc::xcommand_t> chain;
    studio_test::Hook<void, const char*, goldsrc::xcommand_t> hook;

    std::unordered_map<std::string, goldsrc::xcommand_t> commands;
    hook.callback = [&](const char* name, goldsrc::xcommand_t command) { commands[name] = command; };
    get_engine().GetEngineData()->Cmd_AddCommand.SetChain(&chain);
    get_engine().GetEngineData()->Cmd_AddCommand.SetHook(&hook);

    render::Dump_RegisterCommands();
    ASSERT_TRUE(commands.contains("r_studio_rigid_stats"));
    int count = 1;
    int* previous_count = p_cmd_argc;
    const auto previous_argc = p_g_engdstAddrs->Cmd_Argc;
    p_cmd_argc = &count;
    p_g_engdstAddrs->Cmd_Argc = [] {};

    std::string text;
    g_TextOnlyConsole = &text;

    render::StudioSubmissionStats& stats =
        const_cast<render::StudioSubmissionStats&>(render::GetRendererState().studio.get_submission_stats());
    ncl_utils::ScopeExit restore([&] {
        g_TextOnlyConsole = nullptr;
        p_cmd_argc = previous_count;
        p_g_engdstAddrs->Cmd_Argc = previous_argc;
        stats = {};
    });
    stats.physical_draws = 4294967297ull;
    stats.logical_groups = 4294967298ull;
    stats.logical_triangles = 4294967299ull;
    stats.submitted_triangles = 4294967300ull;
    stats.instance_batches = 4294967301ull;
    stats.saved_calls = 4294967302ull;
    stats.instance_bytes = 4294967303ull;
    stats.instance_uploads = 4294967304ull;
    stats.fallbacks[0] = std::numeric_limits<uint64_t>::max();

    commands["r_studio_rigid_stats"]();

    EXPECT_NE(
        text.find(
            "physical_draws=4294967297 logical_groups=4294967298 logical_triangles=4294967299 submitted_triangles=4294967300 "
            "instance_batches=4294967301 saved_calls=4294967302 instance_bytes=4294967303 instance_uploads=4294967304\n"
        ),
        std::string::npos
    );
    EXPECT_NE(text.find("primitive_restart=18446744073709551615\n"), std::string::npos);
}

TEST_F(StudioCaptureGlTest, RigidPerInstanceBlendPreservesTransparentBodyOrder)
{
    CaptureModel model("rigid_instance_alpha", {1, 1, 1});
    model.SetBodyVariants();

    goldsrc::mstudiotexture_t* textures =
        reinterpret_cast<goldsrc::mstudiotexture_t*>(reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex);
    textures[0].flags = 0;

    goldsrc::cvar_t materials{};
    materials.value = 1;

    render::g_RenderCfg.studio_materials = &materials;
    float blend = 1;
    float* previous = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous; });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    std::vector<int> bodies{2, 0, 1, 2};
    std::vector<float> blends{0.15f, 0.8f, 0.4f, 0.6f};

    const auto prepare = [&](size_t index) {
        blend = blends[index];
        SetLighting(32, 192);

        const float angle = 0.9f + bodies[index] * 0.25f;
        g_Capture.bones[0] = std::cos(angle);
        g_Capture.bones[2] = std::sin(angle);
        g_Capture.bones[8] = -std::sin(angle);
        g_Capture.bones[10] = std::cos(angle);
        g_Capture.bones[3] = 0;
    };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare, true);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare, true));

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_EQ(g_Capture.bone_uploads, (std::vector<GLsizeiptr>{256}));

    std::reverse(bodies.begin(), bodies.end());
    std::reverse(blends.begin(), blends.end());

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> reversed = DrawOrdinarySequence(model, bodies, prepare, true);

    EXPECT_NE(ordinary, reversed);
    EXPECT_EQ(reversed, DrawRigidSequence(model, bodies, prepare, true));

    EXPECT_EQ(g_RigidDraws, 1);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

TEST_F(StudioCaptureGlTest, RigidPerInstanceBlendPreservesSameRangeAlpha)
{
    CaptureModel model("rigid_instance_same_alpha", {1, 1, 1});
    model.SetBodyVariants();

    float blend = 1;
    float* previous = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous; });
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);

    const float blends[] = {0.1f, 0.75f, 0.35f, 0.6f};

    const auto prepare = [&](size_t index) {
        blend = blends[index];
        g_Capture.bones[3] = 0;
    };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, {0, 0, 0, 0}, prepare, true);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, {0, 0, 0, 0}, prepare, true));

    EXPECT_EQ(g_RigidDraws, 1);
    EXPECT_EQ(render::GetRendererState().studio.get_submission_stats().saved_calls, 3u);

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
}

TEST_F(StudioCaptureGlTest, RigidPerInstanceBlendPreservesAdditiveAndMaskedAlpha)
{
    GLuint depth = 0;
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, kImageWidth, kImageHeight);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_BLEND);
    glEnable(GL_FOG);
    glFogi(GL_FOG_MODE, GL_LINEAR);

    const float fog[] = {0.2f, 0.4f, 0.6f, 1};
    glFogfv(GL_FOG_COLOR, fog);
    glFogf(GL_FOG_START, -1);
    glFogf(GL_FOG_END, 1);

    float blend = 1;
    float* previous = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous; });

    for (bool masked : {false, true})
    {
        CaptureModel model(masked ? "rigid_instance_masked" : "rigid_instance_additive", {1, 1, 1});
        model.SetBodyVariants();

        goldsrc::mstudiotexture_t* textures =
            reinterpret_cast<goldsrc::mstudiotexture_t*>(reinterpret_cast<uint8_t*>(model.get_header()) + model.get_header()->textureindex);
        textures[0].flags = goldsrc::STUDIO_NF_FULLBRIGHT | (masked ? goldsrc::STUDIO_NF_MASKED : 0);
        glBindTexture(GL_TEXTURE_2D, g_Capture.textures[0]);
        const uint8_t pixel[] = {64, 32, 16, masked ? uint8_t{128} : uint8_t{255}};
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
        glBlendFunc(masked ? GL_SRC_ALPHA : GL_ONE, masked ? GL_ONE_MINUS_SRC_ALPHA : GL_ONE);

        const float masked_blends[] = {0.99f, 1.01f, 0.2f, 1.0f};
        const float additive_blends[] = {-0.25f, 0.25f, 0.5f, 1.25f};

        const auto prepare = [&](size_t index) {
            blend = masked ? masked_blends[index] : additive_blends[index];
            g_Capture.bones[3] = 0;
            glDepthMask(GL_FALSE);
            g_Capture.entity->curstate.rendermode = masked ? goldsrc::kRenderNormal : goldsrc::kRenderTransAdd;
        };

        const std::vector<int> bodies{2, 0, 1, 2};
        glDepthMask(GL_TRUE);

        const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare, true);
        std::array<float, kImageWidth * kImageHeight> ordinary_depth{};
        glReadPixels(0, 0, kImageWidth, kImageHeight, GL_DEPTH_COMPONENT, GL_FLOAT, ordinary_depth.data());

        glDepthMask(GL_TRUE);
        EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare, true));

        EXPECT_EQ(g_RigidDraws, 1);

        std::array<float, kImageWidth * kImageHeight> candidate_depth{};
        glReadPixels(0, 0, kImageWidth, kImageHeight, GL_DEPTH_COMPONENT, GL_FLOAT, candidate_depth.data());

        EXPECT_EQ(ordinary_depth, candidate_depth);
    }

    glDepthMask(GL_TRUE);
    glDisable(GL_FOG);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    glDeleteRenderbuffers(1, &depth);

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, RigidPerInstanceBlendPreservesInterruptedRunsAndExplicitFlushes)
{
    CaptureModel model("rigid_blend_interruptions", {1, 1, 1});
    model.SetBodyVariants();

    const std::vector<int> bodies{2, 0, 1, 2, 0, 1, 2};
    float blend = 1.0f;
    float* previous_blend = p_r_blend;
    p_r_blend = &blend;
    ncl_utils::ScopeExit restore([&] { p_r_blend = previous_blend; });

    const auto prepare = [&](size_t index) {
        blend = 0.1f + static_cast<float>(index) * 0.1f;
        glDepthFunc(index == 2 ? GL_LESS : GL_LEQUAL);

        if (index == 3)
        {
            render::Studio_Flush(render::StudioFlushReason::Brush);
        }
    };

    const std::array<uint8_t, kImageWidth * kImageHeight * 4> ordinary = DrawOrdinarySequence(model, bodies, prepare, true);

    EXPECT_EQ(ordinary, DrawRigidSequence(model, bodies, prepare, true));

    const render::StudioSubmissionStats& stats = render::GetRendererState().studio.get_submission_stats();

    EXPECT_EQ(g_RigidDraws, 2);
    EXPECT_EQ(g_Capture.draws.size(), 1u);
    EXPECT_EQ(stats.physical_draws, 3u);
    EXPECT_EQ(stats.logical_groups, 7u);
    EXPECT_EQ(stats.saved_calls, 4u);
    EXPECT_EQ(stats.instance_uploads, 2u);
    EXPECT_EQ(stats.instance_bytes, 7u * 64);

    glDepthFunc(GL_LESS);

    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, ScaledAuthoredSnapshotPreservesFullTailAndEnginePalette)
{
    render::g_RenderCfg.studio_materials = nullptr;
    CaptureModel model("models/pose_scaled_tail.mdl", {1});
    studio_test::g_Files["models/pose_scaled_tail.mat"] = R"({"version":1,"materials":{"red":{"program":"studio"}}})";
    goldsrc::cl_entity_t entity{};
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    request.scale = 2.0f;
    std::array<float, render::kStudioBoneFloats> expected{};
    {
        render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(model, entity, 0.25f);
        for (size_t i = 12; i < g_Capture.bones.size(); ++i)
        {
            g_Capture.bones[i] = 17.0f + static_cast<float>(i);
        }
        expected = g_Capture.bones;
        DrawPart(0);
        EndModel();
    }
    using Capture = render::preview::PreviewInteractionTestAccess;
    ASSERT_EQ(Capture::Count(capture), 1u);
    const render::StudioDrawPacket& packet = Capture::Packet(capture, 0);
    ASSERT_TRUE(packet.meshes[0].material.has_value());
    for (size_t i = 0; i < packet.bones.size(); ++i)
    {
        EXPECT_EQ(packet.bones[i], i < 12 ? expected[i] * 2.0f : expected[i]);
        EXPECT_EQ(g_Capture.bones[i], expected[i]);
    }
    EXPECT_NE(
        std::find(
            g_Capture.bone_uploads.begin(), g_Capture.bone_uploads.end(), static_cast<GLsizeiptr>(render::kStudioBoneFloats * sizeof(float))
        ),
        g_Capture.bone_uploads.end()
    );
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

TEST_F(StudioCaptureGlTest, PickingSnapshotSurvivesQueueCapacityGrowthAndRendererReset)
{
    CaptureModel model("models/pose_retained_pick.mdl", {1});
    goldsrc::cl_entity_t entity{};
    render::preview::PreviewInteraction capture(
        render::GetRendererState().resources.device, render::GetRendererState().resources.shaders, render::GetRendererState().coverage
    );
    PreviewDrawParams params{};
    params.mode = PreviewRenderMode::MaterialAuthoring;
    PreviewModel request{};
    std::array<float, render::kStudioBoneFloats> expected{};
    {
        render::StudioPreviewContext context(render::GetRendererState().studio, &entity, params, request, &capture);
        BeginModel(model, entity, 0.25f);
        for (size_t i = 12; i < g_Capture.bones.size(); ++i)
        {
            g_Capture.bones[i] = 100.0f + static_cast<float>(i);
        }
        expected = g_Capture.bones;
        DrawPart(0);
        EndModel();
    }
    using Capture = render::preview::PreviewInteractionTestAccess;
    ASSERT_EQ(Capture::Count(capture), 1u);
    const float* retained_address = Capture::Packet(capture, 0).bones.data();
    render::Studio_BeginOpaquePass();
    for (int index = 0; index < 400; ++index)
    {
        BeginModel(model, entity, static_cast<float>(index));
        g_Capture.bones.back() = -static_cast<float>(index);
        DrawPart(0);
        EndModel();
    }
    render::Studio_EndOpaquePass();
    EXPECT_GE(render::Studio_QueueStats().flushes[static_cast<size_t>(render::StudioFlushReason::Capacity)], 3u);
    render::Studio_DiscardPending();
    ASSERT_EQ(Capture::Count(capture), 1u);
    EXPECT_EQ(Capture::Packet(capture, 0).bones.data(), retained_address);
    for (size_t i = 0; i < expected.size(); ++i)
    {
        EXPECT_EQ(Capture::Packet(capture, 0).bones[i], expected[i]);
    }
    capture.DiscardPackets();
    EXPECT_EQ(Capture::Count(capture), 0u);
    EXPECT_EQ(glGetError(), GLenum(GL_NO_ERROR));
}

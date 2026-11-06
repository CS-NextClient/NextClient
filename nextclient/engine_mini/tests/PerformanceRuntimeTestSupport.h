#pragma once

#include <array>
#include <functional>
#include <memory>

#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/view.h>
#include <nitroapi/hook/GenericHookInterface.h>

#include "performance/PerformanceRuntime.h"

namespace performance
{
    class PerformanceRuntimeTestAccess
    {
    public:
        static goldsrc::cvar_t& MapperControl(PerformanceRuntime& runtime)
        {
            return runtime.perf_map_;
        }
    };
} // namespace performance

namespace performance_test
{
    template <class TResult, class... TArgs>
    class RuntimeHook final : public nitroapi::GenericHookInterface<TResult, TArgs...>
    {
    private:
        std::function<TResult(TArgs...)> callback_;

    public:
        std::function<TResult(TArgs...)> original;
        void UnsetCallbackAndDisableHook() override {}
        uint32_t GetTrampolineAddress() const override
        {
            return 1;
        }
        uint32_t GetOriginalFunctionAddress() const override
        {
            return 1;
        }
        bool IsHookActive() const override
        {
            return true;
        }
        void SetCallbackAndEnableHook(std::function<TResult(TArgs...)>&& callback) override
        {
            callback_ = std::move(callback);
        }
        TResult InvokeOriginal(TArgs... args) override
        {
            return original(args...);
        }
        TResult Invoke(TArgs... args) override
        {
            return callback_ ? callback_(args...) : original(args...);
        }
    };

    struct MapperWorld
    {
        goldsrc::model_t model{};
        goldsrc::mplane_t plane{};
        goldsrc::msurface_t surface{};
        std::array<goldsrc::mvertex_t, 4> vertices{};
        std::array<goldsrc::medge_t, 5> edges{};
        std::array<int, 4> surfedges{1, 2, 3, 4};
        goldsrc::texture_t texture{};
        goldsrc::mtexinfo_t texinfo{};
        char entities[40] = "{ \"classname\" \"worldspawn\" }";

        MapperWorld()
        {
            plane.normal[0] = 1;
            plane.dist = 100;
            surface.plane = &plane;
            surface.numedges = 4;
            surface.texinfo = &texinfo;
            texinfo.texture = &texture;
            for (int edge = 0; edge < 4; ++edge)
            {
                vertices[edge].position[0] = 100;
                vertices[edge].position[1] = edge == 0 || edge == 3 ? -10.0f : 10.0f;
                vertices[edge].position[2] = edge < 2 ? -10.0f : 10.0f;
                edges[edge + 1].v[0] = static_cast<unsigned short>(edge);
                edges[edge + 1].v[1] = static_cast<unsigned short>((edge + 1) % 4);
            }
            model.type = goldsrc::mod_brush;
            model.nummodelsurfaces = 1;
            model.surfaces = &surface;
            model.vertexes = vertices.data();
            model.edges = edges.data();
            model.surfedges = surfedges.data();
            model.entities = entities;
        }
    };

    struct MapperInputs
    {
        std::unique_ptr<goldsrc::client_state_t> client = std::make_unique<goldsrc::client_state_t>();
        std::unique_ptr<goldsrc::client_static_t> session = std::make_unique<goldsrc::client_static_t>();
        std::unique_ptr<goldsrc::server_t> server = std::make_unique<goldsrc::server_t>();
        MapperWorld world;
        goldsrc::refdef_t view{};
        int wpoly{};
        int epoly{};
        int visible{};

        explicit MapperInputs(nitroapi::EngineData& engine)
        {
            client->worldmodel = &world.model;
            session->state = goldsrc::ca_active;
            session->netchan.connect_time = 1;
            session->netchan.remote_address.type = goldsrc::NA_LOOPBACK;
            server->active = true;
            view.vrect = {0, 0, 640, 480, nullptr};
            engine.client_state = client.get();
            engine.client_static = session.get();
            engine.server = server.get();
            engine.c_brush_polys = &wpoly;
            engine.c_alias_polys = &epoly;
            engine.cl_numvisedicts = &visible;
            engine.r_refdef = &view;
        }

        void Enable(performance::PerformanceRuntime& runtime, float level = 1)
        {
            goldsrc::cvar_t& control = performance::PerformanceRuntimeTestAccess::MapperControl(runtime);
            control.value = level;
            runtime.OnMapperChanged(&control);
        }
    };
} // namespace performance_test

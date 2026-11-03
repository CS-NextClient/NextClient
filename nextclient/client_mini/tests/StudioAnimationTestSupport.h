#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <utility>

#include <goldsrc/entity.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/model.h>
#include <nitroapi/modules/client/StudioAnimationData.h>

namespace studio::testing
{
    struct Renderer
    {
        struct ModelStorage
        {
            goldsrc::studiohdr_t header{};
            goldsrc::mstudiobone_t bones[8]{};
            goldsrc::mstudioseqdesc_t sequences[104]{};
            goldsrc::mstudioanim_t animations[9][8]{};
            goldsrc::mstudioseqgroup_t groups[2]{};
        };

        double time{};
        int interpolate{};
        goldsrc::cl_entity_t entity{};
        goldsrc::cl_entity_t* current_entity = &entity;
        goldsrc::model_t model{};
        goldsrc::model_t* render_model = &model;
        goldsrc::player_info_t player{};
        goldsrc::player_info_t* player_info{};
        ModelStorage storage;
        goldsrc::studiohdr_t* header = &storage.header;
        int cached_count{};
        char names[128][32]{};
        nitroapi::StudioMatrix_t cached_bones[128]{};
        nitroapi::StudioMatrix_t cached_lights[128]{};
        nitroapi::StudioMatrix_t rotation{
            {1, 0, 0, 10},
            {0, 1, 0, 20},
            {0, 0, 1, 30}
        };
        nitroapi::StudioMatrix_t alias{
            {1, 0, 0, 1000},
            {0, 1, 0, 2000},
            {0, 0, 1, 3000}
        };
        nitroapi::StudioMatrix_t bones[128]{};
        nitroapi::StudioMatrix_t lights[128]{};
        nitroapi::StudioMatrix_t* rotation_ptr = &rotation;
        nitroapi::StudioMatrix_t* alias_ptr = &alias;
        nitroapi::StudioMatrix_t* bone_ptr = bones;
        nitroapi::StudioMatrix_t* light_ptr = lights;
        int gait[33]{};

        Renderer()
        {
            storage.header.numbones = 2;
            storage.header.numseq = 104;
            storage.header.boneindex = offsetof(ModelStorage, bones);
            storage.header.seqindex = offsetof(ModelStorage, sequences);
            storage.header.seqgroupindex = offsetof(ModelStorage, groups);

            for (goldsrc::mstudioseqdesc_t& sequence : storage.sequences)
            {
                sequence.numframes = 4;
                sequence.numblends = 1;
                sequence.animindex = offsetof(ModelStorage, animations);
            }

            for (goldsrc::mstudiobone_t& bone : storage.bones)
            {
                for (int& controller : bone.bonecontroller)
                {
                    controller = -1;
                }
            }

            std::strcpy(storage.bones[0].name, "root");
            std::strcpy(storage.bones[1].name, "child");
            storage.bones[0].parent = -1;
            storage.bones[1].parent = 0;
            storage.bones[0].value[0] = 1;
            storage.bones[0].value[1] = 2;
            storage.bones[0].value[2] = 3;
            storage.bones[1].value[0] = 4;
            storage.bones[1].value[1] = 5;
            storage.bones[1].value[2] = 6;
        }

        goldsrc::cstrike::CStudioModelRenderer* receiver()
        {
            return reinterpret_cast<goldsrc::cstrike::CStudioModelRenderer*>(this);
        }

        goldsrc::cstrike::CGameStudioModelRenderer* game_receiver()
        {
            return reinterpret_cast<goldsrc::cstrike::CGameStudioModelRenderer*>(this);
        }

        static nitroapi::StudioRendererView View(goldsrc::cstrike::CStudioModelRenderer* receiver)
        {
            Renderer& state = *reinterpret_cast<Renderer*>(receiver);

            return {
                &state.time,
                &state.interpolate,
                &state.current_entity,
                &state.render_model,
                &state.player_info,
                &state.header,
                &state.cached_count,
                state.names,
                state.cached_bones,
                state.cached_lights,
                &state.rotation_ptr,
                &state.alias_ptr,
                &state.bone_ptr,
                &state.light_ptr
            };
        }

        static nitroapi::StudioGameRendererView GameView(goldsrc::cstrike::CGameStudioModelRenderer* receiver)
        {
            Renderer& state = *reinterpret_cast<Renderer*>(receiver);

            return {View(state.receiver()), reinterpret_cast<std::byte*>(state.gait)};
        }
    };

    template <class Result, class... Args>
    struct Hook : nitroapi::GenericHookInterface<Result, Args...>
    {
        std::function<Result(Args...)> original;
        std::function<Result(Args...)> callback;
        std::function<void(Args...)> before;
        bool fail{};

        void SetCallbackAndEnableHook(std::function<Result(Args...)>&& value) override
        {
            if (!fail)
            {
                callback = std::move(value);
            }
        }

        void UnsetCallbackAndDisableHook() override
        {
            callback = {};
        }

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
            return bool(callback);
        }

        Result InvokeOriginal(Args... args) override
        {
            return original(args...);
        }

        Result Invoke(Args... args) override
        {
            if (before)
            {
                before(args...);
            }

            return callback ? callback(args...) : original(args...);
        }
    };

    template <class Result, class... Args>
    struct Chain : nitroapi::HandlersChainInterface<Result, Args...>
    {
        nitroapi::GenericHookInterface<Result, Args...>* hook{};
        nitroapi::HandlerInterface<Result, Args...>* handler{};
        bool reject_handler{};

        void AttachHook(nitroapi::GenericHookInterface<Result, Args...>* value) override
        {
            hook = value;
            hook->SetCallbackAndEnableHook([this](Args... args) -> Result {
                return handler ? handler->Handle(args..., nullptr) : hook->InvokeOriginal(args...);
            });
        }

        void DetachHook() override
        {
            if (hook)
            {
                hook->UnsetCallbackAndDisableHook();
                hook = nullptr;
            }
        }

        bool IsHookAttached() override
        {
            return hook != nullptr;
        }

        nitroapi::GenericHookInterface<Result, Args...>* GetHook() const override
        {
            return hook;
        }

        uint32_t GetSubscribersCount() const override
        {
            return 0;
        }

        uint32_t GetHandlersCount() const override
        {
            return handler ? 1 : 0;
        }

        bool AddHandler(nitroapi::HandlerInterface<Result, Args...>* value) override
        {
            if (reject_handler)
            {
                return false;
            }

            handler = value;

            return true;
        }

        bool RemoveHandler(nitroapi::HandlerInterface<Result, Args...>* value) override
        {
            if (handler != value)
            {
                return false;
            }

            handler = nullptr;

            return true;
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

    template <class Result, class... Args>
    struct Binding
    {
        Chain<Result, Args...> chain;
        Hook<Result, Args...> hook;

        template <class Function, class Owner>
        void Bind(Function& function, Owner& owner, Result (Owner::*method)(Args...))
        {
            function.SetChain(&chain);
            function.SetHook(&hook);
            hook.original = [&owner, method](Args... args) -> Result { return (owner.*method)(args...); };
        }
    };

    template <class Function>
    struct FunctionBinding;

    template <class Result, class... Args>
    struct FunctionBinding<nitroapi::NitroFunction<Result, Args...>> : Binding<Result, Args...>
    {};

    template <class... Args>
    struct FunctionBinding<nitroapi::NitroFunctionVoid<Args...>> : Binding<void, Args...>
    {};
} // namespace studio::testing

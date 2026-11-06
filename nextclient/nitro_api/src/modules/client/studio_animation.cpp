#include "studio_animation.h"

#include <cstddef>

#include <nitroapi/modules/client/StudioAnimationData.h>

#include "chain/HandlersChain.h"

namespace nitroapi
{
    namespace
    {
#ifdef _WIN32
        constexpr size_t kFxSlot = 12;
#else
        constexpr size_t kFxSlot = 13;
#endif

        constexpr size_t Offset(size_t windows_offset)
        {
#ifdef _WIN32

            return windows_offset;
#else

            return windows_offset - sizeof(uint32_t);
#endif
        }

        template <class T>
        T* Field(goldsrc::cstrike::CStudioModelRenderer* receiver, size_t windows_offset)
        {
            return reinterpret_cast<T*>(reinterpret_cast<std::byte*>(receiver) + Offset(windows_offset));
        }

        StudioRendererView GetRendererView(goldsrc::cstrike::CStudioModelRenderer* receiver)
        {
            return {
                Field<double>(receiver, 0x8),
                Field<int>(receiver, 0x18),
                Field<goldsrc::cl_entity_t*>(receiver, 0x30),
                Field<goldsrc::model_t*>(receiver, 0x34),
                Field<goldsrc::player_info_t*>(receiver, 0x38),
                Field<goldsrc::studiohdr_t*>(receiver, 0x44),
                Field<int>(receiver, 0x5c),
                Field<char[32]>(receiver, 0x60),
                Field<StudioMatrix_t>(receiver, 0x1060),
                Field<StudioMatrix_t>(receiver, 0x2860),
                Field<StudioMatrix_t*>(receiver, 0x40a0),
                Field<StudioMatrix_t*>(receiver, 0x40a4),
                Field<StudioMatrix_t*>(receiver, 0x40a8),
                Field<StudioMatrix_t*>(receiver, 0x40ac),
            };
        }

        StudioGameRendererView GetGameRendererView(goldsrc::cstrike::CGameStudioModelRenderer* receiver)
        {
            goldsrc::cstrike::CStudioModelRenderer* base = reinterpret_cast<goldsrc::cstrike::CStudioModelRenderer*>(receiver);

            return {GetRendererView(base), Field<std::byte>(base, 0x40ac)};
        }

        void StudioFxTransform(goldsrc::cstrike::CStudioModelRenderer* receiver, goldsrc::cl_entity_t* entity, StudioMatrix_t* matrix)
        {
#ifdef _WIN32
            using Function_t = void(__thiscall*)(goldsrc::cstrike::CStudioModelRenderer*, goldsrc::cl_entity_t*, StudioMatrix_t*);
#else
            using Function_t = void (*)(goldsrc::cstrike::CStudioModelRenderer*, goldsrc::cl_entity_t*, StudioMatrix_t*);
#endif
            void** table = *reinterpret_cast<void***>(receiver);
            reinterpret_cast<Function_t>(table[kFxSlot])(receiver, entity, matrix);
        }

        template <class T>
        bool IsResolved(const T& function)
        {
            const auto* hook = function.GetHook();

            return hook && hook->GetTrampolineAddress() != 0 && hook->GetTrampolineAddress() != hook->GetOriginalFunctionAddress();
        }

        std::shared_ptr<Unsubscriber> TrackBoneHandler(
            StudioAnimationData& data,
            StudioBoneFunction_t& function,
            StudioBoneHandler_t* handler,
            bool* direct_call
        )
        {
            if ((&function != &data.StudioCalcBoneQuaterion && &function != &data.StudioCalcBonePosition) || function.GetChain() == nullptr)
            {
                return {};
            }

            using Chain_t = HandlersChain<
                void,
                goldsrc::cstrike::CStudioModelRenderer*,
                int,
                float,
                goldsrc::mstudiobone_t*,
                goldsrc::mstudioanim_t*,
                float*,
                float*>;
            Chain_t* chain = dynamic_cast<Chain_t*>(function.GetChain());

            if (chain == nullptr || !chain->TrackSingleHandler(handler, direct_call))
            {
                return {};
            }

            return std::make_shared<Unsubscriber>([chain, direct_call] { chain->TrackSingleHandler(nullptr, direct_call); });
        }
    } // namespace

    void StudioAnimation_InitializeAccess(StudioAnimationData& data)
    {
        static_assert(sizeof(void*) == 4);
        data.GetRendererView = GetRendererView;
        data.GetGameRendererView = GetGameRendererView;
        data.StudioFxTransform = StudioFxTransform;
#if !USE_PROFILER
        data.TrackBoneHandler = TrackBoneHandler;
#else
        data.TrackBoneHandler = nullptr;
#endif
    }

    bool StudioAnimation_IsResolved(const StudioAnimationData& data)
    {
        return data.right_hand && data.shield && data.is_czero && IsResolved(data.StudioSetupBones) &&
               IsResolved(data.GameStudioSetupBones) && IsResolved(data.StudioGetAnim) && IsResolved(data.StudioEstimateInterpolant) &&
               IsResolved(data.StudioEstimateFrame) && IsResolved(data.StudioSlerpBones) && IsResolved(data.StudioCalcBoneAdj) &&
               IsResolved(data.StudioCalcBoneQuaterion) && IsResolved(data.StudioCalcBonePosition) &&
               IsResolved(data.StudioCalcRotations) && IsResolved(data.StudioSaveBones) && IsResolved(data.StudioMergeBones);
    }
} // namespace nitroapi

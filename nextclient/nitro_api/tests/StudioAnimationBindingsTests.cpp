#include <array>
#include <cstddef>
#include <cstring>
#include <memory>

#include <gtest/gtest.h>

#include <goldsrc/entity.h>
#include <nitroapi/modules/client/StudioAnimationData.h>

#include "modules/client/ClientModule.h"
#include "modules/client/studio_animation.h"

namespace
{
    using Renderer_t = goldsrc::cstrike::CStudioModelRenderer;
    using GameRenderer_t = goldsrc::cstrike::CGameStudioModelRenderer;

#ifdef _WIN32
    constexpr size_t kPlatformShift = 0;
    constexpr size_t kFxSlot = 12;
#else
    constexpr size_t kPlatformShift = 4;
    constexpr size_t kFxSlot = 13;
#endif

    Renderer_t* g_FxReceiver{};

#ifdef _WIN32
    void __fastcall FxTransform(Renderer_t* receiver, void*, goldsrc::cl_entity_t* entity, nitroapi::StudioMatrix_t* matrix)
#else
    void FxTransform(Renderer_t* receiver, goldsrc::cl_entity_t* entity, nitroapi::StudioMatrix_t* matrix)
#endif
    {
        g_FxReceiver = receiver;
        (*matrix)[0][3] = static_cast<float>(entity->index);
    }
} // namespace

TEST(StudioAnimationBindings, ClientModuleInvalidatesBindingsOnUnload)
{
    nitroapi::ClientModule module(std::make_shared<nitroapi::HookStorage>(), nullptr);
    std::shared_ptr<nitroapi::ClientData> data = module.GetClientData();

    EXPECT_EQ(data->studio_animation, nullptr);

    nitroapi::StudioAnimationData bindings;
    data->studio_animation = &bindings;
    module.OnLibraryUnload();

    EXPECT_EQ(data->studio_animation, nullptr);
}

TEST(StudioAnimationBindings, ClientModuleInvalidatesBindingsOnDestruction)
{
    std::shared_ptr<nitroapi::ClientData> data;
    nitroapi::StudioAnimationData bindings;
    {
        nitroapi::ClientModule module(std::make_shared<nitroapi::HookStorage>(), nullptr);
        data = module.GetClientData();
        module.OnLibraryUnload();
        data->studio_animation = &bindings;
    }

    EXPECT_EQ(data->studio_animation, nullptr);
}

TEST(StudioAnimationBindings, EmptyBindingsAreUnavailable)
{
    nitroapi::StudioAnimationData data;
    nitroapi::StudioAnimation_InitializeAccess(data);

    EXPECT_FALSE(nitroapi::StudioAnimation_IsResolved(data));
}

TEST(StudioAnimationBindings, ViewsAddressLiveFieldsWithoutCopyingForeignObjects)
{
    alignas(8) std::array<std::byte, 0x4134> storage{};
    nitroapi::StudioAnimationData data;
    nitroapi::StudioAnimation_InitializeAccess(data);
    Renderer_t* receiver = reinterpret_cast<Renderer_t*>(storage.data());
    nitroapi::StudioRendererView view = data.GetRendererView(receiver);

    EXPECT_EQ(reinterpret_cast<std::byte*>(view.time), storage.data() + 0x8 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.current_entity), storage.data() + 0x30 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.studio_header), storage.data() + 0x44 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.cached_names), storage.data() + 0x60 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.cached_bone_transform), storage.data() + 0x1060 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.bone_transform), storage.data() + 0x40a8 - kPlatformShift);
    EXPECT_EQ(reinterpret_cast<std::byte*>(view.light_transform), storage.data() + 0x40ac - kPlatformShift);

    *view.time = 4.5;
    double stored_time;
    std::memcpy(&stored_time, storage.data() + 0x8 - kPlatformShift, sizeof(stored_time));

    EXPECT_DOUBLE_EQ(stored_time, 4.5);

    nitroapi::StudioGameRendererView game = data.GetGameRendererView(reinterpret_cast<GameRenderer_t*>(receiver));
    game.SetPreviousGait(0, 6);
    game.SetPreviousGait(1, 3);
    game.SetPreviousGait(32, 7);
    int observed_word;
    std::memcpy(&observed_word, storage.data() + 0x40ac - kPlatformShift, sizeof(observed_word));

    EXPECT_EQ(observed_word, 6);
    EXPECT_EQ(game.get_previous_gait(1), 3);
    EXPECT_EQ(game.get_previous_gait(32), 7);
}

TEST(StudioAnimationBindings, FxDispatchUsesTheCurrentReceiverVtableAndCallingConvention)
{
    std::array<void*, 26> table{};
    table[kFxSlot] = reinterpret_cast<void*>(&FxTransform);
    void** table_pointer = table.data();
    Renderer_t* receiver = reinterpret_cast<Renderer_t*>(&table_pointer);
    goldsrc::cl_entity_t entity{};
    entity.index = 37;
    nitroapi::StudioMatrix_t matrix{};
    nitroapi::StudioAnimationData data;
    nitroapi::StudioAnimation_InitializeAccess(data);

    data.StudioFxTransform(receiver, &entity, &matrix);

    EXPECT_EQ(g_FxReceiver, receiver);
    EXPECT_FLOAT_EQ(matrix[0][3], 37.0f);
}

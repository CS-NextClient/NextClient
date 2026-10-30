#include <gtest/gtest.h>

#include <utility>
#include <vector>

#include "graphics/render/StudioQueue.h"

namespace
{
    struct QueueObservations
    {
        std::vector<std::pair<render::StudioFlushReason, size_t>> flushes;
        std::vector<render::StudioImmediateReason> immediate;
    };

    void ObserveFlush(void* user, render::StudioFlushReason reason, size_t packets)
    {
        static_cast<QueueObservations*>(user)->flushes.emplace_back(reason, packets);
    }

    void ObserveImmediate(void* user, render::StudioImmediateReason reason)
    {
        static_cast<QueueObservations*>(user)->immediate.push_back(reason);
    }

    class RecordingStudioSink : public render::StudioSubmissionSink
    {
    public:
        int begins{};
        int ends{};
        std::vector<render::StudioDrawPacket> draws;

        void Begin() override
        {
            begins++;
        }

        void Draw(const render::StudioDrawPacket& packet) override
        {
            draws.push_back(packet);
        }

        void End() override
        {
            ends++;
        }
    };

    class StudioQueuePassTest : public testing::TestWithParam<bool>
    {
    protected:
        void BeginPass(render::StudioDrawQueue& queue, bool enabled)
        {
            if (GetParam())
            {
                queue.BeginTransparentPass(enabled);
            }
            else
            {
                queue.BeginOpaquePass(enabled);
            }
        }

        void EndPass(render::StudioDrawQueue& queue)
        {
            if (GetParam())
            {
                queue.EndTransparentPass();
            }
            else
            {
                queue.EndOpaquePass();
            }
        }
    };

    render::StudioDrawPacket MakePacket(unsigned int vao, float bone)
    {
        render::StudioDrawPacket packet;
        packet.vao = vao;
        packet.bones[0] = bone;
        packet.meshes.emplace_back();
        packet.meshes.back().index_count = 3;

        return packet;
    }
} // namespace

TEST_P(StudioQueuePassTest, ConsecutiveModelsShareScopeAndPreserveBrushBoundary)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    queue.Submit(MakePacket(1, 10));
    queue.Submit(MakePacket(2, 20));
    EXPECT_TRUE(sink.draws.empty());

    queue.Flush(render::StudioFlushReason::Brush);
    ASSERT_EQ(sink.draws.size(), 2u);
    EXPECT_EQ(sink.draws[0].vao, 1u);
    EXPECT_EQ(sink.draws[1].vao, 2u);
    EXPECT_EQ(sink.begins, 1);
    EXPECT_EQ(sink.ends, 1);

    queue.Submit(MakePacket(3, 30));
    queue.Submit(MakePacket(4, 40));
    EndPass(queue);
    ASSERT_EQ(sink.draws.size(), 4u);
    EXPECT_EQ(sink.draws[2].vao, 3u);
    EXPECT_EQ(sink.draws[3].vao, 4u);
    EXPECT_EQ(sink.begins, 2);
    EXPECT_EQ(queue.stats().flushes[static_cast<size_t>(render::StudioFlushReason::Brush)], 1u);
}

TEST_P(StudioQueuePassTest, PacketsOwnPoseMaterialsAndRasterInputs)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    render::StudioDrawPacket first = MakePacket(1, 11);
    first.lighting.ambient = 10;
    first.blend = 0.5f;
    first.modelview[12] = 7;
    first.meshes[0].texture = 123;
    first.meshes[0].material.emplace();
    first.meshes[0].material->program = "first";
    queue.Submit(first);

    first.bones[0] = 22;
    first.lighting.ambient = 20;
    first.blend = 1;
    first.modelview[12] = 8;
    first.meshes[0].texture = 456;
    first.meshes[0].material->program = "second";
    queue.Submit(std::move(first));
    EndPass(queue);

    ASSERT_EQ(sink.draws.size(), 2u);
    EXPECT_EQ(sink.draws[0].bones[0], 11);
    EXPECT_EQ(sink.draws[1].bones[0], 22);
    EXPECT_EQ(sink.draws[0].lighting.ambient, 10);
    EXPECT_EQ(sink.draws[0].blend, 0.5f);
    EXPECT_EQ(sink.draws[0].modelview[12], 7);
    EXPECT_EQ(sink.draws[0].meshes[0].texture, 123u);
    EXPECT_EQ(sink.draws[0].meshes[0].material->program, "first");
}

TEST_P(StudioQueuePassTest, BaseShellAndNextEntityRetainSubmissionOrder)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    render::StudioDrawPacket base = MakePacket(1, 1);
    render::StudioDrawPacket shell = MakePacket(1, 1);
    shell.glow_scale = 3;
    shell.meshes[0].additive = true;
    queue.Submit(std::move(base));
    queue.Submit(std::move(shell));
    queue.Submit(MakePacket(2, 2));
    EndPass(queue);

    ASSERT_EQ(sink.draws.size(), 3u);
    EXPECT_EQ(sink.draws[0].glow_scale, 0);
    EXPECT_EQ(sink.draws[1].glow_scale, 3);
    EXPECT_TRUE(sink.draws[1].meshes[0].additive);
    EXPECT_EQ(sink.draws[2].vao, 2u);
}

TEST_P(StudioQueuePassTest, OutsideOpaqueAndDisabledQueueSubmitImmediately)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    queue.Submit(MakePacket(1, 1));
    BeginPass(queue, false);
    queue.Submit(MakePacket(2, 2));
    queue.Submit(MakePacket(3, 3));
    EndPass(queue);
    queue.Submit(MakePacket(4, 4));
    EXPECT_EQ(sink.draws.size(), 4u);
    EXPECT_EQ(sink.begins, 4);
    EXPECT_EQ(sink.ends, 4);
}

TEST_P(StudioQueuePassTest, CapacityFlushesBeforeNextPacketWithoutReordering)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    for (unsigned int i = 0; i < render::kMaxStudioPackets + 1; i++)
    {
        queue.Submit(MakePacket(i, static_cast<float>(i)));
    }

    EXPECT_EQ(sink.draws.size(), render::kMaxStudioPackets);
    EndPass(queue);
    ASSERT_EQ(sink.draws.size(), render::kMaxStudioPackets + 1);
    for (unsigned int i = 0; i < sink.draws.size(); i++)
    {
        EXPECT_EQ(sink.draws[i].vao, i);
    }
    EXPECT_EQ(sink.begins, 2);
}

TEST_P(StudioQueuePassTest, EveryBarrierDrainsAndEmptyFlushDoesNotOpenScope)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    for (size_t i = 0; i < static_cast<size_t>(render::StudioFlushReason::Count); i++)
    {
        render::StudioFlushReason reason = static_cast<render::StudioFlushReason>(i);
        queue.Submit(MakePacket(static_cast<unsigned int>(i), 1));
        queue.Flush(reason);
        queue.Flush(reason);
        EXPECT_EQ(sink.draws.size(), i + 1);
        EXPECT_EQ(sink.begins, i + 1);
    }
    EndPass(queue);
}

TEST_P(StudioQueuePassTest, ResetDiscardsResourcesAndDisablesDeferralWithoutDrawing)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    queue.Submit(MakePacket(1, 1));
    queue.Reset();
    queue.Flush(render::StudioFlushReason::Resource);
    EXPECT_TRUE(sink.draws.empty());
    EXPECT_EQ(sink.begins, 0);
    queue.Submit(MakePacket(2, 2));
    ASSERT_EQ(sink.draws.size(), 1u);
    EXPECT_EQ(sink.draws[0].vao, 2u);
}

TEST_P(StudioQueuePassTest, ClientCallbackPausesDeferralUntilItsOwnDrawingFinishes)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    queue.Submit(MakePacket(1, 1));
    bool deferred = queue.Pause();
    ASSERT_EQ(sink.draws.size(), 1u);
    queue.Submit(MakePacket(2, 2));
    ASSERT_EQ(sink.draws.size(), 2u);
    queue.Resume(deferred);
    queue.Submit(MakePacket(3, 3));
    EXPECT_EQ(sink.draws.size(), 2u);
    EndPass(queue);
    ASSERT_EQ(sink.draws.size(), 3u);
    EXPECT_EQ(sink.begins, 3);
}

TEST_P(StudioQueuePassTest, MutableSkinPacketIsIsolatedFromDeferredPackets)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    queue.Submit(MakePacket(1, 1));
    queue.Submit(MakePacket(2, 2), false);
    ASSERT_EQ(sink.draws.size(), 2u);
    EXPECT_EQ(sink.begins, 2);
    queue.Submit(MakePacket(3, 3));
    EXPECT_EQ(sink.draws.size(), 2u);
    EndPass(queue);
    EXPECT_EQ(sink.draws.size(), 3u);
}

TEST_P(StudioQueuePassTest, ObserverCountsOnlySubmittedRunsAndClassifiesImmediatePackets)
{
    RecordingStudioSink sink;
    QueueObservations observed;
    render::StudioDrawQueue queue(sink, {&observed, ObserveFlush, ObserveImmediate});
    BeginPass(queue, true);
    queue.Submit(MakePacket(1, 1));
    queue.Submit(MakePacket(2, 2));
    EXPECT_TRUE(observed.flushes.empty());
    queue.Flush(render::StudioFlushReason::Brush);
    ASSERT_EQ(observed.flushes.size(), 1u);
    EXPECT_EQ(observed.flushes[0].first, render::StudioFlushReason::Brush);
    EXPECT_EQ(observed.flushes[0].second, 2u);
    queue.Submit(MakePacket(3, 3));
    queue.Reset();
    EXPECT_EQ(observed.flushes.size(), 1u);

    queue.Submit(MakePacket(4, 4));
    BeginPass(queue, false);
    queue.Submit(MakePacket(5, 5));
    EndPass(queue);
    BeginPass(queue, true);
    bool deferred = queue.Pause();
    queue.Submit(MakePacket(6, 6));
    queue.Resume(deferred);
    queue.Submit(MakePacket(7, 7), false);
    EndPass(queue);
    EXPECT_EQ(
        observed.immediate,
        (std::vector<render::StudioImmediateReason>{
            render::StudioImmediateReason::OutsideOpaque,
            render::StudioImmediateReason::QueueDisabled,
            render::StudioImmediateReason::Callback,
            render::StudioImmediateReason::MutableSkin
        })
    );
    EXPECT_EQ(observed.flushes.back().first, render::StudioFlushReason::MutableSkin);
    EXPECT_EQ(observed.flushes.back().second, 1u);
}

TEST_P(StudioQueuePassTest, RecyclesMeshStorageOnlyAfterItsDrawsComplete)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    render::StudioDrawPacket packet = MakePacket(7, 12);
    packet.meshes.reserve(32);
    packet.meshes[0].material.emplace();
    packet.meshes[0].material->program = "owned";
    const render::StudioQueuedMesh* storage = packet.meshes.data();
    queue.Submit(std::move(packet));
    EXPECT_EQ(queue.AcquireMeshStorage().capacity(), 0u);

    EndPass(queue);
    ASSERT_EQ(sink.draws.size(), 1u);
    EXPECT_EQ(sink.draws[0].meshes[0].material->program, "owned");
    std::vector<render::StudioQueuedMesh> recycled = queue.AcquireMeshStorage();
    ASSERT_EQ(recycled.size(), 1u);
    EXPECT_EQ(recycled.data(), storage);
    EXPECT_GE(recycled.capacity(), 32u);
    ASSERT_TRUE(recycled[0].material.has_value());
    EXPECT_EQ(recycled[0].material->program, "owned");
    EXPECT_EQ(recycled[0].program, nullptr);
    EXPECT_EQ(recycled[0].material_bindings, nullptr);
    EXPECT_EQ(recycled[0].texture, 0u);
}

TEST_P(StudioQueuePassTest, ResetReleasesRecycledMeshStorageAndDiscardsPendingDraws)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    queue.Submit(MakePacket(1, 1));
    std::vector<render::StudioQueuedMesh> recycled = queue.AcquireMeshStorage();
    ASSERT_GT(recycled.capacity(), 0u);
    recycled.emplace_back();
    render::StudioDrawPacket packet;
    packet.meshes = std::move(recycled);
    queue.Submit(std::move(packet));
    BeginPass(queue, true);
    queue.Submit(MakePacket(2, 2));
    queue.Reset();
    EXPECT_EQ(queue.AcquireMeshStorage().capacity(), 0u);
    EXPECT_EQ(sink.draws.size(), 2u);
}

TEST_P(StudioQueuePassTest, OversizedMeshStorageDoesNotRemainInTheReusePool)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    render::StudioDrawPacket packet = MakePacket(3, 3);
    packet.meshes.reserve(32768);
    queue.Submit(std::move(packet));
    ASSERT_EQ(sink.draws.size(), 1u);
    EXPECT_EQ(queue.AcquireMeshStorage().capacity(), 0u);
}

TEST_P(StudioQueuePassTest, BoundsTheCombinedCapacityOfRecycledMeshStorage)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    BeginPass(queue, true);
    for (int index = 0; index < 128; ++index)
    {
        render::StudioDrawPacket packet = MakePacket(1, 1);
        packet.meshes.reserve(256);
        queue.Submit(std::move(packet));
    }
    EndPass(queue);
    size_t bytes = 0;
    size_t reused = 0;
    for (;;)
    {
        std::vector<render::StudioQueuedMesh> meshes = queue.AcquireMeshStorage();
        if (!meshes.capacity())
        {
            break;
        }
        bytes += meshes.capacity() * sizeof(render::StudioQueuedMesh);
        ++reused;
    }
    EXPECT_GT(bytes, 0u);
    EXPECT_LE(bytes, 1024u * 1024u);
    EXPECT_LT(reused, 128u);
    EXPECT_EQ(sink.draws.size(), 128u);
}

namespace
{
    ncl_material::Material ReuseMaterial()
    {
        ncl_material::Material material;
        material.program.assign(96, 'p');
        for (int index = 0; index < 3; ++index)
        {
            ncl_material::Param param;
            param.name = std::string(80, 'n') + std::to_string(index);
            param.value.texture_ref.assign(200, 't');
            param.value.scalar = static_cast<float>(index);
            material.params.push_back(std::move(param));
        }
        ncl_material::OverlaySlot overlay;
        overlay.name.assign(80, 'o');
        overlay.rotation = 23;
        material.overlays.push_back(std::move(overlay));
        return material;
    }
} // namespace

TEST_P(StudioQueuePassTest, LiveMaterialStorageReusesNestedCapacitiesAndOwnsEveryPublishedValue)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    render::StudioDrawPacket packet = MakePacket(1, 1);
    packet.meshes[0].material = ReuseMaterial();
    packet.meshes[0].program = reinterpret_cast<const ncl_gl::Program*>(1);
    packet.meshes[0].material_bindings = reinterpret_cast<const render::StudioMaterialBindings*>(2);
    packet.meshes[0].texture = 99;
    packet.meshes[0].images.material[0] = 89;
    packet.meshes[0].images.material_count = 1;
    packet.meshes[0].images.coverage_dissolve_map = 90;
    const char* program = packet.meshes[0].material->program.data();
    const char* name = packet.meshes[0].material->params[0].name.data();
    const char* texture_ref = packet.meshes[0].material->params[0].value.texture_ref.data();
    const char* overlay = packet.meshes[0].material->overlays[0].name.data();
    queue.Submit(std::move(packet));
    std::vector<render::StudioQueuedMesh> storage = queue.AcquireMeshStorage();
    ASSERT_EQ(storage.size(), 1u);
    EXPECT_EQ(storage[0].program, nullptr);
    EXPECT_EQ(storage[0].material_bindings, nullptr);
    EXPECT_EQ(storage[0].texture, 0u);
    EXPECT_EQ(storage[0].images.material_count, 0);
    EXPECT_EQ(storage[0].images.material[0], 0u);
    EXPECT_EQ(storage[0].images.coverage_dissolve_map, 0u);
    EXPECT_EQ(storage[0].images.manifest, nullptr);
    ASSERT_TRUE(storage[0].material);
    const ncl_material::Material replacement = ReuseMaterial();
    *storage[0].material = replacement;
    EXPECT_EQ(storage[0].material->program.data(), program);
    EXPECT_EQ(storage[0].material->params[0].name.data(), name);
    EXPECT_EQ(storage[0].material->params[0].value.texture_ref.data(), texture_ref);
    EXPECT_EQ(storage[0].material->overlays[0].name.data(), overlay);
    EXPECT_EQ(sink.draws[0].meshes[0].material->overlays[0].rotation, 23);
    storage[0].material->params[0].value.texture_ref = "mutated";
    EXPECT_EQ(sink.draws[0].meshes[0].material->params[0].value.texture_ref, std::string(200, 't'));
}

TEST_P(StudioQueuePassTest, MeshPayloadBudgetIncludesInactiveStringsAndReaccountsChangedCapacities)
{
    render::StudioMeshStorage storage(1024 * 1024);
    std::vector<render::StudioQueuedMesh> meshes(1);
    meshes[0].material = ReuseMaterial();
    meshes[0].material->params[0].value.type = ncl_material::ParamType::Scalar;
    meshes[0].material->params[0].value.texture_ref.assign(2 * 1024 * 1024, 'x');
    storage.Recycle(meshes);
    EXPECT_EQ(storage.get_retained_bytes(), 0u);
    EXPECT_TRUE(storage.Acquire().empty());
    meshes[0].material = ReuseMaterial();
    storage.Recycle(meshes);
    ASSERT_GT(storage.get_retained_bytes(), sizeof(render::StudioQueuedMesh));
    EXPECT_LE(storage.get_retained_bytes(), 1024u * 1024u);
    meshes = storage.Acquire();
    EXPECT_EQ(storage.get_retained_bytes(), 0u);
    ASSERT_EQ(meshes.size(), 1u);
    meshes[0].material->params[0].value.texture_ref.assign(4096, 'y');
    storage.Recycle(meshes);
    EXPECT_GT(storage.get_retained_bytes(), 4096u);
    storage.Clear();
    EXPECT_EQ(storage.get_retained_bytes(), 0u);
    EXPECT_EQ(storage.Acquire().capacity(), 0u);
}

TEST_P(StudioQueuePassTest, RetirementCannotBeAcquiredFromAnySubmissionOrFlushCallback)
{
    struct Sink : RecordingStudioSink
    {
        render::StudioDrawQueue* queue{};
        void Check()
        {
            EXPECT_EQ(queue->AcquireMeshStorage().capacity(), 0u);
        }
        void Begin() override
        {
            Check();
            RecordingStudioSink::Begin();
        }
        void Draw(const render::StudioDrawPacket& packet) override
        {
            Check();
            RecordingStudioSink::Draw(packet);
        }
        void End() override
        {
            Check();
            RecordingStudioSink::End();
        }
    } sink;
    render::StudioQueueCallbacks callbacks;
    callbacks.user = &sink;
    callbacks.note_flush = [](void* user, render::StudioFlushReason, size_t) { static_cast<Sink*>(user)->Check(); };
    render::StudioDrawQueue queue(sink, callbacks);
    sink.queue = &queue;
    queue.Submit(MakePacket(1, 1));
    EXPECT_GT(queue.AcquireMeshStorage().capacity(), 0u);
}

TEST(StudioQueueTest, TransparentPassKeepsViewCountersAndEndsBeforeViewmodel)
{
    RecordingStudioSink sink;
    render::StudioDrawQueue queue(sink);
    queue.BeginOpaquePass(true);
    queue.Submit(MakePacket(1, 1));
    queue.EndOpaquePass();

    queue.BeginTransparentPass(true);
    queue.Submit(MakePacket(2, 2));
    queue.Submit(MakePacket(3, 3));
    EXPECT_EQ(sink.draws.size(), 1u);
    queue.EndTransparentPass();

    ASSERT_EQ(sink.draws.size(), 3u);
    EXPECT_EQ(sink.draws[1].vao, 2u);
    EXPECT_EQ(sink.draws[2].vao, 3u);
    EXPECT_EQ(queue.stats().packets, 3u);
    EXPECT_EQ(sink.begins, 2);
    EXPECT_EQ(sink.ends, 2);

    queue.Submit(MakePacket(4, 4));
    ASSERT_EQ(sink.draws.size(), 4u);
    EXPECT_EQ(sink.draws.back().vao, 4u);
    EXPECT_EQ(sink.begins, 3);
}

INSTANTIATE_TEST_SUITE_P(EntityPasses, StudioQueuePassTest, testing::Bool());

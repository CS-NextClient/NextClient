#include "graphics/render/StudioQueue.h"

#include <algorithm>
#include <cassert>
#include <utility>

namespace render
{
    namespace
    {
        bool Charge(size_t count, size_t width, size_t limit, size_t& bytes)
        {
            if (count > (limit - bytes) / width)
            {
                return false;
            }

            bytes += count * width;
            return true;
        }
    } // namespace

    bool StudioMeshStorage::Measure(const std::vector<StudioQueuedMesh>& meshes, size_t limit, size_t& bytes)
    {
        bytes = 0;
        if (!Charge(meshes.capacity(), sizeof(StudioQueuedMesh), limit, bytes))
        {
            return false;
        }

        for (const StudioQueuedMesh& mesh : meshes)
        {
            if (!mesh.material)
            {
                continue;
            }

            const ncl_material::Material& material = *mesh.material;
            if (!Charge(material.program.capacity(), 1, limit, bytes) || !Charge(1, 1, limit, bytes) ||
                !Charge(material.params.capacity(), sizeof(ncl_material::Param), limit, bytes) ||
                !Charge(material.overlays.capacity(), sizeof(ncl_material::OverlaySlot), limit, bytes))
            {
                return false;
            }

            for (const ncl_material::Param& param : material.params)
            {
                if (!Charge(param.name.capacity(), 1, limit, bytes) || !Charge(1, 1, limit, bytes) ||
                    !Charge(param.value.texture_ref.capacity(), 1, limit, bytes) || !Charge(1, 1, limit, bytes))
                {
                    return false;
                }
            }

            for (const ncl_material::OverlaySlot& overlay : material.overlays)
            {
                if (!Charge(overlay.name.capacity(), 1, limit, bytes) || !Charge(1, 1, limit, bytes))
                {
                    return false;
                }
            }
        }

        return true;
    }

    std::vector<StudioQueuedMesh> StudioMeshStorage::Acquire()
    {
        if (retained_.empty())
        {
            return {};
        }

        Retained& retained = retained_.back();
        retained_bytes_ -= retained.bytes;
        std::vector<StudioQueuedMesh> meshes = std::move(retained.meshes);
        retained_.pop_back();
        return meshes;
    }

    void StudioMeshStorage::Recycle(std::vector<StudioQueuedMesh>& meshes)
    {
        for (StudioQueuedMesh& mesh : meshes)
        {
            mesh.program = nullptr;
            mesh.material_bindings = nullptr;
            mesh.texture = 0;
            mesh.images = StudioMeshImages{};
        }

        size_t bytes = 0;
        if (Measure(meshes, budget_ - retained_bytes_, bytes) && bytes != 0)
        {
            retained_.push_back({std::move(meshes), bytes});
            retained_bytes_ += bytes;
        }
    }

    void StudioMeshStorage::Clear()
    {
        retained_.clear();
        retained_bytes_ = 0;
    }

    StudioDrawQueue::StudioDrawQueue(StudioSubmissionSink& sink, StudioQueueCallbacks callbacks) :
        sink_(sink),
        callbacks_(callbacks)
    {}

    void StudioDrawQueue::BeginOpaquePass(bool enabled)
    {
        assert(packets_.empty());
        stats_ = StudioQueueStats{};
        deferred_ = enabled;
        pass_active_ = true;
        queue_enabled_ = enabled;
    }

    void StudioDrawQueue::EndOpaquePass()
    {
        Flush(StudioFlushReason::StageEnd);
        deferred_ = false;
        pass_active_ = false;
    }

    void StudioDrawQueue::BeginTransparentPass(bool enabled)
    {
        assert(packets_.empty());
        deferred_ = enabled;
        pass_active_ = true;
        queue_enabled_ = enabled;
    }

    void StudioDrawQueue::EndTransparentPass()
    {
        Flush(StudioFlushReason::StageEnd);
        deferred_ = false;
        pass_active_ = false;
    }

    std::vector<StudioQueuedMesh> StudioDrawQueue::AcquireMeshStorage()
    {
        return mesh_storage_.Acquire();
    }

    void StudioDrawQueue::Submit(const StudioDrawPacket& packet, bool allow_deferred)
    {
        Submit(StudioDrawPacket(packet), allow_deferred);
    }

    void StudioDrawQueue::Submit(StudioDrawPacket&& packet, bool allow_deferred)
    {
        if (packet.meshes.empty())
        {
            return;
        }

        if (!allow_deferred)
        {
            Flush(StudioFlushReason::MutableSkin);
        }

        if (packets_.size() == kMaxStudioPackets)
        {
            Flush(StudioFlushReason::Capacity);
        }

        packets_.push_back(std::move(packet));
        stats_.packets++;
        stats_.max_run = std::max(stats_.max_run, packets_.size());

        if (!allow_deferred || !deferred_)
        {
            if (callbacks_.note_immediate != nullptr)
            {
                StudioImmediateReason reason = StudioImmediateReason::Callback;

                if (!allow_deferred)
                {
                    reason = StudioImmediateReason::MutableSkin;
                }
                else if (!pass_active_)
                {
                    reason = StudioImmediateReason::OutsideOpaque;
                }
                else if (!queue_enabled_)
                {
                    reason = StudioImmediateReason::QueueDisabled;
                }

                callbacks_.note_immediate(callbacks_.user, reason);
            }

            Flush(allow_deferred ? StudioFlushReason::Immediate : StudioFlushReason::MutableSkin);
        }
    }

    void StudioDrawQueue::Flush(StudioFlushReason reason)
    {
        if (packets_.empty())
        {
            return;
        }

        stats_.flushes[static_cast<size_t>(reason)]++;

        sink_.Begin();

        sink_.DrawRun(packets_);

        sink_.End();

        if (callbacks_.note_flush != nullptr)
        {
            callbacks_.note_flush(callbacks_.user, reason, packets_.size());
        }

        for (StudioDrawPacket& packet : packets_)
        {
            mesh_storage_.Recycle(packet.meshes);
        }

        packets_.clear();
    }

    void StudioDrawQueue::Reset()
    {
        packets_.clear();
        mesh_storage_.Clear();
        deferred_ = false;
        pass_active_ = false;
        queue_enabled_ = false;
    }

    bool StudioDrawQueue::Pause()
    {
        Flush(StudioFlushReason::Client);
        bool deferred = deferred_;
        deferred_ = false;

        return deferred;
    }

    void StudioDrawQueue::Resume(bool deferred)
    {
        assert(packets_.empty());
        deferred_ = deferred;
    }

    void StudioDrawQueue::NoteProgramBind()
    {
        stats_.program_binds++;
    }

    const StudioQueueStats& StudioDrawQueue::stats() const
    {
        return stats_;
    }
} // namespace render

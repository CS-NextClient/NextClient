#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <goldsrc/format/studio.h>

namespace render
{
    inline constexpr size_t kStudioBoneFloats = goldsrc::MAXSTUDIOBONES * 12;

    class StudioPosePool;

    // Capture, handle operations, and reset belong to the renderer's owning thread.
    class StudioPose
    {
    private:
        friend class StudioPosePool;

        struct Slot
        {
            std::array<float, kStudioBoneFloats> values;
            StudioPosePool* owner{};
            Slot* next{};
            size_t references{};
        };

        Slot* slot_{};

    public:
        StudioPose() = default;

        StudioPose(const StudioPose& other) :
            slot_(other.slot_)
        {
            if (slot_ != nullptr)
            {
                ++slot_->references;
            }
        }

        StudioPose(StudioPose&& other) noexcept :
            slot_(std::exchange(other.slot_, nullptr))
        {}

        ~StudioPose()
        {
            Release();
        }

        StudioPose& operator=(const StudioPose& other)
        {
            StudioPose copy(other);
            std::swap(slot_, copy.slot_);

            return *this;
        }

        StudioPose& operator=(StudioPose&& other) noexcept
        {
            StudioPose moved(std::move(other));
            std::swap(slot_, moved.slot_);

            return *this;
        }

        const float* data() const
        {
            return slot_ != nullptr ? slot_->values.data() : nullptr;
        }

        size_t size() const
        {
            return kStudioBoneFloats;
        }

        const float& operator[](size_t index) const
        {
            assert(slot_ != nullptr && index < kStudioBoneFloats);

            return slot_->values[index];
        }

        const float& back() const
        {
            return (*this)[kStudioBoneFloats - 1];
        }

    private:
        explicit StudioPose(Slot* slot) :
            slot_(slot)
        {}

        void Release();
    };

    class StudioPosePool
    {
    private:
        friend class StudioPose;

        std::vector<std::unique_ptr<StudioPose::Slot>> slots_;
        StudioPose::Slot* free_{};

    public:
        StudioPosePool() = default;

        ~StudioPosePool()
        {
            Reset();
        }

        StudioPosePool(const StudioPosePool&) = delete;
        StudioPosePool& operator=(const StudioPosePool&) = delete;

        StudioPose Capture(const float* values)
        {
            assert(values != nullptr);

            StudioPose::Slot* slot = free_;

            if (slot != nullptr)
            {
                free_ = slot->next;
            }
            else
            {
                std::unique_ptr<StudioPose::Slot> owned = std::make_unique<StudioPose::Slot>();
                slot = owned.get();
                slots_.push_back(std::move(owned));
                slot->owner = this;
            }

            std::memcpy(slot->values.data(), values, sizeof(slot->values));
            slot->next = nullptr;
            slot->references = 1;

            return StudioPose(slot);
        }

        void Reset()
        {
            free_ = nullptr;

            for (std::unique_ptr<StudioPose::Slot>& slot : slots_)
            {
                if (slot->references != 0)
                {
                    slot->owner = nullptr;
                    slot.release();
                }
            }

            slots_.clear();
        }
    };

    inline void StudioPose::Release()
    {
        if (slot_ == nullptr || --slot_->references != 0)
        {
            return;
        }

        if (slot_->owner != nullptr)
        {
            slot_->next = slot_->owner->free_;
            slot_->owner->free_ = slot_;
        }
        else
        {
            delete slot_;
        }
    }
} // namespace render

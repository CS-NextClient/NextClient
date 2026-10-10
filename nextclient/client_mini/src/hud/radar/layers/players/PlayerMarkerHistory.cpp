#include "PlayerMarkerHistory.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr float kSampleInterval = 0.05f;
    constexpr int kConsoleLineSize = 512;
    static_assert(kRadarMaxPlayers <= 64);
} // namespace

void PlayerMarkerHistory::Record(Frame frame, std::span<const std::uint32_t, kRadarMaxPlayers> sequences)
{
    if (count_ != 0 && (frame.time < last_time_ || frame.time - last_time_ >= kMaxSamples * kSampleInterval))
    {
        Reset();
    }

    std::uint64_t updated = 0;

    for (int player = 0; player < kRadarMaxPlayers; player++)
    {
        if (sequences[player] != sequences_[player])
        {
            updated |= std::uint64_t{1} << player;
            sequences_[player] = sequences[player];
        }
    }

    std::uint64_t lost = last_drawn_ & ~frame.drawn;

    if (count_ == 0 || frame.time - bucket_time_ >= kSampleInterval)
    {
        samples_[head_] = {frame, lost, updated, frame.fading, frame.jobs, frame.fade_in, frame.overlap, frame.snapshot_steps};
        head_ = (head_ + 1) % kMaxSamples;
        count_ = std::min(count_ + 1, kMaxSamples);
        bucket_time_ = frame.time;
    }
    else
    {
        Sample& sample = samples_[(head_ - 1 + kMaxSamples) % kMaxSamples];
        sample.frame = frame;
        sample.lost |= lost;
        sample.updated |= updated;
        sample.fading |= frame.fading;
        sample.fade_in |= frame.fade_in;
        sample.overlap |= frame.overlap;
        sample.snapshot_steps |= frame.snapshot_steps;
        sample.min_jobs = std::min(sample.min_jobs, frame.jobs);
    }

    last_drawn_ = frame.drawn;
    last_time_ = frame.time;
}

int PlayerMarkerHistory::CopySamples(std::span<Sample> out) const
{
    int count = std::min<int>((int)out.size(), count_);

    for (int index = 0; index < count; index++)
    {
        out[index] = samples_[(head_ - count + index + kMaxSamples) % kMaxSamples];
    }

    return count;
}

void PlayerMarkerHistory::Reset()
{
    *this = PlayerMarkerHistory{};
}

void PlayerMarkerHistory_PrintSample(const PlayerMarkerHistory::Sample& sample, void (*print)(const char*, ...))
{
    const PlayerMarkerHistory::Frame& frame = sample.frame;
    // Preformat masks before crossing the engine's legacy variadic formatting boundary.
    char text[kConsoleLineSize];
    std::snprintf(
        text,
        sizeof(text),
        "history t=%.3f msg=%d layer=%d resets=%d jobs=%d min_jobs=%d batches=%d quads=%d "
        "drawn=%016llx snapshot=%016llx broadcast=%016llx updated=%016llx fading=%016llx lost=%016llx "
        "fade_in=%016llx overlap=%016llx snapshot_steps=%016llx\n",
        frame.time,
        frame.message,
        frame.layer,
        frame.resets,
        frame.jobs,
        sample.min_jobs,
        frame.batches,
        frame.quads,
        (unsigned long long)frame.drawn,
        (unsigned long long)frame.snapshot,
        (unsigned long long)frame.broadcast,
        (unsigned long long)sample.updated,
        (unsigned long long)sample.fading,
        (unsigned long long)sample.lost,
        (unsigned long long)sample.fade_in,
        (unsigned long long)sample.overlap,
        (unsigned long long)sample.snapshot_steps
    );
    print("%s", text);
}

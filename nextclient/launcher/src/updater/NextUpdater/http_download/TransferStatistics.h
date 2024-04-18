#pragma once

#include <chrono>
#include <cassert>

template<class TNum>
class TransferStatistics
{
public:
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

private:
    time_point last_update_{};
    TNum last_measure_{};
    TNum speed_{};

public:
    void UpdateTransferredBytes(TNum total_downloaded_bytes, time_point time)
    {
        using namespace std::chrono;

        assert(last_measure_ <= total_downloaded_bytes);
        assert(last_update_ <= time);

        TNum bytes_inc = total_downloaded_bytes - last_measure_;
        auto time_diff = duration_cast<milliseconds>(time - last_update_);

        float seconds_diff = (float)time_diff.count() / 1000.0F;
        if (seconds_diff != 0)
        {
            speed_ = bytes_inc / seconds_diff;
        }

        last_update_ = time;
        last_measure_ = total_downloaded_bytes;
    }

    // bytes per sec
    [[nodiscard]] TNum get_speed() const noexcept
    {
        return speed_;
    }

    [[nodiscard]] TNum get_downloaded_bytes() const noexcept
    {
        return last_measure_;
    }
};

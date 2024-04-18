#pragma once

#include <chrono>

template<class TNum>
class TransferStatistics
{
public:
    using time_point = std::chrono::time_point<std::chrono::system_clock>;

private:
    time_point last_update_{};
    TNum last_measure_{};
    TNum speed_ = 0;

public:
    void UpdateTransferredBytes(TNum bytes, time_point time)
    {
        using namespace std::chrono;

        if (last_measure_ > bytes || last_update_ > time)
            return;

        TNum bytes_inc = bytes - last_measure_;
        auto time_diff = duration_cast<milliseconds>(time - last_update_);

        float seconds_diff = (float)time_diff.count() / 1000.0F;
        if (seconds_diff != 0)
        {
            speed_ = bytes_inc / seconds_diff;
        }

        last_update_ = time;
        last_measure_ = bytes;
    }

    // bytes per sec
    [[nodiscard]] TNum get_speed() const noexcept
    {
        return speed_;
    }
};
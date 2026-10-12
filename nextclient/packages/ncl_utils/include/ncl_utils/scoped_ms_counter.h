#pragma once

#include <chrono>

namespace ncl_utils
{
    // Adds the wall time the scope lived, in milliseconds, to an accumulator.
    class ScopedMsCounter
    {
    public:
        explicit ScopedMsCounter(float& total) : total_(total), start_(std::chrono::steady_clock::now()) {}

        ScopedMsCounter(const ScopedMsCounter&) = delete;
        ScopedMsCounter& operator=(const ScopedMsCounter&) = delete;

        ~ScopedMsCounter()
        {
            std::chrono::duration<float, std::milli> elapsed = std::chrono::steady_clock::now() - start_;
            total_ += elapsed.count();
        }

    private:
        float& total_;
        std::chrono::steady_clock::time_point start_;
    };
}

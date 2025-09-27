#pragma once
#include <atomic>
#include <memory>
#include <tuple>

#include "exceptions/OperationCanceledException.h"

namespace taskcoro
{
    // TODO Separate into CancellationToken and CancellationTokenSource
    class CancellationToken
    {
        // highest 2 bytes - reason, lowest 2 bytes - flag
        std::atomic<int32_t> cancel_;

        explicit CancellationToken() = default;
        CancellationToken(CancellationToken const&) = delete;
        CancellationToken(CancellationToken &&) = delete;

    public:
        [[nodiscard]] bool IsCanceled() const { return cancel_; }

        [[nodiscard]] std::tuple<bool, int16_t> GetCancelReasonAndCancelFlag() const
        {
            int32_t cancel = cancel_.load();
            return std::make_tuple(cancel, cancel >> 16);
        }

        void SetCanceled() { cancel_ = 1; }

        void SetCanceledWithReason(int16_t reason) { cancel_ = (reason << 16) | 1; }

        void ThrowIfCancelled()
        {
            if (IsCanceled())
            {
                throw OperationCanceledException("Operation canceled");
            }
        }

        static std::shared_ptr<CancellationToken> Create()
        {
            return std::shared_ptr<CancellationToken>(new CancellationToken());
        }
    };
}

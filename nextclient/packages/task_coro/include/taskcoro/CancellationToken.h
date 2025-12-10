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
        std::atomic<int32_t> cancel_{0};

    protected:
        explicit CancellationToken() = default;

    public:
        CancellationToken(CancellationToken const&) = delete;
        CancellationToken(CancellationToken &&) = delete;
        virtual ~CancellationToken() = default;

        [[nodiscard]] virtual bool IsCanceled() const;
        [[nodiscard]] virtual std::tuple<bool, int16_t> GetCancelReasonAndCancelFlag() const;
        void SetCanceled();
        void SetCanceledWithReason(int16_t reason);
        void ThrowIfCancelled();

        static std::shared_ptr<CancellationToken> Create();
    };
}

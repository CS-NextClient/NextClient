#include <taskcoro/TaskCoro.h>

using namespace taskcoro;

bool CancellationToken::IsCanceled() const
{
    return cancel_;
}

std::tuple<bool, int16_t> CancellationToken::GetCancelReasonAndCancelFlag() const
{
    int32_t cancel = cancel_.load();
    return std::make_tuple(cancel, cancel >> 16);
}

void CancellationToken::SetCanceled()
{
    cancel_ = 1;
}

void CancellationToken::SetCanceledWithReason(int16_t reason)
{
    cancel_ = (reason << 16) | 1;
}

void CancellationToken::ThrowIfCancelled()
{
    if (IsCanceled())
    {
        throw OperationCanceledException("Operation cancelled");
    }
}

std::shared_ptr<CancellationToken> CancellationToken::Create()
{
    return std::shared_ptr<CancellationToken>(new CancellationToken());
}

#include <taskcoro/TaskCoro.h>

using namespace taskcoro;

LinkedCancellationToken::LinkedCancellationToken(std::vector<std::shared_ptr<CancellationToken>> tokens) :
    CancellationToken(),
    tokens_(std::move(tokens))
{
}

bool LinkedCancellationToken::IsCanceled() const
{
    if (CancellationToken::IsCanceled())
    {
        return true;
    }

    for (const auto& token : tokens_)
    {
        if (token && token->IsCanceled())
        {
            return true;
        }
    }
    return false;
}

std::tuple<bool, int16_t> LinkedCancellationToken::GetCancelReasonAndCancelFlag() const
{
    auto result = CancellationToken::GetCancelReasonAndCancelFlag();
    if (std::get<0>(result))
    {
        return result;
    }

    for (const auto& token : tokens_)
    {
        if (token)
        {
            result = token->GetCancelReasonAndCancelFlag();
            if (std::get<0>(result))
            {
                return result;
            }
        }
    }

    return { false, 0 };
}

std::shared_ptr<LinkedCancellationToken> LinkedCancellationToken::Create(
    std::vector<std::shared_ptr<CancellationToken>> tokens)
{
    return std::shared_ptr<LinkedCancellationToken>(new LinkedCancellationToken(std::move(tokens)));
}

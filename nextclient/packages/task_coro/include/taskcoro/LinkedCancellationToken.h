#pragma once

namespace taskcoro
{
    class LinkedCancellationToken : public CancellationToken
    {
        std::vector<std::shared_ptr<CancellationToken>> tokens_;

        explicit LinkedCancellationToken(std::vector<std::shared_ptr<CancellationToken>> tokens);

    public:
        [[nodiscard]] bool IsCanceled() const override;
        [[nodiscard]] std::tuple<bool, int16_t> GetCancelReasonAndCancelFlag() const override;

        static std::shared_ptr<LinkedCancellationToken> Create(std::vector<std::shared_ptr<CancellationToken>> tokens);
    };
}

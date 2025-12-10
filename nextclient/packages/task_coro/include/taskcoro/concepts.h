#pragma once

namespace taskcoro
{
    template <class T>
    concept ResultLike =
        requires(T t)
        {
            { t.status() } -> std::same_as<concurrencpp::result_status>;
            t.operator co_await();
            t.get();
        };
}

#pragma once
#include <concurrencpp/concurrencpp.h>

namespace taskcoro
{
    template <class T>
    struct unwrap_result
    {
        using type = T;
        static constexpr bool is = false;
    };

    template <class T>
    struct unwrap_result<concurrencpp::result<T>>
    {
        using type = T;
        static constexpr bool is = true;
    };

    template <class T>
    using unwrap_result_t = typename unwrap_result<T>::type;

    template <class T>
    auto forward_capture(T&& v)
    {
        if constexpr (std::is_lvalue_reference_v<T>)
        {
            return std::ref(v);
        }
        else
        {
            return std::move(v); // NOLINT(*-move-forwarding-reference)
        }
    }
}

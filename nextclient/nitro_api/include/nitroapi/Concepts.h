#pragma once

#include <type_traits>

namespace nitroapi
{
    template<typename T>
    concept FunctionPointer = std::is_pointer_v<T> && std::is_function_v<std::remove_pointer_t<T>>;

    template<typename T>
    concept Pointer = std::is_pointer_v<T>;
}
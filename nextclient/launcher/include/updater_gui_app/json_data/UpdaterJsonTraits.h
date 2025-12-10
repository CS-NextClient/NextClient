#pragma once

#include <tao/json.hpp>
#include <tao/json/contrib/traits.hpp>
#include <tao/json/from_string.hpp>

template<typename T>
struct UpdaterJsonTraits
    : public tao::json::traits<T>
{
};

using UpdaterJsonValue = tao::json::basic_value<UpdaterJsonTraits>;


template<template<typename...> class... Transformers, typename... Ts>
[[nodiscard]] UpdaterJsonValue from_string(Ts &&... ts)
{
    return tao::json::basic_from_string<UpdaterJsonTraits, Transformers...>(std::forward<Ts>(ts)...);
}
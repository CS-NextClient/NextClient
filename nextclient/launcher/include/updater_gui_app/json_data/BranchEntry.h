#pragma once

#include <string>
#include <utility>
#include "UpdaterJsonTraits.h"

struct BranchEntry
{
    std::string name{};
    std::string desc{};
    bool visible{};
};

template<>
struct UpdaterJsonTraits<BranchEntry>
{
    template<template<typename...> class Traits>
    static BranchEntry as(const tao::json::basic_value<Traits>& v)
    {
        BranchEntry result;
        const auto& object = v.get_object();
        result.name = v.at("name").template as<std::string>();
        result.desc = v.at("desc").template as<std::string>();
        result.visible = v.at("visible").template as<bool>();
        return result;
    }
};
#pragma once

#include <string>
#include "FileEntry.h"
#include "UpdaterJsonTraits.h"

struct UpdateEntry
{
    std::string hostname;
    std::vector<FileEntry> files;
};

template<>
struct UpdaterJsonTraits<UpdateEntry>
{
    template<template<typename...> class Traits>
    static UpdateEntry as(const tao::json::basic_value<Traits>& v)
    {
        UpdateEntry result;
        const auto& object = v.get_object();
        result.hostname = v.at("hostname").template as<std::string>();
        result.files = v.at("files").template as<std::vector<FileEntry>>();
        return result;
    }
};
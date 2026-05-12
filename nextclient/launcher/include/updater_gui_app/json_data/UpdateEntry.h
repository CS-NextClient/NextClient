#pragma once

#include <optional>
#include <string>
#include <vector>
#include "FileEntry.h"
#include "UpdaterJsonTraits.h"

struct UpdateEntry
{
    std::string hostname;
    std::vector<std::string> hostnames;
    std::optional<FileEntry> test;
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

        if (object.contains("hostname"))
            result.hostname = v.at("hostname").template as<std::string>();

        if (object.contains("hostnames"))
            result.hostnames = v.at("hostnames").template as<std::vector<std::string>>();

        if (object.contains("test"))
            result.test = v.at("test").template as<FileEntry>();

        result.files = v.at("files").template as<std::vector<FileEntry>>();
        return result;
    }
};
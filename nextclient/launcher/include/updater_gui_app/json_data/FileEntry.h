#pragma once

#include <string>
#include "UpdaterJsonTraits.h"

struct FileEntry
{
    std::string filename;
    std::string hash;
    size_t size{};
};

template<>
struct UpdaterJsonTraits<FileEntry>
{
    template<template<typename...> class Traits>
    static FileEntry as(const tao::json::basic_value<Traits>& v)
    {
        FileEntry result;
        result.filename = v.at("filename").template as<std::string>();
        result.hash = v.at("hash").template as<std::string>();
        result.size = v.at("size").template as<size_t>();
        return result;
    }
};
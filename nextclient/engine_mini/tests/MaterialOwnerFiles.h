#pragma once

#include <map>
#include <string>
#include <vector>

#include "graphics/render/material/MaterialDocumentStore.h"

class MaterialOwnerFiles : public render::MaterialFileBackend
{
public:
    std::map<std::string, std::string> files;
    std::vector<std::string> reads;
    std::vector<size_t> limits;
    std::vector<std::string> reports;
    std::map<std::string, render::MaterialReadStatus> read_statuses;
    std::vector<std::string> writes;

    render::MaterialReadStatus ReadFile(const char* path, std::string& text, size_t max_bytes) override
    {
        reads.emplace_back(path);
        limits.push_back(max_bytes);
        auto status = read_statuses.find(path);
        if (status != read_statuses.end())
        {
            return status->second;
        }

        auto found = files.find(path);
        if (found == files.end())
        {
            return render::MaterialReadStatus::NotFound;
        }
        if (max_bytes != 0 && found->second.size() > max_bytes)
        {
            return render::MaterialReadStatus::TooLarge;
        }

        text = found->second;
        return render::MaterialReadStatus::Ready;
    }

    bool Write(const std::string& path, const std::string& text, std::string&) override
    {
        writes.push_back(path);
        files[path] = text;
        return true;
    }

    void Report(const char* text, bool) override
    {
        reports.emplace_back(text);
    }
};

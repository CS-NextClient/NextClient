#pragma once

#include <vector>
#include <filesystem>
#include <gtest/gtest.h>
#include "test_utils.h"

class NextUpdaterTest : public testing::Test
{
    std::vector<std::filesystem::path> paths_to_remove_;

protected:
    void TearDown() override
    {
        RemovePaths();
    }

    std::filesystem::path CreateTempDir(const std::string& prefix)
    {
        auto path = ::CreateTempDir(prefix);
        paths_to_remove_.push_back(path);

        return path;
    }

    int GetFreePort()
    {
        static int port = 9873;
        return port++;
    }

private:
    void RemovePaths()
    {
        for (auto& path : paths_to_remove_)
            std::filesystem::remove_all(path);
    }
};

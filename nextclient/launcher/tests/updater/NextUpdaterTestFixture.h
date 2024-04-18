#pragma once

#include <vector>
#include <filesystem>
#include <gtest/gtest.h>
#include <easylogging++.h>
#include "test_utils.h"

class NextUpdaterTest : public testing::Test
{
    std::vector<std::filesystem::path> paths_to_remove_;

protected:
    void SetUp() override
    {
    }

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

    el::Logger* GetTestLogger(const std::string& logger_name = "test_logger")
    {
        if (!el::Loggers::hasLogger(logger_name))
            SetUpTestLogger(logger_name);

        return el::Loggers::getLogger(logger_name);
    }

    int GetFreePort() const
    {
        return 9873; // return 4; hehe
    }

private:
    void RemovePaths()
    {
        for (auto& path : paths_to_remove_)
            std::filesystem::remove_all(path);
    }

    void SetUpTestLogger(const std::string& logger_name)
    {
        el::Configurations defaultConf;
        defaultConf.setToDefault();
        defaultConf.set(el::Level::Global, el::ConfigurationType::ToFile, "false");
        defaultConf.set(el::Level::Global, el::ConfigurationType::ToStandardOutput, "false");
        el::Loggers::reconfigureLogger(logger_name, defaultConf);
    }
};
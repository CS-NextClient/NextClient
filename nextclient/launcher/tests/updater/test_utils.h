#pragma once
#include <filesystem>
#include <string>

std::filesystem::path CreateTempDir(const std::string& prefix = "");
void WriteToFile(const std::filesystem::path& path, const std::string& data);
std::string ReadFromFile(const std::filesystem::path& path);

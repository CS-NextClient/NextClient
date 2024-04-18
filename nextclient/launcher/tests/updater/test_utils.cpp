#include "test_utils.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <string>

std::string random_string(size_t length)
{
    auto randchar = []() -> char {
        const char charset[] =
                "0123456789"
                "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                "abcdefghijklmnopqrstuvwxyz";

        static std::random_device rd;
        static std::mt19937 generator(rd());
        std::uniform_int_distribution<> distrib(0, sizeof(charset) - 2);

        return charset[distrib(generator)];
    };
    std::string str(length, 0);
    std::generate_n(str.begin(), length, randchar);
    return str;
}

std::filesystem::path CreateTempDir(const std::string& prefix)
{
    std::filesystem::path base_dir = std::filesystem::temp_directory_path();
    std::filesystem::path dir;

    do {
        dir = base_dir / (prefix + random_string(10));
    } while (std::filesystem::exists(dir));

    std::filesystem::create_directories(dir);
    return dir;
}

void WriteToFile(const std::filesystem::path& path, const std::string& data)
{
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());

    std::ofstream file(path, std::ios::binary | std::ios::out);
    file << data;
}

std::string ReadFromFile(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::in);
    std::stringstream buffer;
    buffer << file.rdbuf();

    return buffer.str();
}

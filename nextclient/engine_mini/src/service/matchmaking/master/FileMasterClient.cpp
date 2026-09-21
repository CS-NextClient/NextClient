#include "FileMasterClient.h"

#include <fstream>
#include <iterator>
#include <utility>
#include <vector>
#include <filesystem>

#ifdef _WIN32
    #include <Shlobj.h>
#else
    #include <cstdlib>
#endif
#include <data_encoding/aes.h>
#include <nitro_utils/random_utils.h>

#include "service/matchmaking/master/MasterListCache.h"

using namespace taskcoro;
using namespace concurrencpp;

FileMasterClient::FileMasterClient(std::string file_name) :
    file_name_(std::move(file_name))
{ }

result<std::vector<MasterServerEntry>> FileMasterClient::GetServerListAsync(
    std::function<void(const MasterServerEntry&)> entry_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::vector<MasterServerEntry> server_list = co_await TaskCoro::RunIO([this] { return ReadFromFile(file_name_); });

    if (entry_received_callback)
    {
        for (const MasterServerEntry& entry : server_list)
        {
            entry_received_callback(entry);
        }
    }

    co_return server_list;
}

void FileMasterClient::Save(const std::vector<MasterServerEntry>& server_list)
{
    WriteToFile(file_name_, server_list);
}

std::vector<MasterServerEntry> FileMasterClient::ReadFromFile(const std::string& file_name)
{
    std::ifstream file(GetSaveFilePath(file_name), std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return MasterListCache_Decode(data);
}

void FileMasterClient::WriteToFile(const std::string& file_name, const std::vector<MasterServerEntry>& server_list)
{
    if (server_list.empty())
    {
        return;
    }

    std::filesystem::create_directories(GetSaveDirectoryPath());

    std::ofstream file(GetSaveFilePath(file_name), std::ios::binary | std::ios::out);
    if (!file.is_open())
    {
        return;
    }

    std::string data = MasterListCache_Encode(server_list);
    file.write(data.data(), data.size());
    file.close();
}

#ifdef _WIN32

namespace
{
    std::string WideToUtf8(const wchar_t* wide)
    {
        int utf8_size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
        if (utf8_size <= 0)
        {
            return {};
        }

        std::string utf8(utf8_size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8.data(), utf8_size, nullptr, nullptr);
        utf8.resize(utf8_size - 1);

        return utf8;
    }
}

std::string FileMasterClient::GetSaveFilePath(const std::string &file_name)
{
    PWSTR roaming_app_data_path;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming_app_data_path) != S_OK)
    {
        return {};
    }

    return std::format("{}\\CS-NextClient\\{}", WideToUtf8(roaming_app_data_path), file_name);
}

std::string FileMasterClient::GetSaveDirectoryPath()
{
    PWSTR roaming_app_data_path;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming_app_data_path) != S_OK)
    {
        return {};
    }

    return std::format("{}\\CS-NextClient", WideToUtf8(roaming_app_data_path));
}

#else

std::string FileMasterClient::GetSaveDirectoryPath()
{
    const char* home = getenv("HOME");
    return std::format("{}/.local/share/CS-NextClient", home ? home : "");
}

std::string FileMasterClient::GetSaveFilePath(const std::string &file_name)
{
    return std::format("{}/{}", GetSaveDirectoryPath(), file_name);
}

#endif

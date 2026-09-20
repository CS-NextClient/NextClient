#include "FileMasterClient.h"

#include <fstream>
#include <iterator>
#include <utility>
#include <vector>
#include <filesystem>

#include <Shlobj.h>
#include <data_encoding/aes.h>
#include <nitro_utils/random_utils.h>

#include "service/matchmaking/master/MasterListCache.h"

using namespace taskcoro;
using namespace concurrencpp;

FileMasterClient::FileMasterClient(std::wstring file_name) :
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

std::vector<MasterServerEntry> FileMasterClient::ReadFromFile(const std::wstring& file_name)
{
    std::ifstream file(GetSaveFilePath(file_name), std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    return MasterListCache_Decode(data);
}

void FileMasterClient::WriteToFile(const std::wstring& file_name, const std::vector<MasterServerEntry>& server_list)
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

std::wstring FileMasterClient::GetSaveFilePath(const std::wstring &file_name)
{
    PWSTR roaming_app_data_path;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming_app_data_path) != S_OK)
    {
        return {};
    }

    return std::format(L"{}\\CS-NextClient\\{}", roaming_app_data_path, file_name);
}

std::wstring FileMasterClient::GetSaveDirectoryPath()
{
    PWSTR roaming_app_data_path;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &roaming_app_data_path) != S_OK)
    {
        return {};
    }

    return std::format(L"{}\\CS-NextClient", roaming_app_data_path);
}

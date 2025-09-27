#include "FileMasterClient.h"

#include <fstream>
#include <utility>
#include <vector>
#include <filesystem>

#include <Shlobj.h>
#include <data_encoding/aes.h>
#include <nitro_utils/random_utils.h>

using namespace taskcoro;
using namespace concurrencpp;

FileMasterClient::FileMasterClient(std::wstring file_name) :
    file_name_(std::move(file_name))
{ }

result<std::vector<netadr_t>> FileMasterClient::GetServerAddressesAsync(
    std::function<void(const netadr_t&)> address_received_callback,
    std::shared_ptr<CancellationToken> cancellation_token
)
{
    std::vector<netadr_t> addresses = co_await TaskCoro::RunIO([this] { return ReadFromFile(file_name_); });

    if (address_received_callback)
    {
        for (const netadr_t& address : addresses)
        {
            address_received_callback(address);
        }
    }

    co_return addresses;
}

void FileMasterClient::Save(const std::vector<netadr_t>& server_list)
{
    WriteToFile(file_name_, server_list);
}

std::vector<netadr_t> FileMasterClient::ReadFromFile(const std::wstring& file_name)
{
    std::ifstream file(GetSaveFilePath(file_name), std::ios::binary);
    if (!file.is_open())
    {
        return {};
    }

    std::vector<netadr_t> result;

    uint32_t ip;
    uint16_t port;

    while (file.read((char*)&ip, sizeof(ip)) &&
           file.read((char*)&port, sizeof(port)))
    {
        result.emplace_back(ip, port);
    }

    return result;
}

void FileMasterClient::WriteToFile(const std::wstring& file_name, const std::vector<netadr_t>& server_list)
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

    for (const auto &server : server_list)
    {
        uint32_t ip = server.GetIPHostByteOrder();
        uint16_t port = server.GetPortHostByteOrder();

        file.write((char*)&ip, sizeof(ip));
        file.write((char*)&port, sizeof(port));
    }

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

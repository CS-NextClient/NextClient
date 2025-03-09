#pragma once
#include "MasterClientInterface.h"
#include "MasterClientCacheInterface.h"

class FileMasterClient : public MasterClientCacheInterface
{
    std::wstring file_name_{};

public:
    explicit FileMasterClient(std::wstring file_name);

    concurrencpp::result<std::vector<netadr_t>> GetServerAddressesAsync(
        std::function<void(const netadr_t&)> address_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

    void Save(const std::vector<netadr_t>& server_list) override;

private:
    static std::vector<netadr_t> ReadFromFile(const std::wstring& file_name);
    static void WriteToFile(const std::wstring& file_name, const std::vector<netadr_t>& server_list);

    static std::wstring GetSaveFilePath(const std::wstring& file_name);
    static std::wstring GetSaveDirectoryPath();
};


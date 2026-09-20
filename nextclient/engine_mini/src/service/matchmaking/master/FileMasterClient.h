#pragma once
#include "MasterClientInterface.h"
#include "MasterClientCacheInterface.h"

class FileMasterClient : public MasterClientCacheInterface
{
    std::wstring file_name_{};

public:
    explicit FileMasterClient(std::wstring file_name);

    concurrencpp::result<std::vector<MasterServerEntry>> GetServerListAsync(
        std::function<void(const MasterServerEntry&)> entry_received_callback,
        std::shared_ptr<taskcoro::CancellationToken> cancellation_token
    ) override;

    void Save(const std::vector<MasterServerEntry>& server_list) override;

private:
    static std::vector<MasterServerEntry> ReadFromFile(const std::wstring& file_name);
    static void WriteToFile(const std::wstring& file_name, const std::vector<MasterServerEntry>& server_list);

    static std::wstring GetSaveFilePath(const std::wstring& file_name);
    static std::wstring GetSaveDirectoryPath();
};


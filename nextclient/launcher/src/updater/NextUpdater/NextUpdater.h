#pragma once

#include <filesystem>
#include <functional>

#include <easylogging++.h>
#include <cpr/cpr.h>
#include <utils/bitmask.h>
#include <updater/NextUpdater/NextUpdaterEvent.h>
#include <updater/json_data/UpdateEntry.h>

#include "NextUpdaterHttpService.h"
#include "http_download/HttpFileResult.h"
#include "FileOpener.h"
#include "UpdaterFileInfo.h"

enum class NextUpdaterResult
{
    Updated,
    UpdatedIncludingLauncher,
    CanceledByUser,
    NothingToUpdate,
    Error
};

class NextUpdater
{
    enum class RestoreFromBackupBehaviour
    {
        None,
        ClearBackupFolderEvenIfRestoreFail
    };

    enum class RestoreFromBackupResult
    {
        Ok,
        ClearBackupFolderError,
    };

    std::filesystem::path install_path_;
    std::filesystem::path backup_path_;
    std::shared_ptr<HttpServiceInterface> http_service_;
    el::Logger* logger_;
    std::function<void(NextUpdaterEvent)> updater_event_callback_;

    NextUpdaterState state_ = NextUpdaterState::Initialization;
    float state_progress_ = 0;
    std::atomic_bool canceled_ = false;

public:
    explicit NextUpdater(std::filesystem::path install_path,
                         std::filesystem::path backup_path,
                         el::Logger* logger,
                         std::shared_ptr<HttpServiceInterface> http_service,
                         std::function<void(NextUpdaterEvent)> updater_event_callback);
    NextUpdaterResult Start();
    void Cancel();
    [[nodiscard]] bool IsCanceled() const;

private:
    void SetStateAndRaiseEvent(NextUpdaterState state);
    void SetErrorAndRaiseEvent(std::string error);
    void SetStateProgressAndRaiseEvent(float progress);

    RestoreFromBackupResult RestoreFromBackup(RestoreFromBackupBehaviour behaviour = RestoreFromBackupBehaviour::None);

    ResultT<UpdateEntry> SendUpdateFilesRequest();
    std::vector<UpdaterFileInfo> CreateUpdaterFileInfos(const std::vector<FileEntry>& remote_files);
    // key is remote file name
    static ResultT<std::unordered_map<std::string, UpdaterFileInfo>> GetFilesToUpdate(const std::vector<UpdaterFileInfo>& files);
    static ResultT<std::vector<HttpFileResult>> DownloadFilesToUpdate(auto files, const std::string& hostname, std::function<bool(cpr::cpr_off_t total, cpr::cpr_off_t downloaded, cpr::cpr_off_t speed)> progress);

    static Result InstallFiles(FileOpener& file_opener, const std::vector<HttpFileResult>& downloaded_files, const std::unordered_map<std::string, UpdaterFileInfo>& updating_file_info);

    // backup functions
    Result ClearBackupFolder();
    Result BackupFiles(auto files);
    ResultT<int> RestoreFilesFromBackup(auto files);
    ResultT<int> RestoreFilesFromBackup();

    // utils
    static std::string GetStreamMd5(std::fstream& stream);
};

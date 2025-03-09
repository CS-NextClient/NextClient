#include "NextUpdater.h"
#include <chrono>
#include <utility>
#include <nitro_utils/string_utils.h>
#include <data_encoding/md5.h>
#include <utils/platform.h>
#include "http_download/HttpFileDownloader.h"

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace saferesult;
namespace fs = std::filesystem;

NextUpdater::NextUpdater(std::filesystem::path install_path,
                         std::filesystem::path backup_path,
                         el::Logger* logger,
                         std::shared_ptr<HttpServiceInterface> http_service,
                         std::function<void(NextUpdaterEvent)> updater_event_callback) :
    install_path_(std::move(install_path)),
    backup_path_(std::move(backup_path)),
    logger_(logger),
    http_service_(std::move(http_service)),
    updater_event_callback_(std::move(updater_event_callback))
{

}

NextUpdaterResult NextUpdater::Start()
{
    auto restoreFromBackupResult = RestoreFromBackup(RestoreFromBackupBehaviour::ClearBackupFolderEvenIfRestoreFail);
    if (restoreFromBackupResult == RestoreFromBackupResult::ClearBackupFolderError)
        return NextUpdaterResult::Error;

    logger_->info("Requesting file list from server");
    SetStateAndRaiseEvent(NextUpdaterState::RequestingFileList);
    ResultT<UpdateEntry> update_entry = SendUpdateFilesRequest();
    if (update_entry.has_error())
    {
        logger_->error("Requesting file list error: %v", update_entry.get_error());
        SetErrorAndRaiseEvent(update_entry.get_error());
        return NextUpdaterResult::Error;
    }

    std::vector<UpdaterFileInfo> file_infos = CreateUpdaterFileInfos(update_entry->files);

    SetStateAndRaiseEvent(NextUpdaterState::GatheringFilesToUpdate);
    logger_->info("Gathering files that need to be updated");
    ResultT<std::unordered_map<std::string, UpdaterFileInfo>> files_to_update = GetFilesToUpdate(file_infos);
    if (files_to_update.has_error())
    {
        logger_->error("Gathering files to update error: %v", files_to_update.get_error());
        SetErrorAndRaiseEvent(files_to_update.get_error());
        return NextUpdaterResult::Error;
    }

    if (files_to_update->empty())
    {
        logger_->info("  nothing to update");
        SetStateAndRaiseEvent(NextUpdaterState::Done);
        return NextUpdaterResult::NothingToUpdate;
    }
    else
    {
        logger_->info("  files to update:");
        for (auto& [filename, file] : *files_to_update)
            logger_->info("  %v", file.get_install_path().string());
    }

    logger_->info("Creating a backup");
    SetStateAndRaiseEvent(NextUpdaterState::Backuping);
    Result backup_files_result = BackupFiles(*files_to_update | std::views::values);
    if (backup_files_result.has_error())
    {
        logger_->error("Backup error: %v", backup_files_result.get_error());
        SetErrorAndRaiseEvent(backup_files_result.get_error());
        return NextUpdaterResult::Error;
    }

    logger_->info("Opening files to install");
    SetStateAndRaiseEvent(NextUpdaterState::OpeningFilesToInstall);
    FileOpener fo_install(*files_to_update | std::views::transform([](const auto& item){ return item.second.get_install_path(); }), std::ios::out | std::ios::binary);
    std::string open_files_to_install_error;
    if (!fo_install.IsAllFilesOpened(FileOpenerErrorFlags::None, open_files_to_install_error))
    {
        logger_->error("Open files to install error: %v", open_files_to_install_error);
        SetErrorAndRaiseEvent(open_files_to_install_error);

        fo_install.CloseAllFiles();
        RestoreFromBackup();
        return NextUpdaterResult::Error;
    }

    logger_->info("Downloading an update");
#if defined(_DEBUG) || defined(LAUNCHER_BUILD_TESTS)
    logger_->info("  base url: %v", update_entry->hostname);
#endif
    SetStateAndRaiseEvent(NextUpdaterState::Downloading);
    ResultT<std::vector<HttpFileResult>> download_results = DownloadFilesToUpdate(*files_to_update | std::views::values, update_entry->hostname,
        [this](cpr::cpr_off_t total, cpr::cpr_off_t downloaded, cpr::cpr_off_t speed) {
            SetStateProgressAndRaiseEvent((float)downloaded / (float)total);
            return canceled_.load();
        });

    if (download_results.has_error())
    {
        logger_->error("Download update error: %v", download_results.get_error());
        SetErrorAndRaiseEvent(download_results.get_error());

        fo_install.CloseAllFiles();
        RestoreFromBackup();
        return NextUpdaterResult::Error;
    }

    if (canceled_)
    {
        logger_->info("Download of the update is canceled by user");
        SetStateAndRaiseEvent(NextUpdaterState::CanceledByUser);

        fo_install.CloseAllFiles();
        RestoreFromBackup();
        return NextUpdaterResult::CanceledByUser;
    }

    logger_->info("Installing an update");
    SetStateAndRaiseEvent(NextUpdaterState::Installing);
    std::string install_error;
    Result install_result = InstallFiles(fo_install, *download_results, *files_to_update);
    if (install_result.has_error())
    {
        logger_->error("Install update error: %v", install_result.get_error());
        SetErrorAndRaiseEvent(install_result.get_error());

        fo_install.CloseAllFiles();
        RestoreFromBackup();
        return NextUpdaterResult::Error;
    }

    // TODO validate installation

    logger_->info("Clearing backup folder");
    SetStateAndRaiseEvent(NextUpdaterState::ClearingBackupFolder);
    Result clear_backup_folder_result = ClearBackupFolder();
    if (clear_backup_folder_result.has_error())
    {
        logger_->error("Clearing backup folder error: %v", clear_backup_folder_result.get_error());
        SetErrorAndRaiseEvent(clear_backup_folder_result.get_error());
        return NextUpdaterResult::Error;
    }

    SetStateAndRaiseEvent(NextUpdaterState::Done);
    return files_to_update->contains("cs.exe") ? NextUpdaterResult::UpdatedIncludingLauncher : NextUpdaterResult::Updated;
}

void NextUpdater::Cancel()
{
    canceled_ = true;
}

[[nodiscard]] bool NextUpdater::IsCanceled() const
{
    return canceled_;
}

void NextUpdater::SetStateAndRaiseEvent(NextUpdaterState state)
{
    state_ = state;
    updater_event_callback_(NextUpdaterEvent(state_, state_progress_, "", NextUpdaterEventFlags::StateChanged));
}

void NextUpdater::SetErrorAndRaiseEvent(std::string error)
{
    updater_event_callback_(NextUpdaterEvent(state_, state_progress_, std::move(error), NextUpdaterEventFlags::ErrorChanged));
}

void NextUpdater::SetStateProgressAndRaiseEvent(float progress)
{
    state_progress_ = progress;
    updater_event_callback_(NextUpdaterEvent(state_, state_progress_, "", NextUpdaterEventFlags::ProgressChanged));
}

NextUpdater::RestoreFromBackupResult NextUpdater::RestoreFromBackup(RestoreFromBackupBehaviour behaviour)
{
    logger_->info("Restoring from backup");
    SetStateAndRaiseEvent(NextUpdaterState::RestoringFromBackup);

    ResultT<int> restore_backup_result = RestoreFilesFromBackup();
    if (restore_backup_result.has_error())
        logger_->error("Restore from backup error: %v", restore_backup_result.get_error());
    else if (*restore_backup_result == 0)
        logger_->info("  nothing to do");
    else
        logger_->info("  restored %v files", *restore_backup_result);

    if (!restore_backup_result.has_error() || behaviour == RestoreFromBackupBehaviour::ClearBackupFolderEvenIfRestoreFail)
    {
        if (restore_backup_result.has_error() && behaviour == RestoreFromBackupBehaviour::ClearBackupFolderEvenIfRestoreFail)
            SetErrorAndRaiseEvent(restore_backup_result.get_error());

        logger_->info("Clearing backup folder");
        SetStateAndRaiseEvent(NextUpdaterState::ClearingBackupFolder);

        Result clear_backup_folder_result = ClearBackupFolder();
        if (clear_backup_folder_result.has_error())
        {
            logger_->error("Clearing backup folder error: %v", clear_backup_folder_result.get_error());
            SetErrorAndRaiseEvent(clear_backup_folder_result.get_error());
            return RestoreFromBackupResult::ClearBackupFolderError;
        }
    }

    return RestoreFromBackupResult::Ok;
}

std::vector<UpdaterFileInfo> NextUpdater::CreateUpdaterFileInfos(const std::vector<FileEntry>& remote_files)
{
    std::vector<UpdaterFileInfo> files;

    for (const auto& remote_file : remote_files)
    {
        std::string filename = remote_file.filename;

        fs::path to_hash_path;
        fs::path install_path;
        fs::path backup_path;

        if (filename == "cs.exe")
        {
            fs::path current_process_filename = GetCurrentProcessPath().filename();

            to_hash_path = install_path_ / current_process_filename;
            install_path = install_path_ / fs::path(current_process_filename.replace_extension().string() + "_new.exe");
        }
        else
        {
            to_hash_path = install_path_ / filename;
            install_path = install_path_ / filename;
        }
        backup_path = backup_path_ / filename;

        to_hash_path.make_preferred();
        install_path.make_preferred();
        backup_path.make_preferred();

        files.emplace_back(to_hash_path, install_path, backup_path, remote_file, false);
    }

    return files;
}

ResultT<UpdateEntry> NextUpdater::SendUpdateFilesRequest()
{
    HttpResponse response = http_service_->Post("launcher_update", "null");
    if (response.has_error())
        return ResultError(std::format("Can't get files list from server, http code: {}; error: {}", response.status_code, response.error.message));

    try
    {
        auto v = tao::json::basic_from_string<UpdaterJsonTraits>(response.data);
        return v.as<UpdateEntry>();
    }
    catch (const std::runtime_error& e)
    {
        return ResultError(std::format("Files list deserialization exception: {}", e.what()));
    }
    catch (...)
    {
        return ResultError("Files list deserialization exception");
    }
}

ResultT<std::vector<HttpFileResult>> NextUpdater::DownloadFilesToUpdate(auto files, const std::string& hostname, std::function<bool(cpr::cpr_off_t total, cpr::cpr_off_t downloaded, cpr::cpr_off_t speed)> progress)
{
    std::vector<HttpFileRequest> files_to_download;
    cpr::cpr_off_t total_bytes_to_download = 0;

    for (const auto& file: files)
    {
        auto file_entry = file.get_remote_file();
        files_to_download.emplace_back(HttpFileRequest(file_entry.filename, file_entry.hash, file_entry.size));
        total_bytes_to_download += file.get_remote_file().size;
    }

    HttpFileDownloader downloader(files_to_download, hostname, [total_bytes_to_download, progress](cpr::cpr_off_t downloaded, cpr::cpr_off_t speed) {
        return progress(total_bytes_to_download, downloaded, speed);
    });
    std::vector<HttpFileResult> download_results = downloader.StartDownloads();

    std::string error_str;
    for (const auto& file: download_results)
    {
        if (file.has_error())
            error_str += std::format("File not downloaded: {}, {}\n", file.get_request().get_filename(), file.get_error());
    }

    nitro_utils::trim(error_str);
    if (!error_str.empty())
        return ResultError(error_str);

    return download_results;
}

ResultT<std::unordered_map<std::string, UpdaterFileInfo>> NextUpdater::GetFilesToUpdate(const std::vector<UpdaterFileInfo>& files)
{
    FileOpener fo(std::ios::in | std::ios::binary);
    for (const auto& item: files)
        fo.OpenFile(item.get_to_hash_path());

    std::string open_files_error;
    if (!fo.IsAllFilesOpened(FileOpenerErrorFlags::IgnoreNotExists, open_files_error))
        return ResultError("Open files error: " + open_files_error);

    std::unordered_map<std::string, UpdaterFileInfo> files_to_update;
    for (const auto& file : files)
    {
        auto& file_info = fo.GetFileInfo(file.get_to_hash_path());

        if (!file_info.IsFileExists() || file.get_remote_file().hash != GetStreamMd5(file_info.stream))
        {
            std::string remote_filename = file.get_remote_file().filename;
            bool need_backup = remote_filename != "cs.exe" && file_info.IsFileExists();

            UpdaterFileInfo file_result(file.get_to_hash_path(), file.get_install_path(), file.get_backup_path(), file.get_remote_file(), need_backup);
            files_to_update.emplace(remote_filename, std::move(file_result));
        }
    }

    return files_to_update;
}

Result NextUpdater::InstallFiles(FileOpener& file_opener, const std::vector<HttpFileResult>& downloaded_files, const std::unordered_map<std::string, UpdaterFileInfo>& updating_file_info)
{
    std::string error;
    for (const auto& download_result : downloaded_files)
    {
        const UpdaterFileInfo& updater_file = updating_file_info.at(download_result.get_request().get_filename());
        std::fstream& install_stream = file_opener.GetFileInfo(updater_file.get_install_path()).stream;

        if (download_result.get_data().empty())
            continue;

        errno = 0;
        install_stream.write(reinterpret_cast<const char*>(download_result.get_data().data()), download_result.get_data().size());
        install_stream.flush();

        if (FileOpener::IsError(install_stream))
            error += FileOpener::CreateErrorMessage(install_stream, updater_file.get_install_path()) + "\n";
    }

    file_opener.CloseAllFiles();

    nitro_utils::trim(error);
    if (!error.empty())
        return ResultError(error);

    return {};
}

Result NextUpdater::ClearBackupFolder()
{
    std::error_code ec_exists;
    std::error_code ec_is_empty;

    if (fs::exists(backup_path_, ec_exists) && !fs::is_empty(backup_path_, ec_is_empty))
    {
        if (ec_is_empty)
            return ResultError(ec_is_empty.message());

        std::error_code ec_remove_all;
        bool files_removed = fs::remove_all(backup_path_, ec_remove_all);
        if (ec_remove_all)
            return ResultError(ec_remove_all.message());

        if (!files_removed)
            ResultError("fs::remove_all returns false, but without errors and with exists backup folder");

        return {};
    }

    if (ec_exists)
        return ResultError(ec_exists.message());

    return {};
}

Result NextUpdater::BackupFiles(auto files)
{
    std::string error;

    for (const auto& file : files)
    {
        if (!file.is_need_backup())
            continue;

        OpenerFile backup_file = FileOpener::OpenSingleFile(file.get_backup_path(), std::ios::out | std::ios::binary);
        if (backup_file.HasError())
            error += std::format("at backup path: {}\n", FileOpener::CreateErrorMessage(backup_file, file.get_backup_path()));

        OpenerFile to_hash_file = FileOpener::OpenSingleFile(file.get_to_hash_path(), std::ios::in | std::ios::binary);
        if (to_hash_file.HasError())
            error += std::format("at to_hash path: {}\n", FileOpener::CreateErrorMessage(to_hash_file, file.get_install_path()));

        if (to_hash_file.HasError() || backup_file.HasError())
            continue;

        if (to_hash_file.IsEmptyOrFullRead())
            continue;

        std::fstream& backup_stream = backup_file.stream;
        std::fstream& to_hash_stream = to_hash_file.stream;

        errno = 0;
        backup_stream << to_hash_stream.rdbuf();

        if (FileOpener::IsError(backup_stream))
            error += std::format("at backup path: {}\n", FileOpener::CreateErrorMessage(backup_stream, file.get_backup_path()));

        if (FileOpener::IsError(to_hash_stream))
            error += std::format("at to_hash path: {}\n", FileOpener::CreateErrorMessage(to_hash_stream, file.get_install_path()));
    }

    nitro_utils::trim(error);
    if (!error.empty())
        return ResultError(error);

    return {};
}

ResultT<int> NextUpdater::RestoreFilesFromBackup()
{
    std::error_code ec_exists;
    if (!fs::exists(backup_path_, ec_exists))
    {
        if (ec_exists)
            return ResultError("fs::exists error: " + ec_exists.message());

        return 0;
    }

    std::error_code ec_iterator;
    auto dir_it = fs::recursive_directory_iterator(backup_path_, ec_iterator);
    if (ec_iterator)
        return ResultError("fs::recursive_directory_iterator error: " + ec_exists.message());

    fs::path root_path = GetCurrentProcessDirectory();

    std::vector<UpdaterFileInfo> updater_files;
    for (const fs::directory_entry& dir_entry : dir_it)
    {
        if (!dir_entry.is_regular_file())
            continue;

        fs::path backup_path = dir_entry.path();
        fs::path install_path = root_path / install_path_ / fs::relative(dir_entry.path(), root_path / backup_path_);

        updater_files.emplace_back(fs::path{}, install_path, backup_path, FileEntry{}, false);
    }

    if (updater_files.empty())
        return 0;

    return RestoreFilesFromBackup(updater_files);
}

ResultT<int> NextUpdater::RestoreFilesFromBackup(auto files)
{
    std::string error;
    int restored_files_count = 0;

    for (const auto& file : files)
    {
        OpenerFile backup_file = FileOpener::OpenSingleFile(file.get_backup_path(), std::ios::in | std::ios::binary);
        if (backup_file.HasError(FileOpenerErrorFlags::IgnoreNotExists))
            error += std::format("at backup path: {}\n", FileOpener::CreateErrorMessage(backup_file, file.get_backup_path()));

        if (!backup_file.IsFileExists())
            continue;

        OpenerFile install_file = FileOpener::OpenSingleFile(file.get_install_path(), std::ios::out | std::ios::binary);
        if (install_file.HasError())
            error += std::format("at install path: {}\n", FileOpener::CreateErrorMessage(install_file, file.get_install_path()));

        if (install_file.HasError() || backup_file.HasError())
            continue;

        if (backup_file.IsEmptyOrFullRead())
            continue;

        std::fstream& backup_stream = backup_file.stream;
        std::fstream& install_stream = install_file.stream;

        errno = 0;
        install_stream << backup_stream.rdbuf();

        if (FileOpener::IsError(backup_stream))
            error += std::format("at backup path: {}\n", FileOpener::CreateErrorMessage(backup_stream, file.get_backup_path()));

        if (FileOpener::IsError(install_stream))
            error += std::format("at install path: {}\n", FileOpener::CreateErrorMessage(install_stream, file.get_install_path()));

        if (!FileOpener::IsError(backup_stream) && !FileOpener::IsError(install_stream))
            restored_files_count++;
    }

    nitro_utils::trim(error);
    if (!error.empty())
        return ResultError(error);

    return restored_files_count;
}

std::string NextUpdater::GetStreamMd5(std::fstream& stream)
{
    stream.seekg(0, std::ifstream::end);
    size_t length = stream.tellg();
    stream.seekg(0, std::ifstream::beg);

    std::vector<char> buffer;
    if (length == 0)
        return "";

    buffer.resize(length);
    stream.read(buffer.data(), length);

    MD5 md5;
    md5.update(buffer.data(), buffer.size());
    md5.finalize();
    return md5.hexdigest();
}

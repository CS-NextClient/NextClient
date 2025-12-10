#pragma once

enum class UpdaterViewState
{
    Initialization,
    RequestingRemoteConfig,
    RestoringFromBackup,
    ClearingBackupFolder,
    RequestingFileList,
    GatheringFilesToUpdate,
    Backuping,
    OpeningFilesToInstall,
    Downloading,
    Installing,
    RequestingBranchList,
    CanceledByUser,
    Done
};

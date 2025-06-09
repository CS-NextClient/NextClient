#include "UpdaterView.h"
#include <string>
#include <gui_app_core/imgui/imgui.h>

const std::unordered_map<UpdaterViewState, std::string> UpdaterView::state_string_ru_
{
    { UpdaterViewState::Initialization,         "Инициализация..." },
    { UpdaterViewState::RequestingRemoteConfig, "Запрос конфигурации" },
    { UpdaterViewState::RestoringFromBackup,    "Восстановление из резервной копии" },
    { UpdaterViewState::ClearingBackupFolder,   "Удаление резервной копии" },
    { UpdaterViewState::RequestingFileList,     "Запрос к серверу обновлений" },
    { UpdaterViewState::GatheringFilesToUpdate, "Проверка файлов" },
    { UpdaterViewState::Backuping,              "Создание резервной копии" },
    { UpdaterViewState::OpeningFilesToInstall,  "Открытие файлов для установки" },
    { UpdaterViewState::Downloading,            "Загрузка обновлений" },
    { UpdaterViewState::Installing,             "Установка обновлений" },
    { UpdaterViewState::RequestingBranchList,   "Запрос к серверу обновлений" },
    { UpdaterViewState::CanceledByUser,         "Отменено пользователем" },
    { UpdaterViewState::Done,                   "Готово" }
};

const std::unordered_map<UpdaterViewState, std::string> UpdaterView::state_string_en_
{
    { UpdaterViewState::Initialization,         "Initialization..." },
    { UpdaterViewState::RequestingRemoteConfig, "Configuration request" },
    { UpdaterViewState::RestoringFromBackup,    "Restoring from backup" },
    { UpdaterViewState::ClearingBackupFolder,   "Deleting a backup" },
    { UpdaterViewState::RequestingFileList,     "Requesting an update server" },
    { UpdaterViewState::GatheringFilesToUpdate, "Checking files" },
    { UpdaterViewState::Backuping,              "Creating a backup" },
    { UpdaterViewState::OpeningFilesToInstall,  "Opening files for installation" },
    { UpdaterViewState::Downloading,            "Downloading updates" },
    { UpdaterViewState::Installing,             "Installing updates" },
    { UpdaterViewState::RequestingBranchList,   "Requesting an update server" },
    { UpdaterViewState::CanceledByUser,         "Canceled by user" },
    { UpdaterViewState::Done,                   "Done" }
};

const std::unordered_map<UpdaterViewState, float> UpdaterView::state_progress_
{
    { UpdaterViewState::Initialization,         0.f },
    { UpdaterViewState::RequestingRemoteConfig, 0.01f },
    { UpdaterViewState::RestoringFromBackup,    0.02f },
    { UpdaterViewState::ClearingBackupFolder,   0.03f },
    { UpdaterViewState::RequestingFileList,     0.04f },
    { UpdaterViewState::GatheringFilesToUpdate, 0.07f },
    { UpdaterViewState::Backuping,              0.08f },
    { UpdaterViewState::OpeningFilesToInstall,  0.09f },
    { UpdaterViewState::Downloading,            0.1f },
    { UpdaterViewState::Installing,             0.9f },
    { UpdaterViewState::RequestingBranchList,   0.95f },
    { UpdaterViewState::CanceledByUser,         1.f },
    { UpdaterViewState::Done,                   1.f }
};

UpdaterView::UpdaterView(std::string language) :
    language_(std::move(language))
{

}

void UpdaterView::OnStart()
{

}

void UpdaterView::OnGui()
{
    static const float buttons_width = 130.f;
    static const float buttons_height = 30.0f;
    const float window_width = ImGui::GetWindowWidth();
    const float item_spacing = ImGui::GetStyle().ItemSpacing.x;

    const std::string& label = language_ == "russian" ? state_string_ru_.at(state_) : state_string_en_.at(state_);
    const std::string exit_label_ = language_ == "russian" ? "Выход" : "Quit";

    ImGui::LabelText("", "%s", label.c_str());
    ImGui::Dummy(ImVec2 {0, 8.0f});
    ImGui::ProgressBar(GetOverallProgress());
    ImGui::Dummy(ImVec2 {0, 8.0f});
    ImGui::Spacing();
    ImGui::SameLine(window_width - buttons_width - item_spacing);

    if (ImGui::Button(exit_label_.c_str(), ImVec2 {buttons_width, buttons_height}))
        exit_callback_();
}

void UpdaterView::SetState(UpdaterViewState state)
{
    state_ = state;
}

void UpdaterView::SetError(std::string error)
{
    error_message_ = std::move(error);
}

void UpdaterView::SetProgress(float progress)
{
    progress_ = progress;
}

void UpdaterView::SetExitCallback(std::function<void()> callback)
{
    exit_callback_ = callback;
}

std::tuple<int, int> UpdaterView::GetWindowSize()
{
    return { kWindowWidth, kWindowHeight };
}

float UpdaterView::GetOverallProgress()
{
    if (state_ == UpdaterViewState::Downloading)
        return state_progress_.at(UpdaterViewState::Downloading) + progress_ * (state_progress_.at(UpdaterViewState::Installing) - state_progress_.at(UpdaterViewState::Downloading));

    return state_progress_.at(state_);
}

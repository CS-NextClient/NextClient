#include "UpdaterView.h"
#include <string>
#include <gui_app_core/imgui/imgui.h>

const std::unordered_map<NextUpdaterState, std::string> UpdaterView::state_string_ru_
{
    { NextUpdaterState::Initialization,         "Инициализация..." },
    { NextUpdaterState::RestoringFromBackup,    "Восстановление из резервной копии" },
    { NextUpdaterState::ClearingBackupFolder,   "Удаление резервной копии" },
    { NextUpdaterState::RequestingFileList,     "Запрос к серверу обновлений" },
    { NextUpdaterState::GatheringFilesToUpdate, "Проверка файлов" },
    { NextUpdaterState::Backuping,              "Создание резервной копии" },
    { NextUpdaterState::OpeningFilesToInstall,  "Открытие файлов для установки" },
    { NextUpdaterState::Downloading,            "Загрузка обновлений" },
    { NextUpdaterState::Installing,             "Установка обновлений" },
    { NextUpdaterState::CanceledByUser,         "Отменено пользователем" },
    { NextUpdaterState::Done,                   "Готово" }
};

const std::unordered_map<NextUpdaterState, std::string> UpdaterView::state_string_en_
{
    { NextUpdaterState::Initialization,         "Initialization..." },
    { NextUpdaterState::RestoringFromBackup,    "Restoring from backup" },
    { NextUpdaterState::ClearingBackupFolder,   "Deleting a backup" },
    { NextUpdaterState::RequestingFileList,     "Requesting an update server" },
    { NextUpdaterState::GatheringFilesToUpdate, "Checking files" },
    { NextUpdaterState::Backuping,              "Creating a backup" },
    { NextUpdaterState::OpeningFilesToInstall,  "Opening files for installation" },
    { NextUpdaterState::Downloading,            "Downloading updates" },
    { NextUpdaterState::Installing,             "Installing updates" },
    { NextUpdaterState::CanceledByUser,         "Canceled by user" },
    { NextUpdaterState::Done,                   "Done" }
};

const std::unordered_map<NextUpdaterState, float> UpdaterView::state_progress_
{
    { NextUpdaterState::Initialization,         0.f },
    { NextUpdaterState::RestoringFromBackup,    0.01f },
    { NextUpdaterState::ClearingBackupFolder,   0.02f },
    { NextUpdaterState::RequestingFileList,     0.03f },
    { NextUpdaterState::GatheringFilesToUpdate, 0.07f },
    { NextUpdaterState::Backuping,              0.08f },
    { NextUpdaterState::OpeningFilesToInstall,  0.09f },
    { NextUpdaterState::Downloading,            0.1f },
    { NextUpdaterState::Installing,             0.9f },
    { NextUpdaterState::CanceledByUser,         1.f },
    { NextUpdaterState::Done,                   1.f }
};

UpdaterView::UpdaterView(std::string language) :
    language_(std::move(language))
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

void UpdaterView::SetState(NextUpdaterState state)
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
    if (state_ == NextUpdaterState::Downloading)
        return state_progress_.at(NextUpdaterState::Downloading) + progress_ * (state_progress_.at(NextUpdaterState::Installing) - state_progress_.at(NextUpdaterState::Downloading));

    return state_progress_.at(state_);
}

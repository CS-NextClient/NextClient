#pragma once
#include <string>
#include <functional>

#include <easylogging++.h>
#include <next_launcher/UserInfoClient.h>
#include <updater/NextUpdater/NextUpdaterEvent.h>

#include "UpdaterDoneStatus.h"
#include "json_data/BranchEntry.h"

std::tuple<UpdaterDoneStatus, std::vector<BranchEntry>> RunUpdaterGuiApp(
    const std::string& service_url, std::shared_ptr<next_launcher::UserInfoClient> user_info, el::Logger* logger,
    const std::string& language, const std::function<void(NextUpdaterEvent)>& error_event_callback);

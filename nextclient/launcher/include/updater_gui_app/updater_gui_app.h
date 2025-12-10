#pragma once
#include <string>
#include <functional>

#include <easylogging++.h>
#include <next_engine_mini/AnalyticsInterface.h>
#include <next_launcher/IBackendAddressResolver.h>
#include <next_launcher/IUserStorage.h>
#include <next_launcher/UserInfoClient.h>
#include <updater_gui_app/NextUpdater/NextUpdaterEvent.h>

#include "UpdaterFlags.h"
#include "UpdaterResult.h"

UpdaterResult RunUpdaterGuiApp(
    std::shared_ptr<next_launcher::UserInfoClient> user_info,
    std::shared_ptr<next_launcher::IUserStorage> user_storage,
    std::shared_ptr<AnalyticsInterface> analytics,
    UpdaterFlags updater_flags,
    const std::string& language,
    const std::function<void(NextUpdaterEvent)>& error_event_callback,
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver);

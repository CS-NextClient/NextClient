#include <gui_app_core/gui_app_core.h>
#include <updater_gui_app/updater_gui_app.h>

#include "Updater.h"

UpdaterResult RunUpdaterGuiApp(
    std::shared_ptr<next_launcher::UserInfoClient> user_info,
    std::shared_ptr<next_launcher::IUserStorage> user_storage,
    std::shared_ptr<AnalyticsInterface> analytics,
    UpdaterFlags updater_flags,
    const std::string& language,
    const std::function<void(NextUpdaterEvent)>& error_event_callback,
    std::shared_ptr<next_launcher::IBackendAddressResolver> backend_address_resolver)
{
    std::unique_ptr<Updater> updater_app = std::make_unique<Updater>(
        analytics,
        backend_address_resolver,
        user_storage,
        user_info,
        language,
        updater_flags,
        error_event_callback);

    return RunGuiApp<UpdaterResult>(std::move(updater_app));
}

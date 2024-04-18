#include <gui_app_core/gui_app_core.h>
#include <updater/updater_gui_app.h>

#include "Updater.h"
#include "UpdaterView.h"
#include "NextUpdater/NextUpdaterHttpService.h"

std::tuple<UpdaterDoneStatus, std::vector<BranchEntry>> RunUpdaterGuiApp(
    const std::string& service_url, std::shared_ptr<next_launcher::UserInfoClient> user_info, el::Logger* logger,
    const std::string& language, const std::function<void(NextUpdaterEvent)>& error_event_callback)
{
    auto updater_service = std::make_shared<NextUpdaterHttpService>(service_url, 5000, user_info);

    auto updater_view = std::make_shared<UpdaterView>(language);
    auto updater_app = std::make_shared<Updater>(updater_service, updater_view, logger, error_event_callback);

    RunGuiApp(updater_app);

    return {updater_app->DoneStatus(), updater_app->Branches()};
}

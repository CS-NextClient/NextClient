#include <Windows.h>

#include <easylogging++.h>
#include <next_launcher/version.h>
#include <nitro_utils/string_utils.h>
#include <saferesult/Result.h>
#include <utils/platform.h>

#include "ClientLauncher.h"

INITIALIZE_EASYLOGGINGPP

using namespace std::chrono_literals;
using namespace saferesult;

static void SetupLogger()
{
    el::Configurations config;
    config.setToDefault();
    config.set(el::Level::Global, el::ConfigurationType::MaxLogFileSize, std::to_string(MAX_LOGFILE_SIZE));
    config.set(el::Level::Global, el::ConfigurationType::Format, "%datetime [%level] %msg");
    el::Loggers::reconfigureLogger(el::base::consts::kDefaultLoggerId, config);
}

static void SetupLocale()
{
    setlocale(LC_CTYPE, "");
    setlocale(LC_TIME, "");
    setlocale(LC_COLLATE, "");
    setlocale(LC_MONETARY, "");
}

static void FinishLauncherUpdate()
{
    LOG(INFO) << "Finishing launcher update...";

    bool updated = true;

    std::filesystem::path current_path = GetCurrentProcessPath();
    std::string filename = current_path.filename().string();

    if (filename.ends_with("_new.exe"))
    {
        std::string new_filename = filename;
        nitro_utils::replace_all(new_filename, "_new.exe", ".exe");

        std::filesystem::path copy_to_path = current_path;
        copy_to_path.replace_filename(new_filename);

        // do a few tries to copy cs_new.exe to cs.exe because cs.exe may not be closed immediately after it runs new_cs.exe
        std::error_code ec_copy_file;
        for (int i = 0; i < 5; i++)
        {
            if (std::filesystem::copy_file(current_path, copy_to_path, std::filesystem::copy_options::overwrite_existing, ec_copy_file))
            {
                break;
            }

            std::this_thread::sleep_for(200ms);
        }

        if (ec_copy_file)
        {
            LOG(WARNING) << "Finish launcher update error: Can't copy " << current_path.string() << " to " << copy_to_path.string() << ": " << ec_copy_file.message();
            updated = false;
        }
    }
    else
    {
        std::string filename_to_delete = current_path.filename().replace_extension("").string() + "_new.exe";

        std::error_code ec_remove_file;
        if (!std::filesystem::remove(filename_to_delete, ec_remove_file))
        {
            if (ec_remove_file)
            {
                LOG(WARNING) << "Finish launcher update error: Can't delete " << filename_to_delete << ": " << ec_remove_file.message();
                return;
            }

            updated = false;
        }
    }

    LOG(INFO) << "Finishing launcher update: " << (updated ? "done" : "nothing to do");
}

int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ PWSTR lpCmdLine, _In_ int nCmdShow)
{
    SetupLogger();

    LOG(INFO) << "-----------------------------------------------";
    LOG(INFO) << "Start " << GetCurrentProcessPath().filename();
    LOG(INFO) << "Version: " << NEXT_CLIENT_BUILD_VERSION;

    SetupLocale();

#ifdef UPDATER_ENABLE
    FinishLauncherUpdate();
#endif

    {
        auto launcher = std::make_unique<ClientLauncher>(hInstance, GetCommandLineA());
        launcher->Run();
    }

    LOG(INFO) << "Exit";

    return EXIT_SUCCESS;
}
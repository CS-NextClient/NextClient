#include <thread>
#include <fstream>

#include <gtest/gtest.h>
#include <uwebsockets/App.h>

#include <NextUpdater/NextUpdater.h>
#include <nitro_utils/string_utils.h>

#include "mocks/HttpServiceMock.h"
#include "NextUpdaterTestFixture.h"

INITIALIZE_EASYLOGGINGPP

TEST_F(NextUpdaterTest, UpdateAndInstallSuccessExpected)
{
    int server_port = GetFreePort();

    std::thread server_thread([server_port]{
        uWS::App()
        .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
        .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
        .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
        .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
        .listen(server_port, [](us_listen_socket_t* listenSocket) { })
        .run();
    });
    server_thread.detach();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");
    WriteToFile(install_path / "cstrike/cl_dlls/client_mini.dll", "old content");
    DWORD client_mini_set_attr_result = SetFileAttributesW((install_path / "cstrike/cl_dlls/client_mini.dll").c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse> {
        {"launcher_update", HttpResponse(200, cpr::Error(), nitro_utils::replace_all_copy(R"(
{
    "hostname": "http://localhost:<server_port>/branch/test",
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2},
        {"filename": "nitro_api.dll",
         "hash": "21ad0bd836b90d08f4cf640b4c298e7c",
         "size": 2},
        {"filename": "cstrike/cl_dlls/client_mini.dll",
         "hash": "e0323a9039add2978bf5b49550572c7c",
         "size": 2},
        {"filename": "cstrike/cl_dlls/GameUI.dll",
         "hash": "1aabac6d068eef6a7bad3fdf50a05cc8",
         "size": 2}
    ]
}
)", "<server_port>", std::to_string(server_port))
        )}
    });

    // check pre install state
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "old content");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/client_mini.dll"), "old content");
    EXPECT_NE(client_mini_set_attr_result, 0);
    EXPECT_FALSE(std::filesystem::exists(install_path / "nitro_api.dll"));
    EXPECT_FALSE(std::filesystem::exists(install_path / "cstrike/cl_dlls/GameUI.dll"));

    NextUpdater next_updater(install_path, backup_path, GetTestLogger(), http_service, [this](const NextUpdaterEvent& event) { });
    NextUpdaterResult updater_result = next_updater.Start();

    EXPECT_EQ(updater_result, NextUpdaterResult::Updated);

    // check after install state
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "aa");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/client_mini.dll"), "bb");
    EXPECT_EQ(GetFileAttributesW((install_path / "cstrike/cl_dlls/client_mini.dll").c_str()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);
    EXPECT_EQ(ReadFromFile(install_path / "nitro_api.dll"), "cc");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/GameUI.dll"), "dd");
    EXPECT_FALSE(std::filesystem::exists(backup_path));
}

TEST_F(NextUpdaterTest, OpenFileErrorTest)
{
    int server_port = GetFreePort();

    std::thread server_thread([server_port]{
        uWS::App()
        .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
        .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
        .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
        .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
        .listen(server_port, [](us_listen_socket_t* listenSocket) { })
        .run();
    });
    server_thread.detach();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");
    HANDLE next_engine_file = CreateFileA((install_path / "next_engine_mini.dll").string().c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse> {
            {"launcher_update", HttpResponse(200, cpr::Error(), nitro_utils::replace_all_copy(R"(
{
    "hostname": "http://localhost:<server_port>/branch/test",
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)", "<server_port>", std::to_string(server_port))
            )}
    });

    // check pre install state
    EXPECT_NE(next_engine_file, INVALID_HANDLE_VALUE);

    NextUpdater next_updater(install_path, backup_path, GetTestLogger(), http_service, [this](const NextUpdaterEvent& event) { });
    NextUpdaterResult updater_result = next_updater.Start();

    EXPECT_EQ(updater_result, NextUpdaterResult::Error);

    CloseHandle(next_engine_file);
}
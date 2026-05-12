#include <thread>
#include <fstream>
#include <future>

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

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
            .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
            .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");
    WriteToFile(install_path / "cstrike/cl_dlls/client_mini.dll", "old content");
    DWORD client_mini_set_attr_result =
        SetFileAttributesW((install_path / "cstrike/cl_dlls/client_mini.dll").c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY);

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             nitro_utils::replace_all_copy(
                 R"(
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
)",
                 "<server_port>",
                 std::to_string(server_port)
             )
         )}
    });

    // check pre install state
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "old content");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/client_mini.dll"), "old content");
    EXPECT_NE(client_mini_set_attr_result, 0);
    EXPECT_FALSE(std::filesystem::exists(install_path / "nitro_api.dll"));
    EXPECT_FALSE(std::filesystem::exists(install_path / "cstrike/cl_dlls/GameUI.dll"));

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Updated);

    // check after install state
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "aa");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/client_mini.dll"), "bb");
    EXPECT_EQ(
        GetFileAttributesW((install_path / "cstrike/cl_dlls/client_mini.dll").c_str()), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY
    );
    EXPECT_EQ(ReadFromFile(install_path / "nitro_api.dll"), "cc");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/GameUI.dll"), "dd");
    EXPECT_FALSE(std::filesystem::exists(backup_path));
}

TEST_F(NextUpdaterTest, OpenFileErrorTest)
{
    int server_port = GetFreePort();

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
            .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
            .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");
    HANDLE next_engine_file = CreateFileA(
        (install_path / "next_engine_mini.dll").string().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             nitro_utils::replace_all_copy(
                 R"(
{
    "hostname": "http://localhost:<server_port>/branch/test",
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)",
                 "<server_port>",
                 std::to_string(server_port)
             )
         )}
    });

    // check pre install state
    EXPECT_NE(next_engine_file, INVALID_HANDLE_VALUE);

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Error);

    CloseHandle(next_engine_file);
}

TEST_F(NextUpdaterTest, UpdateWithHostnamesAndTestSuccessExpected)
{
    int server_port = GetFreePort();

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/test.txt", [](auto* response, auto* request) { response->end("test_content"); })
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
            .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
            .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");
    WriteToFile(install_path / "cstrike/cl_dlls/client_mini.dll", "old content");

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             nitro_utils::replace_all_copy(
                 R"(
{
    "hostnames": ["http://localhost:<server_port>/branch/test"],
    "test": {
        "filename": "test.txt",
        "size": 12,
        "hash": "27565f9a57c128674736aa644012ce67"
    },
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
)",
                 "<server_port>",
                 std::to_string(server_port)
             )
         )}
    });

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Updated);

    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "aa");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/client_mini.dll"), "bb");
    EXPECT_EQ(ReadFromFile(install_path / "nitro_api.dll"), "cc");
    EXPECT_EQ(ReadFromFile(install_path / "cstrike/cl_dlls/GameUI.dll"), "dd");
    EXPECT_FALSE(std::filesystem::exists(backup_path));
}

TEST_F(NextUpdaterTest, UpdateWithHostnamesFailoverExpected)
{
    int server_port = GetFreePort();

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/test.txt", [](auto* response, auto* request) { response->end("test_content"); })
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .get("/branch/test/cstrike/cl_dlls/client_mini.dll", [](auto* response, auto* request) { response->end("bb"); })
            .get("/branch/test/nitro_api.dll", [](auto* response, auto* request) { response->end("cc"); })
            .get("/branch/test/cstrike/cl_dlls/GameUI.dll", [](auto* response, auto* request) { response->end("dd"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");

    int bad_port = GetFreePort();

    std::string json = R"(
{
    "hostnames": [
        "http://localhost:<bad_port>/branch/test",
        "http://localhost:<server_port>/branch/test"
    ],
    "test": {
        "filename": "test.txt",
        "size": 12,
        "hash": "27565f9a57c128674736aa644012ce67"
    },
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)";
    nitro_utils::replace_all(json, "<bad_port>", std::to_string(bad_port));
    nitro_utils::replace_all(json, "<server_port>", std::to_string(server_port));

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update", HttpResponse(200, cpr::Error(), json)}
    });

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Updated);
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "aa");
    EXPECT_FALSE(std::filesystem::exists(backup_path));
}

TEST_F(NextUpdaterTest, UpdateWithHostnamesWithoutTestExpected)
{
    int server_port = GetFreePort();

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             nitro_utils::replace_all_copy(
                 R"(
{
    "hostnames": ["http://localhost:<server_port>/branch/test"],
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)",
                 "<server_port>",
                 std::to_string(server_port)
             )
         )}
    });

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Updated);
    EXPECT_EQ(ReadFromFile(install_path / "next_engine_mini.dll"), "aa");
    EXPECT_FALSE(std::filesystem::exists(backup_path));
}

TEST_F(NextUpdaterTest, AllHostnamesErrorExpected)
{
    int bad_port1 = GetFreePort();
    int bad_port2 = GetFreePort();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");

    std::string json = R"(
{
    "hostnames": [
        "http://localhost:<bad_port1>/branch/test",
        "http://localhost:<bad_port2>/branch/test"
    ],
    "test": {
        "filename": "test.txt",
        "size": 12,
        "hash": "27565f9a57c128674736aa644012ce67"
    },
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)";
    nitro_utils::replace_all(json, "<bad_port1>", std::to_string(bad_port1));
    nitro_utils::replace_all(json, "<bad_port2>", std::to_string(bad_port2));

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update", HttpResponse(200, cpr::Error(), json)}
    });

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Error);
}

TEST_F(NextUpdaterTest, NoHostnameAndNoHostnamesErrorExpected)
{
    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             R"(
{
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)"
         )}
    });

    NextUpdater next_updater(install_path, backup_path, http_service, [this](const NextUpdaterEvent& event) {});
    NextUpdaterResult updater_result = next_updater.Start().get();

    EXPECT_EQ(updater_result, NextUpdaterResult::Error);
}

TEST_F(NextUpdaterTest, CancelBeforeStartExpected)
{
    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{});

    NextUpdater next_updater(install_path, backup_path, http_service, [](const NextUpdaterEvent& event) {});
    next_updater.Cancel();

    NextUpdaterResult updater_result = next_updater.Start().get();
    EXPECT_EQ(updater_result, NextUpdaterResult::CanceledByUser);
}

TEST_F(NextUpdaterTest, CancelDuringSelectBaseUrlExpected)
{
    int bad_port = GetFreePort();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");

    std::string json = R"(
{
    "hostnames": [
        "http://localhost:<bad_port>/branch/test"
    ],
    "test": {
        "filename": "test.txt",
        "size": 12,
        "hash": "27565f9a57c128674736aa644012ce67"
    },
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)";
    nitro_utils::replace_all(json, "<bad_port>", std::to_string(bad_port));

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update", HttpResponse(200, cpr::Error(), json)}
    });

    NextUpdater next_updater(install_path, backup_path, http_service,
        [&next_updater](const NextUpdaterEvent& event) {
            if (event.state == NextUpdaterState::RequestingFileList)
                next_updater.Cancel();
        });

    NextUpdaterResult updater_result = next_updater.Start().get();
    EXPECT_EQ(updater_result, NextUpdaterResult::CanceledByUser);
}

TEST_F(NextUpdaterTest, CancelDuringBackupExpected)
{
    int server_port = GetFreePort();

    std::promise<void> server_ready;
    auto server_ready_future = server_ready.get_future();

    std::thread server_thread([server_port, &server_ready] {
        uWS::App()
            .get("/branch/test/next_engine_mini.dll", [](auto* response, auto* request) { response->end("aa"); })
            .listen(server_port, [&server_ready](us_listen_socket_t* listenSocket) { server_ready.set_value(); })
            .run();
    });
    server_thread.detach();
    server_ready_future.wait();

    auto install_path = CreateTempDir("ncl_launcher_test_install_folder");
    auto backup_path = CreateTempDir("ncl_launcher_test_backup_folder");

    WriteToFile(install_path / "next_engine_mini.dll", "old content");

    auto http_service = std::make_shared<HttpServiceMock>(std::unordered_map<std::string, HttpResponse>{
        {"launcher_update",
         HttpResponse(
             200,
             cpr::Error(),
             nitro_utils::replace_all_copy(
                 R"(
{
    "hostname": "http://localhost:<server_port>/branch/test",
    "files":[
        {"filename": "next_engine_mini.dll",
         "hash": "4124bc0a9335c27f086f24ba207a4912",
         "size": 2}
    ]
}
)",
                 "<server_port>",
                 std::to_string(server_port)
             )
         )}
    });

    NextUpdater next_updater(install_path, backup_path, http_service,
        [&next_updater](const NextUpdaterEvent& event) {
            if (event.state == NextUpdaterState::Backuping)
                next_updater.Cancel();
        });

    NextUpdaterResult updater_result = next_updater.Start().get();
    EXPECT_EQ(updater_result, NextUpdaterResult::CanceledByUser);
}

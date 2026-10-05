#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "DiscordSession.h"

// Runs DiscordSession over the real IPC on its own thread, so a Discord that stops reading
// can only stall that thread, never the game's frame
class DiscordWorker
{
    std::string app_id_{};
    std::thread thread_{};

    std::mutex mutex_{};
    std::condition_variable wake_{};
    bool stop_{};
    bool has_activity_{};
    std::string activity_{};
    std::string fallback_{};
    std::vector<DiscordEvent> events_{};

#ifdef _WIN32
    // A real handle to the worker thread, for cancelling a WriteFile stuck on a pipe nobody reads
    std::atomic<void*> thread_handle_{};
#endif
    std::atomic<bool> finished_{};

    void Run();

public:
    explicit DiscordWorker(std::string app_id);
    ~DiscordWorker();

    DiscordWorker(const DiscordWorker&) = delete;
    DiscordWorker& operator=(const DiscordWorker&) = delete;

    // Both do nothing when the worker is already in that state
    void Start();
    void Stop();

    // Main thread hands over the newest activity; the worker only keeps the latest one
    void SetActivity(std::string activity, std::string fallback);

    // Join requests and log lines for the main thread, which owns the console and the engine
    std::vector<DiscordEvent> TakeEvents();
};

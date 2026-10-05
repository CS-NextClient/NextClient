#include "DiscordWorker.h"

#include <chrono>
#include <exception>
#include <system_error>
#include <utility>

#include "DiscordIpc.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#else
#include <unistd.h>
#endif

namespace
{
    // How long the worker sleeps on an open connection when Discord has nothing to say
    constexpr int kWaitMs = 100;
    constexpr std::chrono::milliseconds kDisconnectedWait{250};

    int GetCurrentPid()
    {
#ifdef _WIN32
        return _getpid();
#else
        return getpid();
#endif
    }
} // namespace

DiscordWorker::DiscordWorker(std::string app_id) :
    app_id_(std::move(app_id))
{}

DiscordWorker::~DiscordWorker()
{
    Stop();
}

void DiscordWorker::Start()
{
    if (thread_.joinable())
    {
        return;
    }

    {
        std::lock_guard lock(mutex_);
        stop_ = false;
        // A new session starts from scratch, so it needs the last activity again
        has_activity_ = !activity_.empty();
    }

    finished_ = false;

    try
    {
        thread_ = std::thread(&DiscordWorker::Run, this);
    }
    catch (const std::system_error& e)
    {
        std::lock_guard lock(mutex_);
        events_.push_back({DiscordEvent::Type::Log, std::string("could not start the worker thread: ") + e.what()});
    }
}

void DiscordWorker::Stop()
{
    if (!thread_.joinable())
    {
        return;
    }

    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();

#ifdef _WIN32
    // A synchronous WriteFile waits for as long as Discord does not read, and only this gets it out
    while (!finished_)
    {
        if (void* handle = thread_handle_.load())
        {
            CancelSynchronousIo(handle);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
#endif

    thread_.join();

#ifdef _WIN32
    if (void* handle = thread_handle_.exchange(nullptr))
    {
        CloseHandle(handle);
    }
#endif
}

void DiscordWorker::SetActivity(std::string activity, std::string fallback)
{
    std::lock_guard lock(mutex_);
    activity_ = std::move(activity);
    fallback_ = std::move(fallback);
    has_activity_ = true;
}

std::vector<DiscordEvent> DiscordWorker::TakeEvents()
{
    std::lock_guard lock(mutex_);
    return std::exchange(events_, {});
}

void DiscordWorker::Run()
{
#ifdef _WIN32
    HANDLE handle = nullptr;
    if (DuplicateHandle(GetCurrentProcess(), GetCurrentThread(), GetCurrentProcess(), &handle, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {
        thread_handle_ = handle;
    }
#endif

    try
    {
        DiscordIpc ipc;
        DiscordSession session(ipc, app_id_, GetCurrentPid());
        auto start = std::chrono::steady_clock::now();

        for (;;)
        {
            {
                std::lock_guard lock(mutex_);

                if (stop_)
                {
                    break;
                }

                if (has_activity_)
                {
                    session.SetActivity(activity_, fallback_);
                    has_activity_ = false;
                }
            }

            session.Tick(std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count());

            std::vector<DiscordEvent> events = session.TakeEvents();
            if (!events.empty())
            {
                std::lock_guard lock(mutex_);
                events_.insert(events_.end(), std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
            }

            if (ipc.is_open())
            {
                ipc.Wait(kWaitMs);
            }
            else
            {
                std::unique_lock lock(mutex_);
                wake_.wait_for(lock, kDisconnectedWait, [this] { return stop_; });
            }
        }
    }
    catch (const std::exception& e)
    {
        std::lock_guard lock(mutex_);
        events_.push_back({DiscordEvent::Type::Log, std::string("worker stopped: ") + e.what()});
    }

    finished_ = true;
}

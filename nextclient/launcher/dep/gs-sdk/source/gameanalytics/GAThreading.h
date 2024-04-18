//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#pragma once


#include <functional>
#include <atomic>
#if USE_TIZEN
#include <Ecore.h>
#else
#include <vector>
#include <chrono>
#include <memory>
#include <future>
#include <mutex>
#include <algorithm>
#endif

namespace gameanalytics
{
    namespace threading
    {
        class GAThreading
        {
         public:

            typedef std::function<void()> Block;

            static void performTaskOnGAThread(const Block& taskBlock);

            // timers
            static void scheduleTimer(double interval, const Block& callback);

            static void endThread();

            static bool isThreadFinished();

            static bool isThreadEnding();

         private:

#if USE_TIZEN
            struct BlockHolder
            {
                BlockHolder() {}

                BlockHolder(const Block& block) :block(block) {}

                Block block;
            };

            static Eina_Bool _scheduled_function(void* data);
            static void _perform_task_function(void* data, Ecore_Thread* thread);
            static void _end_function(void* data, Ecore_Thread* thread);

            static std::atomic<bool> initialized;
            static void initIfNeeded();
#else
            //timers
            struct TimedBlock
            {
                typedef std::chrono::steady_clock::time_point time_point;

                TimedBlock() {}

                TimedBlock(const Block& block, const time_point& deadline) :block(block), deadline(deadline) {}

                Block block;
                time_point deadline;

                bool operator < (const TimedBlock& o) const
                {
                    // the priority_queue orders the items descending. but we want to retrieve the items in ascending order.
                    return deadline > o.deadline;
                }
            };

            typedef std::vector<TimedBlock> TimedBlocks;
            typedef void (*start_routine) (std::atomic<bool>&, std::atomic_llong&);

            struct State
            {
                State()
                {
                    std::make_heap(blocks.begin(), blocks.end());
                    scheduledBlock = { {}, std::chrono::steady_clock::now() };
                    hasScheduledBlockRun = true;
                }

                void setThread(start_routine routine, std::atomic<bool>& endThread, std::atomic_llong& threadDeadline)
                {
                    handle = std::async(std::launch::async, routine, std::ref(endThread), std::ref(threadDeadline));
                }

                bool isThreadFinished()
                {
                    return !handle.valid() || handle.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
                }

                ~State()
                {
                    endThread();

                    while (!isThreadFinished())
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                }

                TimedBlocks blocks;
                TimedBlock scheduledBlock;
                bool hasScheduledBlockRun;
                std::mutex mutex;
                std::future<void> handle;
            };

            static std::atomic<bool> _endThread;
            static std::atomic_llong _threadDeadline;
            static std::unique_ptr<State> state;

            static long long getTimeInNs();
            static long long getTimeInNs(double delay);

            //< The function that's running in the gaThread
            static void thread_routine(std::atomic<bool>& endThread, std::atomic_llong& threadDeadline);
            /*!
            retrieves the next block to execute.
            This will either be a regular Block or a Timed Block.
            return true, if a Block is retrieved, false if a TimedBlock is retrieved.
            */
            static bool getNextBlock(TimedBlock& timedBlock);
            static bool getScheduledBlock(TimedBlock& timedBlock);
            static void runBlocks();
#endif
        };
    }
}

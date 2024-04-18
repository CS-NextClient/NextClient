//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GAThreading.h"
#include <cassert>
#include <algorithm>
#include <stdexcept>
#include "GALogger.h"
#include <thread>
#include <exception>

namespace gameanalytics
{
    namespace threading
    {
        // static members
        std::atomic<bool> GAThreading::_endThread(false);
        std::atomic_llong GAThreading::_threadDeadline(GAThreading::getTimeInNs());
        std::unique_ptr<GAThreading::State> GAThreading::state(new GAThreading::State());

        void GAThreading::scheduleTimer(double interval, const Block& callback)
        {
            if(_endThread)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(state->mutex);

            if(state->hasScheduledBlockRun)
            {
                state->scheduledBlock = { callback, std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(1000 * interval)) };
                state->hasScheduledBlockRun = false;
                GAThreading::_threadDeadline = GAThreading::getTimeInNs(interval + 2.0);
                if(state->isThreadFinished())
                {
                    state->setThread(GAThreading::thread_routine, GAThreading::_endThread, GAThreading::_threadDeadline);
                }
            }
        }

        void GAThreading::performTaskOnGAThread(const Block& taskBlock)
        {
            if(_endThread)
            {
                return;
            }
            std::lock_guard<std::mutex> lock(state->mutex);
            state->blocks.push_back({ taskBlock, std::chrono::steady_clock::now()} );
            std::push_heap(state->blocks.begin(), state->blocks.end());
            GAThreading::_threadDeadline = GAThreading::getTimeInNs(10.0);
            if(state->isThreadFinished())
            {
                state->setThread(GAThreading::thread_routine, GAThreading::_endThread, GAThreading::_threadDeadline);
            }
        }

        void GAThreading::endThread()
        {
            _endThread = true;
        }

        bool GAThreading::isThreadFinished()
        {
            return state->isThreadFinished();
        }

        bool GAThreading::isThreadEnding()
        {
            return _endThread;
        }

        bool GAThreading::getNextBlock(TimedBlock& timedBlock)
        {
            std::lock_guard<std::mutex> lock(state->mutex);

            if((!state->blocks.empty() && state->blocks.front().deadline <= std::chrono::steady_clock::now()))
            {
                timedBlock = state->blocks.front();
                std::pop_heap(state->blocks.begin(), state->blocks.end());
                state->blocks.pop_back();
                return true;
            }

            return false;
        }

        bool GAThreading::getScheduledBlock(TimedBlock& timedBlock)
        {
            std::lock_guard<std::mutex> lock(state->mutex);

            if(!state->hasScheduledBlockRun && state->scheduledBlock.deadline <= std::chrono::steady_clock::now())
            {
                state->hasScheduledBlockRun = true;
                timedBlock = state->scheduledBlock;
                return true;
            }

            return false;
        }

        long long GAThreading::getTimeInNs()
        {
            return GAThreading::getTimeInNs(0);
        }

        long long GAThreading::getTimeInNs(double delay)
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>((std::chrono::steady_clock::now() + std::chrono::milliseconds(static_cast<int>(1000 * delay))).time_since_epoch()).count();
        }

        void GAThreading::runBlocks()
        {
            if(!state)
            {
                return;
            }

            TimedBlock timedBlock;

            while (getNextBlock(timedBlock))
            {
                assert(timedBlock.block);
                assert(timedBlock.deadline <= std::chrono::steady_clock::now());
                timedBlock.block();
                // clear the block, so that the assert works
                timedBlock.block = {};
            }

            if(getScheduledBlock(timedBlock))
            {
                assert(timedBlock.block);
                assert(timedBlock.deadline <= std::chrono::steady_clock::now());
                timedBlock.block();
                // clear the block, so that the assert works
                timedBlock.block = {};
            }
        }

        void GAThreading::thread_routine(std::atomic<bool>& endThread, std::atomic_llong& threadDeadline)
        {
            logging::GALogger::d("thread_routine start");

            try
            {
                while (!endThread && threadDeadline >= GAThreading::getTimeInNs())
                {
                    if(!state)
                    {
                        break;
                    }
                    runBlocks();
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                // run any last blocks added
                runBlocks();

                if(!endThread)
                {
                    logging::GALogger::d("thread_routine stopped");
                }
            }
            catch(const std::exception& e)
            {
                if(!endThread)
                {
                    logging::GALogger::e("Error on GA thread");
                    logging::GALogger::e(e.what());
                }
            }
        }
    }
}

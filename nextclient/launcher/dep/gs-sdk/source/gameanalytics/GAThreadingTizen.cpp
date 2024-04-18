#if USE_TIZEN
//
// GA-SDK-CPP
// Copyright 2018 GameAnalytics C++ SDK. All rights reserved.
//

#include "GAThreading.h"
#include "GALogger.h"

namespace gameanalytics
{
    namespace threading
    {
        std::atomic<bool> GAThreading::initialized(false);

        void GAThreading::initIfNeeded()
        {
            if (!initialized)
            {
                ecore_thread_max_set(1);
                initialized = true;
            }
        }

        void GAThreading::scheduleTimer(double interval, const Block& callback)
        {
            initIfNeeded();
            ecore_timer_add(interval, _scheduled_function, new BlockHolder(callback));
        }

        void GAThreading::performTaskOnGAThread(const Block& taskBlock)
        {
            initIfNeeded();
            ecore_thread_run(_perform_task_function, _end_function, NULL, new BlockHolder(taskBlock));
        }

        void GAThreading::endThread()
        {
        }

        bool GAThreading::isThreadFinished()
        {
            return true;
        }

        bool GAThreading::isThreadEnding()
        {
            return false;
        }

        Eina_Bool GAThreading::_scheduled_function(void* data)
        {
            BlockHolder* blockHolder = static_cast<BlockHolder*>(data);

            try
            {
                blockHolder->block();
            }
            catch(const std::exception& e)
            {
                logging::GALogger::e("Error on GA thread");
                logging::GALogger::e(e.what());
            }

            delete blockHolder;
            return ECORE_CALLBACK_DONE;
        }

        void GAThreading::_perform_task_function(void* data, Ecore_Thread* thread)
        {
            BlockHolder* blockHolder = static_cast<BlockHolder*>(data);

            try
            {
                blockHolder->block();
            }
            catch(const std::exception& e)
            {
                logging::GALogger::e("Error on GA thread");
                logging::GALogger::e(e.what());
            }

            delete blockHolder;
        }

        void GAThreading::_end_function(void* data, Ecore_Thread* thread)
        {
            //logging::GALogger::d("GAThreading::_perform_task_function has finished");
        }
    }
}
#endif

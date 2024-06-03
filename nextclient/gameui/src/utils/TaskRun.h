#pragma once

#include <memory>

#include "Result.h"
#include "TaskRunImpl.h"

class TaskRun
{
    static std::shared_ptr<TaskRunImpl> task_impl_;

public:
    static bool IsInitialized() { return task_impl_ != nullptr; }

    static void Initialize(std::shared_ptr<TaskRunImpl> task_run_impl)
    {
        task_impl_ = task_run_impl;
    }

    static void UnInitialize()
    {
        task_impl_ = nullptr;
    }

    template<class callable_type, class... argument_types>
    static auto RunInMainThreadAndWait(callable_type&& callable, argument_types&&... arguments)
    {
        return task_impl_->RunInMainThreadAndWait(std::forward<callable_type>(callable), std::forward<argument_types>(arguments)...);
    }

    template<class callable_type, class... argument_types>
    static Result RunInMainThread(callable_type&& callable, argument_types&&... arguments)
    {
        return task_impl_->RunInMainThread(std::forward<callable_type>(callable), std::forward<argument_types>(arguments)...);
    }

};

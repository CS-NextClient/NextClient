#include "TaskRunImpl.h"

TaskCoroImpl::TaskCoroImpl()
{
    ccruntime_ = std::make_unique<concurrencpp::runtime>();
    update_executor_ = ccruntime_->make_manual_executor();
}

void TaskCoroImpl::OnUpdate()
{
    if (update_executor_ && !update_executor_->shutdown_requested())
        update_executor_->loop_once();
}

void TaskCoroImpl::OnShutdown()
{
    if (update_executor_ && !update_executor_->shutdown_requested())
    {
        update_executor_->shutdown();
        while (!update_executor_->empty())
            update_executor_->loop_once();
    }
}

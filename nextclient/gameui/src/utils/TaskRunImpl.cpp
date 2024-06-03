#include "TaskRunImpl.h"

TaskRunImpl::TaskRunImpl()
{
    ccruntime_ = std::make_unique<concurrencpp::runtime>();
    update_executor_ = ccruntime_->make_manual_executor();
}

void TaskRunImpl::OnUpdate()
{
    if (update_executor_ && !update_executor_->shutdown_requested())
        update_executor_->loop_once();
}

void TaskRunImpl::OnShutdown()
{
    if (update_executor_ && !update_executor_->shutdown_requested())
    {
        update_executor_->shutdown();
        while (!update_executor_->empty())
            update_executor_->loop_once();
    }
}

#include "TaskRunImpl.h"

TaskRunImpl::TaskRunImpl(nitroapi::NitroApiInterface* nitro_api) :
    nitroapi::NitroApiHelper(nitro_api)
{
    ccruntime_ = std::make_unique<concurrencpp::runtime>();
    update_executor_ = ccruntime_->make_manual_executor();

    DeferUnsub(eng()->Host_FilterTime += [this](float delta, int result) { if (result) OnUpdate(); });
    DeferUnsub(eng()->Host_Shutdown |= [this](const auto& next) {  OnShutdown(); next->Invoke(); });
}

TaskRunImpl::~TaskRunImpl()
{
    OnShutdown();
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

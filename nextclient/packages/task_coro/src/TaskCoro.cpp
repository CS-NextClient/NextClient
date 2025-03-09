#include <memory>
#include <taskcoro/TaskCoroImpl.h>
#include <taskcoro/TaskCoro.h>

using namespace taskcoro;

std::shared_ptr<TaskCoroImpl> TaskCoro::task_impl_;

#include "../engine.h"

void NotifyDedicatedServerUI(const char* message)
{
    eng()->NotifyDedicatedServerUI.InvokeChained(message);
}

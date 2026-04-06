#pragma once
#include <string>

namespace hwid
{
    std::string Collect();

    bool IsReady();

    void Reset();
}

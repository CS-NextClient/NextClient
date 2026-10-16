#pragma once

#include <memory>
#include <vector>

namespace nitroapi
{
    class NitroApiInterface;
    class Unsubscriber;
} // namespace nitroapi

// Replaces the stock chat's wrapping of a line too wide for the screen.
void SayTextWrap_Subscribe(nitroapi::NitroApiInterface* nitro_api, std::vector<std::shared_ptr<nitroapi::Unsubscriber>>& unsub);

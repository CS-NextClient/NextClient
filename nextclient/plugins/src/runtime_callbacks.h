#pragma once
#include <array>
#include <cstddef>

namespace plugins::runtime
{
    // clang-format off
#define NC_CALLBACKS(X) \
    X(Event, event, true) \
    X(Frame, frame, true) \
    X(Draw, draw, true) \
    X(Command, command, true) \
    X(Filter, filter, true) \
    X(Module, module, false) \
    X(Entry, entry, false) \
    X(Load, load, false) \
    X(Unload, unload, false) \
    X(ModuleUnload, module_unload, false) \
    X(Setting, setting, false) \
    X(Action, action, false) \
    X(Console, console, false)
    enum class CallbackCategory
    {
#define NC_CATEGORY(id, name, budgeted) id,
        NC_CALLBACKS(NC_CATEGORY)
#undef NC_CATEGORY
        Count
    };
    inline constexpr size_t callback_count = static_cast<size_t>(CallbackCategory::Count);
    struct CallbackDescriptor
    {
        const char* name;
        bool budgeted;
    };
    inline constexpr std::array<CallbackDescriptor, callback_count> callback_descriptors{{
#define NC_DESCRIPTOR(id, name, budgeted) {#name, budgeted},
        NC_CALLBACKS(NC_DESCRIPTOR)
#undef NC_DESCRIPTOR
    }};
#undef NC_CALLBACKS
    // clang-format on
    inline constexpr const CallbackDescriptor& callback_descriptor(CallbackCategory category)
    {
        return callback_descriptors[static_cast<size_t>(category)];
    }
} // namespace plugins::runtime

#include "runtime_internal.h"
#include "runtime_schedule.h"
#include <nextclient/events.h>
#include <cmath>
#include <chrono>

namespace plugins::runtime
{
    bool enqueue(Loaded& p, Event event)
    {
        if (!permitted(&p, event.permission) || !p.api.event)
            return false;
        if (p.pending.size() >= 2048 || p.pending_bytes + event.json.size() > 1024 * 1024)
        {
            ++p.dropped;
            if (!p.first_dropped)
                p.first_dropped = event.serial;
            p.last_dropped = event.serial;
            return false;
        }
        try
        {
            event.memory.resize(
                sizeof(Event) * 2 + 128 + string_memory(event.json) + string_memory(event.name) + string_memory(event.cvar)
            );
        }
        catch (...)
        {
            ++p.dropped;
            if (!p.first_dropped)
                p.first_dropped = event.serial;
            p.last_dropped = event.serial;
            return false;
        }
        p.pending_bytes += event.json.size();
        p.pending.push_back(std::move(event));
        return true;
    }
    void notify(Loaded& p, const char* name, const Json& value, uint32_t permission)
    {
        enqueue(p, {++event_serial, name, tao::json::to_string(value), {}, permission, true});
    }
    void fail(Loaded& p)
    {
        p.host.log(p.host.context, "Callback failed; callbacks disabled until restart.");
        p.failed = true;
        // Successful dependencies always precede their dependents, so one forward
        // pass retires the entire dependent chain before another callback can run.
        for (auto& dependent : loaded)
            for (const auto& relation : dependent->item.manifest.required)
                for (const auto& provider : loaded)
                    if (provider->failed && provider->item.manifest.id == relation.id)
                        dependent->failed = true;
        for (auto& plugin : loaded)
            if (plugin->failed)
                retire(*plugin);
        retire(p); // Also handles a plugin still inside initialization.
    }
    void retire(Loaded& p)
    {
        if (p.retired)
            return;
        p.failed = p.retired = true;
        extensions_detach(p); // Invalidates cancellation tokens before freeing data.
        p.pending.clear();
        p.pending.shrink_to_fit();
        p.pending_bytes = 0;
        p.results.clear();
        p.store = p.values = p.windows = tao::json::empty_object;
        p.tabs = p.controls = p.settings = tao::json::empty_array;
        p.events.clear();
        p.cvar_watches.clear();
        p.messages.clear();
        p.filters.clear();
        p.commands.clear();
        p.commands.shrink_to_fit();
        p.cvars.clear();
        p.cvars.shrink_to_fit();
        p.store_pending = false;
        p.hidden_ui = 0;
        p.values_memory.resize(0);
        p.store_memory.resize(0);
        p.ui_memory.resize(0);
        p.registration_memory.resize(0);
        p.result_memory.resize(0);
        ++p.ui_revision;
    }
} // namespace plugins::runtime

using namespace plugins;
using namespace plugins::runtime;

void nc_runtime_command(NcCommand* command, const NcPlayer* player)
{
    static size_t cursor{};
    const auto preferred =
        preferred_callback(cursor, CallbackCategory::Command, [](const Loaded& p) { return !p.failed && p.api.command; });
    for (size_t index = 0; index < loaded.size(); ++index)
    {
        auto& p = loaded[index];
        if (!p->failed && p->api.command && ordered_callback_budget(index, preferred))
        {
            auto before = *command;
            int result = invoke(*p, CallbackCategory::Command, [&] { return p->api.command(command, player); });
            bool finite = std::isfinite(command->forward_move) && std::isfinite(command->side_move) && std::isfinite(command->up_move);
            for (auto f : command->view_angles)
                finite &= std::isfinite(f);
            if (result != 0 || !finite || command->size != sizeof(NcCommand) || command->buttons > 65535)
            {
                *command = before;
                fail(*p);
            }
            if (!permitted(p.get(), NC_PERMISSION_PLAYER_WRITE))
                *command = before;
        }
    }
}

void nc_runtime_bind_client(const NcClientServices* value)
{
    services = value ? *value : NcClientServices{};
    for (auto& p : loaded)
    {
        if (!p->failed && !install_commands(*p))
            fail(*p);
        if (!p->failed && services.watch_message)
        {
            for (const auto& name : p->messages)
                services.watch_message(name.c_str());
            for (const auto& [name, filter] : p->filters)
                services.watch_message(name.c_str());
        }
    }
}
void nc_runtime_console(int32_t argc, const char* const* argv)
{
    if (GetCurrentThreadId() != main_thread || argc < 1 || argc > 64 || !argv || !argv[0])
        return;
    for (auto& p : loaded)
        if (!p->failed && p->api.console_command)
            for (const auto& id : p->commands)
                if (!_stricmp(("nc." + p->item.manifest.id + "." + id).c_str(), argv[0]))
                {
                    if (invoke(*p, CallbackCategory::Console, [&] { return p->api.console_command(id.c_str(), argc - 1, argv + 1); }) != 0)
                        fail(*p);
                    return;
                }
}
void nc_runtime_draw(const NcDrawContext* context)
{
    if (GetCurrentThreadId() != main_thread || !context || context->size != sizeof(*context))
        return;
    static size_t cursor{};
    struct Batch
    {
        Budget memory;
        std::vector<DrawOperation> operations;
    };
    struct DrawingGuard
    {
        ~DrawingGuard()
        {
            drawing = nullptr;
            std::vector<DrawOperation>().swap(draw_operations);
            draw_text_bytes = 0;
        }
    } guard;
    try
    {
        Budget batches_memory(loaded.size() * sizeof(Batch) * 2);
        std::vector<Batch> batches(loaded.size());
        const auto start = std::chrono::steady_clock::now();
        const auto first = cursor;
        for (size_t count = 0; count < loaded.size(); ++count)
        {
            const auto index = (first + count) % loaded.size();
            auto& p = loaded[index];
            if (!permitted(p.get(), NC_PERMISSION_UI_DRAW) || !p->api.draw)
                continue;
            if (!pass_budget(CallbackCategory::Draw, start))
            {
                ++deferred_callbacks;
                continue;
            }
            cursor = index + 1;
            Budget drawing_memory;
            try
            {
                drawing_memory.resize(1024 * 1024);
            }
            catch (...)
            {
                continue;
            }
            draw_text_bytes = 0;
            drawing = p.get();
            const int result = invoke(*p, CallbackCategory::Draw, [&] { return p->api.draw(context); });
            drawing = nullptr;
            if (result != 0)
            {
                fail(*p);
                std::vector<DrawOperation>().swap(draw_operations);
            }
            else
            {
                auto& batch = batches[index];
                batch.operations.swap(draw_operations);
                size_t bytes = batch.operations.capacity() * sizeof(DrawOperation) * 2;
                for (const auto& op : batch.operations)
                    bytes += string_memory(op.text);
                drawing_memory.resize(bytes);
                batch.memory = std::move(drawing_memory);
            }
        }
        // Callback scheduling may rotate, but overlapping HUD output must always
        // preserve the configured load order. Failed owners contribute no output.
        for (size_t index = 0; index < batches.size(); ++index)
            if (!loaded[index]->failed)
                for (const auto& op : batches[index].operations)
                {
                    if (op.is_text && services.draw_text)
                        services.draw_text(op.x, op.y, op.text.c_str(), op.rgba);
                    else if (!op.is_text && services.draw_rect)
                        services.draw_rect(op.x, op.y, op.width, op.height, op.rgba);
                }
    }
    catch (...)
    {} // Resource pressure must not propagate into the client drawing hook.
}
void nc_runtime_frame(const NcSession* context)
{
    if (GetCurrentThreadId() != main_thread || !context || context->size != sizeof(*context))
        return;
    if (dispatching_events)
        return;
    dispatching_events = true;
    struct DispatchGuard
    {
        ~DispatchGuard()
        {
            dispatching_events = false;
        }
    } guard;
    reset_callback_budget();
    extensions_pump();
    const auto last_serial = event_serial;
    const auto start = std::chrono::steady_clock::now();
    static size_t event_cursor{}, frame_cursor{};
    std::vector<size_t> counts(loaded.size());
    size_t idle{};
    for (size_t total = 0; !loaded.empty() && total < 512 && idle < loaded.size();)
    {
        if (!pass_budget(CallbackCategory::Event, start))
            break;
        const auto index = event_cursor++ % loaded.size();
        auto& p = loaded[index];
        if (!available(p.get()) || counts[index] >= 128)
        {
            ++idle;
            continue;
        }
        if (available(p.get()) && p->api.event && p->first_dropped)
        {
            auto loss = tao::json::to_string(Json{{"dropped", p->dropped}, {"first", p->first_dropped}, {"last", p->last_dropped}});
            p->first_dropped = p->last_dropped = 0;
            if (invoke(*p, CallbackCategory::Event, [&] { return p->api.event("sdk.overflow", loss.c_str()); }) != 0)
                fail(*p);
            ++counts[index];
            ++total;
            idle = 0;
            continue;
        }
        if (!p->pending.empty() && !p->failed && p->pending.front().serial <= last_serial)
        {
            ++counts[index];
            ++total;
            idle = 0;
            auto event = std::move(p->pending.front());
            p->pending.pop_front();
            p->pending_bytes -= event.json.size();
            auto& subscriptions = event.cvar.empty() ? p->events : p->cvar_watches;
            auto found = subscriptions.find(event.cvar.empty() ? event.name : event.cvar);
            if ((event.direct || (found != subscriptions.end() && event.serial >= found->second)) && permitted(p.get(), event.permission) &&
                (!event.service_request || Services_IsRequestPending(*p, event.service_request)) && p->api.event)
            {
                const auto result =
                    invoke(*p, CallbackCategory::Event, [&] { return p->api.event(event.name.c_str(), event.json.c_str()); });
                ++p->delivered;
                if (result != 0)
                    fail(*p);
            }
        }
        else
            ++idle;
    }
    // Count actual eligible callbacks left waiting, including work deferred by
    // either deadline and the event-count caps. A queued event can wait on more
    // than one pump; each such deferral is counted separately.
    for (const auto& p : loaded)
        if (available(p.get()) && p->api.event)
        {
            if (p->first_dropped)
                ++deferred_callbacks;
            for (const auto& event : p->pending)
            {
                if (event.serial > last_serial)
                    break;
                const auto& subscriptions = event.cvar.empty() ? p->events : p->cvar_watches;
                const auto found = subscriptions.find(event.cvar.empty() ? event.name : event.cvar);
                if ((event.direct || (found != subscriptions.end() && event.serial >= found->second)) &&
                    permitted(p.get(), event.permission) &&
                    (!event.service_request || Services_IsRequestPending(*p, event.service_request)))
                {
                    ++deferred_callbacks;
                }
            }
        }
    const auto frame_start = std::chrono::steady_clock::now();
    const auto first = frame_cursor;
    for (size_t count = 0; count < loaded.size(); ++count)
    {
        const auto index = (first + count) % loaded.size();
        auto& p = loaded[index];
        if (p->failed || !p->api.frame)
            continue;
        if (!pass_budget(CallbackCategory::Frame, frame_start))
        {
            ++deferred_callbacks;
            continue;
        }
        frame_cursor = index + 1;
        if (invoke(*p, CallbackCategory::Frame, [&] { return p->api.frame(context); }) != 0)
            fail(*p);
    }
}
void nc_runtime_pump()
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    NcSession session{};
    session.size = sizeof(session);
    if (services.get_session)
        services.get_session(&session);
    nc_runtime_frame(&session);
}
int32_t nc_runtime_ui_hidden(uint32_t element)
{
    for (const auto& p : loaded)
        if (permitted(p.get(), NC_PERMISSION_UI_HIDE) && (p->hidden_ui & element))
            return 1;
    return 0;
}

void nc_runtime_event(const char* raw, const char* payload)
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    try
    {
        const auto name = text(raw, 64);
        if (event_permission(name) == UINT32_MAX || name == "cvar.changed")
            return;
        auto json = text(payload, 65536);
        parse(json);
        const auto serial = ++event_serial;
        for (auto& p : loaded)
            if (auto it = p->events.find(name); it != p->events.end() && serial >= it->second)
                enqueue(*p, {serial, name, json, {}, event_permission(name)});
    }
    catch (...)
    {}
}
void nc_runtime_cvar_changed(const char* raw, const char* before, const char* after)
{
    if (GetCurrentThreadId() != main_thread || !started)
        return;
    try
    {
        const auto name = cvar_name(raw), old_value = text(before, 65535), new_value = text(after, 65535);
        if (name.empty() || old_value == new_value)
            return;
        auto json = tao::json::to_string(Json{{"name", name}, {"old", old_value}, {"value", new_value}});
        const auto serial = ++event_serial;
        for (auto& p : loaded)
            if (auto it = p->cvar_watches.find(name); it != p->cvar_watches.end() && serial >= it->second)
                enqueue(*p, {serial, "cvar.changed", json, name, NC_PERMISSION_CVARS_READ});
    }
    catch (...)
    {}
}

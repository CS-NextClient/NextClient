#include <nextclient/plugin.hpp>
#include <windows.h>
#include <array>
#include <cstring>

NC_MANIFEST(
    "{\"schema\":1,\"id\":\"" NC_DISPATCH_ID
    "\",\"name\":\"Dispatch\",\"author\":\"Tests\",\"description\":\"Scheduler fixture\","
    "\"version\":\"1.0.0\",\"sdk\":\"1.0.0\",\"abi\":1,\"api\":1,\"compatibility_revision\":1,"
    "\"permissions\":[\"ui.draw\",\"player.write\",\"messages.read\",\"messages.filter\"]}"
)
namespace
{
    enum Category
    {
        Event,
        Frame,
        Draw,
        Command,
        Filter,
        Count
    };
    std::array<int, Count> calls{}, delays{};
    const NcHost* host{};
    void record(Category category)
    {
        ++calls[category];
        if (delays[category])
            Sleep(static_cast<DWORD>(delays[category]));
    }
    int32_t NC_CALL filter(void*, const char*, const uint8_t* input, uint32_t size, uint8_t* output, uint32_t* capacity)
    {
        record(Filter);
        if (size != 6 || *capacity < size)
            return -1;
        std::memcpy(output, input, size);
        output[0] = static_cast<uint8_t>(input[0] * 10 + NC_DISPATCH_COLOR);
        *capacity = size;
        return 1;
    }
} // namespace
extern "C" NC_EXPORT const NcHost* NC_CALL nc_dispatch_host()
{
    return host;
}
extern "C" NC_EXPORT void NC_CALL nc_dispatch_delay(int category, int milliseconds)
{
    delays.at(category) = milliseconds;
}
extern "C" NC_EXPORT int NC_CALL nc_dispatch_count(int category)
{
    return calls.at(category);
}
class Dispatch : public nextclient::Plugin
{
public:
    void load() override
    {
        calls.fill(0);
        delays.fill(0);
        host = host_;
        subscribe_event("player.health", true);
        const auto* messages = host_->query_interface(host_->context, "nextclient.messages", 1);
        if (messages)
            messages->set_filter(host_->context, "ScreenShake", filter, nullptr);
    }
    void event(const char*, const char*) override
    {
        record(Event);
    }
    void frame(const NcSession&) override
    {
        record(Frame);
    }
    void draw(const NcDrawContext&) override
    {
        record(Draw);
        draw_rect(0, 0, 10, 10, NC_DISPATCH_COLOR);
        draw_text(0, 0, NC_DISPATCH_ID, NC_DISPATCH_COLOR);
    }
    void command(NcCommand& value, const NcPlayer&) override
    {
        record(Command);
        value.buttons = value.buttons * 10 + NC_DISPATCH_COLOR;
    }
};
NC_PLUGIN(Dispatch)

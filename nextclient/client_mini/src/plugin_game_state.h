#pragma once
#include <tao/json.hpp>
#include <map>
#include <string>
#include <string_view>
#include <vector>

struct PluginGameEvent
{
    std::string name;
    tao::json::value data;
};

// Copied, bounded protocol state. No engine globals or message-reader side effects.
class PluginGameState
{
public:
    static bool Observes(std::string_view name);
    std::vector<PluginGameEvent> Message(std::string_view name, const void* bytes, size_t size, float time);
    void Reset();
    void RemovePlayer(int index);
    tao::json::value Player(int index) const;
    tao::json::value Weapon(int index) const;
    tao::json::value Ammo() const;
    tao::json::value Match() const;
    int Armor() const;

private:
    bool round_active_{};
    std::map<int, tao::json::value> players_, weapons_, ammo_;
    tao::json::value match_ = tao::json::empty_object;
};

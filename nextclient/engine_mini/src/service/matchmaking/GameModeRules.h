#pragma once

#include <string>
#include <string_view>
#include <vector>

struct GameModeKeyword
{
    // the words, case-folded as the matched texts are, joined by single spaces
    std::string words{};

    // whether the first word may end a longer word, and the last word start one
    bool open_start{};
    bool open_end{};
};

struct GameModeRule
{
    std::string game_mode{};
    int priority{};
    std::vector<GameModeKeyword> keywords{};
    std::vector<GameModeKeyword> maps{};
};

// Guesses the game mode of a server from its own answer; docs/master-server-http-protocol.md describes the rules file
class GameModeRules
{
    std::vector<GameModeRule> rules_{};
    std::string default_game_mode_{};
    bool loaded_{};

public:
    // false leaves no rules
    bool Parse(std::string_view json);
    bool is_loaded() const;

    // The identifier lives as long as the rules
    const char* Detect(std::string_view game_description, std::string_view server_name, std::string_view map_name) const;
};

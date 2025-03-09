#pragma once
#include <string>
#include <vector>

struct SQ_INFO
{
	uint8_t version{};
	std::string hostname{};
	std::string map{};
	std::string game_directory{};
	std::string game_description{};
	int16_t app_id{};
	uint8_t num_players{};
	uint8_t max_players{};
	uint8_t num_of_bots{};
	char type{};
	char os{};
	bool password{};
	bool secure{};
	std::string game_version{};
	uint16_t port{};
	uint64_t steamid{};
	uint16_t tvport{};
	std::string tvname{};
	std::string tags{};
	uint64_t gameid{};
	std::string address{};
};

struct SQ_PLAYER
{
	uint8_t index{};
	std::string player_name{};
	int32_t kills{};
	float time_connected{};
};

typedef std::vector<SQ_PLAYER> SQ_PLAYERS;

struct SQ_RULE
{
	std::string name{};
	std::string value{};
};

typedef std::vector<SQ_RULE> SQ_RULES;

enum SQ_SERVER_REGION
{
    REGION_USA_EAST = 0,
    REGION_USA_WEST,
    REGION_SOUTH_AMERICA,
    REGION_EUROPE,
    REGION_ASIA,
    REGION_AUSTRALIA,
    REGION_MIDDLE_EAST,
    REGION_AFRICA,
    REGION_WORLD = 255
};

typedef std::vector<std::string> SQ_SERVERS;

struct SQ_FILTER_PROPERTIES
{
    bool dedicated{};
    bool secure = true;
    std::string gamedir{};
    std::string map{};
    bool linux{};
    bool empty{};
    bool full{};
    bool proxy{};
    int32_t nappid{};
    bool noplayers{};
    bool white{};
    std::string gametype{};
    std::string gamedata{};
    std::string gamedataor{};
};

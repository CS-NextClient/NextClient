#pragma once

#include <cstddef>
#include <tao/json.hpp>

struct entity_state_s;
struct clientdata_s;
struct playermove_s;

tao::json::value PluginSnapshot_Vector(const float* values, size_t count = 3);
tao::json::value PluginSnapshot_Entity(const entity_state_s& state);
tao::json::value PluginSnapshot_Client(const clientdata_s& state);
tao::json::value PluginSnapshot_Movement(const playermove_s& movement);

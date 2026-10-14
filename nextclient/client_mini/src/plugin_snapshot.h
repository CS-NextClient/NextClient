#pragma once

#include <cstddef>
#include <tao/json.hpp>

#include <goldsrc/fwd.h>

tao::json::value PluginSnapshot_Vector(const float* values, size_t count = 3);
tao::json::value PluginSnapshot_Entity(const goldsrc::entity_state_t& state);
tao::json::value PluginSnapshot_Client(const goldsrc::clientdata_t& state);
tao::json::value PluginSnapshot_Movement(const goldsrc::playermove_t& movement);

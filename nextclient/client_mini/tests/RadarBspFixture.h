#pragma once

#include <filesystem>
#include <string>

#include "hud/radar/map/MapGeometry.h"

bool RadarBspFixture_Load(const std::filesystem::path& path, RadarMapGeometry& out, std::string& error);

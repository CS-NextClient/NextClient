#pragma once
#include <chrono>

constexpr std::chrono::milliseconds kDefaultNextRequestDelay{2000};
constexpr std::chrono::milliseconds kDefaultQueryDelay{850};
constexpr std::chrono::milliseconds kDefaultQueryTimeout{5000};

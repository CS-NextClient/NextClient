#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

class ClientStringRegistry
{
public:
    enum class RegisterResult
    {
        Registered,
        Ignored,
        CapacityExceeded
    };

private:
    std::unordered_map<uint16_t, std::string> entries_;

public:
    const char* Find(uint16_t string_id) const;
    RegisterResult Register(uint16_t string_id, std::string value);
    void Clear();

    const std::unordered_map<uint16_t, std::string>& get_entries() const
    {
        return entries_;
    }
};

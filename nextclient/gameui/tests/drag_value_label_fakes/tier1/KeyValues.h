#pragma once

#include <map>
#include <string>

class KeyValues
{
private:
    std::string name_;
    std::map<std::string, std::string> strings_;

public:
    KeyValues(const char* name, const char* key, const char* value) :
        name_(name)
    {
        SetString(key, value);
    }

    void SetString(const char* key, const char* value)
    {
        strings_[key] = value;
    }

    const char* GetName() const
    {
        return name_.c_str();
    }

    const char* GetString(const char* key) const
    {
        auto found = strings_.find(key);
        return found != strings_.end() ? found->second.c_str() : "";
    }
};

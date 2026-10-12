#pragma once

#include <unordered_set>

namespace nitro_utils
{
    class PtrValidator
    {
    private:
        std::unordered_set<std::string> null_names_;

    public:
        template<class T, class V>
        void Assign(T& target, std::string target_name, V value)
        {
            if (value == nullptr)
                null_names_.emplace(target_name);

            target = value;
        }

        template<class T>
        T Validate(T value, std::string name)
        {
            if (value == nullptr)
                null_names_.emplace(name);

            return value;
        }

        [[nodiscard]] bool HasNullPtr() const
        {
            return !null_names_.empty();
        }

        [[nodiscard]] const std::unordered_set<std::string>& GetNullPtrNames() const
        {
            return null_names_;
        }
    };
}
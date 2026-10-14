#pragma once

#include <cstdio>
#include <unordered_map>
#include <unordered_set>

namespace utility
{
    template<class THandle, class TValue>
    class ContainerWithHandles
    {
        std::unordered_map<THandle, TValue> values_;
        std::unordered_set<THandle> free_handles_;

        THandle GetFreeHandle()
        {
            auto handle = values_.size() + 1;
            if (!free_handles_.empty())
            {
                handle = *free_handles_.begin();
                free_handles_.erase(handle);
            }
            else
            {
                while (values_.count(handle) > 1)
                    handle++;
            }

            return handle;
        }

    public:
        THandle Add(TValue&& value)
        {
            THandle handle = GetFreeHandle();
            values_.insert_or_assign(handle, value);

            return handle;
        }

        THandle Add(TValue& value)
        {
            THandle handle = GetFreeHandle();
            values_.insert_or_assign(handle, value);

            return handle;
        }

        THandle Add(const TValue&& value)
        {
            THandle handle = GetFreeHandle();
            values_.insert_or_assign(handle, value);

            return handle;
        }

        THandle Add(const TValue& value)
        {
            THandle handle = GetFreeHandle();
            values_.insert_or_assign(handle, value);

            return handle;
        }

        void Remove(THandle handle)
        {
            if (values_.erase(handle))
            {
                free_handles_.emplace(handle);
            }
        }

        auto Remove(typename std::unordered_map<THandle, TValue>::iterator it)
        {
            free_handles_.emplace(it->first);
            auto it_after = values_.erase(it);

            return it_after;
        }

        void clear()
        {
            values_.clear();
            free_handles_.clear();
        }

        auto begin()
        {
            return values_.begin();
        }

        auto end()
        {
            return values_.end();
        }

        auto cbegin()
        {
            return values_.cbegin();
        }

        auto cend()
        {
            return values_.cend();
        }

        size_t size()
        {
            return values_.size();
        }

        TValue at(THandle handle)
        {
            return values_.at(handle);
        }

        bool contains(THandle handle)
        {
            return values_.contains(handle);
        }
    };
}
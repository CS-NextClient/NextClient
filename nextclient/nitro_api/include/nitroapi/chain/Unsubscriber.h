#pragma once

#include <functional>

namespace nitroapi
{
    class Unsubscriber
    {
        std::function<void()> unsubscribe_func_;
        bool is_unsubscribed_ = false;

    public:
        Unsubscriber() :
            is_unsubscribed_(true)
        {}

        explicit Unsubscriber(std::function<void()> unsubscribe_func) :
            unsubscribe_func_(std::move(unsubscribe_func))
        {}

        void Unsubscribe()
        {
            if (!is_unsubscribed_)
            {
                is_unsubscribed_ = true;
                unsubscribe_func_();
            }
        }
    };
}
#pragma once
#include <algorithm>
#include <vector>

// IDs are sparse after deletion and may be reused. Preserve insertion order
// independently of both panel children and the menu's current display order.
class GameMenuOrder
{
public:
    void Add(int id)
    {
        ids_.push_back(id);
    }
    void Remove(int id)
    {
        std::erase(ids_, id);
    }
    template <class Priority, class Move>
    void Apply(bool inGame, Priority priority, Move move) const
    {
        auto order = ids_;
        if (inGame)
            std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return priority(a) < priority(b); });
        for (size_t i = order.size(); i > 1; --i)
            move(order[i - 2], order[i - 1]);
    }

private:
    std::vector<int> ids_;
};

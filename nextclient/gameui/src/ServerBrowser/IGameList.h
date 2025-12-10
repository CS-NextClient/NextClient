#ifndef IGAMELIST_H
#define IGAMELIST_H

#ifdef _WIN32
#pragma once
#endif

#include "serveritem.h"

class IGameList
{
public:
    enum class InterfaceItem
    {
        Filters           = 0,
        GetNewList        = 1,
        AddServer         = 2,
        AddCurrentServer  = 3,
    };

    enum class CancelQueryReason
    {
        UserCancellation  = 0,
        ConnectToServer   = 1,
        NewQuery          = 2,
        FilterChanged     = 3,
        PageClosed        = 4,
        Other             = 5
    };

    virtual ~IGameList() = default;
    virtual bool SupportsItem(InterfaceItem item) = 0;
    virtual void StartRefresh() = 0;
    virtual void GetNewServerList() = 0;
    virtual void StopRefresh(CancelQueryReason reason) = 0;
    virtual bool IsRefreshing() = 0;
    virtual serveritem_t &GetServer(int serverID) = 0;
    virtual void ApplyFilters() = 0;
    virtual int GetInvalidServerListID() = 0;
};

#endif
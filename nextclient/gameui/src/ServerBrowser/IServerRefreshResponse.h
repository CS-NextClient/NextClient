#ifndef ISERVERREFRESHRESPONSE_H
#define ISERVERREFRESHRESPONSE_H

#ifdef _WIN32
#pragma once
#endif

struct serveritem_t;

class IServerRefreshResponse
{
public:
    virtual void ServerResponded(serveritem_t &server) = 0;
    virtual void ServerFailedToRespond(serveritem_t &server) = 0;
    virtual void RefreshComplete() = 0;
};

#endif
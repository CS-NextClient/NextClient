#ifndef SERVER_H
#define SERVER_H

#ifdef _WIN32
#pragma once
#endif

#include <steam/steam_api.h>

struct serveritem_t
{
    gameserveritem_t gs{};
    int serverID;
    int listEntryID;
    bool hadSuccessfulResponse;

    explicit serveritem_t(bool successful_response, int serverID, gameserveritem_t gameserveritem) :
        gs(std::move(gameserveritem)),
        serverID(serverID),
        listEntryID(-1),
        hadSuccessfulResponse(successful_response)
    {

    }

    explicit serveritem_t() :
        serverID(-1),
        listEntryID(-1),
        hadSuccessfulResponse(false)
    {

    }
};

#endif

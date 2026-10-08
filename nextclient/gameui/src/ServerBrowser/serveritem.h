#ifndef SERVER_H
#define SERVER_H

#ifdef _WIN32
#pragma once
#endif

#include <steam/steam_api.h>
#include <next_engine_mini/ServerDetailsNext.h>

struct serveritem_t
{
    gameserveritem_t gs{};
    ServerDetailsNext next_details{};
    int serverID;
    int listEntryID;
    bool hadSuccessfulResponse;

    explicit serveritem_t(bool successful_response, int serverID, gameserveritem_t gameserveritem, ServerDetailsNext next_details) :
        gs(std::move(gameserveritem)),
        next_details(next_details),
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

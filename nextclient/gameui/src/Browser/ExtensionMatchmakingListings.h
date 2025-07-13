#pragma once

#include <cef.h>
#include <cef_utils.h>
#include <interface.h>
#include <steam/steam_api.h>
#include <string>
#include <map>

#include "ExtensionMatchmaking.h"

/*
	inteface Server {
		address: string,
		info: ServerInfo | null
	}

	nextclient.matchmaking.addFavoriteServer(ip: string): void
	nextclient.matchmaking.removeFavoriteServer(ip: string): void
	nextclient.matchmaking.getFavoriteServers(): Promise<Server[]>
	nextclient.matchmaking.getHistoryServers(): Promise<Server[]>
*/

class CListingsQueryResponseHandler : public CefJsPromiseLike, public ISteamMatchmakingServerListResponse {
	HServerListRequest queryHandle = nullptr;

	struct server_t {
		bool responded;
		std::string address;
		gameserveritem_t info;
	};
	std::unordered_map<int, server_t> servers_;

	void ServerResponded(HServerListRequest hRequest, int iServer) override;
	void ServerFailedToRespond(HServerListRequest hRequest, int iServer) override;
	void RefreshComplete(HServerListRequest hRequest, EMatchMakingServerResponse response) override;

	void RefreshCompleteCefTask(HServerListRequest hRequest, EMatchMakingServerResponse response);

protected:
	virtual HServerListRequest StartQuery() = 0;
	void FinishQuery();

public:
	CListingsQueryResponseHandler(CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> resolveFunc, CefRefPtr<CefV8Value> rejectFunc);
	~CListingsQueryResponseHandler();

	void Start();
};

class CFavoritesListingQuery : public CListingsQueryResponseHandler {
	using CListingsQueryResponseHandler::CListingsQueryResponseHandler;
	HServerListRequest StartQuery() override;
};

class CHistoryListingQuery : public CListingsQueryResponseHandler {
	using CListingsQueryResponseHandler::CListingsQueryResponseHandler;
	HServerListRequest StartQuery() override;
};

class CExtensionMatchmakingListingsHandler : public CefV8Handler {
	IMPLEMENT_REFCOUNTING(CExtensionMatchmakingHandler);

public:
	CExtensionMatchmakingListingsHandler() {};

	bool Execute(
		const CefString& name, CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception);
};

void RegisterMatchmakingListingsJsApi();
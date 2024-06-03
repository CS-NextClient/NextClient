#pragma once

#include <cef.h>
#include <cef_utils.h>
#include <interface.h>
#include <steam/steam_api.h>
#include <string>
#include <vector>

/*
	interface ServerInfo {
		appId: number,
		gameDir: string,
		gameDesc: string,
		address: string,
		hostname: string,
		map: string,
		playersOnline: number,
		botsOnline: number,
		playersMax: number,
		isPasswordProtected: boolean,
		isVacSecured: boolean,
		unixTimeLastPlayed: number
	}

	interface ServerPlayer {
		name: string,
		score: number,
		timePlayedSeconds: number
	}

	interface ServerRule {
		rule: string,
		value: string
	}

	nextclient.matchmaking.getServerInfo(ip: string): Promise<ServerInfo>
	nextclient.matchmaking.getPlayersInfo(ip: string): Promise<ServerPlayer[]>
	nextclient.matchmaking.getRules(ip: string): Promise<ServerRule[]>
*/

class CServerQueryResponseHandler : public CefJsPromiseLike {
	HServerQuery queryHandle = HSERVERQUERY_INVALID;
	std::string serverIp_;

protected:
	virtual HServerQuery StartQuery(uint32 unIP, uint16 usPort) = 0;
	void FinishQuery();

public:
	CServerQueryResponseHandler(
		std::string serverIp, 
		CefRefPtr<CefV8Context> context, CefRefPtr<CefV8Value> resolveFunc, CefRefPtr<CefV8Value> rejectFunc
	);
	~CServerQueryResponseHandler();

	void Start();
};

class CPingServerQuery : public CServerQueryResponseHandler, public ISteamMatchmakingPingResponse {
	using CServerQueryResponseHandler::CServerQueryResponseHandler;

	HServerQuery StartQuery(uint32 unIP, uint16 usPort);

    void ServerResponded(gameserveritem_t &server) override;
    void ServerFailedToRespond() override;

	void ServerRespondedCefTask(gameserveritem_t server);
	void ServerFailedToRespondCefTask();
};

class CPlayerDetailsQuery : public CServerQueryResponseHandler, public ISteamMatchmakingPlayersResponse {
	using CServerQueryResponseHandler::CServerQueryResponseHandler;

	HServerQuery StartQuery(uint32 unIP, uint16 usPort);

	struct player_entry_t {
		std::string playerName;
		int score;
		float timePlayedSeconds;
	};
	std::vector<player_entry_t> players_;

    void AddPlayerToList(const char *playerName, int score, float timePlayedSeconds) override;
    void PlayersFailedToRespond() override;
    void PlayersRefreshComplete() override;

	void PlayersFailedToRespondCefTask();
    void PlayersRefreshCompleteCefTask();
};

class CServerRulesQuery : public CServerQueryResponseHandler, public ISteamMatchmakingRulesResponse {
	using CServerQueryResponseHandler::CServerQueryResponseHandler;

	HServerQuery StartQuery(uint32 unIP, uint16 usPort);

	struct rules_entry_t {
		std::string rule;
		std::string value;
	};
	std::vector<rules_entry_t> rules_;

	void RulesResponded( const char *pchRule, const char *pchValue ) override;
	void RulesFailedToRespond() override;
	void RulesRefreshComplete() override;

	void RulesFailedToRespondCefTask();
	void RulesRefreshCompleteCefTask();
};

class CExtensionMatchmakingHandler : public CefV8Handler {
	IMPLEMENT_REFCOUNTING(CExtensionMatchmakingHandler);

public:
	CExtensionMatchmakingHandler() {};

	bool Execute(
		const CefString& name, CefRefPtr<CefV8Value> object,
		const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception);
};

void RegisterExtensionMatchmaking();
CefRefPtr<CefV8Value> GameServerItemToV8Object(const gameserveritem_t& server);
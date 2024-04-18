#include "ExtensionMatchmaking.h"
#include "AcceptedDomains.h"
#include <nitro_utils/net_utils.h>

CServerQueryResponseHandler::CServerQueryResponseHandler(
	std::string serverIp, 
	CefRefPtr<CefV8Value> resolveFunc, CefRefPtr<CefV8Value> rejectFunc
) : CefJsPromiseLike(resolveFunc, rejectFunc), serverIp_(serverIp) {}

void CServerQueryResponseHandler::Start() {
	uint32_t ip; uint16_t port;
	nitro_utils::inet_stonp(serverIp_, ip, port, true);

	queryHandle = StartQuery(ip, port);
}

void CServerQueryResponseHandler::FinishQuery() {
	queryHandle = HSERVERQUERY_INVALID;
}

CServerQueryResponseHandler::~CServerQueryResponseHandler() {
	if(queryHandle != HSERVERQUERY_INVALID)
		SteamMatchmakingServers()->CancelServerQuery(queryHandle);
}

HServerQuery CPingServerQuery::StartQuery(uint32 unIP, uint16 usPort) {
	return SteamMatchmakingServers()->PingServer(unIP, usPort, this);
}

void CPingServerQuery::ServerResponded(gameserveritem_t &server) {
	CefPostTask(TID_UI, new CefFunctionTask([=, &server] {
		gameserveritem_t srv = server;
		ServerRespondedCefTask(srv);
	}));
}

CefRefPtr<CefV8Value> GameServerItemToV8Object(gameserveritem_t* server) {
	auto constexpr defProperty = V8_PROPERTY_ATTRIBUTE_NONE;

	auto value = CefV8Value::CreateObject(nullptr);
	if(!value.get()) return NULL;

	value->SetValue("appId", CefV8Value::CreateInt(server->m_nAppID), defProperty);
	value->SetValue("gameDir", CefV8Value::CreateString(server->m_szGameDir), defProperty);
	value->SetValue("address", CefV8Value::CreateString(server->m_NetAdr.GetConnectionAddressString()), defProperty);
	value->SetValue("hostname", CefV8Value::CreateString(server->GetName()), defProperty);
	value->SetValue("map", CefV8Value::CreateString(server->m_szMap), defProperty);
	value->SetValue("playersOnline", CefV8Value::CreateInt(server->m_nPlayers), defProperty);
	value->SetValue("botsOnline", CefV8Value::CreateInt(server->m_nBotPlayers), defProperty);
	value->SetValue("playersMax", CefV8Value::CreateInt(server->m_nMaxPlayers), defProperty);
	value->SetValue("isPasswordProtected", CefV8Value::CreateBool(server->m_bPassword), defProperty);
	value->SetValue("isVacSecured", CefV8Value::CreateBool(server->m_bSecure), defProperty);
	value->SetValue("unixTimeLastPlayed", CefV8Value::CreateInt(server->m_ulTimeLastPlayed), defProperty);

	return value;
}

void CPingServerQuery::ServerRespondedCefTask(gameserveritem_t &server) {
	CefV8ContextCapture capture(GetContext());

	auto value = GameServerItemToV8Object(&server);
	Resolve(value);

	FinishQuery();
	delete this;
}

void CPingServerQuery::ServerFailedToRespond() {
	CefPostTask(TID_UI, new CefFunctionTask([=] { ServerFailedToRespondCefTask(); }));
}

void CPingServerQuery::ServerFailedToRespondCefTask() {
	CefV8ContextCapture capture(GetContext());

	Reject(CefV8Value::CreateUndefined());
	FinishQuery();
	delete this;
}

HServerQuery CPlayerDetailsQuery::StartQuery(uint32 unIP, uint16 usPort) {
	return SteamMatchmakingServers()->PlayerDetails(unIP, usPort, this);
}

void CPlayerDetailsQuery::AddPlayerToList(const char *playerName, int score, float timePlayedSeconds) {
	players_.push_back({ playerName, score, timePlayedSeconds });
}

void CPlayerDetailsQuery::PlayersRefreshComplete() {
	CefPostTask(TID_UI, new CefFunctionTask([=] { PlayersRefreshCompleteCefTask(); }));
}

void CPlayerDetailsQuery::PlayersRefreshCompleteCefTask() {
	CefV8ContextCapture capture(GetContext());

	auto constexpr defProperty = V8_PROPERTY_ATTRIBUTE_NONE;

	auto value = CefV8Value::CreateArray();
	int index = 0;

	for(auto &pl : players_) {
		auto playerObj = CefV8Value::CreateObject(nullptr, nullptr);
		playerObj->SetValue("name", CefV8Value::CreateString(pl.playerName), defProperty);
		playerObj->SetValue("score", CefV8Value::CreateInt(pl.score), defProperty);
		playerObj->SetValue("timePlayedSeconds", CefV8Value::CreateInt(pl.timePlayedSeconds), defProperty);

		value->SetValue(index++, playerObj);
	}

	Resolve(value);
	FinishQuery();
	delete this;
}

void CPlayerDetailsQuery::PlayersFailedToRespond() {
	CefPostTask(TID_UI, new CefFunctionTask([=] { PlayersFailedToRespondCefTask(); }));
}

void CPlayerDetailsQuery::PlayersFailedToRespondCefTask() {
	CefV8ContextCapture capture(GetContext());

	Reject(CefV8Value::CreateUndefined());
	FinishQuery();
	delete this;
}

HServerQuery CServerRulesQuery::StartQuery(uint32 unIP, uint16 usPort) {
	return SteamMatchmakingServers()->ServerRules(unIP, usPort, this);
}

void CServerRulesQuery::RulesResponded(const char *pchRule, const char *pchValue) {
	rules_.push_back({ pchRule, pchValue });
}

void CServerRulesQuery::RulesRefreshComplete() {
	CefPostTask(TID_UI, new CefFunctionTask([=] { RulesRefreshCompleteCefTask(); }));
}

void CServerRulesQuery::RulesRefreshCompleteCefTask() {
	CefV8ContextCapture capture(GetContext());

	auto constexpr defProperty = V8_PROPERTY_ATTRIBUTE_NONE;

	auto value = CefV8Value::CreateArray();
	int index = 0;

	for(auto &rule : rules_) {
		auto playerObj = CefV8Value::CreateObject(nullptr, nullptr);
		playerObj->SetValue("rule", CefV8Value::CreateString(rule.rule), defProperty);
		playerObj->SetValue("value", CefV8Value::CreateString(rule.value), defProperty);

		value->SetValue(index++, playerObj);
	}

	Resolve(value);
	FinishQuery();
	delete this;
}

void CServerRulesQuery::RulesFailedToRespond() {
	CefPostTask(TID_UI, new CefFunctionTask([=] { RulesFailedToRespondCefTask(); }));
}

void CServerRulesQuery::RulesFailedToRespondCefTask() {
	CefV8ContextCapture capture(GetContext());

	Reject(CefV8Value::CreateUndefined());
	FinishQuery();
	delete this;
}

bool CExtensionMatchmakingHandler::Execute(
	const CefString& name, CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception
) {
	if(!IsV8CurrentContextOnAcceptedDomain()) return false;

	if(arguments.size() > 2) {
		auto ip = arguments[0]->GetStringValue();
		auto resolve = arguments[1];
		auto reject = arguments[2];

		if(name == "getServerInfo") {
			(new CPingServerQuery(ip, resolve, reject))->Start();
			return true;
		}
		else if(name == "getPlayersInfo") {
			(new CPlayerDetailsQuery(ip, resolve, reject))->Start();
			return true;
		}
		else if(name == "getRules") {
			(new CServerRulesQuery(ip, resolve, reject))->Start();
			return true;
		}
	}

	return false;
}

void RegisterExtensionMatchmaking() {
	CefString code = 
		"if(!nextclient.matchmaking) nextclient.matchmaking = {};"
		"(function() {"
		"	nextclient.matchmaking.getServerInfo = function(ip) {"
		"		native function getServerInfo();"
		"		if(typeof ip == 'string') {"
		"			return new Promise(function(resolve, reject) {"
		"				getServerInfo(ip, resolve, reject);"
		"			});"
		"		};"
		"	};"
		"	nextclient.matchmaking.getPlayersInfo = function(ip) {"
		"		native function getPlayersInfo();"
		"		if(typeof ip == 'string') {"
		"			return new Promise(function(resolve, reject) {"
		"				getPlayersInfo(ip, resolve, reject);"
		"			});"
		"		};"
		"	};"
		"	nextclient.matchmaking.getRules = function(ip) {"
		"		native function getRules();"
		"		if(typeof ip == 'string') {"
		"			return new Promise(function(resolve, reject) {"
		"				getRules(ip, resolve, reject);"
		"			});"
		"		};"
		"	};"
		"})()";

	CefRegisterExtension("ncl/matchmaking", code, new CExtensionMatchmakingHandler);
}
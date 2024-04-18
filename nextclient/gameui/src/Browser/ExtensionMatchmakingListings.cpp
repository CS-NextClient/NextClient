#include "ExtensionMatchmakingListings.h"
#include "AcceptedDomains.h"
#include <nitro_utils/net_utils.h>

CListingsQueryResponseHandler::CListingsQueryResponseHandler(
	CefRefPtr<CefV8Value> resolveFunc, CefRefPtr<CefV8Value> rejectFunc
) : CefJsPromiseLike(resolveFunc, rejectFunc) {}

void CListingsQueryResponseHandler::Start() {
	queryHandle = StartQuery();
}

void CListingsQueryResponseHandler::FinishQuery() {
	queryHandle = nullptr;
}

void CListingsQueryResponseHandler::ServerResponded(
	HServerListRequest hRequest, int iServer
) {
	auto details = SteamMatchmakingServers()->GetServerDetails(hRequest, iServer);
	servers_[iServer] = { true, details->m_NetAdr.GetConnectionAddressString(), details };
}

void CListingsQueryResponseHandler::ServerFailedToRespond(
	HServerListRequest hRequest, int iServer
) {
	auto details = SteamMatchmakingServers()->GetServerDetails(hRequest, iServer);
	servers_[iServer] = { false, details->m_NetAdr.GetConnectionAddressString(), details };
}

void CListingsQueryResponseHandler::RefreshComplete(
	HServerListRequest hRequest, EMatchMakingServerResponse response
) {
	CefPostTask(TID_UI, new CefFunctionTask([=] { RefreshCompleteCefTask(hRequest, response); }));
}

void CListingsQueryResponseHandler::RefreshCompleteCefTask(
	HServerListRequest hRequest, EMatchMakingServerResponse response
) {
	CefV8ContextCapture capture(GetContext());
	auto constexpr defProperty = V8_PROPERTY_ATTRIBUTE_NONE;

	if(response != eServerResponded) Reject(CefV8Value::CreateUndefined());
	else {
		auto value = CefV8Value::CreateArray();
		int index = 0;

		for(auto &[key, srv] : servers_) {
			auto item = CefV8Value::CreateObject(nullptr);
			item->SetValue("address", CefV8Value::CreateString(srv.address), defProperty);
			item->SetValue("info", srv.responded ? GameServerItemToV8Object(srv.info) :  CefV8Value::CreateNull(), defProperty);

			value->SetValue(index++, item);
		}

		Resolve(value);
	}

	FinishQuery();
	delete this;
}

CListingsQueryResponseHandler::~CListingsQueryResponseHandler() {
	if(queryHandle != nullptr) {
		SteamMatchmakingServers()->ReleaseRequest(queryHandle);
		SteamMatchmakingServers()->CancelQuery(queryHandle);
	}
}

HServerListRequest CFavoritesListingQuery::StartQuery() {
	return SteamMatchmakingServers()->RequestFavoritesServerList(SteamUtils()->GetAppID(), nullptr, 0, this);
};

HServerListRequest CHistoryListingQuery::StartQuery() {
	return SteamMatchmakingServers()->RequestHistoryServerList(SteamUtils()->GetAppID(), nullptr, 0, this);
}

bool CExtensionMatchmakingListingsHandler::Execute(
	const CefString& name, CefRefPtr<CefV8Value> object,
	const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception
) {
	if(!IsV8CurrentContextOnAcceptedDomain()) return false;

	if(arguments.size() > 1) {
		auto resolve = arguments[0];
		auto reject = arguments[1];
		
		if(name == "getFavoriteServers") {
			(new CFavoritesListingQuery(resolve, reject))->Start();
			return true;
		}
		else if(name == "getHistoryServers") {
			(new CHistoryListingQuery(resolve, reject))->Start();
			return true;
		}
	}
	else if(arguments.size() > 0) {
		uint32_t ip; uint16_t port;
	
		if(name == "addFavoriteServer") {
			nitro_utils::inet_stonp(arguments[0]->GetStringValue(), ip, port, true);
			SteamMatchmaking()->AddFavoriteGame(SteamUtils()->GetAppID(), ip, port, port, k_unFavoriteFlagFavorite, 0);
		}
		else if(name == "removeFavoriteServer") {
			nitro_utils::inet_stonp(arguments[0]->GetStringValue(), ip, port, true);
			SteamMatchmaking()->RemoveFavoriteGame(SteamUtils()->GetAppID(), ip, port, port, k_unFavoriteFlagFavorite);
		}
	}

	return false;
}

void RegisterExtensionMatchmakingListings() {
	CefString code = 
		"if(!nextclient.matchmaking) nextclient.matchmaking = {};"
		"(function() {"
		"	nextclient.matchmaking.getFavoriteServers = function() {"
		"		native function getFavoriteServers();"
		"		return new Promise(function(resolve, reject) {"
		"			getFavoriteServers(resolve, reject);"
		"		});"
		"	};"
		"	nextclient.matchmaking.getHistoryServers = function() {"
		"		native function getHistoryServers();"
		"		return new Promise(function(resolve, reject) {"
		"			getHistoryServers(resolve, reject);"
		"		});"
		"	};"
		"	nextclient.matchmaking.addFavoriteServer = function(ip) {"
		"		native function addFavoriteServer();"
		"		return typeof ip == 'string' ? addFavoriteServer(ip) : undefined;"
		"	};"
		"	nextclient.matchmaking.removeFavoriteServer = function(ip) {"
		"		native function removeFavoriteServer();"
		"		return typeof ip == 'string' ? removeFavoriteServer(ip) : undefined;"
		"	};"
		"})()";

	CefRegisterExtension("ncl/matchmaking/listings", code, new CExtensionMatchmakingListingsHandler);
}
#include "ExtensionConnectionApi.h"
#include <steam/steam_api.h>
#include <taskcoro/TaskCoro.h>

using namespace taskcoro;

CExtensionConnectionApiEvents::CExtensionConnectionApiEvents(
	nitroapi::NitroApiInterface* nitro_api,
	CefRefPtr<CefV8Context> context)
	: NitroApiHelper::NitroApiHelper(nitro_api)
	, context_(context) {

	DeferUnsub(eng()->CL_Connect_f += [this]() {
		auto ip = std::string(cls()->servername);

		CefPostTask(TID_UI, new CefFunctionTask([this, ip] {
			CefV8ContextCapture capture(context_);
			if (!context_->GetFrame().get())
				return;

			std::string code = std::format("nextclient.dispatchEvent('connect', {{ address: '{}' }})", ip);
			context_->GetFrame()->ExecuteJavaScript(code, "", 0);
		}));
	});

	DeferUnsub(eng()->CL_Disconnect += [this]() {
		auto ip = std::string(cls()->servername);

		CefPostTask(TID_UI, new CefFunctionTask([this, ip] {
			CefV8ContextCapture capture(context_);
			if (!context_->GetFrame().get())
				return;

			std::string code = std::format("nextclient.dispatchEvent('disconnect', {{ address: '{}' }})", ip);
			context_->GetFrame()->ExecuteJavaScript(code, "", 0);
		}));
	});

	DeferUnsub(eng()->SVC_SignOnNum |= [this](auto* next) {
		int read_count = *msg_readcount();
		int sign_on_num = eng()->MSG_ReadByte();
		*msg_readcount() = read_count;

		if (sign_on_num > cls()->signon) {
			auto ip = std::string(cls()->servername);

			CefPostTask(TID_UI, new CefFunctionTask([this, ip, sign_on_num] {
				CefV8ContextCapture capture(context_);
				if (!context_->GetFrame().get())
					return;

				std::string code = std::format("nextclient.dispatchEvent('putinserver', {{ address: '{}' }})", ip);
				context_->GetFrame()->ExecuteJavaScript(code, "", 0);
			}));
		}

		next->Invoke();
	});
}

struct GetConnectedHostData
{
	std::string address{};
	bool is_putinserver{};
	std::vector<hud_player_info_t> players{};
};

ContainerExtensionConnectionApi::ContainerExtensionConnectionApi(nitroapi::NitroApiInterface* nitro_api)
	: NitroApiHelper::NitroApiHelper(nitro_api) {

	CefRegisterExtension(
		"ncl/connection-api",

		"(function() {"
		"	native function init(); init();"
		"	nextclient.__defineGetter__('connectedHost', function() {"
		"		native function getConnectedHost();"
		"		return getConnectedHost();"
		"	});"
		"})()",

		new CefFunctionV8Handler(
			[this](const CefString& name, CefRefPtr<CefV8Value> object,
				const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval, CefString& exception) -> bool {
				if (!TaskCoro::IsInitialized())
					return false;

				if (name == "init") {
					if (is_initialized_)
						return true;

					auto context = CefV8Context::GetCurrentContext();
					TaskCoro::RunInMainThread<void>([this, context] {
						events_.push_back(std::make_unique<CExtensionConnectionApiEvents>(api(), context));
					});

					is_initialized_ = true;
					return true;
				} 
				else if (name == "getConnectedHost") {
					GetConnectedHostData result;

					auto task = TaskCoro::RunInMainThread<GetConnectedHostData>([this] {
						GetConnectedHostData result{};

						auto address = cls()->servername;
						if (!address[0] || cls()->state == ca_disconnected)
							return result;

						result.address = address;
						result.is_putinserver = cls()->state == ca_active || cls()->signon == 1;

						for (int i = 1; i < MAX_PLAYERS; i++)
						{
							hud_player_info_t pl {};
							cl_enginefunc()->pfnGetPlayerInfo(i, &pl);
							if (!pl.name)
								continue;

							result.players.emplace_back(pl);
						}

						return result;
					});

					try
					{
						result = task.get();
					}
					catch (const std::exception& e)
					{
						return false;
					}

					if (!result.address.empty())
					{
						retval = CefV8Value::CreateObject(nullptr);
						if (retval.get()) {
							auto constexpr defProperty = V8_PROPERTY_ATTRIBUTE_NONE;
							auto playersArray = CefV8Value::CreateArray();
							retval->SetValue("address", CefV8Value::CreateString(result.address), defProperty);
							retval->SetValue("state", CefV8Value::CreateString(result.is_putinserver ? "joined" : "loading"), defProperty);
							retval->SetValue("players", playersArray, defProperty);

							int index = 0;
							for (const auto& player : result.players)
							{
								auto obj = CefV8Value::CreateObject(nullptr);

								obj->SetValue("name", CefV8Value::CreateString(player.name), defProperty);
								obj->SetValue("steamId", CefV8Value::CreateString(std::to_string(player.m_nSteamID)), defProperty);
								obj->SetValue("isInSteamMode", CefV8Value::CreateBool(CSteamID(player.m_nSteamID).BIndividualAccount()), defProperty);
								obj->SetValue("isLocalPlayer", CefV8Value::CreateBool(player.thisplayer), defProperty);

								playersArray->SetValue(index++, obj);
							}
						}
					} else {
						retval = CefV8Value::CreateNull();
					}

					return true;
				}

				return false;
			}
		)
	);
}

ContainerExtensionConnectionApi::~ContainerExtensionConnectionApi() {
}
#pragma once

#include <engine.h>
#include <cef.h>
#include <cef_utils.h>
#include <nitroapi/NitroApiHelper.h>

/*
	nextclient.addEventListener('connect', function(event) {
		var address = event.detail.address;
	});

	nextclient.addEventListener('disconnect', function(event) {
	});

	nextclient.addEventListener('putinserver', function(event) {
	});

	interface ConnectedHost {
		address: string,
		state: 'loading' | 'joined',
		players: Array<{
			name: string,
			steamId: number,
			isInSteamMode: boolean
		}>
	}

	nextclient.connectedHost: ConnectedHost | null;
*/

class CExtensionConnectionApiEvents : protected nitroapi::NitroApiHelper {
	CefRefPtr<CefV8Context> context_{};

public:
	explicit CExtensionConnectionApiEvents(
		nitroapi::NitroApiInterface* nitro_api,
		CefRefPtr<CefV8Context> context);
};

class ContainerExtensionConnectionApi : protected nitroapi::NitroApiHelper {
	std::vector<std::unique_ptr<CExtensionConnectionApiEvents>> events_{};
	std::atomic_bool is_initialized_{};

public:
	ContainerExtensionConnectionApi(nitroapi::NitroApiInterface* nitro_api);
	~ContainerExtensionConnectionApi();
};
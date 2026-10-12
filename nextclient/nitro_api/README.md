NitroApi for NextClient
=======================

NitroApi in NextClient takes care of all the work of installing hooks in the game engine and other libraries. Provides a convenient interface for interacting with hooked functions:

```c++
// It is assumed that the nitro_api variable has type nitroapi::NitroApiInterface* and was initialized earlier.
auto eng = nitro_api->GetEngineData();

// 1. Handler installation
auto unsubscriber1 = eng->Cvar_Set |= [](const char *name, const char *value, const auto& next) {
    // yours code here
    next->Invoke(name, value);
};

// 2. Subscription
auto unsubscriber2 = eng->Cvar_Set += [](const char *name, const char *value) {
    // yours code here
};

// 3. Calling the original function
eng->Cvar_Set("cl_minmodels", "0");

// 4. Call of the function through the whole chain of handlers
eng->Cvar_Set.InvokeChained("cl_minmodels", "0");
```


Currently NitroApi only supports explicit linking. To include NitroApi in your project, add nitro_api::nitro_api to the
target_link_libraries.
This will add the necessary includes, defines and libraries to your target (also hlsdk will be added implicitly) 
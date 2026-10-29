# NitroApi for NextClient

NitroApi installs hooks in the stock engine and related libraries. Its typed wrappers support chained handlers, observers, and direct calls to the original function. Engine addresses and signatures are maintained under the [NitroApi rules](../nextclient/nitro_api/AGENTS.md).

## Getting started

Link the consuming CMake target to `nitro_api::nitro_api` and include `<nitroapi/NitroApiInterface.h>`. The target supplies the public headers, helper library, compile definitions, and SDK dependencies. Obtain the `NITRO_API_001` interface through the loaded provider's factory. The example below assumes `nitro_api` is an initialized `nitroapi::NitroApiInterface*` and the engine's `Cvar_Set` binding is ready.

```cmake
target_link_libraries(your_target PRIVATE nitro_api::nitro_api)
```

## Hook and call examples

```cpp
nitroapi::EngineData* engine = nitro_api->GetEngineData();

auto handler = engine->Cvar_Set |= [nitro_api](
    const char* name,
    const char* value,
    nitroapi::NextHandlerInterface<void, const char*, const char*>* next)
{
    nitro_api->WriteLog("Cvar_Set handler", name);
    next->Invoke(name, value);
};

auto observer = engine->Cvar_Set += [nitro_api](const char* name, const char*)
{
    nitro_api->WriteLog("Cvar_Set observer", name);
};

engine->Cvar_Set("cl_minmodels", "0");
engine->Cvar_Set.InvokeChained("cl_minmodels", "0");

observer->Unsubscribe();
handler->Unsubscribe();
```

`|=` installs a handler that controls forwarding through `next`; `+=` subscribes an observer. The direct call invokes the original function. `InvokeChained` runs the handler chain and invokes the registered observers, producing the example log entries.

Keep the returned subscription handles with the owning subsystem. Call `Unsubscribe()` before destroying captured objects or unloading their code, while the provider remains available. Destroying the handle alone does not unsubscribe. The example removes both registrations after demonstrating the calls.

Frame observers use the separate [frame lifecycle contract](frame-lifecycle.md). For exposing an additional engine function or variable, follow [Extending engine access](../nextclient/nitro_api/AGENTS.md#extending-engine-access).

# NitroApi frame lifecycle ownership

[Russian translation](frame-lifecycle.ru.md) | [NitroApi rules](../nextclient/nitro_api/AGENTS.md#frame-lifecycle)

## Ownership

The private concrete `nitroapi::FrameLifecycleService` owns registrations, monotonically increasing subscription IDs, the current invocation chain and retained recipient storage. Each `FrameLifecycleInvocation` receives its service by reference. The service must outlive its invocations and their exit callbacks. Independent services can nest without changing one another's registrations or current invocation.

The module owns one stable default service for its DLL lifetime. The public `NITRO_FRAME_LIFECYCLE_001` factory returns that service. `NitroApi` passes it explicitly to `EngineModule`, which uses the same reference for its Host callbacks and `HostAbortAdapter`. `NitroApi::UnInitialize()` clears registrations before module unload.

During registration, `EngineModule` supplies a `CallbackWrapperFunc` for each of `Host_Frame`, `Host_FrameInternal` and `Host_FilterTime`. `GenericHookStatic::SetCallbackAndEnableHook` invokes this factory when setting the hook callback. The resulting `CallbackFunc` wraps profiling and the complete handlers chain. Frame dispatch invokes the prepared callback; reattachment wraps a new chain callback. Each outer `Host_Frame` callback creates a local `FrameLifecycleInvocation`, while filter and body-depth reporting use the captured service reference.

## Storage reuse

Each active invocation leases a frame from a retained `std::deque`. Its recipient vector records subscription IDs and entry/acceptance flags in registration order. The deque preserves references when nesting adds frames. The vector retains capacity between invocations and grows only when its registration count exceeds capacity. Storage grows to the observed count and leased depth; neither has an imposed limit.

The invocation restores its previous current invocation before reverse exit dispatch. Its frame lease remains active until every reverse-exit callback finishes, so an exit callback can enter another Host without overwriting outgoing recipients. `Clear()` removes registrations and preserves leased frames, retained capacity and the ID sequence. Every callback resolves its subscription ID immediately before dispatch. Observers own their lifetime and must unsubscribe before destruction.

## Notification order

Entry and acceptance run in registration order. Acceptance is idempotent and requires entry to have begun for that recipient. Exit runs in reverse entry order after the ordinary hook chain. Registrations added during dispatch enter the next invocation. Body-depth markers surround `Host_FrameInternal`. A private entry adapter on the stock variadic `Host_EndGame` marks the current invocation of the same service as aborted before forwarding to the original. The outer `Host_Frame` reports completion only when its body depth is zero and its invocation has no abort marker. This covers both an unfinished internal body and a nonlocal return to the internal Host setjmp followed by an ordinary return. The marker belongs to one invocation; nested owners and subsequent frames retain their own state.

## Native abort adapter

`EngineModule` installs its private `HostAbortAdapter` after ordinary engine address registration and removes it before engine unload. The address providers own the Windows build-8684 RVA `0x55ea0` and Linux `Host_EndGame` export lookup. No public hook ID, engine data field or factory version is added.

The x86 entry stub preserves flags, general-purpose registers, the original stack pointer, return address and all variadic arguments. It aligns the stack to 16 bytes before its nonthrowing owner notification and tail-jumps to the funchook trampoline after restoring the original state. The notification only sets the current invocation's abort marker. It does not dispatch observers or retain work after the original call. Adapter removal restores the native entry before executable storage is destroyed. Preparation errors are exceptions before installation; installation, removal and destruction failures terminate the process because executable storage cannot safely be released while a detour may remain active.

## Verification boundaries

The focused tests exercise real independent owners, A/B/A nesting, mutation during every boundary and exit reentry. `EngineFrameLifecycleTest` uses production `EngineModule` registration to check the explicitly supplied service, argument/result forwarding, rejected filters, complete-chain exit order, handlers that suppress the original, reattachment, nesting and both internal and outer nonlocal aborts followed by ordinary frames. `FrameLifecycleTest.GenericHostHookDoesNotEmitLifecycleEvents` checks an undecorated static hook.

Native adapter tests also execute more than eight internal setjmp aborts, verify int/pointer/double forwarding, independent and nested owners, a subsequent ordinary frame, and install/remove/destruction. The monitor integration test subscribes a real monitor to the private service and verifies partial-frame discard, CPU/GPU token retirement, balanced trace closure and pending dump retry through that native adapter.

Storage tests warm counts 0, 1 and 129 at depths 1, 12 and 36, repeat scalar-only callbacks, and compare frame addresses, recipient addresses and capacities outside the repeated interval. This proves retained recipient storage does not grow or relocate at warmed dimensions. It does not measure allocations in arbitrary observers or the full hook chain.

The [shipping factory procedure](testing/installation-and-factory.md#actual-provider-factory) checks the actual provider's factory and subscription ABI. It does not initialize the engine or execute initialized shutdown. Service `Clear()` tests and the `UnInitialize()` call path establish the local invalidation contract; live engine behavior remains a separate observation.

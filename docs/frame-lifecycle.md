# NitroApi frame lifecycle ownership

[Russian translation](frame-lifecycle.ru.md) | [NitroApi rules](../nextclient/nitro_api/AGENTS.md#frame-lifecycle)

## Ownership

The private concrete `nitroapi::FrameLifecycleService` owns registrations, monotonically increasing subscription IDs, the current invocation chain and retained recipient storage. Each `FrameLifecycleInvocation` receives its service by reference. The service must outlive its invocations and their exit callbacks. Independent services can nest without changing one another's registrations or current invocation.

The module owns one stable default service for its DLL lifetime. The public `NITRO_FRAME_LIFECYCLE_001` factory returns that service. Static Host, internal Host and filter hook installation captures the same concrete service once. Invocation, filter and body dispatch use that bound reference. `NitroApi::UnInitialize()` clears registrations before module unload.

## Storage reuse

Each active invocation leases a frame from a retained `std::deque`. Its recipient vector records subscription IDs and entry/acceptance flags in registration order. The deque preserves references when nesting adds frames. The vector retains capacity between invocations and grows only when its registration count exceeds capacity. Storage grows to the observed count and leased depth; neither has an imposed limit.

The invocation restores its previous current invocation before reverse exit dispatch. Its frame lease remains active until every reverse-exit callback finishes, so an exit callback can enter another Host without overwriting outgoing recipients. `Clear()` removes registrations and preserves leased frames, retained capacity and the ID sequence. Every callback resolves its subscription ID immediately before dispatch. Observers own their lifetime and must unsubscribe before destruction.

## Notification order

Entry and acceptance run in registration order. Acceptance is idempotent and requires entry to have begun for that recipient. Exit runs in reverse entry order after the ordinary hook chain. Registrations added during dispatch enter the next invocation. Body-depth markers surround `Host_FrameInternal`; the outer `Host_Frame` invocation reports an aborted body when the engine returns through its nonlocal abort path without finishing the internal body.

## Verification boundaries

The focused tests exercise real independent owners, A/B/A nesting, mutation during every boundary, exit reentry, actual static-hook forwarding and the outer/internal nonlocal abort boundary. Storage tests warm counts 0, 1 and 129 at depths 1, 12 and 36, repeat scalar-only callbacks, and compare frame addresses, recipient addresses and capacities outside the repeated interval. This proves retained recipient storage does not grow or relocate at warmed dimensions. It does not measure allocations in arbitrary observers or the full hook chain.

The [shipping factory procedure](testing/installation-and-factory.md#actual-provider-factory) checks the actual provider's factory and subscription ABI. It does not initialize the engine or execute initialized shutdown. Service `Clear()` tests and the `UnInitialize()` call path establish the local invalidation contract; live engine behavior remains a separate observation.

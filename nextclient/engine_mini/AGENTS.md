# Engine module

`src/engine.cpp` owns initialization, engine hook subscriptions and shutdown. Stock engine state is reached through NitroApi; local wrappers must keep its ABI and callback ordering intact.

## Lifetime and hooks

- Check which initialization phase provides each dependency. Module loading, game initialization
  and GL initialization are separate events; a resolved address does not prove its pointee is ready.
- Follow the existing `OnGameUninitializing` and `EngineMiniUninitialize` order when adding a
  service. Stop work that can re-enter the engine before clearing engine interfaces or unloading
  code. Review the `Sys_Error` path separately from normal `Host_Shutdown`.
- The manual task executor stops before the threaded executors. Otherwise a worker waiting to
  resume on the main thread can keep shutdown blocked after the frame pump has stopped. The
  detailed cancellation contract is in [task_coro](../packages/task_coro/AGENTS.md).
- A hook that calls `next` can synchronously enter other hooks and mutate the same state. Trace
  that call chain before changing resource iteration, network callbacks or disconnect handling;
  do not retain iterators or references across a call merely because it stays on one thread.
- Treat pointers into client entities and map data as connection-scoped. Disconnect and map
  replacement can invalidate them while the DLL and its services remain loaded.

## Rendering and DLL boundaries

The studio shader renderer has [local rules](src/graphics/render/AGENTS.md). Start with the [architecture guide and diagrams](../../docs/renderer-architecture.md) for scene order, resource ownership, lifetime, and shader-path settings.

- `RendererLifecycle` composes map, disconnect, client-state, context, normal shutdown, and fatal phases. It selects live release only when the tracked native context matches and GL entry points are initialized; loss forgets names without GL calls. Terminal shutdown stops copied studio/preview callbacks before nested disconnect and releases resources before engine services and hooks disappear. Standalone `Draw_Shutdown` releases resources and retains callbacks needed by later context creation. On Windows, terminal cleanup restores the text surface adapter; both live release and loss invalidate its atlas cells while preserving CPU fonts. Static destruction must not call GL or access expired engine objects.
- Save and restore the GL state actually touched by a draw path, including active texture unit,
  bindings, current color and blend state. The stock renderer continues drawing afterward.
- Framebuffer capture must account for the engine's bound draw framebuffer. Do not assume the
  scene is in framebuffer zero or that read and draw bindings are equal.
- VGUI interfaces are resolved through DLL factories. Preserve the identity of the engine's
  shared surface rather than introducing a second surface or font store in a consumer DLL.
  The text renderer has [additional rules](src/graphics/text/AGENTS.md).

The new material-authoring records and interface, and the shader capability interface, belong to `render`. Qualify these types in external consumers and forward declarations. Preserve their field and virtual-method order and the allocated `EngineMini014` and `ShaderRenderer001` factory identifiers.

## Verification

Build `engine_mini` in a non-test configuration when verifying the shipping DLL; with `NEXTCLIENT_BUILD_TESTS=ON`, that target is a static library for tests. Select affected cases from `engine_mini-tests` and the specialized renderer or lifecycle targets listed in the root rules. ABI changes also require checking NitroApi and affected consumers. Keep tests for parsing and state transitions engine-free; record GL and live-engine behavior as unverified when only those tests have run.

## Monitoring ownership

The `performance` subsystem owns frame collection, monitoring data and scopes, snapshot publication, presentation and trace integration. Its [local rules](src/performance/AGENTS.md) define owner-thread use and deferred frame-observer detachment. Package accounting and profiling helpers belong to [`ncl_perf`](../packages/ncl_perf/AGENTS.md).

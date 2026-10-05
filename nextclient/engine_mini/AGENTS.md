# Engine module

`src/engine.cpp` owns initialization, engine hook subscriptions and shutdown. Stock engine state
is reached through NitroApi; local wrappers must keep its ABI and callback ordering intact.

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

- Release GL objects explicitly while the context is live. Static destruction after engine
  teardown is too late for GL calls or access to engine-owned objects.
- Save and restore the GL state actually touched by a draw path, including active texture unit,
  bindings, current color and blend state. The stock renderer continues drawing afterward.
- Framebuffer capture must account for the engine's bound draw framebuffer. Do not assume the
  scene is in framebuffer zero or that read and draw bindings are equal.
- VGUI interfaces are resolved through DLL factories. Preserve the identity of the engine's
  shared surface rather than introducing a second surface or font store in a consumer DLL.

## Verification

Build `engine_mini` and run the affected `engine_mini-tests`. ABI changes also require checking
NitroApi and affected consumers. Keep tests for parsing and state transitions engine-free;
record GL and live-engine behavior as unverified when only those tests have run.

# Coroutine runtime

`TaskCoroImpl` owns the concurrencpp executors; `SynchronizationContext` chooses where continuations
resume. The launcher and engine have separate lifetime and frame-pump arrangements.

## Cancellation and ownership

- `WithCancellation` and `WithTimeout` can finish before the wrapped work finishes. They stop
  waiting; they do not join a worker or interrupt a blocking library call. An owner captured by
  background work must remain alive until that work itself completes.
- `TaskTracker::Token` tracks the lifetime of the token. When a wrapper can return early, the
  worker needs its own token, retained until its last owner access. A token only in the awaiting
  coroutine permits `WaitAsync` to finish while the worker still uses the service.
- After a `co_await` boundary, recheck cancellation and state validity before touching engine
  state or publishing results. Keeping an object alive does not keep its connection or request
  current. Do not retain references into mutable containers across a suspension without proving
  their stability.

## Execution and shutdown

- Main-thread work runs through the manual executor's `Update` pump. A blocking `.get()` on that
  thread is valid only when completion does not require its pump, including indirect continuations.
- Shut the update executor down before threaded executors. Once frames stop, a worker waiting
  for a main-thread continuation would otherwise prevent its executor from shutting down.
- Pool and IO worker contexts are established by matching concurrencpp's internal worker names in
  `src/impl/TaskCoroImpl.cpp`. A dependency upgrade must verify those names and continuation routing;
  `NewThread` and timer workers intentionally have no such context.
- Check both the callee's execution context and the caller's continuation context when changing
  an awaiter. Completing on a worker does not by itself guarantee that the caller resumes there.

## Verification

Exercise cancellation while work is still running, completion after cancellation, and shutdown
after the main-thread pump stops. The updater is a concrete consumer of tracked IO shutdown; its
tests and affected DLL builds are useful checks, but successful HTTP requests alone do not cover
the lifetime contract above.

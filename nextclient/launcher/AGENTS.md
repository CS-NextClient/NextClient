# Launcher and updater

The launcher owns the process, engine startup and relaunch. On Windows, `src/updater_gui_app` owns
the update workflow; the GUI's cancellation request and the end of HTTP or file work are separate
events. The updater GUI is not ported to Linux.

## Shutdown and HTTP work

- Preserve `Updater::OnExit` ordering: request cancellation, wait for HTTP service shutdown, then
  consume the updater task result. Its blocking wait depends on HTTP shutdown completing without
  the UI thread's manual synchronization-context pump.
- In `NextUpdaterHttpService::PostAsync`, the IO closure must retain its own `TaskTracker::Token`.
  `WithCancellation` can return while cpr/curl is still establishing a connection and
  `PostInternal` still uses the service. Removing that worker token reintroduces a use-after-free
  even if the awaiting coroutine keeps its own token.
- New background operations must follow the [task_coro lifetime contract](../packages/task_coro/AGENTS.md).
  Cancellation checks and shared ownership solve different problems; verify both.

## Update transaction

- Keep file work inside `NextUpdater`'s backup, replacement and restoration workflow. Review every
  error and cancellation exit against the backup state, including startup recovery from a previous
  interrupted update.
- Reuse `FileOpener` and the existing error reporting for file operations. Locked files, partial
  writes and a failed restore must produce a useful error; do not convert them into successful
  completion or clear recovery data merely to suppress a failure.
- Keep install, backup and downloaded-file paths distinct. Tests must use their isolated temporary
  directories and local HTTP fixtures, not the developer's installed game or a production backend.

## Verification

`updater-test` is available on Windows with `NEXTCLIENT_BUILD_TESTS` and
`NEXT_LAUNCHER_ENABLE_UPDATER` enabled.
Run its CTest cases sequentially (`ctest --test-dir <test-build> -C <config> -j 1 --output-on-failure`).
`NextUpdaterTestFixture.h` allocates ports from a process-local counter starting at 59873, so parallel
CTest processes can choose the same port. A connection or download failure under parallel execution
can therefore be a fixture collision; check sequentially before changing production networking.

For cancellation or shutdown changes, include the early-exit and failure paths, not only a complete
successful update. Build the affected launcher target as well as the tests.

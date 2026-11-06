# NextClient TestLab

This plugin provides the server scenes used by the [manual test procedures](../../../docs/testing/manual-testing.md).

When adding a scenario, register its name, map, roles, and stages in [core.inc](core.inc). Implement operations in a dedicated include and add dispatch in [radar.inc](radar.inc), a map profile, and server self-tests. Add the manual acceptance procedure to the [scenario checklist](../../../docs/testing/manual-testing-scenarios.md).

Keep scenario state under one owner. Preserve generation and `userid` checks so delayed work cannot affect another run or a player who reused the same slot. Verify actual gameplay results, not only that a request was issued.

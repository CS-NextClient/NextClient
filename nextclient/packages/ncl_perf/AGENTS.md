# Performance accounting package

All package types, constants, accounting and profiling helpers belong to `ncl_perf`. Keep the package independent of engine frames, output records, GL contexts and presentation policy, which belong to the [engine performance owner](../../engine_mini/src/performance/AGENTS.md).

`PerfCpuAccounting.h` and `PerfCpuAccounting.cpp` declare and implement the primary accounting class. `PerfCpuAccounting` owns its registry, nested CPU stack and generation-bound tokens. Preserve sample layouts, integer widths, capacity limits, reverse nesting and sequence-exhaustion behavior. The caller owns the clock and chooses collection boundaries. Profiling macros keep their public names and caller source locations; enabled expansions must qualify `::ncl_perf::NclProfileScope` so any subsystem can use them.

Run `ncl_perf-tests` for accounting changes and compile affected DLL consumers after public declaration changes. Enabled profiling also needs the profiler graph and its lifecycle/connection tests, including the deliberate fake emission boundary. The [monitor contract](../../../docs/performance-monitor.md) owns the engine integration semantics.

# Performance monitor

[Russian version](performance-architecture.ru.md) | [Project architecture](architecture.md) | [Settings and commands](performance-monitor.md)

The monitor helps investigate delays in the game. It measures engine execution time, CPU and GPU time, background task duration, and network traffic. Results are available in the on-screen panel, text dumps, and the Tracy profiler.

## Participating modules

The monitor is part of `engine_mini`. It collects data from the engine, renderer, and client features, stores history, and draws the panel. Shared timing utilities belong to the `ncl_perf` package.

Tracy records a detailed report of game activity for later analysis. The panel, dumps, and Tracy recording are enabled separately: hiding the panel does not disable the other diagnostic tools.

## Measurements

Frame time includes all engine work in one game update: game data processing and rendering. The interval between screen updates shows how often the game presents a new image. These values can differ: the engine can update the screen several times during one game update or not update it at all.

CPU and GPU time are measured separately. CPU time is available when the corresponding work finishes. The GPU can finish rendering later, so its result arrives with a delay. The monitor associates that result with the image it measured.

Background tasks, such as radar layer analysis, run alongside the game. Their duration is shown separately from the main frame time.

## Frame lifecycle

NitroApi tracks when a frame starts and finishes. The monitor in `engine_mini` uses this information to save the timing result after the engine finishes its work.

If the FPS limit prevents the next frame from starting, the engine skips processing it. This skip does not enter the statistics as a frame that took zero time.

If an engine error interrupts a frame, its measurement is incomplete. The monitor excludes it from the statistics and continues measuring subsequent frames. This prevents the error from distorting frame times or stopping data collection after the game recovers.

## Reading the results

The panel shows the latest available values. Sections refresh at different rates. An unavailable result does not mean that an operation took zero milliseconds. The [Reading graphs](performance-monitor.md#reading-graphs) section explains refresh periods and statistics.

Map changes and video restarts reset the affected measurements, keeping data from the previous map or video mode separate from new results.

The [monitor guide](performance-monitor.md) explains dumps and Tracy recording. The [test procedure](testing/performance-monitor.md) describes verification scenarios.

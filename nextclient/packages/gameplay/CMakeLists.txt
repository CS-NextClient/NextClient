# What the game, its HUD and the settings UI have to agree on: cvar defaults, the view math
# and the crosshair's shapes and sizes. Nothing here depends on the engine, so it all tests
# without it.
add_subdirectory(cvars)
add_subdirectory(view)
add_subdirectory(crosshair)

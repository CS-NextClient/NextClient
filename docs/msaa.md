# MSAA

[Russian version](msaa.ru.md)

The Video settings page provides the stock engine's multisample anti-aliasing (MSAA), which smooths geometry edges. Changes take effect when Apply or OK restarts the game. Cancel discards the pending selection.

In the gameplay scene, MSAA applies to map brush geometry and Studio models, including the material shader path. It smooths polygon edges. Texture detail and alpha-mask boundaries require their own filtering or coverage technique.

| Mode | Behavior |
|---|---|
| Engine default | Uses the engine's default policy and preserves external launch options. |
| Off | Disables the engine's MSAA framebuffer. |
| 4x | Requests the engine's four-sample framebuffer and overrides external `-nomsaa` and `-nofbo` options. |

The stock Windows and Linux engines support four samples. If the required OpenGL extensions are unavailable or framebuffer creation fails, the engine continues without MSAA.

Low video detail disables framebuffer rendering. While that option is selected, the MSAA control is disabled and displays the saved selection. The saved mode applies again after low video detail is disabled and the video settings are applied. Enabling low video detail discards an unapplied MSAA selection.

The `msaa_samples` key in `user_game_config.ini` stores `-1` for Engine default, `0` for Off, or `4` for 4x. A missing key or an unsupported value uses Engine default. The launcher applies the selection to the current engine session, preserving the original command line for subsequent process restarts.

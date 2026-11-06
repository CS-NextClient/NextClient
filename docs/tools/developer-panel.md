# Developer tools panel

[Russian version](developer-panel.ru.md)

This native Tkinter panel runs the repository's operator tools through explicit input forms and a shared execution log. Opening the window runs no tool. English is the default interface language; use the visible **RU / EN** selector or `--language ru` for Russian. Switching languages preserves input values, the selected tool, and the log. Panel text, validation messages, status messages, dialog titles and filters, and CLI help are translated. Tools keep their original stdout/stderr, and native dialog buttons follow the operating system language.

## Start

Use Python 3.12 or newer with Tkinter. From the repository root, choose one command: the Windows launch script, the POSIX launch script, or the Python command.

```text
tools\dev_tools.cmd
sh tools/dev_tools.sh
python tools/dev_tools/dev_tools.py
```

Both launchers live directly in `tools/` and work from any current directory. `NEXTCLIENT_PYTHON`, when set to an executable path, selects the interpreter. Otherwise they look for the available bundled Codex runtime, then the system Python (`py -3` or `python` on Windows; `python3` or `python` in the POSIX launcher). Arguments and exit status are preserved. The launchers do not change system settings. All Python tools use the same interpreter as the panel.

Select a server configuration at startup when needed:

```text
python tools/dev_tools/dev_tools.py --config build/test-server/config.local.json
```

Relative paths in fields resolve from the repository root. Values are kept for the current window session. A vertical scrollbar keeps longer forms usable in a smaller window. The language and operation controls are locked while a job is running.

## Documentation

The header buttons open [Documentation](../README.md), [Testing](../testing/README.md), and this **Panel guide**. Each tool form provides a **Tool guide** button. The server tab also links to the [server guide](test-server.md) and the [materials test plugin](../testing/materials-test-plugin.md).

Buttons open local Markdown files in the operating system's associated application. The current panel language selects the English or Russian document. Documentation remains accessible while a tool runs. The panel displays an error dialog if the file is missing or the operating system cannot open it with the associated application. A local Markdown association or platform opener is required.

## Tools

Each non-server tab has a tool selector and a form containing its supported inputs. **Run** starts the selected tool with the values entered in its form. Output files with matching names can be overwritten by the underlying tools.

| Tab | Included tools | Inputs and behavior |
|---|---|---|
| Images | `flags/convert_flags.py`, `flags/make_country_column_icon.py` | Source PNG folder, output folder or file; creates 16×16 TGA flags or a country column icon. |
| Images | `material_masks/make_masks.py` | Output directory, default `assets/cstrike/materials`; generates the full 512×512 collection. |
| Matgen materials | `matgen uv`, `matgen check`, `matgen bake edges` | Built executable and model/material paths; output, image size, skin family, or game directory as supported by each command. |
| Studio | `mdl_flags.py` | Source MDL, output folder, optional first-texture-only flag. |
| Studio | `make_labels.py` | Output WAD and space-separated ASCII labels. The output's parent folder must exist. |
| Studio | `make_finish_sample.py` | MDL, model texture name, optional pattern PNG output; writes five images and an adjacent MAT. |
| Studio | `make_maps.py` | Output folder, MDL variants, performance-model folder/names/count, remap model/file, base WAD, and label WAD. |
| Studio | `make_studio_tests_preset.py` | Output MAT for the sample material preset. |
| Studio | `build_maps.ps1` | Game `cstrike` folder and one map name per invocation; compiles and installs the BSP using the game's external map compilers. |
| Image comparison | `imgdiff.py` | Reference/candidate images, optional region and heatmap, RGB tolerance, allowed difference fraction, and all cluster thresholds. |

The **Studio** game-directory field and **Fill paths from the game directory** button fill the related model, WAD, source-map, and preset paths. Select the local installation before filling paths or enter each path explicitly. Fields that depend on an installation start empty; map compilation requires an explicit game directory. See [Studio tools](studio-content.md), [material images](material-masks.md), and [image comparison](image-comparison.md) for content requirements.

Select a built matgen executable explicitly or let the panel find it in this checkout's CMake outputs. An explicit path takes priority. Build the required configuration first; the panel does not build matgen. The selected executable determines which version of the tool processes the files.

An empty matgen output folder writes beside the model. An empty size uses the model texture dimensions; skin family defaults to `0`. For `check`, an empty game directory uses the repository root because that is the panel's tool working directory. See [matgen](matgen.md) for sizing, validation, and output limits.

The Windows map compiler runs `powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ...` with an argument list. The execution policy applies only to that helper process. The four map compilers must already exist under the game root's `tools/` folder. The panel does not download or build them.

## Dedicated server

Every public [server command](test-server.md) is available:

| Commands | Fields used |
|---|---|
| `prepare` | Configuration, optional API repository/module, optional Valve base repair. Installs all supported plugins. |
| `init` | Configuration and optional Valve base repair. Installs the base and TestLab. |
| `doctor`, `build`, `compile-only`, `results`, `status`, `stop` | Configuration. `compile-only` compiles TestLab without installing the server. |
| `start` | Configuration and optional map, IPv4 bind address, and port. |
| `smoke` | Configuration, suite, optional map/port/case, repetitions from 1 to 5. Starts and stops the server. |
| `calibrate` | Configuration and optional `cs_office` or `de_dust2` map. Starts and stops the server. |
| `report` | Configuration and a required run ID. |

The global `--config` option precedes every subcommand. `prepare` and `init` create a missing configuration; `compile-only` can also use defaults without a saved file. Other panel server actions require an existing configuration. Empty API fields use the saved configuration or automatic discovery; literal `auto` requests discovery. API fields apply to preparation. Map/port and smoke fields apply only where the CLI accepts them.

Preparation downloads the required Steam files and dependencies, but starting the server requires a button press. Before stopping it, the utility checks that the process belongs to the selected server. The window cannot close while an operation is running. If the server is running or a start or stop result is unconfirmed, closing requires explicit agreement to leave it running. To stop it, cancel closing, select the configuration, and click **Stop**. **Status** requests the current state; the panel does not poll the server automatically.

## Dependencies and execution

Tkinter and the Python standard library run the panel. Flag generation, labels, and sample finish artwork require Pillow. Material images and image comparison require Pillow and NumPy. If a required package is missing, the panel displays an installation command for the exact selected Python interpreter. Dependencies are installed separately, for example:

```text
python -m pip install Pillow numpy
```

One operation runs at a time in a background worker. The log shows the argument list, combined stdout/stderr, and exit result. Windows helper consoles are hidden. Wait for an operation to finish before closing; process cancellation is unavailable. Server timeouts belong to its configuration. The log keeps the latest 200,000 characters; **Save log...** exports the displayed part as UTF-8. If the comparison tool returns `1`, the panel reports a failed comparison and keeps the measurements in the log.

## Verification

```text
python -X utf8 -m unittest discover -s tools/dev_tools/tests -v
python -X utf8 tools/dev_tools/dev_tools.py --language en --self-check
python -X utf8 tools/dev_tools/dev_tools.py --language ru --self-check
```

The tests cover commands, processes, window behavior, translation completeness, language switching, launch-script exit status, and files produced by generators in temporary directories for the test cases. GUI tests require a usable desktop and Tcl/Tk file access. The self-check creates and destroys a hidden window without running tools. Tests do not launch a game or real server, access Steam, install dependencies, compile maps, run matgen generation, or regenerate the complete image collection.

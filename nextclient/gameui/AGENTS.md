# GameUI

`src/GameUi/GameUi.cpp` connects the DLL to the engine and VGUI factories. Controls, dialog resource files and localized strings form one feature; resources are shipped under `assets/cstrike`.

## Interfaces and controls

- Resolve surface, scheme and engine interfaces through the established factory chain. Font and
  texture handles belong to the surface that created them; a separately linked or constructed
  surface cannot safely stand in for the engine's instance.
- VGUI action signals may be posted for later delivery. A temporary boolean around a programmatic
  update does not suppress a signal delivered after that boolean is reset. Use the controls'
  silent-update support, including `SilentActivateItemByRow` for combo boxes where available.
- Enum selections use non-editable combo boxes. Slider text formatting and numeric step size are
  separate settings; changing the displayed precision must not accidentally change the value range
  or keyboard increment.
- Use tier1's existing string and Unicode helpers. Its `V_` names are canonical and `Q_` names are
  aliases; do not add another decoder or bounded string utility for a dialog.

## Live settings and in-game panels

- `CHudRadarDialog` in `src/GameUi/OptionsDialog/HudRadarDialog.cpp` is a live-preview dialog.
  Preserve its snapshot, Apply/OK and discard behavior when extending it. Add each new setting to
  both control synchronization and snapshot restoration.
- Engine commands sent by `pfnClientCmd` are queued. Writing a cvar and immediately reading it back
  can read the previous value. Defaults and programmatic previews must keep control state coherent
  until the engine executes the command.
- Radar cvar names and defaults come from
  `nextclient/client_mini/include/next_client_mini/radar_cvars.h`. Keep the dialog table consistent
  with that definition instead of copying defaults into UI code.
- Every close path must release modal input and restore the underlying in-game panel state,
  including spectator UI. Review Esc, close, Apply/OK and discard paths together; mouse capture and
  keyboard focus are part of the behavior, not just panel visibility.

## Verification

Build `gameui` and run the affected `gameui-tests`; changes to shared gameplay definitions also need `client_mini`. Check resource control names against the C++ bindings and localization keys against the shipped language files. A build does not validate VGUI layout, posted-signal timing or the in-game focus transition; describe those limits when handing off a UI change.

## Material authoring ownership

`src/GameUi/MaterialEditor/` groups the material editor dialog, working-session helpers, catalog, thumbnails, and dedicated light/model/preview controls in the internal `material_editor` namespace. SDK and provider declarations retain their external namespaces. Shared color, image-file, and drag-value controls stay in `src/GameUi/` and `src/GameUi/Controls/`. Player-facing resources remain under `assets/cstrike`.

`CMaterialEditDialog` owns the working edit session, transition decisions, viewport state, and temporary parameter previews. `render::MaterialEditInterface::BeginParamPreview` snapshots each selected texture's parameter; `EndParamPreview` accepts or restores the complete group, including absent and mixed values. Browser and color-picker exits must close that transaction without restarting the working session. Closing hides the viewport, ends the edit session, and calls `ScenePreviewInterface::CloseAuthoring` before the frame completes closure and releases modal input. The preview owner releases authoring targets, captured references, shared model geometry, and material images while preserving metadata, admission accounting, and independent scene handles. The editor is available only in the disconnected main menu; entering a connection immediately removes its material overrides.

CGameUI composes authoring providers at Initialize and developer controls at Start, after receiving the real engine callback table. The dialog owns its resource source and catalog. Image panels and browser entries retain their supplied shared image cache through actual delayed VGUI deletion; drawing and deletion use its original surface. Reject posted edits once terminal closure begins. Shutdown blocks authoring reentry and drains one ordinary VGUI frame before disconnecting providers, independently of server-browser availability.

`material_editor::RenderModelThumbnail` reads validated files and uses `ncl_studio::ReadStudioReferencePose` for CPU thumbnail positions and independent normal bones. Catalog scrolling does not prepare engine models. Selecting a model uses `ScenePreviewInterface::PrepareModel` and the shared process budget of 128 new engine model-table entries, including texture companions. Closing the editor and video changes preserve that accounting. Keep the file-only thumbnail path separate from bounded engine preparation.

Preview interaction ends on actual mouse capture loss or focus loss. A queued capture-loss message must preserve a newer drag while the preview owns capture again; cancellation releases capture only when the preview is its current owner. `material-preview-panel-tests` compiles the production panel and camera state with narrow VGUI fakes; `drag-value-label-tests` verifies the shared fake input/control support.

The maintained behavior and verification contracts are [material authoring](../../docs/weapon-materials-authoring.md) and [scene verification](../../docs/testing/material-editor-scene.md). Geometry, material and preview resource ownership follows the [renderer owner rules](../engine_mini/src/graphics/render/AGENTS.md); shared reference-pose semantics follow the [studio package rules](../packages/ncl_studio/AGENTS.md).

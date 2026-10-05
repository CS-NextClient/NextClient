# GameUI

`src/GameUi/GameUi.cpp` connects the DLL to the engine and VGUI factories. Controls, dialog resource
files and localized strings form one feature; resources are shipped under `assets/cstrike`.

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

- Engine commands sent by `pfnClientCmd` are queued. Writing a cvar and immediately reading it back
  can read the previous value. Defaults and programmatic previews must keep control state coherent
  until the engine executes the command.
- Every close path must release modal input and restore the underlying in-game panel state,
  including spectator UI. Review Esc, close, Apply/OK and discard paths together; mouse capture and
  keyboard focus are part of the behavior, not just panel visibility.

## Verification

Build `gameui` and run the affected `gameui-tests`; changes to shared gameplay definitions also
need `client_mini`. Check resource control names against the C++ bindings and localization keys
against the shipped language files. A build does not validate VGUI layout, posted-signal timing or
the in-game focus transition; describe those limits when handing off a UI change.

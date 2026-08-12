#pragma once

struct ref_params_s;
class MaterialEditInterface;

// The client's half of the material editor: the copy of a model put into the world, and the
// camera the panel flies while it is open.
//
// The copy is there because a material has to be judged the way other players see it. The
// model they see is a different file from the one in the author's hands, and a view model is
// glued to the camera, so turning the camera says nothing about how a weapon looks from
// another angle; this can be turned, flown around and stopped on any frame.
//
// It is an ordinary entity handed to the engine, not a preview of our own: the lighting, the
// chrome and the animation are the real ones because none of them is reimplemented.
void MaterialPreview_Init(MaterialEditInterface* editor);

// Called once a frame while the client is building its entity list.
void MaterialPreview_CreateEntities();

// A map has been loaded. The engine's model table is reusable again from here, which is what
// the editor's own count of what it has loaded into it is measured against.
void MaterialPreview_VidInit();

// How many pixels of the frame the docked panel covers. Anything the hud draws against the
// middle of the screen is drawn against the middle of nothing while a panel is open.
int MaterialPreview_ViewportInset();

// Puts the frame where the panel's camera is. Called at the end of the view calculation, so
// what it overwrites is the finished answer and everything derived from it - the visible
// set, the sound and the radar - follows the camera rather than the body left behind.
void MaterialPreview_ApplyCamera(ref_params_s* pparams);

// Shows, hides or holds the weapon in the author's hands. Called from the same place and for
// the same reason: the engine hands the weapon to the client once a frame and takes the
// answer back, so what is written here lasts exactly one frame and nothing has to undo it.
void MaterialPreview_ApplyViewmodel(ref_params_s* pparams);

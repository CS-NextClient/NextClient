#pragma once

#include <string_view>

#include <ncl_material/MaterialDoc.h>
#include <goldsrc/fwd.h>

class MaterialEditInterface;

namespace render
{
    // The editor's own copy of a material while a panel holds one, which is the top of the
    // resolution order: a slider has to change what is on screen without writing a file and
    // reloading. Null unless a session is open on exactly this target.
    const ncl_material::Material* MaterialEdit_Override(std::string_view model, std::string_view texture_name);

    // What the panel is asking to be shown instead of what the entity says. Negative means
    // it is asking for nothing, which is also the answer for every model but the one being
    // edited. Neither changes what a material applies to; both change what is on screen.
    int MaterialEdit_ShownSkin(std::string_view model);
    int MaterialEdit_ShownBody(std::string_view model);

    // How many pixels of the frame the docked panel covers on the right, given the width of
    // the frame the world is about to be drawn into. Zero unless a panel is docked, which is
    // every frame the editor is not open.
    int MaterialEdit_ViewportInset(int frame_wide);

    // What was taken off this frame, remembered by the renderer that took it. Anything drawn
    // over the world afterwards is placed against the whole screen and has to be moved by
    // half of this to agree with where the world ended up.
    void MaterialEdit_SetAppliedInset(int pixels);
    int MaterialEdit_AppliedInset();

    // Puts the panel's animation on the weapon in the author's hands, by saying it in the
    // only terms the engine reads: the sequence and the moment it started.
    //
    // Writing the entity does not work at any point, however late. R_DrawViewModel rewrites
    // the view entity's whole animation state from these two immediately before it draws,
    // inside the same call, so the entity carries the game's animation no matter who wrote
    // what on it beforehand. The stand-in has no such owner and is driven from the client.
    //
    // Both are the caller's to hand over: this file is the panel's side of the editor, and
    // the client state belongs to the engine side. They are restored when the panel stops
    // driving, so the game gets its weapon back where it left it.
    void MaterialEdit_ApplyViewmodel(goldsrc::cl_entity_t* entity, float now, int* weapon_sequence, float* weapon_start_time);

    // Whether a panel holds the editor at all. What the editor puts into the running game is
    // gated on this, so that a closed panel changes nothing about what the game draws.
    bool MaterialEdit_PanelOpen();

    // Whether the panel is driving that animation at all. The engine's own preparation pass
    // has to be skipped while it is, because that pass poses the weapon once with the game's
    // animation before the draw poses it again with the panel's: the renderer then sees the
    // sequence change twice a frame and blends between the two, which is a weapon that
    // shakes rather than one that plays.
    bool MaterialEdit_DrivesViewmodel();

    // What the weapon in the author's hands held at the moment the renderer came to draw it.
    // The panel writes that state through the client a whole step earlier; whether it is
    // still there when the model is posed is a different question, and one nothing else in
    // the report can answer.
    void MaterialEdit_NoteViewmodel(const goldsrc::cl_entity_t* entity, float now);

    MaterialEditInterface* MaterialEdit_Get();

    // Cmd_AddCommand after host_initialized takes the engine down, so these go up with the
    // rest of the renderer's commands.
    void MaterialEdit_RegisterCommands();
}

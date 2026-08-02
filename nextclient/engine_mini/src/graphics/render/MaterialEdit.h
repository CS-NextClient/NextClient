#pragma once

#include <string_view>

#include <ncl_material/MaterialDoc.h>

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

    // Puts the panel's animation on the weapon in the author's hands, at the last moment
    // before it is drawn.
    //
    // It has to be this late. The engine rewrites that entity's whole animation state from
    // its own cl.weaponsequence and cl.weaponstarttime immediately before drawing it, so
    // anything written earlier - by the client, a frame's worth of work earlier - is thrown
    // away without a trace. The stand-in has no such owner and is driven from the client.
    void MaterialEdit_ApplyViewmodel(struct cl_entity_s* entity, float now);

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
    void MaterialEdit_NoteViewmodel(const struct cl_entity_s* entity, float now);

    MaterialEditInterface* MaterialEdit_Get();

    // Cmd_AddCommand after host_initialized takes the engine down, so these go up with the
    // rest of the renderer's commands.
    void MaterialEdit_RegisterCommands();
}

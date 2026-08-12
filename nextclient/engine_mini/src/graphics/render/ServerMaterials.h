#pragma once

#include <string>
#include <string_view>

#include <ncl_material/MaterialPreset.h>

namespace render
{
    // What one entity may wear at once. Not a shape of the protocol - a binding names its
    // own texture and needs no slot - but a ceiling on what one entity can cost. Public
    // because the side that accumulates the bindings has to hold the same ceiling: what it
    // keeps past this is work whose only result is a refusal.
    constexpr size_t kMaxBindingsPerEntity = 16;

    // What a server has dressed entities in.
    //
    // Nothing announces a preset. A binding names the file it comes from, so the file is
    // read the first time a binding needs it and kept afterwards; a server that dresses
    // nothing costs nothing. The delivery of the file itself is the ordinary download.
    void ServerMaterials_Clear();

    struct EntityMaterialQuery
    {
        int entity_index = 0;
        std::string_view texture_name;
    };

    // The material the server put on this entity for this texture, or nullptr. This is the
    // instance source of the resolution order, applied here rather than in Resolve because
    // Resolve runs once per model and a binding belongs to one entity.
    const ncl_material::Material* ServerMaterials_BindingFor(const EntityMaterialQuery& query);

    // A material of a preset, by the name the preset gives it. Binding one replaces
    // whatever else dressed the same texture of that entity: two materials on one texture
    // is a question with no answer, and the later one is the one that was just asked for.
    void ServerMaterials_SetBinding(int entity_index, const char* preset, const char* name);
    void ServerMaterials_RemoveBinding(int entity_index, const char* preset, const char* name);
    void ServerMaterials_ClearBindings(int entity_index);

    // One instance value, in steps of the quantum the program declares. The key is the
    // parameter's name, optionally prefixed by the texture it belongs to:
    // "uWear" writes over every material the entity wears, "glock_slide.bmp/uWear" over one.
    //
    // False when nothing took it: the entity wears nothing matching, or the program does
    // not declare that name as an instance scalar.
    bool ServerMaterials_SetParam(int entity_index, const char* key, int steps);

    // Diagnostic only, and a no-op unless r_mat_entities armed it: a binding has to be
    // aimed at an entity index, and without a server there is nothing to say which index
    // is which.
    void ServerMaterials_NoteEntity(int entity_index, std::string_view model);

    // Bumped whenever a preset is loaded or dropped, so caches that resolved against one
    // can tell without being told.
    unsigned int ServerMaterials_Generation();

    // Split because the engine refuses Cmd_AddCommand once the host is initialized.
    void ServerMaterials_RegisterCommands();
}

#include "graphics/render/material/ServerMaterials.h"

#include <charconv>
#include <cmath>

#include "engine.h"
#include "common/cmd.h"
#include "common/common.h"
#include "console/console.h"
#include "graphics/render/render_state.h"
#include "graphics/render/material/StudioProgramContract.h"

namespace render
{
    namespace
    {
        bool IsNetworkSession()
        {
            return cls != nullptr && cls->state != goldsrc::ca_disconnected;
        }


        bool CheckConsoleMutation()
        {
            if (!IsNetworkSession())
            {
                return true;
            }
            Con_Printf("materials: local changes require a disconnected client\n");
            return false;
        }

    } // namespace

    bool ServerMaterials_ReplaceOwner(
        const ServerMaterialBindingOwner& owner,
        std::span<const ServerMaterialBinding> bindings,
        std::span<const ServerMaterialParam> params
    )
    {
        return GetServerMaterials().ReplaceOwner(owner, bindings, params);
    }

    bool ServerMaterials_ReplaceOwnerParams(const ServerMaterialBindingOwner& owner, std::span<const ServerMaterialParam> params)
    {
        return GetServerMaterials().ReplaceOwnerParams(owner, params);
    }

    void ServerMaterials_RemoveOwner(const ServerMaterialBindingOwner& owner)
    {
        GetServerMaterials().RemoveOwner(owner);
    }

    void ServerMaterials_Clear()
    {
        GetServerMaterials().Clear();
    }

    const ncl_material::Material* ServerMaterials_BindingFor(const EntityMaterialQuery& query)
    {
        return GetServerMaterials().BindingFor(query, IsNetworkSession());
    }

    bool ServerMaterials_AnyBindingMatches(int entity_index, bool (*matches)(const ncl_material::Material&))
    {
        return GetServerMaterials().AnyBindingMatches(
            entity_index,
            [](void* user, const ncl_material::Material& material) {
                return (*static_cast<bool (**)(const ncl_material::Material&)>(user))(material);
            },
            IsNetworkSession(),
            &matches
        );
    }

    void ServerMaterials_SetBinding(int entity_index, const char* preset, const char* name)
    {
        GetServerMaterials().SetBinding(entity_index, preset, name, IsNetworkSession());
    }

    void ServerMaterials_SetBinding(const ServerMaterialBindingOwner& owner, const char* preset, const char* name)
    {
        GetServerMaterials().SetBinding(owner, preset, name);
    }

    void ServerMaterials_RemoveBinding(int entity_index, const char* preset, const char* name)
    {
        GetServerMaterials().RemoveBinding(entity_index, preset, name, IsNetworkSession());
    }

    void ServerMaterials_ClearBindings(int entity_index)
    {
        GetServerMaterials().ClearBindings(entity_index);
    }

    bool ServerMaterials_SetParam(int entity_index, const char* key, float value)
    {
        return GetServerMaterials().SetParam(entity_index, key, value, IsNetworkSession());
    }

    bool ServerMaterials_SetParam(const ServerMaterialBindingOwner& owner, const char* key, float value)
    {
        return GetServerMaterials().SetParam(owner, key, value);
    }

    void ServerMaterials_NoteEntity(int entity_index, std::string_view model)
    {
        GetServerMaterials().NoteEntity(entity_index, model);
    }

    unsigned int ServerMaterials_Generation()
    {
        return GetServerMaterials().get_generation();
    }

    namespace
    {
        void MaterialEntities_f()
        {
            GetServerMaterials().ArmDiagnostics();

            Con_Printf("r_mat_entities: studio entities drawn from here\n");
        }

        void MaterialPresets_f()
        {
            if (Cmd_Argc() >= 2 && !CheckConsoleMutation())
            {
                return;
            }
            if (Cmd_Argc() >= 2)
            {
                if (V_stricmp(Cmd_Argv(1), "clear") == 0)
                {
                    ServerMaterials_Clear();
                    Con_Printf("materials: presets cleared\n");
                    return;
                }

                GetServerMaterials().PreloadPreset(Cmd_Argv(1));
                return;
            }

            GetServerMaterials().ReportPresets();
        }

        void MaterialBind_f()
        {
            if (Cmd_Argc() >= 2 && !CheckConsoleMutation())
            {
                return;
            }
            if (Cmd_Argc() >= 4)
            {
                ServerMaterials_SetBinding(atoi(Cmd_Argv(1)), Cmd_Argv(2), Cmd_Argv(3));
                return;
            }

            if (Cmd_Argc() == 2)
            {
                ServerMaterials_ClearBindings(atoi(Cmd_Argv(1)));
                return;
            }

            Con_Printf("r_mat_bind <entity> <preset> <material>, or <entity> alone to undress it\n");

            GetServerMaterials().ReportBindings();
        }

        void MaterialParam_f()
        {
            if (!CheckConsoleMutation())
            {
                return;
            }
            if (Cmd_Argc() < 4)
            {
                Con_Printf("r_mat_param <entity> <name|texture/name> <value>\n");
                return;
            }

            const std::string_view text(Cmd_Argv(3));
            float value = 0.0f;
            const std::from_chars_result parsed = std::from_chars(text.data(), text.data() + text.size(), value);
            if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size() || !std::isfinite(value))
            {
                Con_Printf("r_mat_param: expected a finite float value\n");
                return;
            }

            if (!ServerMaterials_SetParam(atoi(Cmd_Argv(1)), Cmd_Argv(2), value))
            {
                Con_Printf(
                    "r_mat_param: nothing took it; the entity wears nothing matching, or "
                    "\"%s\" is not an instance scalar of its program\n",
                    Cmd_Argv(2)
                );
            }
        }
    } // namespace

    void ServerMaterials_RegisterCommands()
    {
        Cmd_AddCommand("r_mat_presets", MaterialPresets_f);
        Cmd_AddCommand("r_mat_bind", MaterialBind_f);
        Cmd_AddCommand("r_mat_entities", MaterialEntities_f);
        Cmd_AddCommand("r_mat_param", MaterialParam_f);
    }
    void ServerMaterials_ApplyEffectPolicy(ncl_material::Material& material, bool server_bound)
    {
        if (IsNetworkSession() && !server_bound)
        {
            StudioContract_DisableRestrictedEffects(material);
        }
    }

} // namespace render

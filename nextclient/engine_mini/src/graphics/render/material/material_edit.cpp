#include "graphics/render/material/material_edit.h"

#include <cstdarg>
#include <cstdlib>
#include <limits>
#include <string>
#include <vector>

#include <next_engine_mini/MaterialEditInterface.h>

#include "common/cmd.h"
#include "common/common.h"
#include "console/console.h"
#include "engine.h"
#include "graphics/render/material/MaterialEditor.h"
#include "graphics/render/material/MaterialDocumentStore.h"
#include "graphics/render/render_state.h"
#include "graphics/render/RenderConfig.h"

namespace render
{
    namespace
    {
        MaterialEditor& Editor()
        {
            return GetMaterialEditor();
        }

        bool IsNetworkSession()
        {
            return cls != nullptr && cls->state != goldsrc::ca_disconnected;
        }

        const char* SourceName(MaterialEditSource source)
        {
            switch (source)
            {
                case MaterialEditSource::Instance:
                    return "server binding";
                case MaterialEditSource::ModelDocument:
                    return "model document";
                case MaterialEditSource::Implicit:
                    return "implicit";
                default:
                    return "none";
            }
        }

        void PrintUsage()
        {
            Con_Printf("r_mat_edit models                   models prepared this map\n");
            Con_Printf("r_mat_edit textures <model>         texture names and where each material comes from\n");
            Con_Printf("r_mat_edit begin <model> <texture>  start editing, seeded from what is on screen\n");
            Con_Printf("r_mat_edit end                      drop the editor's layer\n");
            Con_Printf("r_mat_edit program <name>           switch program, taking its defaults\n");
            Con_Printf("r_mat_edit show                     the schema and the current values\n");
            Con_Printf("r_mat_edit set <param> <value...>   a number, four numbers, or a texture path\n");
            Con_Printf("r_mat_edit clear <param>\n");
            Con_Printf("r_mat_edit save [model]             writes <model>.mat, defaulting to the edited one\n");
            Con_Printf("r_mat_edit stub                     the same material as a preset entry\n");
            Con_Printf("r_mat_edit reload                   re-read the documents beside the models\n");
            Con_Printf("r_mat_state                         what the editor currently believes\n");
        }

        void PrintPrograms()
        {
            Con_Printf("programs with a manifest:\n");

            for (int i = 0; i < Editor().GetProgramCount(); i++)
            {
                Con_Printf("  %s\n", Editor().get_program_name(i));
            }
        }

        void PrintModels()
        {
            int count = Editor().get_model_count();

            for (int i = 0; i < count; i++)
            {
                Con_Printf("  %s\n", Editor().GetModelName(i));
            }

            Con_Printf("%d models\n", count);

            if (count != 0)
            {
                return;
            }

            bool shader_path_on = g_RenderCfg.studio_shader != nullptr && g_RenderCfg.studio_shader->value != 0.0f;

            Con_Printf(
                shader_path_on ? "r_mat_edit: nothing has been drawn through the shader path yet; look at a model first\n"
                               : "r_mat_edit: r_studio_shader is 0, so no model has a material at all; set it to 1 and look at a model\n"
            );
        }

        void PrintTextures(const char* model)
        {
            int count = Editor().get_texture_count(model);

            for (int i = 0; i < count; i++)
            {
                const char* name = Editor().get_texture_name(model, i);
                Con_Printf("  %-24s %s\n", name, SourceName(Editor().GetMaterialSource(model, name)));
            }

            if (count == 0)
            {
                Con_Printf("r_mat_edit: %s is not prepared; look at it once first\n", model);
            }
        }

        void PrintValue(const MaterialEditParam& param)
        {
            if (param.type == MaterialEditParamType::Scalar)
            {
                float value = 0.0f;

                if (Editor().get_scalar(param.name, &value))
                {
                    Con_Printf(" = %.*g", std::numeric_limits<float>::max_digits10, value);
                }

                return;
            }

            if (param.type == MaterialEditParamType::Vec4)
            {
                float value[4] = {};

                if (Editor().get_vec4(param.name, value))
                {
                    Con_Printf(
                        " = %.*g %.*g %.*g %.*g",
                        std::numeric_limits<float>::max_digits10,
                        value[0],
                        std::numeric_limits<float>::max_digits10,
                        value[1],
                        std::numeric_limits<float>::max_digits10,
                        value[2],
                        std::numeric_limits<float>::max_digits10,
                        value[3]
                    );
                }

                return;
            }

            const char* path = Editor().get_texture(param.name);

            if (*path != '\0')
            {
                Con_Printf(" = %s", path);
            }
        }

        void PrintSchema()
        {
            if (!Editor().is_editing())
            {
                Con_Printf("r_mat_edit: nothing is being edited\n");
                return;
            }

            const char* program = Editor().get_edited_program();

            Con_Printf("%s, texture %s\n", Editor().get_edited_model(), Editor().get_edited_texture());
            Con_Printf(
                "program %s, underlying source %s\n",
                program,
                SourceName(Editor().GetMaterialSource(
                    Editor().get_edited_model(), Editor().get_edited_texture(), Editor().get_edited_entity_index()
                ))
            );

            for (int group = 0; group < Editor().GetGroupCount(program); group++)
            {
                const char* label = Editor().GetGroupLabel(program, group);
                Con_Printf("  [%s]\n", *label != '\0' ? label : "-");

                for (int i = 0; i < Editor().GetGroupParamCount(program, group); i++)
                {
                    MaterialEditParam param;

                    if (!Editor().GetParam(program, Editor().GetGroupParam(program, group, i), &param))
                    {
                        continue;
                    }

                    Con_Printf("    %-20s %s", param.name, param.instance ? "instance" : "        ");
                    PrintValue(param);
                    Con_Printf("\n");
                }
            }

            const char* problem = Editor().Validate();
            Con_Printf(*problem != '\0' ? "invalid: %s\n" : "valid%s\n", problem);
        }

        void SetFromArguments()
        {
            const char* name = Cmd_Argv(2);
            int values = Cmd_Argc() - 3;

            if (values == 1)
            {
                const char* text = Cmd_Argv(3);
                char* end = nullptr;
                float number = static_cast<float>(strtod(text, &end));

                bool ok = end != text && *end == '\0' ? Editor().SetScalar(name, number) : Editor().SetTexture(name, text);

                if (!ok)
                {
                    Con_Printf("r_mat_edit: \"%s\" is not a parameter of this program, or is of another type\n", name);
                }

                return;
            }

            if (values == 4)
            {
                float vector[4];

                for (int i = 0; i < 4; i++)
                {
                    vector[i] = static_cast<float>(atof(Cmd_Argv(3 + i)));
                }

                if (!Editor().SetVec4(name, vector))
                {
                    Con_Printf("r_mat_edit: \"%s\" is not a vec4 parameter of this program\n", name);
                }

                return;
            }

            Con_Printf("r_mat_edit set <param> <number|path|four numbers>\n");
        }

        void StateLine(std::string& state, const char* format, ...)
        {
            char line[512];

            va_list args;
            va_start(args, format);
            Q_vsnprintf(line, sizeof(line), format, args);
            va_end(args);

            state += line;
        }

        std::string BuildState()
        {
            std::string state;

            StateLine(
                state,
                "session editing %s, entity %d, %s : %s, program %s\n",
                Editor().is_editing() ? "yes" : "no",
                Editor().get_edited_entity_index(),
                Editor().get_edited_model(),
                Editor().get_edited_texture(),
                Editor().get_edited_program()
            );
            StateLine(
                state,
                "renderer prepared models %d, programs %d\n",
                Editor().get_model_count(),
                Editor().GetProgramCount()
            );

            return state;
        }

        void WriteStateReport()
        {
            const std::string state = BuildState();

            Con_Printf("%s", state.c_str());

            std::string error;

            if (!GetMaterialDocuments().Write("materials_state.txt", state, error))
            {
                Con_Printf("r_mat_state: %s\n", error.c_str());
            }
        }

        void MaterialState_f()
        {
            WriteStateReport();
        }

        void MaterialEdit_f()
        {
            if (Cmd_Argc() < 2)
            {
                PrintUsage();

                if (Editor().is_editing())
                {
                    Con_Printf("\n");
                    PrintSchema();
                }

                return;
            }

            const char* verb = Cmd_Argv(1);

            if (V_stricmp(verb, "models") == 0)
            {
                PrintModels();
                return;
            }

            if (V_stricmp(verb, "programs") == 0)
            {
                PrintPrograms();
                return;
            }

            if (V_stricmp(verb, "textures") == 0 && Cmd_Argc() >= 3)
            {
                PrintTextures(Cmd_Argv(2));
                return;
            }

            if (V_stricmp(verb, "begin") == 0 && Cmd_Argc() >= 4)
            {
                if (cls != nullptr && cls->state != goldsrc::ca_disconnected)
                {
                    Con_Printf("r_mat_edit: editing is available from the disconnected main menu\n");
                    return;
                }

                if (!Editor().Begin(Cmd_Argv(2), Cmd_Argv(3)))
                {
                    Con_Printf("r_mat_edit: no such prepared model or texture\n");
                    return;
                }

                PrintSchema();
                return;
            }

            if (V_stricmp(verb, "end") == 0)
            {
                Editor().End();
                Con_Printf("r_mat_edit: editing ended, the model is back on its own material\n");
                return;
            }

            if (V_stricmp(verb, "program") == 0 && Cmd_Argc() >= 3)
            {
                if (!Editor().SetEditedProgram(Cmd_Argv(2)))
                {
                    Con_Printf("r_mat_edit: no program \"%s\" with a manifest\n", Cmd_Argv(2));
                    return;
                }

                PrintSchema();
                return;
            }

            if (V_stricmp(verb, "show") == 0)
            {
                PrintSchema();
                return;
            }

            if (V_stricmp(verb, "set") == 0 && Cmd_Argc() >= 4)
            {
                SetFromArguments();
                return;
            }

            if (V_stricmp(verb, "clear") == 0 && Cmd_Argc() >= 3)
            {
                if (!Editor().ClearParam(Cmd_Argv(2)))
                {
                    Con_Printf("r_mat_edit: \"%s\" is not set\n", Cmd_Argv(2));
                }

                return;
            }

            if (V_stricmp(verb, "save") == 0)
            {
                char error[256] = {};

                if (!Editor().Save(Cmd_Argc() >= 3 ? Cmd_Argv(2) : nullptr, error, sizeof(error)))
                {
                    Con_Printf("r_mat_edit: %s\n", error);
                    return;
                }

                Con_Printf("r_mat_edit: wrote the document; \"r_mat_edit end\" to see it without the editor's layer\n");
                return;
            }

            if (V_stricmp(verb, "stub") == 0)
            {
                Con_Printf("%s\n", Editor().GetPresetStub());
                return;
            }

            if (V_stricmp(verb, "reload") == 0)
            {
                Editor().Reload();
                Con_Printf("r_mat_edit: documents will be re-read\n");
                return;
            }

            PrintUsage();
        }
    } // namespace

    bool MaterialEditor::Begin(const char* model, const char* texture_name, int entity_index)
    {
        const char* textures[] = {texture_name};
        return BeginGroup(model, textures, 1, entity_index, IsNetworkSession);
    }

    bool MaterialEditor::BeginGroup(const char* model, const char* const* textures, int count, int entity_index)
    {
        return BeginGroup(model, textures, count, entity_index, IsNetworkSession);
    }

    MaterialEditSource MaterialEditor::GetMaterialSource(const char* model, const char* texture_name, int entity_index)
    {
        return GetMaterialSource(model, texture_name, entity_index, IsNetworkSession);
    }

    const ncl_material::Material* MaterialEdit_Override(std::string_view model, std::string_view texture_name)
    {
        return !IsNetworkSession() ? Editor().get_session().FindOverride(model, texture_name) : nullptr;
    }

    void MaterialEdit_End()
    {
        Editor().End();
    }

    MaterialEditInterface* MaterialEdit_Get()
    {
        return &Editor();
    }

    void MaterialEdit_RegisterCommands()
    {
        Cmd_AddCommand("r_mat_edit", MaterialEdit_f);
        Cmd_AddCommand("r_mat_state", MaterialState_f);
    }
} // namespace render

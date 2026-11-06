#include "ncl_studio/studio_mesh_builder.h"

#include <algorithm>
#include <string>
#include <vector>

#include <goldsrc/format/studio.h>

#include "HeaderShape.h"
#include "table_bounds.h"

namespace ncl_studio
{
    namespace
    {
        struct Buffer
        {
            const uint8_t* data;
            size_t size;
        };

        template <class T>
        const T* At(const Buffer& buffer, int offset)
        {
            return reinterpret_cast<const T*>(buffer.data + offset);
        }

        ncl_math::Vector3 ReadVec3(const goldsrc::vec3_t& source)
        {
            return ncl_math::Vector3{source[0], source[1], source[2]};
        }

        std::string MeshContext(int bodypart, int submodel, int mesh)
        {
            return "bodypart " + std::to_string(bodypart) + ", submodel " + std::to_string(submodel) + ", mesh " + std::to_string(mesh);
        }

        // Expands one mesh's command list. A run starts with a signed short count:
        // zero ends the list, a negative count is a triangle fan and a positive one a
        // triangle strip. Vertices are emitted per run entry rather than deduplicated,
        // which matches what the fixed function path feeds the driver today.
        bool BuildMesh(
            const Buffer& buffer,
            const goldsrc::mstudiomodel_t& submodel,
            const goldsrc::mstudiomesh_t& mesh,
            int bone_count,
            std::vector<int>& shell_normals,
            StudioMesh& out,
            std::string& error
        )
        {
            if (!TableFits(buffer.size, mesh.triindex, 1, sizeof(short)))
            {
                error = "triangle command list starts outside the file";

                return false;
            }

            const goldsrc::vec3_t* positions = At<goldsrc::vec3_t>(buffer, submodel.vertindex);
            const goldsrc::vec3_t* normals = At<goldsrc::vec3_t>(buffer, submodel.normindex);
            const uint8_t* position_bones = At<uint8_t>(buffer, submodel.vertinfoindex);
            const uint8_t* normal_bones = At<uint8_t>(buffer, submodel.norminfoindex);

            size_t cursor = static_cast<size_t>(mesh.triindex);

            while (true)
            {
                if (cursor + sizeof(short) > buffer.size)
                {
                    error = "triangle command list runs past the end of the file";

                    return false;
                }

                short run = *reinterpret_cast<const short*>(buffer.data + cursor);
                cursor += sizeof(short);

                if (run == 0)
                {
                    break;
                }

                bool is_fan = run < 0;
                int run_length = is_fan ? -run : run;

                if (run_length < 3)
                {
                    error = "triangle run shorter than a single triangle";

                    return false;
                }

                size_t run_bytes = static_cast<size_t>(run_length) * sizeof(StudioTriVertex_t);

                if (run_bytes > buffer.size - cursor)
                {
                    error = "triangle run runs past the end of the file";

                    return false;
                }

                const StudioTriVertex_t* run_verts = reinterpret_cast<const StudioTriVertex_t*>(buffer.data + cursor);
                cursor += run_bytes;

                uint32_t run_start = static_cast<uint32_t>(out.vertices.size());

                for (int i = 0; i < run_length; i++)
                {
                    const StudioTriVertex_t& source = run_verts[i];

                    if (source.vertindex < 0 || source.normindex < 0 || source.vertindex >= submodel.numverts ||
                        source.normindex >= submodel.numnorms)
                    {
                        error = "triangle references a vertex or normal outside the submodel";

                        return false;
                    }

                    // The bone bytes are file data too, and the renderer turns them into an
                    // index into a fixed bone array, where a value past the model's own bone
                    // count reads outside it.
                    if (position_bones[source.vertindex] >= bone_count || normal_bones[source.normindex] >= bone_count)
                    {
                        error = "vertex references a bone outside the model";

                        return false;
                    }

                    // First mention wins and every later copy of the vertex takes it, which
                    // is what the engine's own table does. Filled while emitting rather
                    // than in a pass of its own: the order the command lists are walked in
                    // is the same one, so the first mention is reached first either way.
                    int& shell_normal = shell_normals[source.vertindex];

                    if (shell_normal < 0)
                    {
                        shell_normal = source.normindex;
                    }

                    Vertex vertex;
                    vertex.position = ReadVec3(positions[source.vertindex]);
                    vertex.normal = ReadVec3(normals[source.normindex]);
                    vertex.shell_normal = ReadVec3(normals[shell_normal]);
                    vertex.texel = ncl_math::Vector2{static_cast<float>(source.s), static_cast<float>(source.t)};
                    vertex.position_bone = position_bones[source.vertindex];
                    vertex.normal_bone = normal_bones[source.normindex];
                    vertex.shell_normal_bone = normal_bones[shell_normal];

                    out.vertices.push_back(vertex);
                }

                // Every vertex emitted comes from one ncl_studio::StudioTriVertex_t in a command list,
                // and in a well formed model each mesh owns its own command list, so the
                // file cannot describe more vertices than it has room for. Nothing stops
                // the body part, submodel and mesh arrays from all pointing at the same
                // region, though, and that turns a small file into an unbounded amount of
                // work; this is the bound the layout implies rather than a chosen ceiling.
                if (out.vertices.size() > buffer.size / sizeof(StudioTriVertex_t))
                {
                    error = "model expands to more vertices than its own size can describe";

                    return false;
                }

                if (is_fan)
                {
                    for (int i = 1; i + 1 < run_length; i++)
                    {
                        out.indices.push_back(run_start);
                        out.indices.push_back(run_start + static_cast<uint32_t>(i));
                        out.indices.push_back(run_start + static_cast<uint32_t>(i) + 1);
                    }
                }
                else
                {
                    // Every other strip triangle is wound the other way round, so the
                    // first two indices swap to keep the whole mesh consistently wound.
                    for (int i = 2; i < run_length; i++)
                    {
                        uint32_t a = run_start + static_cast<uint32_t>(i) - 2;
                        uint32_t b = run_start + static_cast<uint32_t>(i) - 1;
                        uint32_t c = run_start + static_cast<uint32_t>(i);

                        if (i % 2 == 0)
                        {
                            out.indices.push_back(a);
                            out.indices.push_back(b);
                        }
                        else
                        {
                            out.indices.push_back(b);
                            out.indices.push_back(a);
                        }

                        out.indices.push_back(c);
                    }
                }
            }

            return true;
        }

        struct SizeEstimate
        {
            size_t vertices;
            size_t indices;
        };

        bool SpendConversionBudget(size_t count, size_t record_size, size_t& remaining)
        {
            if (count > remaining / record_size)
            {
                return false;
            }

            remaining -= count * record_size;

            return true;
        }

        ncl_utils::ResultT<SizeEstimate> EstimateSize(const Buffer& buffer, const goldsrc::studiohdr_t* header)
        {
            size_t remaining = buffer.size;
            size_t triangles = 0;
            size_t ceiling = buffer.size / sizeof(StudioTriVertex_t);

            // Charge each visit, including shared tables and shell-normal initialization,
            // before traversing it. Disjoint tables fit within the source byte budget.
            if (!SpendConversionBudget(header->numbodyparts, sizeof(goldsrc::mstudiobodyparts_t), remaining))
            {
                return ncl_utils::ResultError("model exceeds its conversion budget");
            }

            const goldsrc::mstudiobodyparts_t* bodyparts = At<goldsrc::mstudiobodyparts_t>(buffer, header->bodypartindex);

            for (int i = 0; i < header->numbodyparts; i++)
            {
                const goldsrc::mstudiobodyparts_t& bodypart = bodyparts[i];

                if (!TableFits(buffer.size, bodypart.modelindex, bodypart.nummodels, sizeof(goldsrc::mstudiomodel_t)))
                {
                    return ncl_utils::ResultError("submodel array of body part " + std::to_string(i) + " lies outside the file");
                }

                if (!SpendConversionBudget(bodypart.nummodels, sizeof(goldsrc::mstudiomodel_t), remaining))
                {
                    return ncl_utils::ResultError("model exceeds its conversion budget");
                }

                const goldsrc::mstudiomodel_t* submodels = At<goldsrc::mstudiomodel_t>(buffer, bodypart.modelindex);

                for (int j = 0; j < bodypart.nummodels; j++)
                {
                    const goldsrc::mstudiomodel_t& submodel = submodels[j];

                    if (!TableFits(buffer.size, submodel.meshindex, submodel.nummesh, sizeof(goldsrc::mstudiomesh_t)) ||
                        !TableFits(buffer.size, submodel.vertindex, submodel.numverts, sizeof(goldsrc::vec3_t)))
                    {
                        return ncl_utils::ResultError(
                            "submodel " + std::to_string(j) + " of body part " + std::to_string(i) + " lies outside the file"
                        );
                    }

                    if (!SpendConversionBudget(submodel.nummesh, sizeof(goldsrc::mstudiomesh_t), remaining) ||
                        !SpendConversionBudget(submodel.numverts, sizeof(goldsrc::vec3_t), remaining))
                    {
                        return ncl_utils::ResultError("model exceeds its conversion budget");
                    }

                    const goldsrc::mstudiomesh_t* meshes = At<goldsrc::mstudiomesh_t>(buffer, submodel.meshindex);

                    for (int k = 0; k < submodel.nummesh; k++)
                    {
                        if (meshes[k].numtris > 0)
                        {
                            triangles += std::min(static_cast<size_t>(meshes[k].numtris), ceiling - triangles);
                        }
                    }
                }
            }

            return SizeEstimate{triangles, triangles * 3};
        }
    } // namespace

    ncl_utils::ResultT<StudioMesh> BuildStudioMesh(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(goldsrc::studiohdr_t))
        {
            return ncl_utils::ResultError("buffer is smaller than a studio header");
        }

        Buffer buffer{static_cast<const uint8_t*>(data), size};
        const goldsrc::studiohdr_t* header = At<goldsrc::studiohdr_t>(buffer, 0);

        std::string shape_error = HeaderShapeError(header);

        if (!shape_error.empty())
        {
            return ncl_utils::ResultError(std::move(shape_error));
        }

        if (!TableFits(buffer.size, header->bodypartindex, header->numbodyparts, sizeof(goldsrc::mstudiobodyparts_t)))
        {
            return ncl_utils::ResultError("body part array lies outside the file");
        }

        ncl_utils::ResultT<SizeEstimate> estimate = EstimateSize(buffer, header);

        if (estimate.has_error())
        {
            return ncl_utils::ResultError(estimate.error_str());
        }

        StudioMesh out;
        out.vertices.reserve(estimate->vertices);
        out.indices.reserve(estimate->indices);

        const goldsrc::mstudiobodyparts_t* bodyparts = At<goldsrc::mstudiobodyparts_t>(buffer, header->bodypartindex);

        for (int i = 0; i < header->numbodyparts; i++)
        {
            const goldsrc::mstudiobodyparts_t& bodypart = bodyparts[i];

            if (!TableFits(buffer.size, bodypart.modelindex, bodypart.nummodels, sizeof(goldsrc::mstudiomodel_t)))
            {
                return ncl_utils::ResultError("submodel array of body part " + std::to_string(i) + " lies outside the file");
            }

            BodyPartRange bodypart_range;
            bodypart_range.first_submodel = static_cast<uint32_t>(out.submodels.size());
            bodypart_range.submodel_count = static_cast<uint32_t>(bodypart.nummodels);

            const goldsrc::mstudiomodel_t* submodels = At<goldsrc::mstudiomodel_t>(buffer, bodypart.modelindex);

            for (int j = 0; j < bodypart.nummodels; j++)
            {
                const goldsrc::mstudiomodel_t& submodel = submodels[j];

                bool layout_ok = TableFits(buffer.size, submodel.vertindex, submodel.numverts, sizeof(goldsrc::vec3_t)) &&
                                 TableFits(buffer.size, submodel.normindex, submodel.numnorms, sizeof(goldsrc::vec3_t)) &&
                                 TableFits(buffer.size, submodel.vertinfoindex, submodel.numverts, sizeof(uint8_t)) &&
                                 TableFits(buffer.size, submodel.norminfoindex, submodel.numnorms, sizeof(uint8_t)) &&
                                 TableFits(buffer.size, submodel.meshindex, submodel.nummesh, sizeof(goldsrc::mstudiomesh_t));

                if (!layout_ok)
                {
                    return ncl_utils::ResultError(
                        "submodel " + std::to_string(j) + " of body part " + std::to_string(i) + " lies outside the file"
                    );
                }

                SubModelRange submodel_range;
                submodel_range.first_mesh = static_cast<uint32_t>(out.meshes.size());
                submodel_range.mesh_count = static_cast<uint32_t>(submodel.nummesh);
                submodel_range.file_offset = static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&submodel) - buffer.data);

                const goldsrc::mstudiomesh_t* meshes = At<goldsrc::mstudiomesh_t>(buffer, submodel.meshindex);

                // Per submodel, because the vertex and normal arrays it indexes are its own.
                std::vector<int> shell_normals(static_cast<size_t>(submodel.numverts), -1);

                for (int k = 0; k < submodel.nummesh; k++)
                {
                    const goldsrc::mstudiomesh_t& mesh = meshes[k];

                    MeshRange mesh_range;
                    mesh_range.first_index = static_cast<uint32_t>(out.indices.size());
                    mesh_range.skinref = mesh.skinref;

                    std::string error;

                    if (!BuildMesh(buffer, submodel, mesh, header->numbones, shell_normals, out, error))
                    {
                        return ncl_utils::ResultError(MeshContext(i, j, k) + ": " + error);
                    }

                    mesh_range.index_count = static_cast<uint32_t>(out.indices.size()) - mesh_range.first_index;
                    out.meshes.push_back(mesh_range);
                }

                out.submodels.push_back(submodel_range);
            }

            out.bodyparts.push_back(bodypart_range);
        }

        return std::move(out);
    }
} // namespace ncl_studio

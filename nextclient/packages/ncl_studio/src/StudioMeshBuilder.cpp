#include "ncl_studio/StudioMeshBuilder.h"

#include <algorithm>
#include <string>
#include <vector>

// studio.h uses vec3_t without declaring it.
#include <tier0/basetypes.h>
#include <studio.h>

#include "HeaderShape.h"

namespace ncl_studio
{
    namespace
    {
        struct Buffer
        {
            const uint8_t* data;
            size_t size;
        };

        bool Fits(const Buffer& buffer, int offset, int count, size_t element_size)
        {
            if (offset < 0 || count < 0)
            {
                return false;
            }

            size_t start = static_cast<size_t>(offset);
            if (start > buffer.size)
            {
                return false;
            }

            size_t available = buffer.size - start;

            return element_size == 0 || static_cast<size_t>(count) <= available / element_size;
        }

        template <class T>
        const T* At(const Buffer& buffer, int offset)
        {
            return reinterpret_cast<const T*>(buffer.data + offset);
        }

        ncl_math::Vector3 ReadVec3(const vec3_t& source)
        {
            return ncl_math::Vector3 {source[0], source[1], source[2]};
        }

        std::string MeshContext(int bodypart, int submodel, int mesh)
        {
            return "bodypart " + std::to_string(bodypart) +
                   ", submodel " + std::to_string(submodel) +
                   ", mesh " + std::to_string(mesh);
        }

        // Expands one mesh's command list. A run starts with a signed short count:
        // zero ends the list, a negative count is a triangle fan and a positive one a
        // triangle strip. Vertices are emitted per run entry rather than deduplicated,
        // which matches what the fixed function path feeds the driver today.
        bool BuildMesh(
            const Buffer& buffer,
            const mstudiomodel_t& submodel,
            const mstudiomesh_t& mesh,
            int bone_count,
            std::vector<int>& shell_normals,
            StudioMesh& out,
            std::string& error
        )
        {
            if (!Fits(buffer, mesh.triindex, 1, sizeof(short)))
            {
                error = "triangle command list starts outside the file";
                return false;
            }

            const vec3_t* positions = At<vec3_t>(buffer, submodel.vertindex);
            const vec3_t* normals = At<vec3_t>(buffer, submodel.normindex);
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

                size_t run_bytes = static_cast<size_t>(run_length) * sizeof(mstudiotrivert_t);
                if (run_bytes > buffer.size - cursor)
                {
                    error = "triangle run runs past the end of the file";
                    return false;
                }

                const mstudiotrivert_t* run_verts = reinterpret_cast<const mstudiotrivert_t*>(buffer.data + cursor);
                cursor += run_bytes;

                uint32_t run_start = static_cast<uint32_t>(out.vertices.size());

                for (int i = 0; i < run_length; i++)
                {
                    const mstudiotrivert_t& source = run_verts[i];

                    if (source.vertindex >= submodel.numverts || source.normindex >= submodel.numnorms)
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
                    vertex.texel = ncl_math::Vector2 {static_cast<float>(source.s), static_cast<float>(source.t)};
                    vertex.position_bone = position_bones[source.vertindex];
                    vertex.normal_bone = normal_bones[source.normindex];
                    vertex.shell_normal_bone = normal_bones[shell_normal];

                    out.vertices.push_back(vertex);
                }

                // Every vertex emitted comes from one mstudiotrivert_t in a command list,
                // and in a well formed model each mesh owns its own command list, so the
                // file cannot describe more vertices than it has room for. Nothing stops
                // the body part, submodel and mesh arrays from all pointing at the same
                // region, though, and that turns a small file into an unbounded amount of
                // work; this is the bound the layout implies rather than a chosen ceiling.
                if (out.vertices.size() > buffer.size / sizeof(mstudiotrivert_t))
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

        size_t CountTriangles(const Buffer& buffer, const studiohdr_t* header)
        {
            size_t triangles = 0;

            if (!Fits(buffer, header->bodypartindex, header->numbodyparts, sizeof(mstudiobodyparts_t)))
            {
                return triangles;
            }

            const mstudiobodyparts_t* bodyparts = At<mstudiobodyparts_t>(buffer, header->bodypartindex);

            for (int i = 0; i < header->numbodyparts; i++)
            {
                const mstudiobodyparts_t& bodypart = bodyparts[i];

                // Anything that does not fit ends the count where it stands: this is an
                // estimate, and the build below is what reports a malformed file.
                if (!Fits(buffer, bodypart.modelindex, bodypart.nummodels, sizeof(mstudiomodel_t)))
                {
                    return triangles;
                }

                const mstudiomodel_t* submodels = At<mstudiomodel_t>(buffer, bodypart.modelindex);

                for (int j = 0; j < bodypart.nummodels; j++)
                {
                    const mstudiomodel_t& submodel = submodels[j];

                    if (!Fits(buffer, submodel.meshindex, submodel.nummesh, sizeof(mstudiomesh_t)))
                    {
                        return triangles;
                    }

                    const mstudiomesh_t* meshes = At<mstudiomesh_t>(buffer, submodel.meshindex);

                    for (int k = 0; k < submodel.nummesh; k++)
                    {
                        if (meshes[k].numtris > 0)
                        {
                            triangles += static_cast<size_t>(meshes[k].numtris);
                        }
                    }
                }
            }

            return triangles;
        }

        // What the command lists imply, so a model of any size is not built by walking up
        // through a doubling sequence: three indices per triangle, and one vertex per
        // command list entry. Vertices are a lower bound rather than the count, because a
        // run of L entries carries L vertices and L - 2 triangles, and counting the two per
        // run would mean walking every command list twice.
        SizeEstimate EstimateSize(const Buffer& buffer, const studiohdr_t* header)
        {
            // The ceiling the build enforces on itself: the body part, submodel and mesh
            // arrays may point at one another, so counts read out of them are not to be
            // trusted past what the file has room to describe.
            size_t ceiling = buffer.size / sizeof(mstudiotrivert_t);
            size_t triangles = std::min(CountTriangles(buffer, header), ceiling);

            return SizeEstimate {triangles, triangles * 3};
        }
    }

    ncl_utils::ResultT<StudioMesh> BuildStudioMesh(const void* data, size_t size)
    {
        if (data == nullptr || size < sizeof(studiohdr_t))
        {
            return ncl_utils::ResultError("buffer is smaller than a studio header");
        }

        Buffer buffer {static_cast<const uint8_t*>(data), size};
        const studiohdr_t* header = At<studiohdr_t>(buffer, 0);

        std::string shape_error = HeaderShapeError(header);
        if (!shape_error.empty())
        {
            return ncl_utils::ResultError(std::move(shape_error));
        }

        if (!Fits(buffer, header->bodypartindex, header->numbodyparts, sizeof(mstudiobodyparts_t)))
        {
            return ncl_utils::ResultError("body part array lies outside the file");
        }

        StudioMesh out;
        SizeEstimate estimate = EstimateSize(buffer, header);
        out.vertices.reserve(estimate.vertices);
        out.indices.reserve(estimate.indices);

        const mstudiobodyparts_t* bodyparts = At<mstudiobodyparts_t>(buffer, header->bodypartindex);

        for (int i = 0; i < header->numbodyparts; i++)
        {
            const mstudiobodyparts_t& bodypart = bodyparts[i];

            if (!Fits(buffer, bodypart.modelindex, bodypart.nummodels, sizeof(mstudiomodel_t)))
            {
                return ncl_utils::ResultError("submodel array of body part " + std::to_string(i) + " lies outside the file");
            }

            BodyPartRange bodypart_range;
            bodypart_range.first_submodel = static_cast<uint32_t>(out.submodels.size());
            bodypart_range.submodel_count = static_cast<uint32_t>(bodypart.nummodels);

            const mstudiomodel_t* submodels = At<mstudiomodel_t>(buffer, bodypart.modelindex);

            for (int j = 0; j < bodypart.nummodels; j++)
            {
                const mstudiomodel_t& submodel = submodels[j];

                bool layout_ok =
                    Fits(buffer, submodel.vertindex, submodel.numverts, sizeof(vec3_t)) &&
                    Fits(buffer, submodel.normindex, submodel.numnorms, sizeof(vec3_t)) &&
                    Fits(buffer, submodel.vertinfoindex, submodel.numverts, sizeof(uint8_t)) &&
                    Fits(buffer, submodel.norminfoindex, submodel.numnorms, sizeof(uint8_t)) &&
                    Fits(buffer, submodel.meshindex, submodel.nummesh, sizeof(mstudiomesh_t));

                if (!layout_ok)
                {
                    return ncl_utils::ResultError(
                        "submodel " + std::to_string(j) + " of body part " + std::to_string(i) + " lies outside the file"
                    );
                }

                SubModelRange submodel_range;
                submodel_range.first_mesh = static_cast<uint32_t>(out.meshes.size());
                submodel_range.mesh_count = static_cast<uint32_t>(submodel.nummesh);
                submodel_range.file_offset =
                    static_cast<uint32_t>(reinterpret_cast<const uint8_t*>(&submodel) - buffer.data);

                const mstudiomesh_t* meshes = At<mstudiomesh_t>(buffer, submodel.meshindex);

                // Per submodel, because the vertex and normal arrays it indexes are its own.
                std::vector<int> shell_normals(static_cast<size_t>(submodel.numverts), -1);

                for (int k = 0; k < submodel.nummesh; k++)
                {
                    const mstudiomesh_t& mesh = meshes[k];

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
}

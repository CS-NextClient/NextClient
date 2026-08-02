#include <gtest/gtest.h>

#include <cstring>
#include <vector>

// studio.h uses vec3_t without declaring it.
#include <tier0/basetypes.h>
#include <studio.h>

#include <ncl_studio/StudioMeshBuilder.h>

using namespace ncl_studio;

namespace
{
    // vec3_t is a raw float[3] and cannot be a container element, so the test carries
    // its own layout-compatible POD.
    struct Vec3Data
    {
        float x, y, z;
    };

    static_assert(sizeof(Vec3Data) == sizeof(vec3_t));

    // Builds the smallest .mdl the converter accepts: one body part, one submodel and
    // one mesh whose command list is supplied by the caller. Offsets are filled in as
    // the blocks are appended, so the layout stays valid whatever the caller passes.
    class ModelBuilder
    {
    public:
        ModelBuilder()
        {
            buffer_.resize(sizeof(studiohdr_t));
            std::memset(buffer_.data(), 0, buffer_.size());
            header().version = kStudioVersion;

            // The fixtures write bone indices spread over the format's range, so the model
            // declares the whole range; a test that wants a tighter ceiling sets it itself.
            header().numbones = MAXSTUDIOBONES;
        }

        studiohdr_t& header()
        {
            return *reinterpret_cast<studiohdr_t*>(buffer_.data());
        }

        void SetGeometry(const std::vector<Vec3Data>& positions, const std::vector<Vec3Data>& normals)
        {
            positions_ = positions;
            normals_ = normals;
        }

        void SetBones(const std::vector<uint8_t>& position_bones, const std::vector<uint8_t>& normal_bones)
        {
            position_bones_ = position_bones;
            normal_bones_ = normal_bones;
        }

        void SetCommands(const std::vector<short>& commands)
        {
            commands_ = commands;
        }

        std::vector<uint8_t> Build()
        {
            int bodypart_offset = Append(nullptr, sizeof(mstudiobodyparts_t));
            int submodel_offset = Append(nullptr, sizeof(mstudiomodel_t));
            int mesh_offset = Append(nullptr, sizeof(mstudiomesh_t));

            int positions_offset = Append(positions_.data(), positions_.size() * sizeof(Vec3Data));
            int normals_offset = Append(normals_.data(), normals_.size() * sizeof(Vec3Data));
            int position_bones_offset = Append(position_bones_.data(), position_bones_.size());
            int normal_bones_offset = Append(normal_bones_.data(), normal_bones_.size());
            int commands_offset = Append(commands_.data(), commands_.size() * sizeof(short));

            header().numbodyparts = 1;
            header().bodypartindex = bodypart_offset;

            mstudiobodyparts_t& bodypart = At<mstudiobodyparts_t>(bodypart_offset);
            bodypart.nummodels = 1;
            bodypart.modelindex = submodel_offset;

            mstudiomodel_t& submodel = At<mstudiomodel_t>(submodel_offset);
            submodel.nummesh = 1;
            submodel.meshindex = mesh_offset;
            submodel.numverts = static_cast<int>(positions_.size());
            submodel.vertindex = positions_offset;
            submodel.numnorms = static_cast<int>(normals_.size());
            submodel.normindex = normals_offset;
            submodel.vertinfoindex = position_bones_offset;
            submodel.norminfoindex = normal_bones_offset;

            mstudiomesh_t& mesh = At<mstudiomesh_t>(mesh_offset);
            mesh.triindex = commands_offset;
            mesh.skinref = 3;

            return buffer_;
        }

    private:
        template <class T>
        T& At(int offset)
        {
            return *reinterpret_cast<T*>(buffer_.data() + offset);
        }

        int Append(const void* data, size_t size)
        {
            while (buffer_.size() % 4 != 0)
            {
                buffer_.push_back(0);
            }

            int offset = static_cast<int>(buffer_.size());
            buffer_.resize(buffer_.size() + size, 0);

            if (data != nullptr && size != 0)
            {
                std::memcpy(buffer_.data() + offset, data, size);
            }

            return offset;
        }

        std::vector<uint8_t> buffer_;
        std::vector<Vec3Data> positions_;
        std::vector<Vec3Data> normals_;
        std::vector<uint8_t> position_bones_;
        std::vector<uint8_t> normal_bones_;
        std::vector<short> commands_;
    };

    // One command word per trivert field, so a run is written as
    // {count, v0.vertindex, v0.normindex, v0.s, v0.t, v1...}.
    void AppendRun(std::vector<short>& commands, short count, int vertex_count)
    {
        commands.push_back(count);

        for (int i = 0; i < vertex_count; i++)
        {
            commands.push_back(static_cast<short>(i));
            commands.push_back(static_cast<short>(i));
            commands.push_back(static_cast<short>(10 + i));
            commands.push_back(static_cast<short>(20 + i));
        }
    }

    ModelBuilder MakeBuilder(int vertex_count)
    {
        ModelBuilder builder;

        std::vector<Vec3Data> positions(vertex_count);
        std::vector<Vec3Data> normals(vertex_count);
        std::vector<uint8_t> position_bones(vertex_count);
        std::vector<uint8_t> normal_bones(vertex_count);

        for (int i = 0; i < vertex_count; i++)
        {
            positions[i] = Vec3Data {static_cast<float>(i), 0.0f, 0.0f};
            normals[i] = Vec3Data {0.0f, 1.0f, 0.0f};

            position_bones[i] = static_cast<uint8_t>(i);
            normal_bones[i] = static_cast<uint8_t>(100 + i);
        }

        builder.SetGeometry(positions, normals);
        builder.SetBones(position_bones, normal_bones);

        return builder;
    }
}

TEST(StudioMeshBuilder, ExpandsStripIntoConsistentlyWoundTriangles)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    EXPECT_EQ(result->vertices.size(), 4u);

    std::vector<uint32_t> expected = {0, 1, 2, 2, 1, 3};
    EXPECT_EQ(result->indices, expected);
}

TEST(StudioMeshBuilder, GivesEveryCopyOfAVertexTheNormalTheFileNamesFirst)
{
    ModelBuilder builder;

    builder.SetGeometry(
        {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}},
        {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, -1.0f, 0.0f}}
    );
    builder.SetBones({0, 1, 2}, {10, 11, 12, 13});

    // A strip whose last entry is the first vertex again, drawn with another normal, which
    // is what a hard edge looks like in the file.
    std::vector<short> commands = {4};
    const short run[4][4] = {{0, 0, 0, 0}, {1, 1, 0, 0}, {2, 2, 0, 0}, {0, 3, 0, 0}};
    for (const auto& entry : run)
    {
        commands.insert(commands.end(), entry, entry + 4);
    }
    commands.push_back(0);

    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->vertices.size(), 4u);

    const Vertex& first = result->vertices[0];
    const Vertex& second = result->vertices[3];

    // Lit by the normal each copy was drawn with.
    EXPECT_EQ(first.normal.x, 1.0f);
    EXPECT_EQ(second.normal.y, -1.0f);
    EXPECT_EQ(first.normal_bone, 10);
    EXPECT_EQ(second.normal_bone, 13);

    // Extruded along the one the file names first for that vertex, so the glow shell stays
    // in one piece across the edge.
    EXPECT_EQ(first.shell_normal.x, 1.0f);
    EXPECT_EQ(second.shell_normal.x, 1.0f);
    EXPECT_EQ(second.shell_normal.y, 0.0f);
    EXPECT_EQ(second.shell_normal.z, 0.0f);
    EXPECT_EQ(first.shell_normal_bone, 10);
    EXPECT_EQ(second.shell_normal_bone, 10);
}

TEST(StudioMeshBuilder, ExpandsFanAroundTheFirstVertex)
{
    std::vector<short> commands;
    AppendRun(commands, -4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    std::vector<uint32_t> expected = {0, 1, 2, 0, 2, 3};
    EXPECT_EQ(result->indices, expected);
}

TEST(StudioMeshBuilder, AppendsRunsIntoSharedArrays)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    AppendRun(commands, -4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    EXPECT_EQ(result->vertices.size(), 8u);
    EXPECT_EQ(result->indices.size(), 12u);

    // The fan run starts at vertex 4, so its indices are offset by the strip run.
    EXPECT_EQ(result->indices[6], 4u);
    EXPECT_EQ(result->indices[7], 5u);
    EXPECT_EQ(result->indices[8], 6u);
}

TEST(StudioMeshBuilder, CarriesTexelsAndBonesThrough)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    const Vertex& second = result->vertices[1];

    EXPECT_FLOAT_EQ(second.position.x, 1.0f);
    EXPECT_FLOAT_EQ(second.normal.y, 1.0f);
    EXPECT_FLOAT_EQ(second.texel.x, 11.0f);
    EXPECT_FLOAT_EQ(second.texel.y, 21.0f);
    EXPECT_EQ(second.position_bone, 1);
    EXPECT_EQ(second.normal_bone, 101);
}

TEST(StudioMeshBuilder, ReportsRangesPerLevel)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    ASSERT_EQ(result->bodyparts.size(), 1u);
    ASSERT_EQ(result->submodels.size(), 1u);
    ASSERT_EQ(result->meshes.size(), 1u);

    EXPECT_EQ(result->bodyparts[0].submodel_count, 1u);
    EXPECT_EQ(result->submodels[0].mesh_count, 1u);
    EXPECT_EQ(result->meshes[0].first_index, 0u);
    EXPECT_EQ(result->meshes[0].index_count, 6u);
    EXPECT_EQ(result->meshes[0].skinref, 3);
}

TEST(StudioMeshBuilder, RecordsTheSubmodelFileOffset)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->submodels.size(), 1u);

    // The offset has to address the mstudiomodel_t the engine hands back at draw time,
    // so it is checked against the body part's own modelindex rather than a constant.
    const studiohdr_t* header = reinterpret_cast<const studiohdr_t*>(model.data());
    const mstudiobodyparts_t* bodypart =
        reinterpret_cast<const mstudiobodyparts_t*>(model.data() + header->bodypartindex);

    EXPECT_EQ(result->submodels[0].file_offset, static_cast<uint32_t>(bodypart->modelindex));
}

TEST(StudioMeshBuilder, RejectsBufferSmallerThanHeader)
{
    std::vector<uint8_t> tiny(8, 0);

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(tiny.data(), tiny.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("studio header"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsUnsupportedVersion)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    builder.header().version = kStudioVersion + 1;
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("unsupported studio version"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsVertexIndexOutsideTheSubmodel)
{
    std::vector<short> commands;
    AppendRun(commands, 4, 4);
    commands.push_back(0);

    // Point the last triangle vertex at an index the submodel does not declare.
    commands[1 + 3 * 4] = 99;

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("outside the submodel"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsBoneIndexOutsideTheModel)
{
    std::vector<short> commands;
    AppendRun(commands, 3, 3);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(3);
    builder.SetCommands(commands);

    // A renderer turns this byte into an index into an array of exactly numbones entries.
    builder.header().numbones = 4;

    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("bone outside the model"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsAModelThatExpandsBeyondItsOwnSize)
{
    std::vector<short> commands;
    AppendRun(commands, 3, 3);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(3);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    // Every array is individually bounded by the file, so the amplification is not one
    // array being too long: it is several levels each pointing at the same region, which
    // multiplies. Here one submodel claims many meshes that all alias the same command
    // list - each costs twenty bytes of file and a full expansion of the list.
    studiohdr_t* header = reinterpret_cast<studiohdr_t*>(model.data());
    mstudiobodyparts_t* bodypart = reinterpret_cast<mstudiobodyparts_t*>(model.data() + header->bodypartindex);
    mstudiomodel_t* submodel = reinterpret_cast<mstudiomodel_t*>(model.data() + bodypart->modelindex);

    mstudiomesh_t mesh = *reinterpret_cast<const mstudiomesh_t*>(model.data() + submodel->meshindex);

    constexpr int kAliasedMeshes = 400;
    int mesh_array = static_cast<int>(model.size());

    for (int i = 0; i < kAliasedMeshes; i++)
    {
        const uint8_t* bytes = reinterpret_cast<const uint8_t*>(&mesh);
        model.insert(model.end(), bytes, bytes + sizeof(mstudiomesh_t));
    }

    // The vector moved, so the header pointers are taken again.
    header = reinterpret_cast<studiohdr_t*>(model.data());
    bodypart = reinterpret_cast<mstudiobodyparts_t*>(model.data() + header->bodypartindex);
    submodel = reinterpret_cast<mstudiomodel_t*>(model.data() + bodypart->modelindex);

    submodel->nummesh = kAliasedMeshes;
    submodel->meshindex = mesh_array;

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("more vertices than its own size"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsBoneCountOutsideTheFormat)
{
    ModelBuilder builder = MakeBuilder(3);
    builder.header().numbones = MAXSTUDIOBONES + 1;

    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("the format allows"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsRunReachingPastTheEndOfTheFile)
{
    std::vector<short> commands;
    // Declares eight vertices but supplies the data for two.
    AppendRun(commands, 8, 2);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("past the end"), std::string::npos) << result.error_str();
}

TEST(StudioMeshBuilder, RejectsRunShorterThanATriangle)
{
    std::vector<short> commands;
    AppendRun(commands, 2, 2);
    commands.push_back(0);

    ModelBuilder builder = MakeBuilder(4);
    builder.SetCommands(commands);
    std::vector<uint8_t> model = builder.Build();

    ncl_utils::ResultT<StudioMesh> result = BuildStudioMesh(model.data(), model.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("shorter than a single triangle"), std::string::npos) << result.error_str();
}

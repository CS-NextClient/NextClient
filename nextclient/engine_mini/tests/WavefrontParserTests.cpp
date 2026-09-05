#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include "graphics/preview/WavefrontParser.h"

namespace
{
    constexpr const char* kQuadAndTriangle =
        "# a quad on the floor and a lit triangle standing on it\n"
        "v 0 0 0\n"
        "v 1 0 0\n"
        "v 1 1 0\n"
        "v 0 1 0\n"
        "v 0 0 1\n"
        "vt 0 0\n"
        "vt 1 0\n"
        "vt 1 1\n"
        "vt 0 1\n"
        "vn 0 -1 0\n"
        "usemtl Floor\n"
        "f 1/1 2/2 3/3 4/4\n"
        "usemtl Wall\n"
        "f 1//1 2//1 5//1\n";

    std::string ReadFile(const char* path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();

        return text.str();
    }
}

TEST(WavefrontParse, FansAQuadIntoTwoTrianglesUnderItsMaterial)
{
    WavefrontMesh mesh = Wavefront_Parse(kQuadAndTriangle);

    ASSERT_EQ(mesh.batches.size(), 2u);
    EXPECT_EQ(mesh.skipped_faces, 0);

    const WavefrontBatch& floor = mesh.batches[0];
    EXPECT_EQ(floor.material, "Floor");
    ASSERT_EQ(floor.vertices.size(), 6u);

    // corners 0 1 2 then 0 2 3
    EXPECT_FLOAT_EQ(floor.vertices[0].position[0], 0.0f);
    EXPECT_FLOAT_EQ(floor.vertices[1].position[0], 1.0f);
    EXPECT_FLOAT_EQ(floor.vertices[2].position[1], 1.0f);
    EXPECT_FLOAT_EQ(floor.vertices[3].position[0], 0.0f);
    EXPECT_FLOAT_EQ(floor.vertices[4].position[1], 1.0f);
    EXPECT_FLOAT_EQ(floor.vertices[5].position[0], 0.0f);
    EXPECT_FLOAT_EQ(floor.vertices[5].position[1], 1.0f);
}

TEST(WavefrontParse, TurnsTheTextureVOverAndKeepsS)
{
    WavefrontMesh mesh = Wavefront_Parse(kQuadAndTriangle);

    const WavefrontVertex& third = mesh.batches[0].vertices[2];
    EXPECT_FLOAT_EQ(third.s, 1.0f);
    EXPECT_FLOAT_EQ(third.t, 0.0f);

    const WavefrontVertex& first = mesh.batches[0].vertices[0];
    EXPECT_FLOAT_EQ(first.t, 1.0f);
}

TEST(WavefrontParse, TakesTheFaceNormalWhereACornerHasNone)
{
    WavefrontMesh mesh = Wavefront_Parse(kQuadAndTriangle);

    // the floor winds counter-clockwise seen from above, so it faces up
    const WavefrontVertex& floor = mesh.batches[0].vertices[0];
    EXPECT_FLOAT_EQ(floor.normal[0], 0.0f);
    EXPECT_FLOAT_EQ(floor.normal[1], 0.0f);
    EXPECT_FLOAT_EQ(floor.normal[2], 1.0f);

    const WavefrontVertex& wall = mesh.batches[1].vertices[0];
    EXPECT_FLOAT_EQ(wall.normal[1], -1.0f);
    EXPECT_EQ(wall.s, 0.0f);
    EXPECT_EQ(wall.t, 0.0f);
}

TEST(WavefrontParse, CountsBackFromTheEndForNegativeIndices)
{
    WavefrontMesh mesh = Wavefront_Parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf -3 -2 -1\n");

    ASSERT_EQ(mesh.batches.size(), 1u);
    EXPECT_EQ(mesh.batches[0].material, "");
    ASSERT_EQ(mesh.batches[0].vertices.size(), 3u);
    EXPECT_FLOAT_EQ(mesh.batches[0].vertices[2].position[1], 1.0f);
}

TEST(WavefrontParse, SkipsFacesItCannotResolve)
{
    WavefrontMesh mesh = Wavefront_Parse("v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 4\nf 1 2\nf 1/9 2/9 3/9\nf 1 2 3\n");

    EXPECT_EQ(mesh.skipped_faces, 3);
    ASSERT_EQ(mesh.batches.size(), 1u);
    EXPECT_EQ(mesh.batches[0].vertices.size(), 3u);
}

TEST(WavefrontParse, GathersLaterFacesUnderAnEarlierMaterial)
{
    WavefrontMesh mesh = Wavefront_Parse(
        "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
        "usemtl A\nf 1 2 3\n"
        "usemtl B\nf 1 2 3\n"
        "usemtl A\nf 1 2 3\n");

    ASSERT_EQ(mesh.batches.size(), 2u);
    EXPECT_EQ(mesh.batches[0].material, "A");
    EXPECT_EQ(mesh.batches[0].vertices.size(), 6u);
    EXPECT_EQ(mesh.batches[1].material, "B");
    EXPECT_EQ(mesh.batches[1].vertices.size(), 3u);
}

TEST(WavefrontParse, ReadsTheShippedCorridor)
{
    std::string text = ReadFile(NEXTCLIENT_ASSETS_DIR "/cstrike/resource/preview/corridor.obj");
    ASSERT_FALSE(text.empty());

    WavefrontMesh mesh = Wavefront_Parse(text);

    EXPECT_EQ(mesh.skipped_faces, 0);
    ASSERT_EQ(mesh.batches.size(), 5u);

    int triangles = 0;
    for (const WavefrontBatch& batch : mesh.batches)
    {
        EXPECT_FALSE(batch.material.empty());
        triangles += static_cast<int>(batch.vertices.size() / 3);
    }

    EXPECT_EQ(triangles, 28);
}

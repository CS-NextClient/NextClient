#pragma once

#include <string>
#include <string_view>
#include <vector>

// The subset of Wavefront OBJ a preview scene is written in: v, vt, vn, f and usemtl, the
// rest skipped. Polygons are fanned into triangles, a corner without a normal takes the
// face's, and the texture coordinate v is turned over into the engine's t, which counts
// rows down from the top.
struct WavefrontVertex
{
    float position[3];
    float normal[3];
    float s;
    float t;
};

// The triangles drawn with one material, three vertices each, in the order the file first
// used the material.
struct WavefrontBatch
{
    std::string material;   // the usemtl in force, empty for faces before any
    std::vector<WavefrontVertex> vertices;
};

struct WavefrontMesh
{
    std::vector<WavefrontBatch> batches;
    int skipped_faces;      // faces with fewer than three corners or an index outside the file
};

WavefrontMesh Wavefront_Parse(std::string_view text);

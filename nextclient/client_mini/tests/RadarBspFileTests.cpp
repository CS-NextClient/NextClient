#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <span>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <locale>
#include <vector>

#include <gtest/gtest.h>
#include <goldsrc/format/bsp.h>

#include "RadarBspFixture.h"
#include "hud/radar/markers/RadarLayerMarker.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

namespace
{
    constexpr size_t kHeaderBytes = 124;

    class BspFile
    {
    public:
        std::vector<char> bytes = std::vector<char>(kHeaderBytes);
        std::filesystem::path directory;
        std::filesystem::path path;
        BspFile()
        {
            directory = std::filesystem::temp_directory_path() /
                        ("nextclient-radar-bsp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

            while (!std::filesystem::create_directory(directory))
            {
                directory += "x";
            }

            path = directory / "fixture.bsp";
            SetInt(0, goldsrc::HLBSP_VERSION);
        }

        ~BspFile()
        {
            std::error_code ignored;
            std::filesystem::remove(path, ignored);
            std::filesystem::remove(directory, ignored);
        }

        void SetInt(size_t offset, int value)
        {
            std::memcpy(bytes.data() + offset, &value, sizeof(value));
        }

        template <class T, size_t N>
        void Add(int lump, const std::array<T, N>& records)
        {
            size_t offset = bytes.size();
            bytes.resize(offset + sizeof(records));
            std::memcpy(bytes.data() + offset, records.data(), sizeof(records));
            SetInt(4 + lump * 8, (int)offset);
            SetInt(8 + lump * 8, (int)sizeof(records));
        }

        void AddFloor()
        {
            Add(1, std::array<float, 5>{0.0f, 0.0f, 1.0f, 0.0f, 0.0f});
            Add(3,
                std::array<std::array<float, 3>, 4>{{{0.0f, 0.0f, 0.0f}, {64.0f, 0.0f, 0.0f}, {64.0f, 64.0f, 0.0f}, {0.0f, 64.0f, 0.0f}}});
            std::array<float, 10> texture{{1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}};
            int absent = -1;
            std::memcpy(&texture[8], &absent, sizeof(absent));
            Add(6, texture);
            Add(7, std::array<int, 5>{0, 0, 4, 0, -1});
            Add(12, std::array<std::array<uint16_t, 2>, 4>{{{0, 1}, {1, 2}, {2, 3}, {3, 0}}});
            Add(13, std::array<int, 4>{0, 1, 2, 3});
            goldsrc::dmodel_t model{};
            model.maxs[0] = model.maxs[1] = 64.0f;

            for (int& head : model.headnode)
            {
                head = goldsrc::CONTENTS_EMPTY;
            }

            model.numfaces = 1;
            Add(14, std::array<goldsrc::dmodel_t, 1>{model});
        }

        bool Load(RadarMapGeometry& geometry, std::string& error)
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output.write(bytes.data(), (std::streamsize)bytes.size());
            output.close();

            return RadarBspFixture_Load(path, geometry, error);
        }
    };

    void WriteMeshSurfaces(std::ostream& output, std::span<const RadarMapSurface> surfaces)
    {
        output << '[';

        for (size_t i = 0; i < surfaces.size(); i++)
        {
            const RadarMapSurface& surface = surfaces[i];

            if (i > 0)
            {
                output << ',';
            }

            output << "{\"surface_id\":" << surface.surface_id << ",\"normal\":[" << surface.normal.x << ',' << surface.normal.y << ','
                   << surface.normal.z << "],\"sky\":" << (surface.sky ? "true" : "false")
                   << ",\"tiled\":" << (surface.tiled ? "true" : "false") << ",\"vertices\":[";

            for (size_t j = 0; j < surface.vertices.size(); j++)
            {
                const RadarMapVertex& vertex = surface.vertices[j];

                if (j > 0)
                {
                    output << ',';
                }

                output << '[' << vertex.pos.x << ',' << vertex.pos.y << ',' << vertex.pos.z << ',' << vertex.uv.x << ',' << vertex.uv.y
                       << ',' << vertex.lightmap_uv.x << ',' << vertex.lightmap_uv.y << ']';
            }

            output << "]}";
        }

        output << ']';
    }

    bool WriteMeshExport(const std::filesystem::path& path, const RadarMapGeometry& geometry, const RadarMapLayers& map)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output(path);
        output.imbue(std::locale::classic());
        output.precision(std::numeric_limits<float>::max_digits10);
        output << "{\"bounds\":[" << geometry.bounds.min.x << ',' << geometry.bounds.min.y << ',' << geometry.bounds.max.x << ','
               << geometry.bounds.max.y << "],\"sources\":";
        WriteMeshSurfaces(output, geometry.surfaces);
        output << ",\"excluded_surfaces\":[";

        const std::vector<int>& excluded = map.get_analysis().reachability.excluded_surfaces;

        for (size_t i = 0; i < excluded.size(); i++)
        {
            if (i > 0)
            {
                output << ',';
            }

            output << excluded[i];
        }

        output << "],\"layers\":[";

        for (size_t i = 0; i < map.get_layers().size(); i++)
        {
            const RadarLayerInfo& layer = map.get_layers()[i];

            if (i > 0)
            {
                output << ',';
            }

            output << "{\"id\":" << layer.id << ",\"height\":" << layer.height << ",\"area\":" << layer.area << ",\"surfaces\":";
            WriteMeshSurfaces(output, map.get_surfaces(layer.id));
            output << '}';
        }

        output << "],\"transitions\":[";
        const std::vector<RadarLayerTransition>& transitions = map.get_analysis().transitions;
        for (size_t i = 0; i < transitions.size(); i++)
        {
            if (i > 0)
            {
                output << ',';
            }
            const RadarLayerTransition& transition = transitions[i];
            output << '[' << transition.lower << ',' << transition.upper << ',' << transition.height << ',' << transition.height_span;
            for (int id : transition.surface_ids)
            {
                output << ',' << id;
            }
            output << ']';
        }
        output << "],\"queries\":[";
        bool first = true;
        for (const RadarMapSurface& surface : geometry.surfaces)
        {
            ncl_math::Vector3 center{};
            for (const RadarMapVertex& vertex : surface.vertices)
            {
                center = center + vertex.pos;
            }
            center = center / (float)surface.vertices.size() + ncl_math::Vector3{0.0f, 0.0f, 0.01f};
            for (RadarLayerQuery query : {RadarLayerQuery::Below, RadarLayerQuery::Nearest})
            {
                RadarLayerMatch match = map.Locate(center, query);
                if (!first)
                {
                    output << ',';
                }
                first = false;
                output << '[' << match.layer << ',' << match.support.x << ',' << match.support.y << ',' << match.support.z << ','
                       << match.contained << ',' << match.surface_id << ']';
            }
        }
        output << "]}";

        return output.good();
    }
} // namespace

TEST(RadarBspFile, RejectsTruncatedHeader)
{
    BspFile file;
    file.bytes.resize(12);
    RadarMapGeometry geometry;
    std::string error;

    EXPECT_FALSE(file.Load(geometry, error));
    EXPECT_FALSE(error.empty());
}

TEST(RadarBspFile, ValidatesOffsetsAndLengthsBeforeAdapter)
{
    for (const std::array<int, 2>& range : {std::array<int, 2>{-1, 0}, {124, -1}, {125, 4}, {124, 2147483647}})
    {
        BspFile file;
        file.SetInt(4 + 1 * 8, range[0]);
        file.SetInt(8 + 1 * 8, range[1]);
        RadarMapGeometry geometry;
        std::string error;

        EXPECT_FALSE(file.Load(geometry, error));
        EXPECT_NE(error.find("lump range"), std::string::npos);
        EXPECT_TRUE(geometry.surfaces.empty());
    }
}

TEST(RadarBspFile, RejectsPartialRecordsAndWrongVersion)
{
    BspFile file;
    file.bytes.resize(125);
    file.SetInt(12, 124);
    file.SetInt(16, 1);
    RadarMapGeometry geometry;
    std::string error;

    EXPECT_FALSE(file.Load(geometry, error));
    EXPECT_NE(error.find("record size"), std::string::npos);

    file.SetInt(0, goldsrc::Q1BSP_VERSION);
    EXPECT_FALSE(file.Load(geometry, error));
    EXPECT_NE(error.find("version"), std::string::npos);
}

TEST(RadarBspFile, AdapterOwnsReconstructedPolygonsAndUvs)
{
    RadarMapGeometry geometry;
    std::string error;
    {
        BspFile file;
        file.AddFloor();
        ASSERT_TRUE(file.Load(geometry, error)) << error;
    }

    ASSERT_EQ(geometry.surfaces.size(), 1u);
    ASSERT_EQ(geometry.surfaces[0].vertices.size(), 4u);
    EXPECT_EQ(geometry.surfaces[0].vertices[2].pos, (ncl_math::Vector3{64.0f, 64.0f, 0.0f}));
    EXPECT_EQ(geometry.surfaces[0].vertices[2].uv, (ncl_math::Vector2{0.5f, 0.5f}));
    EXPECT_EQ(geometry.surfaces[0].vertices[2].lightmap_uv, (ncl_math::Vector2{4.0f, 4.0f}));

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(geometry)), RadarMapStatus::Ready);
    EXPECT_EQ(map.Locate({32.0f, 32.0f, 1.0f}, RadarLayerQuery::Below).layer, 0);
}

TEST(RadarBspFile, AdapterOwnsEntryPoints)
{
    RadarMapGeometry geometry;
    std::string error;
    {
        BspFile file;
        file.AddFloor();
        file.Add(0, std::to_array(R"({ "classname" "info_player_start" "origin" "32 32 36" })"));
        ASSERT_TRUE(file.Load(geometry, error)) << error;
    }

    ASSERT_EQ(geometry.entry_points.size(), 1u);
    EXPECT_EQ(geometry.entry_points[0], (ncl_math::Vector3{32.0f, 32.0f, 36.0f}));
}

TEST(RadarBspFile, RejectsInvalidReferencesAtomically)
{
    BspFile file;
    file.AddFloor();
    int offset = 0;
    std::memcpy(&offset, file.bytes.data() + 4 + 7 * 8, sizeof(offset));
    file.SetInt((size_t)offset + 4, 100);
    RadarMapGeometry geometry;
    geometry.surfaces.emplace_back();
    std::string error;

    EXPECT_FALSE(file.Load(geometry, error));
    EXPECT_NE(error.find("face references"), std::string::npos);
    EXPECT_TRUE(geometry.surfaces.empty());
}

TEST(RadarBspFile, LocalMapAnalysis)
{
    const char* path = std::getenv("NEXTCLIENT_RADAR_BSP");

    if (!path || !path[0])
    {
        GTEST_SKIP() << "Set NEXTCLIENT_RADAR_BSP to a local GoldSrc BSP (version 30).";
    }

    RadarMapGeometry geometry;
    std::string error;

    ASSERT_TRUE(RadarBspFixture_Load(path, geometry, error)) << error;
    ASSERT_TRUE(std::isfinite(geometry.bounds.min.x) && std::isfinite(geometry.bounds.min.y));
    ASSERT_TRUE(std::isfinite(geometry.bounds.max.x) && std::isfinite(geometry.bounds.max.y));
    ASSERT_LT(geometry.bounds.min.x, geometry.bounds.max.x);
    ASSERT_LT(geometry.bounds.min.y, geometry.bounds.max.y);

    std::vector<ncl_math::Vector3> samples;
    size_t input_vertices = 0;

    for (const RadarMapSurface& surface : geometry.surfaces)
    {
        input_vertices += surface.vertices.size();

        if (surface.normal.z < 0.7f || surface.sky)
        {
            continue;
        }

        ncl_math::Vector3 center{};

        for (const RadarMapVertex& vertex : surface.vertices)
        {
            center = center + vertex.pos;
        }

        samples.push_back(center / (float)surface.vertices.size() + ncl_math::Vector3{0.0f, 0.0f, 0.01f});
    }

    RadarLayerOverrides overrides;
    const char* correction_path = std::getenv("NEXTCLIENT_RADAR_CORRECTIONS");

    if (correction_path && correction_path[0])
    {
        std::ifstream input(correction_path);
        ASSERT_TRUE(input.is_open()) << correction_path;

        std::vector<std::string> tokens;
        std::string token;

        while (input >> std::quoted(token))
        {
            tokens.push_back(token);
        }

        ASSERT_TRUE(input.eof());

        ASSERT_TRUE(MapLayerOverrides_Parse(tokens, overrides, error)) << error;
    }

    size_t input_surfaces = geometry.surfaces.size();
    RadarMapLayers map;
    const auto start = std::chrono::steady_clock::now();

    if (correction_path && correction_path[0])
    {
        ASSERT_EQ(map.Build(geometry, overrides, error), RadarMapStatus::Ready);
        ASSERT_TRUE(error.empty()) << error;
    }
    else
    {
        ASSERT_EQ(map.Build(geometry), RadarMapStatus::Ready);
    }

    float analysis_ms = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - start).count();
    ASSERT_GT(map.get_layers().size(), 0u);

    const char* expected_layers = std::getenv("NEXTCLIENT_RADAR_EXPECTED_LAYERS");

    if (expected_layers && expected_layers[0])
    {
        EXPECT_EQ(map.get_layers().size(), std::stoul(expected_layers));
    }

    const RadarMapReachability& reachability = map.get_analysis().reachability;
    std::cout << "reachability=" << (reachability.available ? "ready" : "unavailable") << " collision_regions=" << reachability.regions
              << " connected_regions=" << reachability.connected_regions << " valid_entry_points=" << reachability.valid_entry_points
              << " excluded_surfaces=" << reachability.excluded_surfaces.size() << " reason=" << reachability.reason << std::endl;

    std::cout << "analysis_ms=" << analysis_ms << " layers=" << map.get_layers().size() << std::endl;
    std::cout << "overlap_area_threshold=" << map.get_analysis().overlap_area_threshold
              << " largest_overlap_area=" << map.get_analysis().largest_overlap_area
              << " significant_overlap_regions=" << map.get_analysis().significant_overlap_regions << std::endl;
    size_t mesh_surfaces = 0;
    size_t mesh_vertices = 0;
    float area = 0.0f;

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        ASSERT_TRUE(std::isfinite(layer.height) && std::isfinite(layer.area));
        EXPECT_GT(layer.area, 0.0f);
        EXPECT_FALSE(map.get_surfaces(layer.id).empty());

        area += layer.area;
        mesh_surfaces += map.get_surfaces(layer.id).size();
        size_t sky_surfaces = 0;
        size_t tiled_surfaces = 0;
        size_t ceiling_surfaces = 0;

        for (const RadarMapSurface& surface : map.get_surfaces(layer.id))
        {
            EXPECT_FALSE(
                std::binary_search(reachability.excluded_surfaces.begin(), reachability.excluded_surfaces.end(), surface.surface_id)
            );
            mesh_vertices += surface.vertices.size();
            sky_surfaces += surface.sky;
            tiled_surfaces += surface.tiled;
            ceiling_surfaces += surface.normal.z < 0.0f;
        }

        EXPECT_EQ(sky_surfaces, 0u);
        EXPECT_EQ(ceiling_surfaces, 0u);

        const RadarLayerGroupAnalysis& group = map.get_analysis().layers[layer.id];
        std::cout << "group_bounds=" << group.bounds.min.x << ',' << group.bounds.min.y << ':' << group.bounds.max.x << ','
                  << group.bounds.max.y << " heights=" << group.min_height << ':' << group.max_height << " reasons=";

        for (const std::string& reason : group.reasons)
        {
            std::cout << reason << ';';
        }

        std::cout << ' ';
        std::cout << "layer=" << layer.id << " height=" << layer.height << " area=" << layer.area
                  << " mesh_surfaces=" << map.get_surfaces(layer.id).size() << " sky_surfaces=" << sky_surfaces
                  << " tiled_surfaces=" << tiled_surfaces << " ceiling_surfaces=" << ceiling_surfaces << '\n';
    }

    std::vector<RadarLayerMatch> matches;
    bool stacked = false;

    for (ncl_math::Vector3 sample : samples)
    {
        RadarLayerMatch match = map.Locate(sample, RadarLayerQuery::Below);
        matches.push_back(match);

        if (match.layer >= 0)
        {
            RadarLayerMatch lower = map.Locate(match.support - ncl_math::Vector3{0.0f, 0.0f, 1.0f}, RadarLayerQuery::Below);

            if (lower.layer >= 0 && lower.layer != match.layer)
            {
                stacked = true;
            }
        }
    }

    EXPECT_EQ(stacked, map.get_layers().size() > 1u) << "Separate layers require overlapping supports.";

    const char* mesh_export = std::getenv("NEXTCLIENT_RADAR_MESH_EXPORT");

    if (mesh_export && mesh_export[0])
    {
        ASSERT_TRUE(WriteMeshExport(mesh_export, geometry, map));

        std::cout << "mesh_export=" << mesh_export << std::endl;
    }

    RadarMapLayers repeated;
    ASSERT_EQ(repeated.Build(std::move(geometry)), RadarMapStatus::Ready);

    if (correction_path && correction_path[0])
    {
        ASSERT_TRUE(repeated.ApplyOverrides(overrides, error)) << error;
    }

    ASSERT_EQ(repeated.get_layers().size(), map.get_layers().size());
    EXPECT_EQ(repeated.get_analysis().reachability.excluded_surfaces, reachability.excluded_surfaces);

    for (const RadarLayerInfo& layer : map.get_layers())
    {
        EXPECT_EQ(repeated.get_layers()[layer.id].height, layer.height);
        EXPECT_EQ(repeated.get_layers()[layer.id].area, layer.area);

        std::span<const RadarMapSurface> first = map.get_surfaces(layer.id);
        std::span<const RadarMapSurface> second = repeated.get_surfaces(layer.id);
        ASSERT_EQ(first.size(), second.size());

        bool equal = std::equal(first.begin(), first.end(), second.begin(), [](const RadarMapSurface& a, const RadarMapSurface& b) {
            return a.surface_id == b.surface_id && a.normal == b.normal && a.vertices.size() == b.vertices.size() &&
                   std::equal(
                       a.vertices.begin(), a.vertices.end(), b.vertices.begin(), [](const RadarMapVertex& x, const RadarMapVertex& y) {
                           return x.pos == y.pos && x.uv == y.uv && x.lightmap_uv == y.lightmap_uv;
                       }
                   );
        });

        EXPECT_TRUE(equal) << "Layer mesh must be deterministic: " << layer.id;
    }

    for (size_t i = 0; i < samples.size(); i++)
    {
        RadarLayerMatch match = repeated.Locate(samples[i], RadarLayerQuery::Below);
        EXPECT_EQ(match.layer, matches[i].layer);
        EXPECT_EQ(match.support, matches[i].support);
    }

    ncl_math::Vector2 center = (map.get_bounds().min + map.get_bounds().max) * 0.5f;
    std::array<ncl_math::Vector3, 3> positions{
        {samples.front(),
         {center.x, center.y, map.get_layers().back().height + 100.0f},
         {map.get_bounds().max.x + 1024.0f, map.get_bounds().max.y + 1024.0f, map.get_layers().back().height + 512.0f}}
    };
    int checksum = 0;
    constexpr int iterations = 100;

    for (size_t i = 0; i < positions.size(); i++)
    {
        for (RadarLayerQuery query : {RadarLayerQuery::Below, RadarLayerQuery::Nearest})
        {
            const auto begin = std::chrono::steady_clock::now();

            for (int iteration = 0; iteration < iterations; iteration++)
            {
                checksum += map.Locate(positions[i], query).layer;
            }

            float us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - begin).count() / iterations;
            std::cout << "lookup=" << (query == RadarLayerQuery::Below ? "below" : "nearest") << " sample=" << i << " mean_us=" << us
                      << '\n';
        }
    }

    ncl_math::Aabb3 volume{positions[0] - ncl_math::Vector3{64.0f, 64.0f, 16.0f}, positions[0] + ncl_math::Vector3{64.0f, 64.0f, 128.0f}};
    const auto volume_start = std::chrono::steady_clock::now();

    for (int iteration = 0; iteration < iterations; iteration++)
    {
        checksum += (int)map.Intersect(volume).size();
    }

    float intersect_us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - volume_start).count() / iterations;
    RadarLayerView view = RadarLayerContext_ComputeView(map, 0, positions[0], true);
    const auto style_start = std::chrono::steady_clock::now();

    for (int iteration = 0; iteration < iterations; iteration++)
    {
        checksum += RadarLayerMarker_ComputeVolumeStyle(view, {&volume, 1}).vertical;
    }

    float volume_style_us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - style_start).count() / iterations;
    RadarSiteLayerCache cache;
    cache.Prepare(map, {&volume, 1}, view.active);
    const auto cached_start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < iterations; iteration++)
    {
        cache.Prepare(map, {&volume, 1}, view.active);
        checksum += cache.ComputeStyle(view).vertical;
    }
    float cached_style_us = std::chrono::duration<float, std::micro>(std::chrono::steady_clock::now() - cached_start).count() / iterations;
    std::cout << "cached_volume_style_mean_us=" << cached_style_us << '\n';
    size_t storage_lower_bound =
        (input_surfaces + mesh_surfaces) * sizeof(RadarMapSurface) + (input_vertices + mesh_vertices) * sizeof(RadarMapVertex);
    std::cout << "input_surfaces=" << input_surfaces << " input_vertices=" << input_vertices << " layers=" << map.get_layers().size()
              << " area=" << area << " mesh_surfaces=" << mesh_surfaces << " mesh_vertices=" << mesh_vertices
              << " analysis_ms=" << analysis_ms << " input_mesh_bytes_lower_bound=" << storage_lower_bound
              << " intersect_mean_us=" << intersect_us << " volume_style_mean_us=" << volume_style_us << " checksum=" << checksum << '\n';
}

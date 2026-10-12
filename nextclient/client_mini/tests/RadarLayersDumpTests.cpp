#include <string>

#include <gtest/gtest.h>

#include "hud/radar/map/layers/MapLayerCache.h"
#include "hud/radar/map/MapBspGeometry.h"
#include "hud/radar/map/layers/RadarLayerContext.h"
#include "hud/radar/integration/RadarLayersDump.h"
#include "RadarMapLayerFixtures.h"
#include "hud/radar/map/layers/RadarMapLayers.h"

TEST(RadarLayersDump, ReadDoesNotRebuildOrRender)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerSelection selection;
    selection.Update(map, {{64.0f, 64.0f, 0.0f}, {}, 1.0f, 7, true, true});
    MapLayerCache cache;
    MapLayerRequest request = cache.Request(0, 9);
    cache.Commit(request.slot, 0);
    const RadarMapSurface* surfaces = map.get_surfaces(0).data();
    std::string dump =
        RadarLayersDump_Format(&map, &selection, &cache, "scp_foundation_bw40", "overviews/scp_foundation_bw40_layers.txt", 12.5f);

    EXPECT_NE(dump.find("map=scp_foundation_bw40"), std::string::npos);
    EXPECT_NE(dump.find("active_layer=0"), std::string::npos);
    EXPECT_NE(dump.find("source=7"), std::string::npos);
    EXPECT_NE(dump.find("analysis_ms=12.500"), std::string::npos);
    EXPECT_NE(dump.find("cache="), std::string::npos);
    EXPECT_NE(dump.find("reachability=unavailable"), std::string::npos);
    EXPECT_NE(dump.find("reachability_reason=\"world_collision_hull_unavailable\""), std::string::npos);
    EXPECT_EQ(map.get_surfaces(0).data(), surfaces);
    EXPECT_EQ(selection.get_active(), 0);
    EXPECT_EQ(cache.get_entries()[request.slot].last_frame, 9u);
    EXPECT_TRUE(cache.get_entries()[request.slot].valid);
    EXPECT_FALSE(cache.Request(1, 9).render);
}

TEST(RadarLayersDump, ReportsContactFilterWithoutEntryPoints)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddSupportHull(0);

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);
    std::string dump = RadarLayersDump_Format(&map, nullptr, nullptr, "test", {}, 0.0f);

    EXPECT_NE(dump.find("reachability=ready"), std::string::npos);
    EXPECT_NE(dump.find("valid_entry_points=0"), std::string::npos);
    EXPECT_NE(dump.find("excluded_surfaces=0"), std::string::npos);
}

TEST(RadarLayersDump, ReportsManualConflictsAndSelectionReason)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});

    RadarMapLayers map;
    fixture.AddFloor(48.0f, {{0.0f, 0.0f}, {256.0f, 256.0f}});
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerOverrides overrides{{{"low", -1.0f, 100.0f}, {"high", 200.0f, 300.0f}}, {}};
    std::string error;

    ASSERT_TRUE(map.ApplyOverrides(overrides, error));
    ASSERT_FALSE(map.get_analysis().manual_conflicts.empty());

    error = map.get_analysis().manual_conflicts.front();

    RadarLayerSelection selection;
    selection.Update(map, {{64.0f, 64.0f, 0.0f}, {}, 1.0f, 1, true, true});
    selection.Update(map, {{64.0f, 64.0f, 150.0f}, ncl_math::Vector3{64.0f, 64.0f, 256.0f}, 1.1f, 1, true, true});
    MapLayerCache cache;
    std::string dump = RadarLayersDump_Format(&map, &selection, &cache, "test", "overviews/test_layers.txt", 1.0f);

    EXPECT_NE(dump.find(error), std::string::npos);
    EXPECT_NE(dump.find("candidate_layer=1"), std::string::npos);
    EXPECT_NE(dump.find("selection_reason=pending"), std::string::npos);
    EXPECT_NE(dump.find("height=256.000"), std::string::npos);
    EXPECT_NE(dump.find("bounds="), std::string::npos);
    EXPECT_NE(dump.find("transitions=0"), std::string::npos);
}

TEST(RadarLayersDump, UnavailableOwnerIsSafe)
{
    std::string dump = RadarLayersDump_Format(nullptr, nullptr, nullptr, {}, {}, 0.0f);
    EXPECT_NE(dump.find("map_state=unavailable"), std::string::npos);
    EXPECT_NE(dump.find("selection_reason=unavailable"), std::string::npos);
    EXPECT_NE(dump.find("cache=unavailable"), std::string::npos);
    EXPECT_NE(dump.find("image_state=unavailable"), std::string::npos);
    EXPECT_NE(dump.find("failure_reason=unavailable"), std::string::npos);

    RadarMapLayers map;
    dump = RadarLayersDump_Format(&map, nullptr, nullptr, "test", {}, 0.0f);

    EXPECT_NE(dump.find("map_state=waiting"), std::string::npos);
}

TEST(RadarLayersDump, ReportsLatchedImageFailureWithoutRearmingNotification)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {64.0f, 64.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(fixture.geometry), RadarMapStatus::Ready);

    RadarLayerLifecycle lifecycle;
    lifecycle.SetGeometryStatus(RadarMapStatus::Ready);
    ASSERT_TRUE(lifecycle.SetImageStatus(RadarMapStatus::Failed, "texture storage failed (GL error=0x505)"));

    MapLayerCache cache;
    std::string first = RadarLayersDump_Format(&map, nullptr, &cache, "test", {}, 1.0f, &lifecycle);
    EXPECT_NE(first.find("map_state=ready"), std::string::npos);
    EXPECT_NE(first.find("geometry_state=ready image_state=failed fallback=1"), std::string::npos);
    EXPECT_NE(first.find("failure_reason=\"texture storage failed (GL error=0x505)\""), std::string::npos);
    EXPECT_EQ(RadarLayersDump_Format(&map, nullptr, &cache, "test", {}, 1.0f, &lifecycle), first);

    EXPECT_FALSE(lifecycle.SetImageStatus(RadarMapStatus::Failed, "another reason"));

    for (const MapLayerEntry& entry : cache.get_entries())
    {
        EXPECT_FALSE(entry.valid);
        EXPECT_EQ(entry.layer, -1);
    }
}

TEST(RadarLayersDump, ReportsCompletedAnalysisWithoutSupports)
{
    RadarMapLayers map;
    ASSERT_EQ(map.Build(RadarMapGeometry{}), RadarMapStatus::Failed);

    RadarLayerLifecycle lifecycle;
    ASSERT_TRUE(lifecycle.SetGeometryStatus(RadarMapStatus::Failed));

    std::string dump = RadarLayersDump_Format(&map, nullptr, nullptr, "test", {}, 1.0f, &lifecycle);
    EXPECT_NE(dump.find("geometry_state=failed image_state=waiting fallback=1"), std::string::npos);
    EXPECT_NE(dump.find("analysis produced no usable support layers"), std::string::npos);
}

TEST(RadarLayersDump, ReportsPreparationWaitWithoutChangingOwnedState)
{
    RadarMapLayers map;
    RadarLayerLifecycle lifecycle;
    RadarMapPreparationDiagnostic preparation{"stage=surface_uvs surface=17 flags=0 polys=missing", 12, 5, 12.5f};
    std::string dump = RadarLayersDump_Format(&map, nullptr, nullptr, "adv_clocktower", {}, 0.0f, &lifecycle, &preparation);

    EXPECT_NE(dump.find("analysis_ms=0.000"), std::string::npos);
    EXPECT_NE(dump.find("geometry_state=waiting"), std::string::npos);
    EXPECT_NE(dump.find("preparation_attempts=12 preparation_mode=5"), std::string::npos);
    EXPECT_NE(dump.find("capture_ms=12.500"), std::string::npos);
    EXPECT_NE(dump.find(preparation.waiting_reason), std::string::npos);
    EXPECT_EQ(RadarLayersDump_Format(&map, nullptr, nullptr, "adv_clocktower", {}, 0.0f, &lifecycle, &preparation), dump);
    EXPECT_EQ(preparation.attempts, 12u);
    EXPECT_TRUE(lifecycle.should_build_geometry());
    EXPECT_FALSE(lifecycle.should_use_fallback());

    preparation.waiting_reason.clear();
    dump = RadarLayersDump_Format(&map, nullptr, nullptr, "adv_clocktower", {}, 0.0f, &lifecycle, &preparation);

    EXPECT_NE(dump.find("waiting_reason=\"\""), std::string::npos);
    EXPECT_EQ(dump.find("stage=surface_uvs"), std::string::npos);
}


TEST(RadarLayersDump, ReportsOverlapSignificance)
{
    RadarMapLayerFixture fixture;
    fixture.AddFloor(0.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});
    fixture.AddFloor(256.0f, {{0.0f, 0.0f}, {512.0f, 512.0f}});

    RadarMapLayers map;
    ASSERT_EQ(map.Build(std::move(fixture.geometry)), RadarMapStatus::Ready);

    std::string dump = RadarLayersDump_Format(&map, nullptr, nullptr, "test", {}, 0.0f);
    EXPECT_NE(dump.find("overlap_area_threshold=131072.000"), std::string::npos);
    EXPECT_NE(dump.find("largest_overlap_area=262144.000"), std::string::npos);
    EXPECT_NE(dump.find("significant_overlap_regions=1"), std::string::npos);
}

#include <gtest/gtest.h>

#include "hud/radar/map/layers/MapLayerCache.h"

TEST(MapLayerCache, ProtectsRequestedLayer)
{
    MapLayerCache cache;

    for (int layer = 0; layer < 3; layer++)
    {
        MapLayerRequest request = cache.Request(layer, layer + 1);
        ASSERT_TRUE(request.render);

        cache.Commit(request.slot, layer);
    }

    EXPECT_FALSE(cache.Request(0, 4).render);

    MapLayerRequest next = cache.Request(3, 4);
    ASSERT_TRUE(next.render);

    cache.Commit(next.slot, 3);

    EXPECT_GE(cache.Request(0, 5).slot, 0);
    EXPECT_GE(cache.Request(3, 5).slot, 0);
    EXPECT_TRUE(cache.Request(1, 5).render);
    EXPECT_LE(cache.get_entries().size(), 3u);
}

TEST(MapLayerCache, LimitsGenerationPerFrame)
{
    MapLayerCache cache;
    MapLayerRequest first = cache.Request(0, 10);
    ASSERT_TRUE(first.render);

    cache.Commit(first.slot, 0);

    EXPECT_FALSE(cache.Request(1, 10).render);
    EXPECT_TRUE(cache.Request(1, 11).render);
}

TEST(MapLayerCache, FailedRenderIsNotCommitted)
{
    MapLayerCache cache;
    MapLayerRequest failed = cache.Request(0, 10);
    ASSERT_TRUE(failed.render);
    EXPECT_FALSE(cache.get_entries()[failed.slot].valid);
    EXPECT_FALSE(cache.Request(0, 10).render);
    EXPECT_TRUE(cache.Request(0, 11).render);
}

TEST(MapLayerCache, ModeResetDropsImages)
{
    MapLayerCache cache;
    MapLayerRequest request = cache.Request(0, 10);
    ASSERT_TRUE(request.render);

    cache.Commit(request.slot, 0);
    cache.Reset();

    for (const MapLayerEntry& entry : cache.get_entries())
    {
        EXPECT_FALSE(entry.valid);
        EXPECT_EQ(entry.layer, -1);
    }

    EXPECT_TRUE(cache.Request(0, 11).render);
}

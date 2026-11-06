#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "GameUi/Controls/ImageFileCache.h"

TEST(ImageFileCache, LoadsOnFirstAcquireAndReusesDecodedImage)
{
    std::vector<std::string> loaded_paths;
    ImageFileCache cache(
        2,
        [&](const std::string& path) {
            loaded_paths.push_back(path);
            ImageFilePixels image;
            image.wide = image.tall = 1;

            if (path == "materials/first.tga")
            {
                image.texture_id = 11;
                image.rgba = {255, 0, 0, 255};
            }
            else if (path == "materials/second.tga")
            {
                image.texture_id = 22;
                image.rgba = {0, 255, 0, 255};
            }
            else
            {
                ADD_FAILURE() << "Unexpected image path: " << path;
            }

            return image;
        },
        [](int) {}
    );

    EXPECT_TRUE(loaded_paths.empty());
    const ImageFilePixels* first = cache.Acquire("materials/first.tga");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->texture_id, 11);
    EXPECT_EQ(first->rgba, std::vector<uint8_t>({255, 0, 0, 255}));
    first = cache.Acquire("materials/first.tga");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->texture_id, 11);
    EXPECT_EQ(first->rgba, std::vector<uint8_t>({255, 0, 0, 255}));
    EXPECT_EQ(loaded_paths, std::vector<std::string>({"materials/first.tga"}));

    const ImageFilePixels* second = cache.Acquire("materials/second.tga");
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(second->texture_id, 22);
    EXPECT_EQ(second->rgba, std::vector<uint8_t>({0, 255, 0, 255}));
    first = cache.Acquire("materials/first.tga");
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->rgba, std::vector<uint8_t>({255, 0, 0, 255}));
    EXPECT_EQ(loaded_paths, std::vector<std::string>({"materials/first.tga", "materials/second.tga"}));
    EXPECT_EQ(cache.get_images().size(), 2);
}

TEST(ImageFileCache, EvictsLeastRecentlyPaintedImageBeforeLoading)
{
    int next_id = 0;
    size_t peak_live = 0;
    std::set<int> live;
    std::vector<int> released;
    ImageFileCache cache(
        2,
        [&](const std::string&) {
            ImageFilePixels image;
            image.texture_id = ++next_id;
            live.insert(image.texture_id);
            peak_live = std::max(peak_live, live.size());
            return image;
        },
        [&](int id) {
            EXPECT_EQ(live.erase(id), 1);
            released.push_back(id);
        }
    );

    ASSERT_EQ(cache.Acquire("a")->texture_id, 1);
    ASSERT_EQ(cache.Acquire("b")->texture_id, 2);
    ASSERT_EQ(cache.Acquire("a")->texture_id, 1);
    ASSERT_EQ(cache.Acquire("c")->texture_id, 3);
    EXPECT_EQ(released, std::vector<int>({2}));
    ASSERT_EQ(cache.Acquire("b")->texture_id, 4);
    EXPECT_EQ(released, std::vector<int>({2, 1}));
    EXPECT_EQ(peak_live, 2);
    EXPECT_EQ(cache.get_images().size(), 2);
}

TEST(ImageFileCache, BoundsFailedLoadsAndRetriesAfterEviction)
{
    int loads = 0;
    ImageFileCache cache(
        2,
        [&](const std::string&) {
            loads++;
            return ImageFilePixels{};
        },
        [](int) {
            ADD_FAILURE() << "A failed load has no texture to release";
        }
    );

    EXPECT_EQ(cache.Acquire("missing"), nullptr);
    EXPECT_EQ(cache.Acquire("missing"), nullptr);
    EXPECT_EQ(loads, 1);

    for (int index = 0; index < 1000; index++)
    {
        EXPECT_EQ(cache.Acquire(std::to_string(index)), nullptr);
        EXPECT_LE(cache.get_images().size(), 2);
    }

    EXPECT_EQ(cache.Acquire("missing"), nullptr);
    EXPECT_EQ(loads, 1002);
}

TEST(ImageFileCache, ClearReleasesEveryTextureOnceAndAllowsReload)
{
    int next_id = 0;
    std::vector<int> released;
    ImageFileCache cache(
        2,
        [&](const std::string&) {
            ImageFilePixels image;
            image.texture_id = ++next_id;
            return image;
        },
        [&](int id) {
            released.push_back(id);
        }
    );

    cache.Acquire("a");
    cache.Acquire("b");
    cache.Clear();
    cache.Clear();
    EXPECT_EQ(released, std::vector<int>({1, 2}));
    EXPECT_TRUE(cache.get_images().empty());
    ASSERT_EQ(cache.Acquire("a")->texture_id, 3);
}

TEST(ImageFileCache, OwnerDestructionReleasesRetainedTextures)
{
    std::vector<int> released;

    {
        ImageFileCache cache(
            1,
            [](const std::string&) {
                ImageFilePixels image;
                image.texture_id = 7;
                return image;
            },
            [&](int id) {
                released.push_back(id);
            }
        );

        EXPECT_EQ(cache.Acquire(""), nullptr);
        ASSERT_NE(cache.Acquire("a"), nullptr);
        EXPECT_TRUE(released.empty());
    }

    EXPECT_EQ(released, std::vector<int>({7}));
}

#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <goldsrc/format/studio.h>
#include <ncl_studio/StudioMetadata.h>
#include <ncl_studio/StudioTextures.h>

namespace
{
    constexpr int kStudioVersion = 10;

    class MetadataBlock
    {
    private:
        std::vector<uint8_t> bytes_;

    public:
        explicit MetadataBlock(int choices = 2)
        {
            bytes_.resize(
                sizeof(goldsrc::studiohdr_t) + sizeof(goldsrc::mstudiobodyparts_t) + choices * sizeof(goldsrc::mstudiomodel_t) +
                3 * sizeof(goldsrc::mstudioseqdesc_t)
            );
            header().version = kStudioVersion;
            header().length = static_cast<int>(bytes_.size()) + 4096;
            header().numbones = 1;
            header().numseq = 3;
            header().seqindex = static_cast<int>(bytes_.size() - 3 * sizeof(goldsrc::mstudioseqdesc_t));
            header().bbmin[0] = -8.0f;
            header().bbmax[2] = 24.0f;
            header().numbodyparts = 1;
            header().bodypartindex = sizeof(goldsrc::studiohdr_t);
            body().base = 1;
            body().nummodels = choices;
            body().modelindex = header().bodypartindex + sizeof(goldsrc::mstudiobodyparts_t);
            std::strcpy(submodel(0).name, "body");

            if (choices > 1)
            {
                std::strcpy(submodel(1).name, "alternate");
            }
        }

        goldsrc::studiohdr_t& header()
        {
            return *reinterpret_cast<goldsrc::studiohdr_t*>(bytes_.data());
        }

        goldsrc::mstudiobodyparts_t& body()
        {
            return *reinterpret_cast<goldsrc::mstudiobodyparts_t*>(bytes_.data() + header().bodypartindex);
        }

        goldsrc::mstudiomodel_t& submodel(int index)
        {
            return reinterpret_cast<goldsrc::mstudiomodel_t*>(bytes_.data() + body().modelindex)[index];
        }

        goldsrc::mstudioseqdesc_t& sequence(int index)
        {
            return reinterpret_cast<goldsrc::mstudioseqdesc_t*>(bytes_.data() + header().seqindex)[index];
        }

        const void* data() const
        {
            return bytes_.data();
        }

        size_t size() const
        {
            return bytes_.size();
        }
    };
} // namespace

TEST(StudioMetadata, ReadsOwnedMetadataFromCachedBlockWithoutGeometryOrPixels)
{
    MetadataBlock block;
    ncl_utils::ResultT<ncl_studio::StudioMetadata> result = ncl_studio::ReadStudioMetadata(block.data(), block.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(result->minimum[0], -8.0f);
    EXPECT_EQ(result->maximum[2], 24.0f);
    EXPECT_EQ(result->sequences.size(), 3u);
    ASSERT_EQ(result->bodyparts.size(), 1u);
    EXPECT_EQ(result->bodyparts[0].base, 1);
    ASSERT_EQ(result->bodyparts[0].submodels.size(), 2u);
    EXPECT_EQ(result->bodyparts[0].submodels[1], "alternate");

    std::strcpy(block.submodel(1).name, "changed");
    EXPECT_EQ(result->bodyparts[0].submodels[1], "alternate");
}

TEST(StudioMetadata, BoundsUnterminatedSubmodelNames)
{
    MetadataBlock block;
    std::memset(block.submodel(0).name, 'x', sizeof(block.submodel(0).name));
    ncl_utils::ResultT<ncl_studio::StudioMetadata> result = ncl_studio::ReadStudioMetadata(block.data(), block.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(result->bodyparts[0].submodels[0], std::string(sizeof(block.submodel(0).name), 'x'));
}

TEST(StudioMetadata, RetainsBodyChoicesBeyondConsumerCap)
{
    MetadataBlock block(65);
    ncl_utils::ResultT<ncl_studio::StudioMetadata> result = ncl_studio::ReadStudioMetadata(block.data(), block.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    EXPECT_EQ(result->bodyparts[0].submodels.size(), 65u);
}

TEST(StudioMetadata, RejectsTruncatedBodyAndSubmodelTables)
{
    MetadataBlock block;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), sizeof(goldsrc::studiohdr_t)).has_error());
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.header().seqindex - 1).has_error());
    block.body().modelindex = -1;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
}

TEST(StudioMetadata, RejectsNegativeBodyCountsAndSequenceCount)
{
    MetadataBlock block;
    block.body().nummodels = -1;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.body().nummodels = 2;
    block.header().numbodyparts = -1;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.header().numbodyparts = 1;
    block.header().numseq = -1;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
}

TEST(StudioMetadata, RejectsNonFiniteBounds)
{
    MetadataBlock block;
    block.header().bbmin[1] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.header().bbmin[1] = 0.0f;
    block.header().bbmax[2] = std::numeric_limits<float>::infinity();
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
}

TEST(StudioMetadata, AcceptsTextureCompanionWithoutGeometry)
{
    struct Companion
    {
        goldsrc::studiohdr_t header{};
        goldsrc::mstudiotexture_t texture{};
        short skin{};
    } block;
    block.header.version = kStudioVersion;
    block.header.length = sizeof(block) + 4096;
    block.header.textureindex = offsetof(Companion, texture);
    block.header.texturedataindex = sizeof(block);
    block.header.numtextures = 1;
    block.header.numskinref = 1;
    block.header.numskinfamilies = 1;
    block.header.skinindex = offsetof(Companion, skin);
    std::strcpy(block.texture.name, "companion.bmp");
    block.texture.width = 32;
    block.texture.height = 16;

    ncl_utils::ResultT<ncl_studio::StudioMetadata> metadata = ncl_studio::ReadStudioMetadata(&block, sizeof(block));
    ASSERT_FALSE(metadata.has_error()) << metadata.error_str();
    EXPECT_TRUE(metadata->bodyparts.empty());
    ncl_utils::ResultT<ncl_studio::StudioTextures> textures = ncl_studio::ReadStudioTextures(&block, sizeof(block));
    ASSERT_FALSE(textures.has_error()) << textures.error_str();
    EXPECT_EQ(textures->textures[0].name, "companion.bmp");
    EXPECT_EQ(textures->skins.TextureFor(0, 0), 0);
}

TEST(StudioMetadata, RejectsSequenceTableOutsideSuppliedCacheExtent)
{
    MetadataBlock block;
    block.header().seqindex = std::numeric_limits<int>::max();
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.header().seqindex = static_cast<int>(block.size() - sizeof(goldsrc::mstudioseqdesc_t));
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.header().seqindex = -1;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
    block.header().seqindex = sizeof(goldsrc::studiohdr_t);
    block.header().numseq = std::numeric_limits<int>::max();
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
}

TEST(StudioMetadata, RejectsTruncatedSequenceTableDespiteLongerDeclaredLength)
{
    MetadataBlock block;
    EXPECT_TRUE(ncl_studio::ReadStudioMetadata(block.data(), block.size() - 1).has_error());
}

TEST(StudioMetadata, AcceptsEmptySequenceTable)
{
    MetadataBlock block;
    block.header().numseq = 0;
    block.header().seqindex = 0;
    EXPECT_FALSE(ncl_studio::ReadStudioMetadata(block.data(), block.size()).has_error());
}

TEST(StudioMetadata, OwnsSequenceNamesAndTimingWithBoundedUnterminatedLabels)
{
    MetadataBlock block;
    std::memset(block.sequence(0).label, 's', sizeof(block.sequence(0).label));
    block.sequence(0).fps = 12.0f;
    block.sequence(0).numframes = 25;
    ncl_utils::ResultT<ncl_studio::StudioMetadata> result = ncl_studio::ReadStudioMetadata(block.data(), block.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();
    ASSERT_EQ(result->sequences.size(), 3u);
    std::memset(&block.sequence(0), 0, sizeof(block.sequence(0)));
    EXPECT_EQ(result->sequences[0].name, std::string(32, 's'));
    EXPECT_FLOAT_EQ(result->sequences[0].fps, 12.0f);
    EXPECT_EQ(result->sequences[0].frame_count, 25);
}

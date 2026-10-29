#include <gtest/gtest.h>

#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include <ncl_studio/studio_validate.h>

#include <goldsrc/format/studio.h>


namespace
{
    constexpr int kStudioVersion = 10;

    // A file that passes every check, so a test can break exactly one thing and know that
    // the rejection it sees is the one it asked for.
    class FileBuilder
    {
    public:
        FileBuilder()
        {
            buffer_.resize(sizeof(goldsrc::studiohdr_t) + 4096);
            std::memset(buffer_.data(), 0, buffer_.size());

            header().version = kStudioVersion;
            header().numbones = 1;
            header().boneindex = sizeof(goldsrc::studiohdr_t);

            header().numbodyparts = 1;
            header().bodypartindex = sizeof(goldsrc::studiohdr_t) + 512;

            header().numskinref = 2;
            header().numskinfamilies = 2;
            header().skinindex = sizeof(goldsrc::studiohdr_t) + 1024;

            header().numtextures = 1;
            header().textureindex = sizeof(goldsrc::studiohdr_t) + 1536;
            header().texturedataindex = sizeof(goldsrc::studiohdr_t) + 2048;
            header().length = static_cast<int>(buffer_.size());

            texture().width = 4;
            texture().height = 4;
            texture().index = header().texturedataindex;
        }

        goldsrc::studiohdr_t& header() { return *reinterpret_cast<goldsrc::studiohdr_t*>(buffer_.data()); }

        goldsrc::mstudiotexture_t& texture()
        {
            return *reinterpret_cast<goldsrc::mstudiotexture_t*>(buffer_.data() + header().textureindex);
        }

        const void* data() const { return buffer_.data(); }
        size_t size() const { return buffer_.size(); }

    private:
        std::vector<uint8_t> buffer_;
    };
}

TEST(StudioValidate, AcceptsAWellFormedFile)
{
    FileBuilder file;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    EXPECT_FALSE(result.has_error()) << result.error_str();
}

TEST(StudioValidate, RejectsABufferShorterThanTheHeader)
{
    FileBuilder file;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), sizeof(goldsrc::studiohdr_t) - 1);
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("shorter than a studio header"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsALengthLongerThanTheFile)
{
    FileBuilder file;
    file.header().textureindex = 0;
    file.header().length = static_cast<int>(file.size()) + 1;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("do not fit a file"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsATextureCountThatOverrunsTheCachedBlock)
{
    FileBuilder file;

    // The count is what the loader walks; the block ends at texturedataindex.
    file.header().numtextures = 100000;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("texture array"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsASkinTableOutsideTheCachedBlock)
{
    FileBuilder file;
    file.header().skinindex = file.header().texturedataindex;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("skin table"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, BoundsTheWholeSkinRectangleNotTheFirstRow)
{
    FileBuilder file;

    // One row fits where the whole rectangle does not, and the entity's skin selects a row.
    file.header().numskinfamilies = 4096;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("skin table"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsABodyPartArrayOutsideTheCachedBlock)
{
    FileBuilder file;
    file.header().numbodyparts = 100000;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("body part array"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsNegativeOffsetsAndCounts)
{
    FileBuilder file;
    file.header().skinindex = -64;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("skin table"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsTexturePixelsOutsideTheFile)
{
    FileBuilder file;
    file.texture().width = 4096;
    file.texture().height = 4096;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("pixel data"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsATextureWithNoSize)
{
    FileBuilder file;
    file.texture().height = 0;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("declares a size of"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, RejectsABoneCountOutsideTheFormat)
{
    FileBuilder file;
    file.header().numbones = goldsrc::MAXSTUDIOBONES + 1;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("the format allows"), std::string::npos) << result.error_str();
}

// The companion file of a model whose textures live outside it. Rejecting it costs the
// model its textures, because the loader answers a failed check by replacing the header
// with an empty stub.
TEST(StudioValidate, AcceptsATextureOnlyCompanionWithNoBones)
{
    FileBuilder file;
    file.header().numbones = 0;
    file.header().boneindex = 0;
    file.header().numbodyparts = 0;
    file.header().bodypartindex = 0;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    EXPECT_FALSE(result.has_error()) << result.error_str();
}

TEST(StudioValidate, RejectsBodyPartsWithoutBones)
{
    FileBuilder file;
    file.header().numbones = 0;
    file.header().boneindex = 0;

    ncl_utils::Result<> result = ncl_studio::ValidateStudioFile(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("no bones to move them"), std::string::npos) << result.error_str();
}

TEST(StudioValidate, CachedSizeStopsAtTheTextureDataWhenTexturesAreEmbedded)
{
    FileBuilder file;

    EXPECT_EQ(ncl_studio::CachedStudioSize(file.data(), file.size()), static_cast<size_t>(file.header().texturedataindex));
}

TEST(StudioValidate, CachedSizeIsTheDeclaredLengthWithoutEmbeddedTextures)
{
    FileBuilder file;
    file.header().textureindex = 0;
    file.header().numtextures = 0;

    EXPECT_EQ(ncl_studio::CachedStudioSize(file.data(), file.size()), static_cast<size_t>(file.header().length));
}

TEST(StudioValidate, CachedSizeIsZeroForAFileItDoesNotFit)
{
    FileBuilder file;
    file.header().texturedataindex = static_cast<int>(file.size()) + 1;

    EXPECT_EQ(ncl_studio::CachedStudioSize(file.data(), file.size()), 0u);
}

TEST(StudioValidate, RejectsSequenceTableOutsideCacheEvenWhenInsideFile)
{
    FileBuilder file;
    file.header().numseq = 1;
    file.header().seqindex = file.header().texturedataindex;
    EXPECT_TRUE(ncl_studio::ValidateStudioFile(file.data(), file.size()).has_error());
    file.header().seqindex = file.header().texturedataindex - sizeof(goldsrc::mstudioseqdesc_t) + 1;
    EXPECT_TRUE(ncl_studio::ValidateStudioFile(file.data(), file.size()).has_error());
    file.header().seqindex = std::numeric_limits<int>::max();
    EXPECT_TRUE(ncl_studio::ValidateStudioFile(file.data(), file.size()).has_error());
    file.header().seqindex = sizeof(goldsrc::studiohdr_t);
    file.header().numseq = std::numeric_limits<int>::max();
    EXPECT_TRUE(ncl_studio::ValidateStudioFile(file.data(), file.size()).has_error());
}

TEST(StudioValidate, AcceptsCompleteSequenceTableAtCacheBoundary)
{
    FileBuilder file;
    file.header().numseq = 1;
    file.header().seqindex = file.header().texturedataindex - sizeof(goldsrc::mstudioseqdesc_t);
    EXPECT_FALSE(ncl_studio::ValidateStudioFile(file.data(), file.size()).has_error());
}

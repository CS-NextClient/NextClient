#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <vector>

#include <ncl_studio/StudioTextures.h>

#include <goldsrc/format/studio.h>


namespace
{
    constexpr int kStudioVersion = 10;

    // Two textures and a two by two skin table, laid out so that a test can move exactly one
    // number and know which rejection it is looking at.
    class TextureFileBuilder
    {
    public:
        TextureFileBuilder()
        {
            buffer_.resize(sizeof(goldsrc::studiohdr_t) + 4096);
            std::memset(buffer_.data(), 0, buffer_.size());

            header().version = kStudioVersion;
            header().length = static_cast<int>(buffer_.size());

            header().numbones = 1;
            header().boneindex = sizeof(goldsrc::studiohdr_t);

            header().numbodyparts = 1;
            header().bodypartindex = sizeof(goldsrc::studiohdr_t) + 512;

            header().numtextures = 2;
            header().textureindex = sizeof(goldsrc::studiohdr_t) + 1536;
            header().texturedataindex = sizeof(goldsrc::studiohdr_t) + 2048;

            header().numskinref = 2;
            header().numskinfamilies = 2;
            header().skinindex = sizeof(goldsrc::studiohdr_t) + 1024;

            for (int i = 0; i < header().numtextures; i++)
            {
                std::strcpy(texture(i).name, i == 0 ? "base.bmp" : "slide.bmp");
                texture(i).width = 4;
                texture(i).height = 4;
                texture(i).index = header().texturedataindex;
            }

            // Family 0 uses both textures as they stand, family 1 swaps them.
            skins()[0] = 0;
            skins()[1] = 1;
            skins()[2] = 1;
            skins()[3] = 0;
        }

        goldsrc::studiohdr_t& header() { return *reinterpret_cast<goldsrc::studiohdr_t*>(buffer_.data()); }

        goldsrc::mstudiotexture_t& texture(int index)
        {
            goldsrc::mstudiotexture_t* textures =
                reinterpret_cast<goldsrc::mstudiotexture_t*>(buffer_.data() + header().textureindex);

            return textures[index];
        }

        short* skins() { return reinterpret_cast<short*>(buffer_.data() + header().skinindex); }

        const void* data() const { return buffer_.data(); }
        size_t size() const { return buffer_.size(); }

    private:
        std::vector<uint8_t> buffer_;
    };
}

TEST(StudioTextures, ReadsTheTableAndTheSkinFamilies)
{
    TextureFileBuilder file;

    ncl_utils::ResultT<ncl_studio::StudioTextures> result = ncl_studio::ReadStudioTextures(file.data(), file.size());
    ASSERT_FALSE(result.has_error()) << result.error_str();

    ASSERT_EQ(result->textures.size(), 2u);
    EXPECT_EQ(result->textures[0].name, "base.bmp");
    EXPECT_EQ(result->textures[1].name, "slide.bmp");

    EXPECT_EQ(result->skins.families, 2);
    EXPECT_EQ(result->skins.references, 2);
    EXPECT_EQ(result->skins.TextureFor(0, 1), 1);
    EXPECT_EQ(result->skins.TextureFor(1, 1), 0);

    // Out of range is answered rather than indexed.
    EXPECT_EQ(result->skins.TextureFor(2, 0), -1);
    EXPECT_EQ(result->skins.TextureFor(0, -1), -1);
}

TEST(StudioTextures, RejectsABufferShorterThanTheHeader)
{
    TextureFileBuilder file;

    ncl_utils::ResultT<ncl_studio::StudioTextures> result = ncl_studio::ReadStudioTextures(file.data(), sizeof(goldsrc::studiohdr_t) - 1);
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("smaller than a studio header"), std::string::npos) << result.error_str();
}

TEST(StudioTextures, RejectsASkinReferenceNamingATextureTheModelDoesNotHave)
{
    TextureFileBuilder file;
    file.skins()[3] = 7;

    ncl_utils::ResultT<ncl_studio::StudioTextures> result = ncl_studio::ReadStudioTextures(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("does not have"), std::string::npos) << result.error_str();
}

// The two dimensions multiplied in the file's own int wrap to a small positive number, so a
// table declared as billions of entries would pass a bound it does not meet and leave the
// dimensions describing more values than were read.
TEST(StudioTextures, RejectsASkinTableWhoseDimensionsOverflow)
{
    TextureFileBuilder file;
    file.header().numskinfamilies = 65536;
    file.header().numskinref = 65536;

    ncl_utils::ResultT<ncl_studio::StudioTextures> result = ncl_studio::ReadStudioTextures(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("skin table"), std::string::npos) << result.error_str();
}

TEST(StudioTextures, RejectsANegativeSkinTable)
{
    TextureFileBuilder file;
    file.header().numskinref = -2;

    ncl_utils::ResultT<ncl_studio::StudioTextures> result = ncl_studio::ReadStudioTextures(file.data(), file.size());
    ASSERT_TRUE(result.has_error());
    EXPECT_NE(result.error_str().find("skin table"), std::string::npos) << result.error_str();
}

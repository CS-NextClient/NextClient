#include <gtest/gtest.h>
#include <nextclient/plugin.hpp>
#include <cstring>

TEST(SdkBuffers, IncludesTerminatorAtPayloadLimitAndRejectsIncompleteWrites)
{
    std::string value;
    auto exact = [](char* out, uint32_t size) -> uint32_t {
        if (out && size >= 4)
            std::memcpy(out, "abc", 4);
        return 4;
    };
    EXPECT_TRUE(nextclient::Plugin::read_string(exact, value, 3));
    EXPECT_EQ(value, "abc");
    EXPECT_FALSE(nextclient::Plugin::read_string(exact, value, 2));
    EXPECT_TRUE(value.empty());
    auto unterminated = [](char* out, uint32_t) -> uint32_t {
        if (out)
            out[0] = 'x';
        return 2;
    };
    EXPECT_FALSE(nextclient::Plugin::read_string(unterminated, value, 1));
    EXPECT_TRUE(value.empty());
    auto changed = [](char* out, uint32_t) -> uint32_t { return out ? 3 : 2; };
    EXPECT_FALSE(nextclient::Plugin::json_result(changed, value));
    EXPECT_TRUE(value.empty());
}

#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "client/ncl_entity/ncl_entity_message.h"

namespace
{
    std::vector<uint8_t> FloatMessage(float value)
    {
        uint32_t bits = std::bit_cast<uint32_t>(value);
        return {
            77,
            1,
            0x34,
            0x12,
            static_cast<uint8_t>(bits),
            static_cast<uint8_t>(bits >> 8),
            static_cast<uint8_t>(bits >> 16),
            static_cast<uint8_t>(bits >> 24),
            1,
            0x78,
            0x56
        };
    }

    const std::vector<ClientNclEntityFieldDescriptor> kFields{
        {1, ncl_entity::FieldType::BYTE},
        {2, ncl_entity::FieldType::ARRAY_STRING_ID_FLOAT_PAIRS_DELTA}
    };
} // namespace

TEST(NclEntityMessageTests, PreservesEveryFiniteFloatBit)
{
    for (float value : {0.1234567f, -0.7654321f, std::numeric_limits<float>::denorm_min(), -0.0f, 0.0f})
    {
        std::vector<uint8_t> bytes = FloatMessage(value);
        MessageReader reader(bytes.data(), bytes.size());
        NclEntityFields fields;
        ASSERT_TRUE(NclEntityMessage::ReadFields(reader, kFields, true, 3, fields));
        const auto& delta = std::get<ncl_entity::ArrayStringIdFloatPairsDelta>(fields.at(2).value);
        ASSERT_EQ(delta.added.size(), 1);
        EXPECT_EQ(delta.added[0].first, 0x1234);
        EXPECT_EQ(std::bit_cast<uint32_t>(delta.added[0].second), std::bit_cast<uint32_t>(value));
        ASSERT_EQ(delta.removed.size(), 1);
        EXPECT_EQ(delta.removed[0], 0x5678);
    }
}

TEST(NclEntityMessageTests, RejectsEveryTruncatedPrefixWithoutPublishingFields)
{
    std::vector<uint8_t> bytes = FloatMessage(0.1234567f);
    for (size_t size = 0; size < bytes.size(); ++size)
    {
        MessageReader reader(bytes.data(), size);
        NclEntityFields fields{{99, ncl_entity::FieldValue{uint8_t{42}}}};
        EXPECT_FALSE(NclEntityMessage::ReadFields(reader, kFields, true, 3, fields)) << size;
        ASSERT_EQ(fields.size(), 1);
        EXPECT_EQ(std::get<uint8_t>(fields.at(99).value), 42);
    }
}

TEST(NclEntityMessageTests, RejectsNonfiniteValuesWithoutPublishingEarlierFields)
{
    for (float value :
         {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()})
    {
        std::vector<uint8_t> bytes = FloatMessage(value);
        MessageReader reader(bytes.data(), bytes.size());
        NclEntityFields fields;
        EXPECT_FALSE(NclEntityMessage::ReadFields(reader, kFields, true, 3, fields));
        EXPECT_TRUE(fields.empty());
    }
}

TEST(NclEntityMessageTests, PreservesIdPairsAndClearSentinel)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{{1, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA}};
    std::vector<uint8_t> bytes{1, 0x34, 0x12, 0x78, 0x56, 1, 0x9a, 0xbc, 0xde, 0xf0};
    MessageReader reader(bytes.data(), bytes.size());
    NclEntityFields fields;
    ASSERT_TRUE(NclEntityMessage::ReadFields(reader, schema, true, 1, fields));
    const auto& delta = std::get<ncl_entity::ArrayStringIdPairsDelta>(fields.at(1).value);
    EXPECT_EQ(delta.added, (std::vector<std::pair<uint16_t, uint16_t>>{{0x1234, 0x5678}}));
    EXPECT_EQ(delta.removed, (std::vector<std::pair<uint16_t, uint16_t>>{{0xbc9a, 0xf0de}}));
    uint8_t clear = 255;
    MessageReader clear_reader(&clear, 1);
    ASSERT_TRUE(NclEntityMessage::ReadFields(clear_reader, schema, true, 1, fields));
    EXPECT_TRUE(std::get<ncl_entity::ArrayStringIdPairsDelta>(fields.at(1).value).clear);
    MessageReader create_reader(&clear, 1);
    EXPECT_FALSE(NclEntityMessage::ReadFields(create_reader, schema, false, 0, fields));
}

TEST(NclEntityMessageTests, RejectsTruncatedIdPairsWithoutPublishing)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{{1, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA}};
    const std::vector<uint8_t> bytes{1, 1, 0, 2, 0, 1, 3, 0, 4, 0};
    for (size_t size = 0; size < bytes.size(); ++size)
    {
        MessageReader reader(bytes.data(), size);
        NclEntityFields fields;
        EXPECT_FALSE(NclEntityMessage::ReadFields(reader, schema, true, 1, fields)) << size;
        EXPECT_TRUE(fields.empty());
    }
}

TEST(NclEntityMessageTests, ReadsFloatCreatesAndRejectsCreateRemovalsOrClear)
{
    std::vector<uint8_t> bytes = FloatMessage(-0.1234567f);
    bytes.resize(9);
    bytes.back() = 0;
    MessageReader reader(bytes.data(), bytes.size());
    NclEntityFields fields;
    ASSERT_TRUE(NclEntityMessage::ReadFields(reader, kFields, false, 0, fields));
    EXPECT_EQ(std::get<uint8_t>(fields.at(1).value), 77);
    const auto& delta = std::get<ncl_entity::ArrayStringIdFloatPairsDelta>(fields.at(2).value);
    ASSERT_EQ(delta.added.size(), 1);
    EXPECT_EQ(std::bit_cast<uint32_t>(delta.added[0].second), std::bit_cast<uint32_t>(-0.1234567f));
    bytes = FloatMessage(0.1234567f);
    MessageReader removal_reader(bytes.data(), bytes.size());
    EXPECT_FALSE(NclEntityMessage::ReadFields(removal_reader, kFields, false, 0, fields));
    bytes = {77, 255};
    MessageReader clear_reader(bytes.data(), bytes.size());
    EXPECT_FALSE(NclEntityMessage::ReadFields(clear_reader, kFields, false, 0, fields));
}

TEST(NclEntityMessageTests, RejectsUnknownTypesAndUnknownUpdateBits)
{
    uint8_t bytes = 0;
    MessageReader reader(&bytes, 1);
    NclEntityFields fields;
    EXPECT_FALSE(NclEntityMessage::ReadFields(reader, {{1, static_cast<ncl_entity::FieldType>(99)}}, true, 1, fields));
    MessageReader mask_reader(&bytes, 1);
    EXPECT_FALSE(NclEntityMessage::ReadFields(mask_reader, kFields, true, 4, fields));
}

TEST(NclEntityMessageTests, RejectsTrailingBytesAfterFloatClearWithoutPublishingFields)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{{4, ncl_entity::FieldType::ARRAY_STRING_ID_FLOAT_PAIRS_DELTA}};
    const std::vector<uint8_t> bytes{0xFF, 0x01};
    MessageReader reader(bytes.data(), bytes.size());
    NclEntityFields fields{{99, ncl_entity::FieldValue{uint8_t{42}}}};

    EXPECT_FALSE(NclEntityMessage::ReadFields(reader, schema, true, 4, fields));
    ASSERT_EQ(fields.size(), 1);
    EXPECT_EQ(std::get<uint8_t>(fields.at(99).value), 42);
}

TEST(NclEntityMessageTests, RejectsTrailingBytesAfterIdPairClearWithoutPublishingFields)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{{2, ncl_entity::FieldType::ARRAY_STRING_ID_PAIRS_DELTA}};
    const std::vector<uint8_t> bytes{0xFF, 0x01};
    MessageReader reader(bytes.data(), bytes.size());
    NclEntityFields fields{{99, ncl_entity::FieldValue{uint8_t{42}}}};

    EXPECT_FALSE(NclEntityMessage::ReadFields(reader, schema, true, 2, fields));
    ASSERT_EQ(fields.size(), 1);
    EXPECT_EQ(std::get<uint8_t>(fields.at(99).value), 42);
}

TEST(NclEntityMessageTests, RejectsTrailingBytesAfterScalarFieldsWithoutPublishingFields)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{
        {1, ncl_entity::FieldType::BYTE},
        {2, ncl_entity::FieldType::USHORT}
    };
    const std::vector<uint8_t> bytes{7, 0x34, 0x12, 0x99};
    for (bool update : {false, true})
    {
        MessageReader reader(bytes.data(), bytes.size());
        NclEntityFields fields{{99, ncl_entity::FieldValue{uint8_t{42}}}};

        EXPECT_FALSE(NclEntityMessage::ReadFields(reader, schema, update, 3, fields));
        ASSERT_EQ(fields.size(), 1);
        EXPECT_EQ(std::get<uint8_t>(fields.at(99).value), 42);
    }
}

TEST(NclEntityMessageTests, RejectsTrailingBytesAfterFloatValuesWithoutPublishingFields)
{
    std::vector<uint8_t> bytes = FloatMessage(0.1234567f);
    bytes.push_back(0x99);
    MessageReader reader(bytes.data(), bytes.size());
    NclEntityFields fields{{99, ncl_entity::FieldValue{uint8_t{42}}}};

    EXPECT_FALSE(NclEntityMessage::ReadFields(reader, kFields, true, 3, fields));
    ASSERT_EQ(fields.size(), 1);
    EXPECT_EQ(std::get<uint8_t>(fields.at(99).value), 42);
}

TEST(NclEntityMessageTests, ReadsAllScalarFieldsBeforeCheckingPayloadEnd)
{
    const std::vector<ClientNclEntityFieldDescriptor> schema{
        {1, ncl_entity::FieldType::BYTE},
        {2, ncl_entity::FieldType::USHORT}
    };
    const std::vector<uint8_t> bytes{7, 0x34, 0x12};
    for (bool update : {false, true})
    {
        MessageReader reader(bytes.data(), bytes.size());
        NclEntityFields fields;

        ASSERT_TRUE(NclEntityMessage::ReadFields(reader, schema, update, 3, fields));
        ASSERT_EQ(fields.size(), 2);
        EXPECT_EQ(std::get<uint8_t>(fields.at(1).value), 7);
        EXPECT_EQ(std::get<uint16_t>(fields.at(2).value), 0x1234);
    }
}

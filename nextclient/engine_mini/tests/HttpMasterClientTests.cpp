#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "service/matchmaking/master/HttpMasterClient.h"

namespace
{
    cpr::Response Answer(long status_code, std::string text)
    {
        cpr::Response response;
        response.status_code = status_code;
        response.text = std::move(text);

        return response;
    }
} // namespace

TEST(HttpMasterClientTest, TransportErrorFailsTheRequest)
{
    cpr::Response response = Answer(200, "1.2.3.4:27015");
    response.error.code = cpr::ErrorCode::OPERATION_TIMEDOUT;

    EXPECT_FALSE(HttpMasterClient_ReadServerList(response).has_value());
}

TEST(HttpMasterClientTest, StatusOtherThanOkFailsTheRequest)
{
    EXPECT_FALSE(HttpMasterClient_ReadServerList(Answer(502, "1.2.3.4:27015")).has_value());
}

TEST(HttpMasterClientTest, BodyInNoKnownLayoutFailsTheRequest)
{
    EXPECT_FALSE(HttpMasterClient_ReadServerList(Answer(200, "<html>Bad Gateway</html>")).has_value());
}

TEST(HttpMasterClientTest, AnswerListingNoServerGivesTheZeroAddressEntry)
{
    for (const char* body : {"[]", "", "0.0.0.0:0"})
    {
        std::optional<std::vector<MasterServerEntry>> server_list = HttpMasterClient_ReadServerList(Answer(200, body));

        ASSERT_TRUE(server_list.has_value()) << body;
        ASSERT_EQ(server_list->size(), 1u) << body;
        EXPECT_EQ((*server_list)[0].address, netadr_t{}) << body;
    }
}

TEST(HttpMasterClientTest, AnswerListingServersGivesThem)
{
    std::optional<std::vector<MasterServerEntry>> server_list =
        HttpMasterClient_ReadServerList(Answer(200, R"([{"address": "1.2.3.4:27015", "game_mode": "Surf"}])"));

    ASSERT_TRUE(server_list.has_value());
    ASSERT_EQ(server_list->size(), 1u);
    EXPECT_EQ((*server_list)[0].address, netadr_t("1.2.3.4:27015"));
    EXPECT_EQ((*server_list)[0].details.game_mode, "Surf");
}

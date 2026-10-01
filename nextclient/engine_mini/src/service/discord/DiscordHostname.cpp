#include "DiscordHostname.h"

#include <memory>

#include <taskcoro/TaskCoro.h>

#include "engine.h"
#include "service/matchmaking/sourcequery/MultiSourceQuery.h"

using namespace taskcoro;

static std::shared_ptr<MultiSourceQuery> g_SourceQuery;
static std::shared_ptr<CancellationToken> g_CancellationToken;

static bool g_HasQueriedAddress = false;
static netadr_t g_QueriedAddress;
static std::string g_Hostname;

void DiscordHostname_Init()
{
    g_SourceQuery = std::make_shared<MultiSourceQuery>(750, 3);
    g_CancellationToken = CancellationToken::Create();
}

void DiscordHostname_Shutdown()
{
    // A query still in flight holds its own references and finds the token cancelled
    if (g_CancellationToken)
        g_CancellationToken->SetCanceled();

    g_CancellationToken.reset();
    g_SourceQuery.reset();
}

void DiscordHostname_Update()
{
    if (!g_SourceQuery || cls->state != ca_active || cls->demoplayback || cls->netchan.remote_address.IsLoopback())
    {
        g_HasQueriedAddress = false;
        g_Hostname.clear();
        return;
    }

    netadr_t address = cls->netchan.remote_address;
    if (g_HasQueriedAddress && address == g_QueriedAddress)
        return;

    g_HasQueriedAddress = true;
    g_QueriedAddress = address;
    g_Hostname.clear();

    TaskCoro::RunInMainThread([query = g_SourceQuery, ct = g_CancellationToken, address]() -> concurrencpp::result<void>
    {
        SQResponseInfo<SQ_INFO> info = co_await query->GetInfoAsync(address);

        co_await TaskCoro::SwitchToMainThread();

        // the player may have moved to another server while the answer was on its way
        if (ct->IsCanceled() || info.error_code != SQErrorCode::Ok)
            co_return;

        if (g_HasQueriedAddress && g_QueriedAddress == address)
            g_Hostname = std::move(info.value.hostname);
    });
}

const std::string& DiscordHostname_Get()
{
    return g_Hostname;
}

#include "../engine.h"

#include <next_engine_mini/cl_private_resources.h>

#include "CmdChecker.h"
#include "CmdLoggerAggregator.h"
#include "ConsoleCmdLogger.h"

static char kEmpty[1] = { '\0' };

static std::shared_ptr<CmdLoggerAggregator> g_CmdLogger;
static std::unique_ptr<CmdChecker> g_CmdChecker;
static std::vector<std::shared_ptr<nitroapi::Unsubscriber>> g_Unsubs;

static bool g_CommandFromServer;
static bool g_CommandFromStufftext;
static bool g_Cbuf_AddText_called;

// TODO It is necessary to restore the code of functions on which hooks are now created. And get rid of this file.

static char* Cbuf_AddTextHandler(const char *text, sizebuf_t *buf, nitroapi::NextHandlerInterface<char*, const char*, sizebuf_t*>* next)
{
    g_Cbuf_AddText_called = true;

    // if empty command do nothing
    if (text == nullptr || text[0] == '\0')
        return next->Invoke(text, buf);

    std::string cmd = text;
    if (cmd.starts_with(kPrivateResourceMsgMarker))
        return kEmpty;

    std::string filtered_cmd = g_CmdChecker->GetFilteredCmd(cmd, g_CommandFromServer, g_CommandFromStufftext);
    if (!filtered_cmd.empty())
        return next->Invoke(filtered_cmd.c_str(), buf);

    return kEmpty;
}

static void CL_ConnectionlessPacket(nitroapi::NextHandlerInterface<void>* next)
{
    int read_count = *pMsg_readcount;

    MSG_BeginReading();
    MSG_ReadLong();
    const char* args = MSG_ReadStringLine();

    *pMsg_readcount = read_count;

    eng()->Cmd_TokenizeString(args);
    const char* c = gEngfuncs.Cmd_Argv(0);

    if (Q_stricmp(c, "challenge") && *c == 'L')
    {
        gEngfuncs.pfnClientCmd("disconnect\n");

        g_CmdLogger->LogCommand("[NOT COMMAND] redirect challenge", "", LogCommandType::BlockedAllCommand);
        return;
    }

    g_CommandFromServer = true;
    next->Invoke();
    g_CommandFromServer = false;
}

static void SVC_StufftextHandler(nitroapi::NextHandlerInterface<void>* next)
{
    int read_count = *pMsg_readcount;
    const char* cmd = MSG_ReadString();
    *pMsg_readcount = read_count;

    g_Cbuf_AddText_called = false;

    g_CommandFromServer = true;
    g_CommandFromStufftext = true;
    next->Invoke();
    g_CommandFromStufftext = false;
    g_CommandFromServer = false;

    // This can happen when the command is blocked by the engine, for example, when cl_filterstuffcmd 1 is enabled
    if (!g_Cbuf_AddText_called)
    {
        g_CmdLogger->LogCommand(cmd, "", LogCommandType::BlockedStufftextComamnd);
    }
}

static void SVC_DirectorHandler(nitroapi::NextHandlerInterface<void>* next)
{
    int read_count = *pMsg_readcount;

    int size_command = MSG_ReadByte();
    int direct_type = MSG_ReadByte();
    const char* direct_command = MSG_ReadString();

    if (direct_type == DRC_CMD_STUFFTEXT)
    {
        g_CmdLogger->LogCommand(direct_command, "", LogCommandType::BlockedDirectorCommand);
        return;
    }

    if (direct_type == DRC_CMD_BANNER) // block spectator banner left-upper corner
    {
        g_CmdLogger->LogCommand("[NOT COMMAND] Show DRC_CMD_BANNER", "", LogCommandType::BlockedBanner);
        return;
    }

    *pMsg_readcount = read_count;

    next->Invoke();
}

static void SVC_DisconnectHandler(nitroapi::NextHandlerInterface<void>* next)
{
    int read_count = *pMsg_readcount;
    const char* reason = MSG_ReadString();
    *pMsg_readcount = read_count;

    g_CmdLogger->LogCommand("disconnect", reason, LogCommandType::AllowServerCommand);

    next->Invoke();
}

static void SVC_ResourceLocationHandler(nitroapi::NextHandlerInterface<void>* next)
{
    int read_count = *pMsg_readcount;
    const char* url = MSG_ReadString();
    *pMsg_readcount = read_count;

    g_CmdLogger->LogCommand("sv_downloadurl", url, LogCommandType::AllowServerCommand);

    next->Invoke();
}

void PROTECTOR_Init(std::shared_ptr<nitro_utils::ConfigProviderInterface> config_provider)
{
    g_CommandFromServer = false;
    g_CommandFromStufftext = false;
    g_Cbuf_AddText_called = false;

    if (g_CmdLogger == nullptr)
        g_CmdLogger = std::make_shared<CmdLoggerAggregator>();

    g_CmdLogger->AddLogger(new ConsoleCmdLogger());

    g_CmdChecker = std::make_unique<CmdChecker>(g_CmdLogger, config_provider);

    g_Unsubs.emplace_back(eng()->Cbuf_AddText |= Cbuf_AddTextHandler);
    g_Unsubs.emplace_back(eng()->CL_ConnectionlessPacket |= CL_ConnectionlessPacket);
    g_Unsubs.emplace_back(eng()->SVC_StuffText |= SVC_StufftextHandler);
    g_Unsubs.emplace_back(eng()->SVC_Director |= SVC_DirectorHandler);
    g_Unsubs.emplace_back(eng()->SVC_Disconnect |= SVC_DisconnectHandler);
    g_Unsubs.emplace_back(eng()->SVC_ResourceLocation |= SVC_ResourceLocationHandler);
}

void PROTECTOR_Shutdown()
{
    for (auto& unsubscriber : g_Unsubs)
        unsubscriber->Unsubscribe();

    g_CmdChecker = nullptr;
    g_CmdLogger = nullptr;
}

bool PROTECTOR_AddCmdLogger(CommandLoggerInterface* logger)
{
    if (g_CmdLogger == nullptr)
        g_CmdLogger = std::make_shared<CmdLoggerAggregator>();

    return g_CmdLogger->AddLogger(logger);
}

bool PROTECTOR_RemoveCmdLogger(CommandLoggerInterface* logger)
{
    if (g_CmdLogger == nullptr)
        g_CmdLogger = std::make_shared<CmdLoggerAggregator>();

    return g_CmdLogger->RemoveLogger(logger);
}

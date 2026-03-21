#include "engine.h"
#include "detailtexture.h"

#include <SDL.h>
#include <SDL_opengl.h>
#include <optick.h>
#include <ankerl/unordered_dense.h>

#include "common/cvar.h"
#include "common/filesystem.h"
#include "graphics/texture_loader.h"
#include "console/console.h"

#include "gl_local.h"

enum class ParseState
{
    START,
    DIFFUSE_NAME,
    DETAIL_TEXTURE,
    S_SCALE_FACTOR,
    T_SCALE_FACTOR,
    ENTRY,
    ERROR_STATE,
    DONE
};

struct DetailMap
{
    std::string _diffuseName;
    std::string _detailName;
    float _sScale;
    float _tScale;
    unsigned int _oglTextureId = 0;
    unsigned int _oglDetailId = 0;
};

static cvar_t r_detailtextures{"r_detailtextures", const_cast<char*>("1"), FCVAR_ARCHIVE};
static cvar_t r_detailtexturessupported{"r_detailtexturessupported", const_cast<char*>("1"), FCVAR_SPONLY};

static bool detTexSupported;
static bool g_detTexLoaded;
static std::string g_levelName;
static bool g_demandLoad;
static std::vector<std::pair<std::string, int>> g_decalTexIDs;
static std::vector<DetailMap> g_detailVector;
static ankerl::unordered_dense::map<int, DetailMap*> g_idMap;
static ankerl::unordered_dense::map<std::string, int> g_detailNameToIdMap;
static ankerl::unordered_dense::map<std::string, int> g_detailLoadFailedMap;
static ankerl::unordered_dense::map<std::string, DetailMap*> g_diffuseNameToDetailMap;

static GLint lastDetTex = -1;
static float lastDetSScale = 1.0f;
static float lastDetTScale = 1.0f;

void DT_LoadDetailTexture(const char* diffuseName, int diffuseId)
{
    if (!detTexSupported)
    {
        return;
    }

    if (r_detailtextures.value == 0.0f)
    {
        g_decalTexIDs.emplace_back(diffuseName, diffuseId);
        return;
    }

    auto detailIt = g_diffuseNameToDetailMap.find(diffuseName);
    if (detailIt == g_diffuseNameToDetailMap.end())
    {
        return;
    }

    DetailMap* detail = detailIt->second;
    const std::string& detailName = detail->_detailName;

    if (g_detailLoadFailedMap.find(detailName) != g_detailLoadFailedMap.end())
    {
        return;
    }

    auto it = g_detailNameToIdMap.find(detailName);
    if (it == g_detailNameToIdMap.end())
    {
        tex::Texture texture;
        if (!tex::LoadTextureFromFile(texture, detailName.c_str(), detailName.c_str(), true, GL_LINEAR_MIPMAP_LINEAR))
        {
            Con_Printf("Detail texture map load failed: %s\n", detailName.c_str());
            g_detailLoadFailedMap[detailName] = 0;
        }
        else
        {
            detail->_oglTextureId = diffuseId;
            detail->_oglDetailId = texture.texnum;
            g_idMap[diffuseId] = detail;
            g_detailNameToIdMap[detailName] = texture.texnum;
        }
    }
    else
    {
        detail->_oglTextureId = diffuseId;
        detail->_oglDetailId = it->second;
        g_idMap[diffuseId] = detail;
    }
}

qboolean DT_SetRenderState(int diffuseId)
{
    if (!detTexSupported || r_detailtextures.value == 0.0f)
    {
        return FALSE;
    }

    if (!g_detTexLoaded)
    {
        Con_Printf("Loading Detail Textures...\n");

        g_demandLoad = true;
        DT_LoadDetailMapFile(g_levelName.c_str());

        size_t index = 0;
        while (index < g_decalTexIDs.size())
        {
            std::string decalName = g_decalTexIDs[index].first;
            int decalId = g_decalTexIDs[index].second;
            DT_LoadDetailTexture(decalName.c_str(), decalId);
            ++index;
        }

        g_detTexLoaded = true;
        g_demandLoad = false;
    }

    if (g_idMap.empty())
    {
        return FALSE;
    }

    auto detailIt = g_idMap.find(diffuseId);
    if (detailIt == g_idMap.end())
    {
        return FALSE;
    }

    DetailMap* detail = detailIt->second;
    if (!detail->_oglDetailId || !detail->_oglTextureId)
    {
        return FALSE;
    }

    qglSelectTextureSGIS(TEXTURE2_SGIS);
    qglEnable(GL_TEXTURE_2D);

    GLuint detailTextureId = detail->_oglDetailId;
    
    bool need_bind = lastDetTex == -1 || lastDetTex != detailTextureId;
    if (need_bind)
    {
        lastDetTex = detailTextureId;
        qglBindTexture(GL_TEXTURE_2D, detailTextureId);
    }

    bool need_scale_update = detail->_sScale != lastDetSScale || detail->_tScale != lastDetTScale;
    if (need_scale_update)
    {
        lastDetSScale = detail->_sScale;
        lastDetTScale = detail->_tScale;
        
        qglMatrixMode(GL_TEXTURE);
        qglLoadIdentity();
        qglScalef(detail->_sScale, detail->_tScale, 1.0f);
        qglMatrixMode(GL_MODELVIEW);
    }

    return TRUE;
}

static bool DT_GetToken(const char** cPtr, ParseState* parseState, std::string& token)
{
    OPTICK_EVENT();

    static const std::string whitespace = " \t\r\n";
    static const std::string specialChars = "_.-/{}\"~!+[]";

    const char* start = *cPtr;
    const char* current = start;
    token.clear();

    while (true)
    {
        // Skip whitespace
        current = start + strspn(start, whitespace.c_str());
        if (current == start)
            break;
        start = current;
    }

    if (*current == '\0')
    {
        *parseState = ParseState::DONE;
        return false;
    }

    // Handle comments
    if (current[0] == '/' && current[1] == '/')
    {
        current += 2;
        while (*current && *current != '\n' && *current != '\r')
        {
            current++;
        }
        *cPtr = current;
        return DT_GetToken(cPtr, parseState, token);
    }

    const char* end = current;
    while (*end && (std::isalnum(static_cast<unsigned char>(*end)) || (specialChars.find(*end) != std::string::npos)))
    {
        end++;
    }

    if (end == current)
    {
        *parseState = ParseState::ERROR_STATE;
        return false;
    }

    token.assign(current, end - current);
    *cPtr = end;

    return *parseState != ParseState::ERROR_STATE && *parseState != ParseState::DONE;
}

static void DT_Parse(const char* pBuffer)
{
    OPTICK_EVENT();

    ParseState state = ParseState::START;
    const char* currentPos = pBuffer;
    std::string diffuseName;
    std::string detailName;
    std::string token;
    float sScale = 1.0f;
    float tScale = 1.0f;

    try
    {
        while (state != ParseState::DONE && state != ParseState::ERROR_STATE)
        {
            switch (state)
            {
            case ParseState::START:
                state = ParseState::DIFFUSE_NAME;
                break;

            case ParseState::DIFFUSE_NAME:
                if (!DT_GetToken(&currentPos, &state, diffuseName))
                {
                    if (state != ParseState::DONE)
                        state = ParseState::ERROR_STATE;
                    break;
                }
                state = ParseState::DETAIL_TEXTURE;
                break;

            case ParseState::DETAIL_TEXTURE:
                if (!DT_GetToken(&currentPos, &state, detailName))
                {
                    state = ParseState::ERROR_STATE;
                    break;
                }
                state = ParseState::S_SCALE_FACTOR;
                break;

            case ParseState::S_SCALE_FACTOR:
                if (!DT_GetToken(&currentPos, &state, token))
                {
                    state = ParseState::ERROR_STATE;
                    break;
                }
                sScale = std::stof(token);
                state = ParseState::T_SCALE_FACTOR;
                break;

            case ParseState::T_SCALE_FACTOR:
                if (!DT_GetToken(&currentPos, &state, token))
                {
                    state = ParseState::ERROR_STATE;
                    break;
                }
                tScale = std::stof(token);
                state = ParseState::ENTRY;
                break;

            case ParseState::ENTRY:
                {
                    DetailMap newEntry;
                    newEntry._diffuseName = std::move(diffuseName);
                    newEntry._detailName = std::move(detailName);
                    newEntry._sScale = sScale;
                    newEntry._tScale = tScale;

                    g_detailVector.emplace_back(std::move(newEntry));

                    // Reset for next entry
                    diffuseName.clear();
                    detailName.clear();
                    sScale = 1.0f;
                    tScale = 1.0f;
                    state = ParseState::DIFFUSE_NAME;
                    break;
                }

            case ParseState::ERROR_STATE:
                Con_Printf("Error parsing detail texture file.\n");
                state = ParseState::DONE;
                break;

            default:
                state = ParseState::ERROR_STATE;
                break;
            }
        }
    }
    catch (const std::exception& e)
    {
        Con_Printf("Error parsing detail texture: %s\n", e.what());
    }
}

void DT_LoadDetailMapFile(const char* level_name)
{
    OPTICK_EVENT();

    if (!detTexSupported)
    {
        return;
    }

    if (r_detailtextures.value == 0.0f)
    {
        g_detTexLoaded = false;
        g_levelName = level_name;
        return;
    }

    g_detTexLoaded = true;

    if (!g_demandLoad)
    {
        g_decalTexIDs.clear();
    }

    g_detailVector.clear();

    g_idMap.clear();
    g_detailNameToIdMap.clear();
    g_detailLoadFailedMap.clear();
    g_diffuseNameToDetailMap.clear();

    std::string fileName = std::format("maps/{}_detail.txt", level_name);

    FileHandle_t file = FS_Open(fileName.c_str(), "rb");
    if (file == FILESYSTEM_INVALID_HANDLE)
    {
        Con_Printf("No detail texture mapping file: %s\n", fileName.c_str());
        return;
    }

    int file_size = FS_FileSize(fileName.c_str());

    std::unique_ptr<char[]> buffer(new char[file_size + 1]);

    if (FS_Read(buffer.get(), file_size, file))
    {
        buffer[file_size] = '\0';
        FS_Close(file);
        DT_Parse(buffer.get());
    }
    else
    {
        FS_Close(file);
        Con_Printf("Detail texture mapping file read failed\n");
    }

    for (DetailMap& detail : g_detailVector)
    {
        g_diffuseNameToDetailMap[detail._diffuseName] = &detail;
    }
}

void DT_Initialize()
{
    OPTICK_EVENT();

    Cvar_RegisterVariable(&r_detailtextures);
    Cvar_RegisterVariable(&r_detailtexturessupported);
    qglSelectTextureSGIS(TEXTURE2_SGIS);
    qglEnable(GL_TEXTURE_2D);
    qglTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE);
    qglTexEnvf(GL_TEXTURE_ENV, GL_COMBINE_RGB, GL_MODULATE);
    qglTexEnvf(GL_TEXTURE_ENV, GL_RGB_SCALE, 2.0);
    qglDisable(GL_TEXTURE_2D);
    qglSelectTextureSGIS(TEXTURE0_SGIS);

    detTexSupported = true;
}

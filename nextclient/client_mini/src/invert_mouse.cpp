#include "invert_mouse.h"
#include "main.h"
#include <algorithm>
#include <ncl_math/vec3.h>
#include <data_types/MessageReader.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cvar.h>
#include <goldsrc/usercmd.h>

namespace
{
    constexpr int INVERT_MOUSE_PITCH = 1 << 0;
    constexpr int INVERT_MOUSE_YAW = 1 << 1;
    
    int g_InvertMouseFlags;
    bool g_InvertMouseNeedInit;

    float g_LastOriginalPitch;
    float g_LastInvertedPitch;
    float g_LastOriginalYaw;
    float g_LastInvertedYaw;

    goldsrc::cvar_t* cl_pitchup;
    goldsrc::cvar_t* cl_pitchdown;

    int MsgFunc_InvertMouse(const char* name, int size, void* data)
    {
        MessageReader reader(data, size);

        int flags = reader.ReadByte();
        if (reader.is_overflowed())
        {
            return 1;
        }

        int prev_flags = g_InvertMouseFlags;
        g_InvertMouseFlags = flags;

        if (flags != 0 && prev_flags != flags)
        {
            g_InvertMouseNeedInit = true;
        }

        return 1;
    }
} // namespace

void InvertMouseInit()
{
    cl_pitchup = gEngfuncs.pfnGetCvarPointer("cl_pitchup");
    cl_pitchdown = gEngfuncs.pfnGetCvarPointer("cl_pitchdown");
    gEngfuncs.pfnHookUserMsg("InvertMouse", MsgFunc_InvertMouse);
}

void ResetInvertMouse()
{
    g_InvertMouseFlags = 0;
    g_InvertMouseNeedInit = false;
    g_LastOriginalPitch = 0.0f;
    g_LastInvertedPitch = 0.0f;
    g_LastOriginalYaw = 0.0f;
    g_LastInvertedYaw = 0.0f;
}

void CL_CreateMove_InvertMousePre(float frametime, goldsrc::usercmd_t* cmd, int active)
{
    if (!active)
    {
        return;
    }

    if (g_InvertMouseNeedInit || g_InvertMouseFlags == 0)
    {
        float current_angles[3];
        gEngfuncs.GetViewAngles(current_angles);

        g_LastOriginalPitch = current_angles[0];
        g_LastInvertedPitch = current_angles[0];
        g_LastOriginalYaw = current_angles[1];
        g_LastInvertedYaw = current_angles[1];
        g_InvertMouseNeedInit = false;

        if (g_InvertMouseFlags == 0)
        {
            return;
        }
    }

    float angles[3];
    gEngfuncs.GetViewAngles(angles);

    if (g_InvertMouseFlags & INVERT_MOUSE_PITCH)
    {
        angles[0] = g_LastOriginalPitch;
    }

    if (g_InvertMouseFlags & INVERT_MOUSE_YAW)
    {
        angles[1] = g_LastOriginalYaw;
    }

    gEngfuncs.SetViewAngles(angles);
}

void CL_CreateMove_InvertMousePost(float frametime, goldsrc::usercmd_t* cmd, int active)
{
    if (!active)
    {
        return;
    }

    if (g_InvertMouseFlags == 0)
    {
        return;
    }

    float current_angles[3];
    gEngfuncs.GetViewAngles(current_angles);

    if (g_InvertMouseFlags & INVERT_MOUSE_PITCH)
    {
        float delta_pitch = current_angles[0] - g_LastOriginalPitch;
        g_LastOriginalPitch = current_angles[0];

        float max_pitch_up = cl_pitchup ? cl_pitchup->value : 89.0f;
        float max_pitch_down = cl_pitchdown ? cl_pitchdown->value : 89.0f;
        float inverted_pitch = std::clamp(g_LastInvertedPitch - delta_pitch, -max_pitch_up, max_pitch_down);
        g_LastInvertedPitch = inverted_pitch;

        current_angles[0] = inverted_pitch;
        cmd->viewangles[0] = inverted_pitch;
    }

    if (g_InvertMouseFlags & INVERT_MOUSE_YAW)
    {
        float delta_yaw = current_angles[1] - g_LastOriginalYaw;
        g_LastOriginalYaw = current_angles[1];

        float inverted_yaw = ncl_math::AngleNormalize(g_LastInvertedYaw - delta_yaw);
        g_LastInvertedYaw = inverted_yaw;

        current_angles[1] = inverted_yaw;
        cmd->viewangles[1] = inverted_yaw;
    }

    gEngfuncs.SetViewAngles(current_angles);
}

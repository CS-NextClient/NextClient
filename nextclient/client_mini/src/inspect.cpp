#include "hlsdk.h"
#include <event_api.h>
#include <studio.h>
#include <weapontype.h>
#include <cbase.h>
#include <weapons.h>
#include "main.h"
#include "utils.h"

static float inspectEndTime;
static float nextInspectTime;
static float oldViewAnimTime;
static int oldViewSequence;
static int g_LastSetInspectAnimation;

static void SetInspectTime(studiohdr_t *hdr, int seq)
{
    float duration;
    float inspectStartTime;
    mstudioseqdesc_t *desc;

    desc = (mstudioseqdesc_t *)((byte *)hdr + hdr->seqindex) + seq;

    duration = (float)desc->numframes / desc->fps;

    inspectStartTime = gEngfuncs.GetClientTime();

    inspectEndTime = inspectStartTime + duration;
    nextInspectTime = std::min(inspectStartTime + 0.5f, inspectEndTime);
}

/* temp, makes this easier to look at */
#define INSPECT_NO 0

static int inspectAnims[] =
{
        INSPECT_NO, /* none */
        7, /* p228 */
        13, /* glock */
        5, /* scout */
        INSPECT_NO, /* hegrenade */
        7, /* xm1014 */
        INSPECT_NO, /* c4 */
        6, /* mac10 */
        6, /* aug */
        INSPECT_NO, /* smokegrenade */
        16, /* elite */
        6, /* fiveseven */
        6, /* ump45 */
        5, /* sg550 */
        6, /* galil */
        6, /* famas */
        16, /* usp */
        13, /* glock18 */
        6, /* awp */
        6, /* mp5n */
        5, /* m249 */
        7, /* m3 */
        14, /* m4a1 */
        6, /* tmp */
        5, /* g3sg1 */
        INSPECT_NO, /* flashbang */
        6, /* deagle */
        6, /* sg552 */
        6, /* ak47 */
        8, /* knife */
        6, /* p90 */
#if 0 /* shieldgun's weaponid is 99 so shouldn't even have it here */
        INSPECT_NO, /* shieldgun */
#endif
};

static int LookupInspect(int curSequence, int weaponID)
{
    if (weaponID >= sizeof(inspectAnims) / sizeof(int))
        return inspectAnims[0];

    if (weaponID == WEAPON_USP)
    {
        if ((curSequence >= USP_UNSIL_IDLE) &&
            (curSequence != inspectAnims[weaponID]))
            return inspectAnims[weaponID] + 1;
    }
    else if (weaponID == WEAPON_M4A1)
        if ((curSequence >= M4A1_UNSIL_IDLE) &&
            (curSequence != inspectAnims[weaponID]))
            return inspectAnims[weaponID] + 1;

    return inspectAnims[weaponID];
}

static void Inspect_f()
{
    cl_entity_t *vm;
    studiohdr_t *studiohdr;
    int sequence;

    weapon_data_t* weapondata = &g_LastPlayerState.weapondata[g_LastPlayerState.client.m_iId];
    vm = gEngfuncs.GetViewModel();

    if (!vm || !vm->model || !IsDefaultModelPath(vm->model->name))
    {
        gEngfuncs.pfnServerCmd("lookat");
        return;
    }

    if (g_iUser1) /* spectating, don't inspect */
        return;

    /* currentWeapon can be used here because
    this code is never called when spectating */
    if (weapondata->m_fInReload || weapondata->m_fInSpecialReload ||
        weapondata->m_flNextPrimaryAttack > 0 || weapondata->m_flNextSecondaryAttack > 0)
        return;

    if (nextInspectTime > gEngfuncs.GetClientTime())
        return;

    switch (g_CurrentWeaponId)
    {
        /* these weapons have no inspect anims */
        case WEAPON_NONE:
        case WEAPON_HEGRENADE:
        case WEAPON_C4:
        case WEAPON_SMOKEGRENADE:
        case WEAPON_FLASHBANG:
        case WEAPON_SHIELDGUN:
            return;

            /* don't inspect if doing silencer stuff on usp/m4a1 */
        case WEAPON_USP:

            switch (vm->curstate.sequence)
            {
                case USP_ATTACH_SILENCER:
                case USP_DETACH_SILENCER:
                    return;
            }

            break;

        case WEAPON_M4A1:

            switch (vm->curstate.sequence)
            {
                case M4A1_ATTACH_SILENCER:
                case M4A1_DETACH_SILENCER:
                    return;
            }

            break;
    }

    studiohdr = (studiohdr_t *)vm->model->cache.data;
    if (!studiohdr)
        return;

    sequence = LookupInspect(vm->curstate.sequence, g_CurrentWeaponId);

    if (sequence >= studiohdr->numseq)
        return; /* no inspect animation in this model */

    g_LastSetInspectAnimation = sequence;

    SetInspectTime(studiohdr, sequence);

    gEngfuncs.pEventAPI->EV_WeaponAnimation(sequence, vm->curstate.body);
}

static bool IsIdleSequence(int seq)
{
    /* most idle animations have index of 0 */
    if (seq == 0)
        return true;

    /* handle idle animations that aren't 0 */
    switch (g_CurrentWeaponId)
    {
        case WEAPON_ELITE:
            return seq == ELITE_IDLE_LEFTEMPTY;
        case WEAPON_USP:
            return seq == USP_UNSIL_IDLE;
        case WEAPON_GLOCK18:
            return seq == GLOCK18_IDLE2 || seq == GLOCK18_IDLE3;
        case WEAPON_M4A1:
            return seq == M4A1_UNSIL_IDLE;
    }

    return false;
}

void InspectThink()
{
    if (inspectEndTime > gEngfuncs.GetClientTime())
    {
        cl_entity_t *vm = gEngfuncs.GetViewModel();

        if (vm && vm->curstate.animtime != oldViewAnimTime && IsIdleSequence(vm->curstate.sequence))
        {
            /* animation has changed and it's the idle animation, change back to inspect */
            vm->curstate.sequence = oldViewSequence;
            vm->curstate.animtime = oldViewAnimTime;
        }

        if (!vm || vm->curstate.sequence != g_LastSetInspectAnimation)
        {
            nextInspectTime = 0;
        }

        /* update vars */
        oldViewSequence = vm->curstate.sequence;
        oldViewAnimTime = vm->curstate.animtime;
    }
}

void InspectInit()
{
    gEngfuncs.pfnAddCommand("lookat", Inspect_f);
}

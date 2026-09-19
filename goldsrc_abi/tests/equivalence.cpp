#include <cstddef>
#include <cstdint>
#include <string_view>
#include <type_traits>

#include <goldsrc/fwd.h>
#include <goldsrc/client_api.h>
#include <goldsrc/cstrike/game.h>
#include <goldsrc/cstrike/hud.h>
#include <goldsrc/cstrike/hud_elements.h>
#include <goldsrc/cstrike/spectator.h>
#include <goldsrc/cstrike/weapons.h>
#include <goldsrc/cvar.h>
#include <goldsrc/engine/client_state.h>
#include <goldsrc/engine/cmd.h>
#include <goldsrc/engine/event.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/engine/net.h>
#include <goldsrc/engine/resource.h>
#include <goldsrc/engine/server_state.h>
#include <goldsrc/engine/sound.h>
#include <goldsrc/engine/user_msg.h>
#include <goldsrc/engine/video.h>
#include <goldsrc/entity.h>
#include <goldsrc/event_api.h>
#include <goldsrc/format/bsp.h>
#include <goldsrc/format/sprite.h>
#include <goldsrc/format/studio.h>
#include <goldsrc/format/wad.h>
#include <goldsrc/hltv.h>
#include <goldsrc/keys.h>
#include <goldsrc/limits.h>
#include <goldsrc/pmove.h>
#include <goldsrc/protocol.h>
#include <goldsrc/server_api.h>
#include <goldsrc/studio_api.h>
#include <goldsrc/triangle_api.h>
#include <goldsrc/types.h>
#include <goldsrc/usercmd.h>
#include <goldsrc/view.h>
#include <goldsrc/voice_tweak.h>

namespace goldsrc_equivalence
{
    template <class T>
    struct map
    {
        using type = T;
    };
    template <class T>
    using map_t = typename map<T>::type;
    template <class T>
    struct map<T*>
    {
        using type = map_t<T>*;
    };
    template <class T>
    struct map<T&>
    {
        using type = map_t<T>&;
    };
    template <class T>
    struct map<const T>
    {
        using type = const map_t<T>;
    };
    template <class T, std::size_t N>
    struct map<T[N]>
    {
        using type = map_t<T>[N];
    };
    template <class R, class... A>
    struct map<R(__cdecl*)(A...)>
    {
        using type = map_t<R>(__cdecl*)(map_t<A>...);
    };
    template <class R, class... A>
    struct map<R(__cdecl*)(A..., ...)>
    {
        using type = map_t<R>(__cdecl*)(map_t<A>..., ...);
    };

    using r0 = goldsrc::SCREENINFO;
    using r1 = goldsrc::client_data_t;
    using r2 = goldsrc::hud_player_info_t;
    using r3 = goldsrc::cldll_func_t;
    using r4 = goldsrc::modfuncs_t;
    using r5 = goldsrc::cl_enginefunc_dst_t;
    using r6 = goldsrc::cl_enginefunc_t;
    using r7 = goldsrc::cstrike::CHud;
    using r8 = goldsrc::cstrike::CHudBase;
    using r9 = goldsrc::cstrike::CHudBase::POSITION;
    using r10 = goldsrc::cstrike::extra_player_info_t;
    using r11 = goldsrc::cstrike::CHudAmmo;
    using r12 = goldsrc::cstrike::CHudAmmo::RGBA;
    using r13 = goldsrc::cstrike::CHudAmmoSecondary;
    using r14 = goldsrc::cstrike::CHudMessage;
    using r15 = goldsrc::cstrike::CHudMessage::client_message_t;
    using r16 = goldsrc::cstrike::CHudNightVision;
    using r17 = goldsrc::cstrike::CHudRoundTimer;
    using r18 = goldsrc::cstrike::CHudStatusBar;
    using r19 = goldsrc::cstrike::CHudStatusIcons;
    using r20 = goldsrc::cstrike::CHudStatusIcons::icon_sprite_t;
    using r21 = goldsrc::cstrike::DAMAGE_IMAGE;
    using r22 = goldsrc::cstrike::CHudHealth;
    using r23 = goldsrc::cvar_t;
    using r24 = goldsrc::cmd_t;
    using r25 = goldsrc::consistency_t;
    using r26 = goldsrc::packet_entities_t;
    using r27 = goldsrc::player_info_t;
    using r28 = goldsrc::soundfade_t;
    using r29 = goldsrc::client_static_t;
    using r30 = goldsrc::frame_t;
    using r31 = goldsrc::client_state_t;
    using r32 = goldsrc::event_info_s;
    using r33 = goldsrc::event_t;
    using r34 = goldsrc::event_state_t;
    using r35 = goldsrc::cacheentry_t;
    using r36 = goldsrc::glpoly_t;
    using r37 = goldsrc::hull_t;
    using r38 = goldsrc::medge_t;
    using r39 = goldsrc::mod_known_info_t;
    using r40 = goldsrc::mplane_t;
    using r41 = goldsrc::mspriteframe_t;
    using r42 = goldsrc::mspriteframedesc_t;
    using r43 = goldsrc::mspritegroup_t;
    using r44 = goldsrc::msurface_t;
    using r45 = goldsrc::mtexinfo_t;
    using r46 = goldsrc::mvertex_t;
    using r47 = goldsrc::skin_t;
    using r48 = goldsrc::texture_t;
    using r49 = goldsrc::cachewad_t;
    using r50 = goldsrc::model_t;
    using r51 = goldsrc::msprite_t;
    using r52 = goldsrc::downloadtime_t;
    using r53 = goldsrc::flowstats_t;
    using r54 = goldsrc::sizebuf_t;
    using r55 = goldsrc::flow_t;
    using r56 = goldsrc::fragbuf_t;
    using r57 = goldsrc::incomingtransfer_t;
    using r58 = goldsrc::netadr_t;
    using r59 = goldsrc::netchan_t;
    using r60 = goldsrc::_resourceinfo_t;
    using r61 = goldsrc::resource_t;
    using r62 = goldsrc::resourceinfo_t;
    using r63 = goldsrc::customization_t;
    using r64 = goldsrc::server_static_t;
    using r65 = goldsrc::server_t;
    using r66 = goldsrc::sfx_t;
    using r67 = goldsrc::EngineMsg;
    using r68 = goldsrc::transObjRef;
    using r69 = goldsrc::viddef_t;
    using r70 = goldsrc::clientdata_t;
    using r71 = goldsrc::entity_state_t;
    using r72 = goldsrc::latchedvars_t;
    using r73 = goldsrc::mouth_t;
    using r74 = goldsrc::position_history_t;
    using r75 = goldsrc::weapon_data_t;
    using r76 = goldsrc::cl_entity_t;
    using r77 = goldsrc::local_state_t;
    using r78 = goldsrc::event_api_s;
    using r79 = goldsrc::event_args_t;
    using r80 = goldsrc::dmodel_t;
    using r81 = goldsrc::dspriteframe_t;
    using r82 = goldsrc::dspritegroup_t;
    using r83 = goldsrc::dspriteinterval_t;
    using r84 = goldsrc::dsprite_t;
    using r85 = goldsrc::dspriteframetype_t;
    using r86 = goldsrc::mstudioanim_t;
    using r87 = goldsrc::mstudioanimvalue_t;
    using r88 = goldsrc::mstudioattachment_t;
    using r89 = goldsrc::mstudiobodyparts_t;
    using r90 = goldsrc::mstudiobone_t;
    using r91 = goldsrc::mstudioevent_s;
    using r92 = goldsrc::mstudioseqdesc_t;
    using r93 = goldsrc::mstudiotexture_t;
    using r94 = goldsrc::studiohdr_t;
    using r95 = goldsrc::lumpinfo_t;
    using r96 = goldsrc::qpic_t;
    using r97 = goldsrc::wadinfo_t;
    using r98 = goldsrc::movevars_s;
    using r99 = goldsrc::physent_t;
    using r100 = goldsrc::pmplane_t;
    using r101 = goldsrc::pmtrace_t;
    using r102 = goldsrc::playermove_t;
    using r103 = goldsrc::enginefuncs_t;
    using r104 = goldsrc::engine_studio_api_t;
    using r105 = goldsrc::r_studio_interface_t;
    using r106 = goldsrc::triangleapi_t;
    using r107 = goldsrc::cache_user_t;
    using r108 = goldsrc::color24;
    using r109 = goldsrc::colorVec;
    using r110 = goldsrc::vrect_t;
    using r111 = goldsrc::wrect_t;
    using r112 = goldsrc::kbutton_t;
    using r113 = goldsrc::usercmd_t;
    using r114 = goldsrc::ref_params_t;
    using r115 = goldsrc::refdef_t;
    using r116 = goldsrc::screenfade_t;
    using r117 = goldsrc::IVoiceTweak;
    using r118 = goldsrc::cstrike::CHudAccountBalance;
    using r119 = goldsrc::cstrike::CHudBattery;
    using r120 = goldsrc::cstrike::CHudCareerTaskTimer;
    using r121 = goldsrc::cstrike::CHudDeathNotice;
    using r122 = goldsrc::cstrike::CHudFlashlight;
    using r123 = goldsrc::cstrike::CHudGeiger;
    using r124 = goldsrc::cstrike::CHudMenu;
    using r125 = goldsrc::cstrike::CHudProgressBar;
    using r126 = goldsrc::cstrike::CHudSayText;
    using r127 = goldsrc::cstrike::CHudScenarioStatus;
    using r128 = goldsrc::cstrike::CHudSniperScope;
    using r129 = goldsrc::cstrike::CHudSpectator;
    using r130 = goldsrc::cstrike::CHudTextMessage;
    using r131 = goldsrc::cstrike::CHudTrain;
    using r132 = goldsrc::cstrike::CHudVGUI2Print;
    using r133 = goldsrc::cstrike::CInterpolation;
    using r134 = goldsrc::cstrike::HUDLIST;
    using r135 = goldsrc::TraceResult;
    using r136 = goldsrc::cstrike::WEAPON;
    using r137 = goldsrc::cstrike::cameraWayPoint_t;
    using r138 = goldsrc::cldll_func_dst_t;
    using r139 = goldsrc::client_s;
    using r140 = goldsrc::client_sprite_t;
    using r141 = goldsrc::client_textmessage_t;
    using r142 = goldsrc::cmdalias_t;
    using r143 = goldsrc::dclipnode_t;
    using r144 = goldsrc::dlight_t;
    using r145 = goldsrc::edict_t;
    using r146 = goldsrc::efrag_t;
    using r147 = goldsrc::efx_api_s;
    using r148 = goldsrc::engdata_t;
    using r149 = goldsrc::entvars_s;
    using r150 = goldsrc::fragbufwaiting_t;
    using r151 = goldsrc::cstrike::hostage_info_t;
    using r152 = goldsrc::cstrike::message_parms_t;
    using r153 = goldsrc::mleaf_s;
    using r154 = goldsrc::mnode_t;
    using r155 = goldsrc::modchelpers_t;
    using r156 = goldsrc::modshelpers_t;
    using r157 = goldsrc::UserMsg;
    using r158 = goldsrc::VoiceRecordInterface;
    using r159 = goldsrc::cstrike::overviewEntity_t;
    using r160 = goldsrc::cstrike::overviewInfo_t;
    using r161 = goldsrc::sentenceEntry_s;
    using r162 = goldsrc::sequenceEntry_s;
    using r163 = goldsrc::TEMPENTITY;
    using r164 = goldsrc::trace_t;
    using r165 = goldsrc::cstrike::WeaponIdType;
    using r166 = goldsrc::cstrike::WeaponState;
    using r167 = goldsrc::cstrike::elite_e;
    using r168 = goldsrc::cstrike::glock18_e;
    using r169 = goldsrc::cstrike::m4a1_e;
    using r170 = goldsrc::cstrike::usp_e;
    using r171 = goldsrc::CareerStateType;
    using r172 = goldsrc::cactive_t;
    using r173 = goldsrc::cmd_source_t;
    using r174 = goldsrc::modtype_t;
    using r175 = goldsrc::clc_commands_e;
    using r176 = goldsrc::netadrtype_t;
    using r177 = goldsrc::netsrc_t;
    using r178 = goldsrc::resourcetype_t;
    using r179 = goldsrc::VidTypes;
    using r180 = goldsrc::spriteframetype_t;
    using r181 = goldsrc::synctype_t;
    using r182 = goldsrc::keydest_t;
    using r183 = goldsrc::ALERT_TYPE;
    using r184 = goldsrc::FORCE_TYPE;
    using r185 = goldsrc::PRINT_TYPE;
    using r186 = goldsrc::TRICULLSTYLE;
    using r187 = goldsrc::VoiceTweakControl;
    using r188 = ::tagPOINT;
    using r189 = goldsrc::alight_t;
    using r190 = goldsrc::channel_t;
    using r191 = goldsrc::con_nprint_t;
    using r192 = goldsrc::delta_t;
    using r193 = goldsrc::demo_api_t;
    using r194 = goldsrc::dma_t;
    using r195 = goldsrc::net_api_t;
    using r196 = goldsrc::sfxcache_t;
    using r197 = goldsrc::wavstream_t;
    constexpr std::size_t s0 = sizeof(goldsrc::SCREENINFO);
    constexpr std::size_t o0 = offsetof(goldsrc::SCREENINFO, iSize);
    using t0 = decltype(goldsrc::SCREENINFO::iSize);
    constexpr std::size_t o1 = offsetof(goldsrc::SCREENINFO, iWidth);
    using t1 = decltype(goldsrc::SCREENINFO::iWidth);
    constexpr std::size_t o2 = offsetof(goldsrc::SCREENINFO, iHeight);
    using t2 = decltype(goldsrc::SCREENINFO::iHeight);
    constexpr std::size_t o3 = offsetof(goldsrc::SCREENINFO, iFlags);
    using t3 = decltype(goldsrc::SCREENINFO::iFlags);
    constexpr std::size_t o4 = offsetof(goldsrc::SCREENINFO, iCharHeight);
    using t4 = decltype(goldsrc::SCREENINFO::iCharHeight);
    constexpr std::size_t o5 = offsetof(goldsrc::SCREENINFO, charWidths);
    using t5 = decltype(goldsrc::SCREENINFO::charWidths);
    constexpr std::size_t s1 = sizeof(goldsrc::client_data_t);
    constexpr std::size_t o6 = offsetof(goldsrc::client_data_t, origin);
    using t6 = decltype(goldsrc::client_data_t::origin);
    constexpr std::size_t o7 = offsetof(goldsrc::client_data_t, viewangles);
    using t7 = decltype(goldsrc::client_data_t::viewangles);
    constexpr std::size_t o8 = offsetof(goldsrc::client_data_t, iWeaponBits);
    using t8 = decltype(goldsrc::client_data_t::iWeaponBits);
    constexpr std::size_t o9 = offsetof(goldsrc::client_data_t, fov);
    using t9 = decltype(goldsrc::client_data_t::fov);
    constexpr std::size_t s2 = sizeof(goldsrc::hud_player_info_t);
    constexpr std::size_t o10 = offsetof(goldsrc::hud_player_info_t, name);
    using t10 = decltype(goldsrc::hud_player_info_t::name);
    constexpr std::size_t o11 = offsetof(goldsrc::hud_player_info_t, ping);
    using t11 = decltype(goldsrc::hud_player_info_t::ping);
    constexpr std::size_t o12 = offsetof(goldsrc::hud_player_info_t, thisplayer);
    using t12 = decltype(goldsrc::hud_player_info_t::thisplayer);
    constexpr std::size_t o13 = offsetof(goldsrc::hud_player_info_t, spectator);
    using t13 = decltype(goldsrc::hud_player_info_t::spectator);
    constexpr std::size_t o14 = offsetof(goldsrc::hud_player_info_t, packetloss);
    using t14 = decltype(goldsrc::hud_player_info_t::packetloss);
    constexpr std::size_t o15 = offsetof(goldsrc::hud_player_info_t, model);
    using t15 = decltype(goldsrc::hud_player_info_t::model);
    constexpr std::size_t o16 = offsetof(goldsrc::hud_player_info_t, topcolor);
    using t16 = decltype(goldsrc::hud_player_info_t::topcolor);
    constexpr std::size_t o17 = offsetof(goldsrc::hud_player_info_t, bottomcolor);
    using t17 = decltype(goldsrc::hud_player_info_t::bottomcolor);
    constexpr std::size_t o18 = offsetof(goldsrc::hud_player_info_t, m_nSteamID);
    using t18 = decltype(goldsrc::hud_player_info_t::m_nSteamID);
    constexpr std::size_t s3 = sizeof(goldsrc::cldll_func_t);
    constexpr std::size_t o19 = offsetof(goldsrc::cldll_func_t, pInitFunc);
    using t19 = decltype(goldsrc::cldll_func_t::pInitFunc);
    constexpr std::size_t o20 = offsetof(goldsrc::cldll_func_t, pHudInitFunc);
    using t20 = decltype(goldsrc::cldll_func_t::pHudInitFunc);
    constexpr std::size_t o21 = offsetof(goldsrc::cldll_func_t, pHudVidInitFunc);
    using t21 = decltype(goldsrc::cldll_func_t::pHudVidInitFunc);
    constexpr std::size_t o22 = offsetof(goldsrc::cldll_func_t, pHudRedrawFunc);
    using t22 = decltype(goldsrc::cldll_func_t::pHudRedrawFunc);
    constexpr std::size_t o23 = offsetof(goldsrc::cldll_func_t, pHudUpdateClientDataFunc);
    using t23 = decltype(goldsrc::cldll_func_t::pHudUpdateClientDataFunc);
    constexpr std::size_t o24 = offsetof(goldsrc::cldll_func_t, pHudResetFunc);
    using t24 = decltype(goldsrc::cldll_func_t::pHudResetFunc);
    constexpr std::size_t o25 = offsetof(goldsrc::cldll_func_t, pClientMove);
    using t25 = decltype(goldsrc::cldll_func_t::pClientMove);
    constexpr std::size_t o26 = offsetof(goldsrc::cldll_func_t, pClientMoveInit);
    using t26 = decltype(goldsrc::cldll_func_t::pClientMoveInit);
    constexpr std::size_t o27 = offsetof(goldsrc::cldll_func_t, pClientTextureType);
    using t27 = decltype(goldsrc::cldll_func_t::pClientTextureType);
    constexpr std::size_t o28 = offsetof(goldsrc::cldll_func_t, pIN_ActivateMouse);
    using t28 = decltype(goldsrc::cldll_func_t::pIN_ActivateMouse);
    constexpr std::size_t o29 = offsetof(goldsrc::cldll_func_t, pIN_DeactivateMouse);
    using t29 = decltype(goldsrc::cldll_func_t::pIN_DeactivateMouse);
    constexpr std::size_t o30 = offsetof(goldsrc::cldll_func_t, pIN_MouseEvent);
    using t30 = decltype(goldsrc::cldll_func_t::pIN_MouseEvent);
    constexpr std::size_t o31 = offsetof(goldsrc::cldll_func_t, pIN_ClearStates);
    using t31 = decltype(goldsrc::cldll_func_t::pIN_ClearStates);
    constexpr std::size_t o32 = offsetof(goldsrc::cldll_func_t, pIN_Accumulate);
    using t32 = decltype(goldsrc::cldll_func_t::pIN_Accumulate);
    constexpr std::size_t o33 = offsetof(goldsrc::cldll_func_t, pCL_CreateMove);
    using t33 = decltype(goldsrc::cldll_func_t::pCL_CreateMove);
    constexpr std::size_t o34 = offsetof(goldsrc::cldll_func_t, pCL_IsThirdPerson);
    using t34 = decltype(goldsrc::cldll_func_t::pCL_IsThirdPerson);
    constexpr std::size_t o35 = offsetof(goldsrc::cldll_func_t, pCL_GetCameraOffsets);
    using t35 = decltype(goldsrc::cldll_func_t::pCL_GetCameraOffsets);
    constexpr std::size_t o36 = offsetof(goldsrc::cldll_func_t, pFindKey);
    using t36 = decltype(goldsrc::cldll_func_t::pFindKey);
    constexpr std::size_t o37 = offsetof(goldsrc::cldll_func_t, pCamThink);
    using t37 = decltype(goldsrc::cldll_func_t::pCamThink);
    constexpr std::size_t o38 = offsetof(goldsrc::cldll_func_t, pCalcRefdef);
    using t38 = decltype(goldsrc::cldll_func_t::pCalcRefdef);
    constexpr std::size_t o39 = offsetof(goldsrc::cldll_func_t, pAddEntity);
    using t39 = decltype(goldsrc::cldll_func_t::pAddEntity);
    constexpr std::size_t o40 = offsetof(goldsrc::cldll_func_t, pCreateEntities);
    using t40 = decltype(goldsrc::cldll_func_t::pCreateEntities);
    constexpr std::size_t o41 = offsetof(goldsrc::cldll_func_t, pDrawNormalTriangles);
    using t41 = decltype(goldsrc::cldll_func_t::pDrawNormalTriangles);
    constexpr std::size_t o42 = offsetof(goldsrc::cldll_func_t, pDrawTransparentTriangles);
    using t42 = decltype(goldsrc::cldll_func_t::pDrawTransparentTriangles);
    constexpr std::size_t o43 = offsetof(goldsrc::cldll_func_t, pStudioEvent);
    using t43 = decltype(goldsrc::cldll_func_t::pStudioEvent);
    constexpr std::size_t o44 = offsetof(goldsrc::cldll_func_t, pPostRunCmd);
    using t44 = decltype(goldsrc::cldll_func_t::pPostRunCmd);
    constexpr std::size_t o45 = offsetof(goldsrc::cldll_func_t, pShutdown);
    using t45 = decltype(goldsrc::cldll_func_t::pShutdown);
    constexpr std::size_t o46 = offsetof(goldsrc::cldll_func_t, pTxferLocalOverrides);
    using t46 = decltype(goldsrc::cldll_func_t::pTxferLocalOverrides);
    constexpr std::size_t o47 = offsetof(goldsrc::cldll_func_t, pProcessPlayerState);
    using t47 = decltype(goldsrc::cldll_func_t::pProcessPlayerState);
    constexpr std::size_t o48 = offsetof(goldsrc::cldll_func_t, pTxferPredictionData);
    using t48 = decltype(goldsrc::cldll_func_t::pTxferPredictionData);
    constexpr std::size_t o49 = offsetof(goldsrc::cldll_func_t, pReadDemoBuffer);
    using t49 = decltype(goldsrc::cldll_func_t::pReadDemoBuffer);
    constexpr std::size_t o50 = offsetof(goldsrc::cldll_func_t, pConnectionlessPacket);
    using t50 = decltype(goldsrc::cldll_func_t::pConnectionlessPacket);
    constexpr std::size_t o51 = offsetof(goldsrc::cldll_func_t, pGetHullBounds);
    using t51 = decltype(goldsrc::cldll_func_t::pGetHullBounds);
    constexpr std::size_t o52 = offsetof(goldsrc::cldll_func_t, pHudFrame);
    using t52 = decltype(goldsrc::cldll_func_t::pHudFrame);
    constexpr std::size_t o53 = offsetof(goldsrc::cldll_func_t, pKeyEvent);
    using t53 = decltype(goldsrc::cldll_func_t::pKeyEvent);
    constexpr std::size_t o54 = offsetof(goldsrc::cldll_func_t, pTempEntUpdate);
    using t54 = decltype(goldsrc::cldll_func_t::pTempEntUpdate);
    constexpr std::size_t o55 = offsetof(goldsrc::cldll_func_t, pGetUserEntity);
    using t55 = decltype(goldsrc::cldll_func_t::pGetUserEntity);
    constexpr std::size_t o56 = offsetof(goldsrc::cldll_func_t, pVoiceStatus);
    using t56 = decltype(goldsrc::cldll_func_t::pVoiceStatus);
    constexpr std::size_t o57 = offsetof(goldsrc::cldll_func_t, pDirectorMessage);
    using t57 = decltype(goldsrc::cldll_func_t::pDirectorMessage);
    constexpr std::size_t o58 = offsetof(goldsrc::cldll_func_t, pStudioInterface);
    using t58 = decltype(goldsrc::cldll_func_t::pStudioInterface);
    constexpr std::size_t o59 = offsetof(goldsrc::cldll_func_t, pChatInputPosition);
    using t59 = decltype(goldsrc::cldll_func_t::pChatInputPosition);
    constexpr std::size_t o60 = offsetof(goldsrc::cldll_func_t, pGetPlayerTeam);
    using t60 = decltype(goldsrc::cldll_func_t::pGetPlayerTeam);
    constexpr std::size_t o61 = offsetof(goldsrc::cldll_func_t, pClientFactory);
    using t61 = decltype(goldsrc::cldll_func_t::pClientFactory);
    constexpr std::size_t s4 = sizeof(goldsrc::modfuncs_t);
    constexpr std::size_t o62 = offsetof(goldsrc::modfuncs_t, m_pfnLoadMod);
    using t62 = decltype(goldsrc::modfuncs_t::m_pfnLoadMod);
    constexpr std::size_t o63 = offsetof(goldsrc::modfuncs_t, m_pfnCloseMod);
    using t63 = decltype(goldsrc::modfuncs_t::m_pfnCloseMod);
    constexpr std::size_t o64 = offsetof(goldsrc::modfuncs_t, m_pfnNCall);
    using t64 = decltype(goldsrc::modfuncs_t::m_pfnNCall);
    constexpr std::size_t o65 = offsetof(goldsrc::modfuncs_t, m_pfnGetClDstAddrs);
    using t65 = decltype(goldsrc::modfuncs_t::m_pfnGetClDstAddrs);
    constexpr std::size_t o66 = offsetof(goldsrc::modfuncs_t, m_pfnGetEngDstAddrs);
    using t66 = decltype(goldsrc::modfuncs_t::m_pfnGetEngDstAddrs);
    constexpr std::size_t o67 = offsetof(goldsrc::modfuncs_t, m_pfnModuleLoaded);
    using t67 = decltype(goldsrc::modfuncs_t::m_pfnModuleLoaded);
    constexpr std::size_t o68 = offsetof(goldsrc::modfuncs_t, m_pfnProcessOutgoingNet);
    using t68 = decltype(goldsrc::modfuncs_t::m_pfnProcessOutgoingNet);
    constexpr std::size_t o69 = offsetof(goldsrc::modfuncs_t, m_pfnProcessIncomingNet);
    using t69 = decltype(goldsrc::modfuncs_t::m_pfnProcessIncomingNet);
    constexpr std::size_t o70 = offsetof(goldsrc::modfuncs_t, m_pfnTextureLoad);
    using t70 = decltype(goldsrc::modfuncs_t::m_pfnTextureLoad);
    constexpr std::size_t o71 = offsetof(goldsrc::modfuncs_t, m_pfnModelLoad);
    using t71 = decltype(goldsrc::modfuncs_t::m_pfnModelLoad);
    constexpr std::size_t o72 = offsetof(goldsrc::modfuncs_t, m_pfnFrameBegin);
    using t72 = decltype(goldsrc::modfuncs_t::m_pfnFrameBegin);
    constexpr std::size_t o73 = offsetof(goldsrc::modfuncs_t, m_pfnFrameRender1);
    using t73 = decltype(goldsrc::modfuncs_t::m_pfnFrameRender1);
    constexpr std::size_t o74 = offsetof(goldsrc::modfuncs_t, m_pfnFrameRender2);
    using t74 = decltype(goldsrc::modfuncs_t::m_pfnFrameRender2);
    constexpr std::size_t o75 = offsetof(goldsrc::modfuncs_t, m_pfnSetModSHelpers);
    using t75 = decltype(goldsrc::modfuncs_t::m_pfnSetModSHelpers);
    constexpr std::size_t o76 = offsetof(goldsrc::modfuncs_t, m_pfnSetModCHelpers);
    using t76 = decltype(goldsrc::modfuncs_t::m_pfnSetModCHelpers);
    constexpr std::size_t o77 = offsetof(goldsrc::modfuncs_t, m_pfnSetEngData);
    using t77 = decltype(goldsrc::modfuncs_t::m_pfnSetEngData);
    constexpr std::size_t o78 = offsetof(goldsrc::modfuncs_t, m_nVersion);
    using t78 = decltype(goldsrc::modfuncs_t::m_nVersion);
    constexpr std::size_t o79 = offsetof(goldsrc::modfuncs_t, m_pfnConnectClient);
    using t79 = decltype(goldsrc::modfuncs_t::m_pfnConnectClient);
    constexpr std::size_t o80 = offsetof(goldsrc::modfuncs_t, m_pfnRecordIP);
    using t80 = decltype(goldsrc::modfuncs_t::m_pfnRecordIP);
    constexpr std::size_t o81 = offsetof(goldsrc::modfuncs_t, m_pfnPlayerStatus);
    using t81 = decltype(goldsrc::modfuncs_t::m_pfnPlayerStatus);
    constexpr std::size_t o82 = offsetof(goldsrc::modfuncs_t, m_pfnSetEngineVersion);
    using t82 = decltype(goldsrc::modfuncs_t::m_pfnSetEngineVersion);
    constexpr std::size_t o83 = offsetof(goldsrc::modfuncs_t, m_nVoid2);
    using t83 = decltype(goldsrc::modfuncs_t::m_nVoid2);
    constexpr std::size_t o84 = offsetof(goldsrc::modfuncs_t, m_nVoid3);
    using t84 = decltype(goldsrc::modfuncs_t::m_nVoid3);
    constexpr std::size_t o85 = offsetof(goldsrc::modfuncs_t, m_nVoid4);
    using t85 = decltype(goldsrc::modfuncs_t::m_nVoid4);
    constexpr std::size_t o86 = offsetof(goldsrc::modfuncs_t, m_nVoid5);
    using t86 = decltype(goldsrc::modfuncs_t::m_nVoid5);
    constexpr std::size_t o87 = offsetof(goldsrc::modfuncs_t, m_nVoid6);
    using t87 = decltype(goldsrc::modfuncs_t::m_nVoid6);
    constexpr std::size_t o88 = offsetof(goldsrc::modfuncs_t, m_nVoid7);
    using t88 = decltype(goldsrc::modfuncs_t::m_nVoid7);
    constexpr std::size_t o89 = offsetof(goldsrc::modfuncs_t, m_nVoid8);
    using t89 = decltype(goldsrc::modfuncs_t::m_nVoid8);
    constexpr std::size_t o90 = offsetof(goldsrc::modfuncs_t, m_nVoid9);
    using t90 = decltype(goldsrc::modfuncs_t::m_nVoid9);
    constexpr std::size_t s5 = sizeof(goldsrc::cl_enginefunc_dst_t);
    constexpr std::size_t o91 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Load);
    using t91 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Load);
    constexpr std::size_t o92 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Frames);
    using t92 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Frames);
    constexpr std::size_t o93 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Height);
    using t93 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Height);
    constexpr std::size_t o94 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Width);
    using t94 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Width);
    constexpr std::size_t o95 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Set);
    using t95 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Set);
    constexpr std::size_t o96 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_Draw);
    using t96 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_Draw);
    constexpr std::size_t o97 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_DrawHoles);
    using t97 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_DrawHoles);
    constexpr std::size_t o98 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_DrawAdditive);
    using t98 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_DrawAdditive);
    constexpr std::size_t o99 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_EnableScissor);
    using t99 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_EnableScissor);
    constexpr std::size_t o100 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_DisableScissor);
    using t100 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_DisableScissor);
    constexpr std::size_t o101 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_GetList);
    using t101 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_GetList);
    constexpr std::size_t o102 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnFillRGBA);
    using t102 = decltype(goldsrc::cl_enginefunc_dst_t::pfnFillRGBA);
    constexpr std::size_t o103 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetScreenInfo);
    using t103 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetScreenInfo);
    constexpr std::size_t o104 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetCrosshair);
    using t104 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetCrosshair);
    constexpr std::size_t o105 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnRegisterVariable);
    using t105 = decltype(goldsrc::cl_enginefunc_dst_t::pfnRegisterVariable);
    constexpr std::size_t o106 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetCvarFloat);
    using t106 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetCvarFloat);
    constexpr std::size_t o107 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetCvarString);
    using t107 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetCvarString);
    constexpr std::size_t o108 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnAddCommand);
    using t108 = decltype(goldsrc::cl_enginefunc_dst_t::pfnAddCommand);
    constexpr std::size_t o109 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnHookUserMsg);
    using t109 = decltype(goldsrc::cl_enginefunc_dst_t::pfnHookUserMsg);
    constexpr std::size_t o110 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnServerCmd);
    using t110 = decltype(goldsrc::cl_enginefunc_dst_t::pfnServerCmd);
    constexpr std::size_t o111 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnClientCmd);
    using t111 = decltype(goldsrc::cl_enginefunc_dst_t::pfnClientCmd);
    constexpr std::size_t o112 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetPlayerInfo);
    using t112 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetPlayerInfo);
    constexpr std::size_t o113 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaySoundByName);
    using t113 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaySoundByName);
    constexpr std::size_t o114 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaySoundByIndex);
    using t114 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaySoundByIndex);
    constexpr std::size_t o115 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnAngleVectors);
    using t115 = decltype(goldsrc::cl_enginefunc_dst_t::pfnAngleVectors);
    constexpr std::size_t o116 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnTextMessageGet);
    using t116 = decltype(goldsrc::cl_enginefunc_dst_t::pfnTextMessageGet);
    constexpr std::size_t o117 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawCharacter);
    using t117 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawCharacter);
    constexpr std::size_t o118 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawConsoleString);
    using t118 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawConsoleString);
    constexpr std::size_t o119 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawSetTextColor);
    using t119 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawSetTextColor);
    constexpr std::size_t o120 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawConsoleStringLen);
    using t120 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawConsoleStringLen);
    constexpr std::size_t o121 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnConsolePrint);
    using t121 = decltype(goldsrc::cl_enginefunc_dst_t::pfnConsolePrint);
    constexpr std::size_t o122 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnCenterPrint);
    using t122 = decltype(goldsrc::cl_enginefunc_dst_t::pfnCenterPrint);
    constexpr std::size_t o123 = offsetof(goldsrc::cl_enginefunc_dst_t, GetWindowCenterX);
    using t123 = decltype(goldsrc::cl_enginefunc_dst_t::GetWindowCenterX);
    constexpr std::size_t o124 = offsetof(goldsrc::cl_enginefunc_dst_t, GetWindowCenterY);
    using t124 = decltype(goldsrc::cl_enginefunc_dst_t::GetWindowCenterY);
    constexpr std::size_t o125 = offsetof(goldsrc::cl_enginefunc_dst_t, GetViewAngles);
    using t125 = decltype(goldsrc::cl_enginefunc_dst_t::GetViewAngles);
    constexpr std::size_t o126 = offsetof(goldsrc::cl_enginefunc_dst_t, SetViewAngles);
    using t126 = decltype(goldsrc::cl_enginefunc_dst_t::SetViewAngles);
    constexpr std::size_t o127 = offsetof(goldsrc::cl_enginefunc_dst_t, GetMaxClients);
    using t127 = decltype(goldsrc::cl_enginefunc_dst_t::GetMaxClients);
    constexpr std::size_t o128 = offsetof(goldsrc::cl_enginefunc_dst_t, Cvar_SetValue);
    using t128 = decltype(goldsrc::cl_enginefunc_dst_t::Cvar_SetValue);
    constexpr std::size_t o129 = offsetof(goldsrc::cl_enginefunc_dst_t, Cmd_Argc);
    using t129 = decltype(goldsrc::cl_enginefunc_dst_t::Cmd_Argc);
    constexpr std::size_t o130 = offsetof(goldsrc::cl_enginefunc_dst_t, Cmd_Argv);
    using t130 = decltype(goldsrc::cl_enginefunc_dst_t::Cmd_Argv);
    constexpr std::size_t o131 = offsetof(goldsrc::cl_enginefunc_dst_t, Con_Printf);
    using t131 = decltype(goldsrc::cl_enginefunc_dst_t::Con_Printf);
    constexpr std::size_t o132 = offsetof(goldsrc::cl_enginefunc_dst_t, Con_DPrintf);
    using t132 = decltype(goldsrc::cl_enginefunc_dst_t::Con_DPrintf);
    constexpr std::size_t o133 = offsetof(goldsrc::cl_enginefunc_dst_t, Con_NPrintf);
    using t133 = decltype(goldsrc::cl_enginefunc_dst_t::Con_NPrintf);
    constexpr std::size_t o134 = offsetof(goldsrc::cl_enginefunc_dst_t, Con_NXPrintf);
    using t134 = decltype(goldsrc::cl_enginefunc_dst_t::Con_NXPrintf);
    constexpr std::size_t o135 = offsetof(goldsrc::cl_enginefunc_dst_t, PhysInfo_ValueForKey);
    using t135 = decltype(goldsrc::cl_enginefunc_dst_t::PhysInfo_ValueForKey);
    constexpr std::size_t o136 = offsetof(goldsrc::cl_enginefunc_dst_t, ServerInfo_ValueForKey);
    using t136 = decltype(goldsrc::cl_enginefunc_dst_t::ServerInfo_ValueForKey);
    constexpr std::size_t o137 = offsetof(goldsrc::cl_enginefunc_dst_t, GetClientMaxspeed);
    using t137 = decltype(goldsrc::cl_enginefunc_dst_t::GetClientMaxspeed);
    constexpr std::size_t o138 = offsetof(goldsrc::cl_enginefunc_dst_t, CheckParm);
    using t138 = decltype(goldsrc::cl_enginefunc_dst_t::CheckParm);
    constexpr std::size_t o139 = offsetof(goldsrc::cl_enginefunc_dst_t, Key_Event);
    using t139 = decltype(goldsrc::cl_enginefunc_dst_t::Key_Event);
    constexpr std::size_t o140 = offsetof(goldsrc::cl_enginefunc_dst_t, GetMousePosition);
    using t140 = decltype(goldsrc::cl_enginefunc_dst_t::GetMousePosition);
    constexpr std::size_t o141 = offsetof(goldsrc::cl_enginefunc_dst_t, IsNoClipping);
    using t141 = decltype(goldsrc::cl_enginefunc_dst_t::IsNoClipping);
    constexpr std::size_t o142 = offsetof(goldsrc::cl_enginefunc_dst_t, GetLocalPlayer);
    using t142 = decltype(goldsrc::cl_enginefunc_dst_t::GetLocalPlayer);
    constexpr std::size_t o143 = offsetof(goldsrc::cl_enginefunc_dst_t, GetViewModel);
    using t143 = decltype(goldsrc::cl_enginefunc_dst_t::GetViewModel);
    constexpr std::size_t o144 = offsetof(goldsrc::cl_enginefunc_dst_t, GetEntityByIndex);
    using t144 = decltype(goldsrc::cl_enginefunc_dst_t::GetEntityByIndex);
    constexpr std::size_t o145 = offsetof(goldsrc::cl_enginefunc_dst_t, GetClientTime);
    using t145 = decltype(goldsrc::cl_enginefunc_dst_t::GetClientTime);
    constexpr std::size_t o146 = offsetof(goldsrc::cl_enginefunc_dst_t, V_CalcShake);
    using t146 = decltype(goldsrc::cl_enginefunc_dst_t::V_CalcShake);
    constexpr std::size_t o147 = offsetof(goldsrc::cl_enginefunc_dst_t, V_ApplyShake);
    using t147 = decltype(goldsrc::cl_enginefunc_dst_t::V_ApplyShake);
    constexpr std::size_t o148 = offsetof(goldsrc::cl_enginefunc_dst_t, PM_PointContents);
    using t148 = decltype(goldsrc::cl_enginefunc_dst_t::PM_PointContents);
    constexpr std::size_t o149 = offsetof(goldsrc::cl_enginefunc_dst_t, PM_WaterEntity);
    using t149 = decltype(goldsrc::cl_enginefunc_dst_t::PM_WaterEntity);
    constexpr std::size_t o150 = offsetof(goldsrc::cl_enginefunc_dst_t, PM_TraceLine);
    using t150 = decltype(goldsrc::cl_enginefunc_dst_t::PM_TraceLine);
    constexpr std::size_t o151 = offsetof(goldsrc::cl_enginefunc_dst_t, CL_LoadModel);
    using t151 = decltype(goldsrc::cl_enginefunc_dst_t::CL_LoadModel);
    constexpr std::size_t o152 = offsetof(goldsrc::cl_enginefunc_dst_t, CL_CreateVisibleEntity);
    using t152 = decltype(goldsrc::cl_enginefunc_dst_t::CL_CreateVisibleEntity);
    constexpr std::size_t o153 = offsetof(goldsrc::cl_enginefunc_dst_t, GetSpritePointer);
    using t153 = decltype(goldsrc::cl_enginefunc_dst_t::GetSpritePointer);
    constexpr std::size_t o154 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaySoundByNameAtLocation);
    using t154 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaySoundByNameAtLocation);
    constexpr std::size_t o155 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPrecacheEvent);
    using t155 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPrecacheEvent);
    constexpr std::size_t o156 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaybackEvent);
    using t156 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaybackEvent);
    constexpr std::size_t o157 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnWeaponAnim);
    using t157 = decltype(goldsrc::cl_enginefunc_dst_t::pfnWeaponAnim);
    constexpr std::size_t o158 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnRandomFloat);
    using t158 = decltype(goldsrc::cl_enginefunc_dst_t::pfnRandomFloat);
    constexpr std::size_t o159 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnRandomLong);
    using t159 = decltype(goldsrc::cl_enginefunc_dst_t::pfnRandomLong);
    constexpr std::size_t o160 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnHookEvent);
    using t160 = decltype(goldsrc::cl_enginefunc_dst_t::pfnHookEvent);
    constexpr std::size_t o161 = offsetof(goldsrc::cl_enginefunc_dst_t, Con_IsVisible);
    using t161 = decltype(goldsrc::cl_enginefunc_dst_t::Con_IsVisible);
    constexpr std::size_t o162 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetGameDirectory);
    using t162 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetGameDirectory);
    constexpr std::size_t o163 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetCvarPointer);
    using t163 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetCvarPointer);
    constexpr std::size_t o164 = offsetof(goldsrc::cl_enginefunc_dst_t, Key_LookupBinding);
    using t164 = decltype(goldsrc::cl_enginefunc_dst_t::Key_LookupBinding);
    constexpr std::size_t o165 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetLevelName);
    using t165 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetLevelName);
    constexpr std::size_t o166 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetScreenFade);
    using t166 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetScreenFade);
    constexpr std::size_t o167 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetScreenFade);
    using t167 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetScreenFade);
    constexpr std::size_t o168 = offsetof(goldsrc::cl_enginefunc_dst_t, VGui_GetPanel);
    using t168 = decltype(goldsrc::cl_enginefunc_dst_t::VGui_GetPanel);
    constexpr std::size_t o169 = offsetof(goldsrc::cl_enginefunc_dst_t, VGui_ViewportPaintBackground);
    using t169 = decltype(goldsrc::cl_enginefunc_dst_t::VGui_ViewportPaintBackground);
    constexpr std::size_t o170 = offsetof(goldsrc::cl_enginefunc_dst_t, COM_LoadFile);
    using t170 = decltype(goldsrc::cl_enginefunc_dst_t::COM_LoadFile);
    constexpr std::size_t o171 = offsetof(goldsrc::cl_enginefunc_dst_t, COM_ParseFile);
    using t171 = decltype(goldsrc::cl_enginefunc_dst_t::COM_ParseFile);
    constexpr std::size_t o172 = offsetof(goldsrc::cl_enginefunc_dst_t, COM_FreeFile);
    using t172 = decltype(goldsrc::cl_enginefunc_dst_t::COM_FreeFile);
    constexpr std::size_t o173 = offsetof(goldsrc::cl_enginefunc_dst_t, pTriAPI);
    using t173 = decltype(goldsrc::cl_enginefunc_dst_t::pTriAPI);
    constexpr std::size_t o174 = offsetof(goldsrc::cl_enginefunc_dst_t, pEfxAPI);
    using t174 = decltype(goldsrc::cl_enginefunc_dst_t::pEfxAPI);
    constexpr std::size_t o175 = offsetof(goldsrc::cl_enginefunc_dst_t, pEventAPI);
    using t175 = decltype(goldsrc::cl_enginefunc_dst_t::pEventAPI);
    constexpr std::size_t o176 = offsetof(goldsrc::cl_enginefunc_dst_t, pDemoAPI);
    using t176 = decltype(goldsrc::cl_enginefunc_dst_t::pDemoAPI);
    constexpr std::size_t o177 = offsetof(goldsrc::cl_enginefunc_dst_t, pNetAPI);
    using t177 = decltype(goldsrc::cl_enginefunc_dst_t::pNetAPI);
    constexpr std::size_t o178 = offsetof(goldsrc::cl_enginefunc_dst_t, pVoiceTweak);
    using t178 = decltype(goldsrc::cl_enginefunc_dst_t::pVoiceTweak);
    constexpr std::size_t o179 = offsetof(goldsrc::cl_enginefunc_dst_t, IsSpectateOnly);
    using t179 = decltype(goldsrc::cl_enginefunc_dst_t::IsSpectateOnly);
    constexpr std::size_t o180 = offsetof(goldsrc::cl_enginefunc_dst_t, LoadMapSprite);
    using t180 = decltype(goldsrc::cl_enginefunc_dst_t::LoadMapSprite);
    constexpr std::size_t o181 = offsetof(goldsrc::cl_enginefunc_dst_t, COM_AddAppDirectoryToSearchPath);
    using t181 = decltype(goldsrc::cl_enginefunc_dst_t::COM_AddAppDirectoryToSearchPath);
    constexpr std::size_t o182 = offsetof(goldsrc::cl_enginefunc_dst_t, COM_ExpandFilename);
    using t182 = decltype(goldsrc::cl_enginefunc_dst_t::COM_ExpandFilename);
    constexpr std::size_t o183 = offsetof(goldsrc::cl_enginefunc_dst_t, PlayerInfo_ValueForKey);
    using t183 = decltype(goldsrc::cl_enginefunc_dst_t::PlayerInfo_ValueForKey);
    constexpr std::size_t o184 = offsetof(goldsrc::cl_enginefunc_dst_t, PlayerInfo_SetValueForKey);
    using t184 = decltype(goldsrc::cl_enginefunc_dst_t::PlayerInfo_SetValueForKey);
    constexpr std::size_t o185 = offsetof(goldsrc::cl_enginefunc_dst_t, GetPlayerUniqueID);
    using t185 = decltype(goldsrc::cl_enginefunc_dst_t::GetPlayerUniqueID);
    constexpr std::size_t o186 = offsetof(goldsrc::cl_enginefunc_dst_t, GetTrackerIDForPlayer);
    using t186 = decltype(goldsrc::cl_enginefunc_dst_t::GetTrackerIDForPlayer);
    constexpr std::size_t o187 = offsetof(goldsrc::cl_enginefunc_dst_t, GetPlayerForTrackerID);
    using t187 = decltype(goldsrc::cl_enginefunc_dst_t::GetPlayerForTrackerID);
    constexpr std::size_t o188 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnServerCmdUnreliable);
    using t188 = decltype(goldsrc::cl_enginefunc_dst_t::pfnServerCmdUnreliable);
    constexpr std::size_t o189 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetMousePos);
    using t189 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetMousePos);
    constexpr std::size_t o190 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetMousePos);
    using t190 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetMousePos);
    constexpr std::size_t o191 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetMouseEnable);
    using t191 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetMouseEnable);
    constexpr std::size_t o192 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetFilterMode);
    using t192 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetFilterMode);
    constexpr std::size_t o193 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetFilterColor);
    using t193 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetFilterColor);
    constexpr std::size_t o194 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSetFilterBrightness);
    using t194 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSetFilterBrightness);
    constexpr std::size_t o195 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSequenceGet);
    using t195 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSequenceGet);
    constexpr std::size_t o196 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSPR_DrawGeneric);
    using t196 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSPR_DrawGeneric);
    constexpr std::size_t o197 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnSequencePickSentence);
    using t197 = decltype(goldsrc::cl_enginefunc_dst_t::pfnSequencePickSentence);
    constexpr std::size_t o198 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawString);
    using t198 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawString);
    constexpr std::size_t o199 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnDrawStringReverse);
    using t199 = decltype(goldsrc::cl_enginefunc_dst_t::pfnDrawStringReverse);
    constexpr std::size_t o200 = offsetof(goldsrc::cl_enginefunc_dst_t, LocalPlayerInfo_ValueForKey);
    using t200 = decltype(goldsrc::cl_enginefunc_dst_t::LocalPlayerInfo_ValueForKey);
    constexpr std::size_t o201 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnVGUI2DrawCharacter);
    using t201 = decltype(goldsrc::cl_enginefunc_dst_t::pfnVGUI2DrawCharacter);
    constexpr std::size_t o202 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnVGUI2DrawCharacterAdd);
    using t202 = decltype(goldsrc::cl_enginefunc_dst_t::pfnVGUI2DrawCharacterAdd);
    constexpr std::size_t o203 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaySoundVoiceByName);
    using t203 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaySoundVoiceByName);
    constexpr std::size_t o204 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPrimeMusicStream);
    using t204 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPrimeMusicStream);
    constexpr std::size_t o205 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnProcessTutorMessageDecayBuffer);
    using t205 = decltype(goldsrc::cl_enginefunc_dst_t::pfnProcessTutorMessageDecayBuffer);
    constexpr std::size_t o206 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnConstructTutorMessageDecayBuffer);
    using t206 = decltype(goldsrc::cl_enginefunc_dst_t::pfnConstructTutorMessageDecayBuffer);
    constexpr std::size_t o207 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnResetTutorMessageDecayData);
    using t207 = decltype(goldsrc::cl_enginefunc_dst_t::pfnResetTutorMessageDecayData);
    constexpr std::size_t o208 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnPlaySoundByNameAtPitch);
    using t208 = decltype(goldsrc::cl_enginefunc_dst_t::pfnPlaySoundByNameAtPitch);
    constexpr std::size_t o209 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnFillRGBABlend);
    using t209 = decltype(goldsrc::cl_enginefunc_dst_t::pfnFillRGBABlend);
    constexpr std::size_t o210 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetAppID);
    using t210 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetAppID);
    constexpr std::size_t o211 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnGetAliasList);
    using t211 = decltype(goldsrc::cl_enginefunc_dst_t::pfnGetAliasList);
    constexpr std::size_t o212 = offsetof(goldsrc::cl_enginefunc_dst_t, pfnVguiWrap2_GetMouseDelta);
    using t212 = decltype(goldsrc::cl_enginefunc_dst_t::pfnVguiWrap2_GetMouseDelta);
    constexpr std::size_t s6 = sizeof(goldsrc::cl_enginefunc_t);
    constexpr std::size_t o213 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Load);
    using t213 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Load);
    constexpr std::size_t o214 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Frames);
    using t214 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Frames);
    constexpr std::size_t o215 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Height);
    using t215 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Height);
    constexpr std::size_t o216 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Width);
    using t216 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Width);
    constexpr std::size_t o217 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Set);
    using t217 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Set);
    constexpr std::size_t o218 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_Draw);
    using t218 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_Draw);
    constexpr std::size_t o219 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_DrawHoles);
    using t219 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_DrawHoles);
    constexpr std::size_t o220 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_DrawAdditive);
    using t220 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_DrawAdditive);
    constexpr std::size_t o221 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_EnableScissor);
    using t221 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_EnableScissor);
    constexpr std::size_t o222 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_DisableScissor);
    using t222 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_DisableScissor);
    constexpr std::size_t o223 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_GetList);
    using t223 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_GetList);
    constexpr std::size_t o224 = offsetof(goldsrc::cl_enginefunc_t, pfnFillRGBA);
    using t224 = decltype(goldsrc::cl_enginefunc_t::pfnFillRGBA);
    constexpr std::size_t o225 = offsetof(goldsrc::cl_enginefunc_t, pfnGetScreenInfo);
    using t225 = decltype(goldsrc::cl_enginefunc_t::pfnGetScreenInfo);
    constexpr std::size_t o226 = offsetof(goldsrc::cl_enginefunc_t, pfnSetCrosshair);
    using t226 = decltype(goldsrc::cl_enginefunc_t::pfnSetCrosshair);
    constexpr std::size_t o227 = offsetof(goldsrc::cl_enginefunc_t, pfnRegisterVariable);
    using t227 = decltype(goldsrc::cl_enginefunc_t::pfnRegisterVariable);
    constexpr std::size_t o228 = offsetof(goldsrc::cl_enginefunc_t, pfnGetCvarFloat);
    using t228 = decltype(goldsrc::cl_enginefunc_t::pfnGetCvarFloat);
    constexpr std::size_t o229 = offsetof(goldsrc::cl_enginefunc_t, pfnGetCvarString);
    using t229 = decltype(goldsrc::cl_enginefunc_t::pfnGetCvarString);
    constexpr std::size_t o230 = offsetof(goldsrc::cl_enginefunc_t, pfnAddCommand);
    using t230 = decltype(goldsrc::cl_enginefunc_t::pfnAddCommand);
    constexpr std::size_t o231 = offsetof(goldsrc::cl_enginefunc_t, pfnHookUserMsg);
    using t231 = decltype(goldsrc::cl_enginefunc_t::pfnHookUserMsg);
    constexpr std::size_t o232 = offsetof(goldsrc::cl_enginefunc_t, pfnServerCmd);
    using t232 = decltype(goldsrc::cl_enginefunc_t::pfnServerCmd);
    constexpr std::size_t o233 = offsetof(goldsrc::cl_enginefunc_t, pfnClientCmd);
    using t233 = decltype(goldsrc::cl_enginefunc_t::pfnClientCmd);
    constexpr std::size_t o234 = offsetof(goldsrc::cl_enginefunc_t, pfnGetPlayerInfo);
    using t234 = decltype(goldsrc::cl_enginefunc_t::pfnGetPlayerInfo);
    constexpr std::size_t o235 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaySoundByName);
    using t235 = decltype(goldsrc::cl_enginefunc_t::pfnPlaySoundByName);
    constexpr std::size_t o236 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaySoundByIndex);
    using t236 = decltype(goldsrc::cl_enginefunc_t::pfnPlaySoundByIndex);
    constexpr std::size_t o237 = offsetof(goldsrc::cl_enginefunc_t, pfnAngleVectors);
    using t237 = decltype(goldsrc::cl_enginefunc_t::pfnAngleVectors);
    constexpr std::size_t o238 = offsetof(goldsrc::cl_enginefunc_t, pfnTextMessageGet);
    using t238 = decltype(goldsrc::cl_enginefunc_t::pfnTextMessageGet);
    constexpr std::size_t o239 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawCharacter);
    using t239 = decltype(goldsrc::cl_enginefunc_t::pfnDrawCharacter);
    constexpr std::size_t o240 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawConsoleString);
    using t240 = decltype(goldsrc::cl_enginefunc_t::pfnDrawConsoleString);
    constexpr std::size_t o241 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawSetTextColor);
    using t241 = decltype(goldsrc::cl_enginefunc_t::pfnDrawSetTextColor);
    constexpr std::size_t o242 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawConsoleStringLen);
    using t242 = decltype(goldsrc::cl_enginefunc_t::pfnDrawConsoleStringLen);
    constexpr std::size_t o243 = offsetof(goldsrc::cl_enginefunc_t, pfnConsolePrint);
    using t243 = decltype(goldsrc::cl_enginefunc_t::pfnConsolePrint);
    constexpr std::size_t o244 = offsetof(goldsrc::cl_enginefunc_t, pfnCenterPrint);
    using t244 = decltype(goldsrc::cl_enginefunc_t::pfnCenterPrint);
    constexpr std::size_t o245 = offsetof(goldsrc::cl_enginefunc_t, GetWindowCenterX);
    using t245 = decltype(goldsrc::cl_enginefunc_t::GetWindowCenterX);
    constexpr std::size_t o246 = offsetof(goldsrc::cl_enginefunc_t, GetWindowCenterY);
    using t246 = decltype(goldsrc::cl_enginefunc_t::GetWindowCenterY);
    constexpr std::size_t o247 = offsetof(goldsrc::cl_enginefunc_t, GetViewAngles);
    using t247 = decltype(goldsrc::cl_enginefunc_t::GetViewAngles);
    constexpr std::size_t o248 = offsetof(goldsrc::cl_enginefunc_t, SetViewAngles);
    using t248 = decltype(goldsrc::cl_enginefunc_t::SetViewAngles);
    constexpr std::size_t o249 = offsetof(goldsrc::cl_enginefunc_t, GetMaxClients);
    using t249 = decltype(goldsrc::cl_enginefunc_t::GetMaxClients);
    constexpr std::size_t o250 = offsetof(goldsrc::cl_enginefunc_t, Cvar_SetValue);
    using t250 = decltype(goldsrc::cl_enginefunc_t::Cvar_SetValue);
    constexpr std::size_t o251 = offsetof(goldsrc::cl_enginefunc_t, Cmd_Argc);
    using t251 = decltype(goldsrc::cl_enginefunc_t::Cmd_Argc);
    constexpr std::size_t o252 = offsetof(goldsrc::cl_enginefunc_t, Cmd_Argv);
    using t252 = decltype(goldsrc::cl_enginefunc_t::Cmd_Argv);
    constexpr std::size_t o253 = offsetof(goldsrc::cl_enginefunc_t, Con_Printf);
    using t253 = decltype(goldsrc::cl_enginefunc_t::Con_Printf);
    constexpr std::size_t o254 = offsetof(goldsrc::cl_enginefunc_t, Con_DPrintf);
    using t254 = decltype(goldsrc::cl_enginefunc_t::Con_DPrintf);
    constexpr std::size_t o255 = offsetof(goldsrc::cl_enginefunc_t, Con_NPrintf);
    using t255 = decltype(goldsrc::cl_enginefunc_t::Con_NPrintf);
    constexpr std::size_t o256 = offsetof(goldsrc::cl_enginefunc_t, Con_NXPrintf);
    using t256 = decltype(goldsrc::cl_enginefunc_t::Con_NXPrintf);
    constexpr std::size_t o257 = offsetof(goldsrc::cl_enginefunc_t, PhysInfo_ValueForKey);
    using t257 = decltype(goldsrc::cl_enginefunc_t::PhysInfo_ValueForKey);
    constexpr std::size_t o258 = offsetof(goldsrc::cl_enginefunc_t, ServerInfo_ValueForKey);
    using t258 = decltype(goldsrc::cl_enginefunc_t::ServerInfo_ValueForKey);
    constexpr std::size_t o259 = offsetof(goldsrc::cl_enginefunc_t, GetClientMaxspeed);
    using t259 = decltype(goldsrc::cl_enginefunc_t::GetClientMaxspeed);
    constexpr std::size_t o260 = offsetof(goldsrc::cl_enginefunc_t, CheckParm);
    using t260 = decltype(goldsrc::cl_enginefunc_t::CheckParm);
    constexpr std::size_t o261 = offsetof(goldsrc::cl_enginefunc_t, Key_Event);
    using t261 = decltype(goldsrc::cl_enginefunc_t::Key_Event);
    constexpr std::size_t o262 = offsetof(goldsrc::cl_enginefunc_t, GetMousePosition);
    using t262 = decltype(goldsrc::cl_enginefunc_t::GetMousePosition);
    constexpr std::size_t o263 = offsetof(goldsrc::cl_enginefunc_t, IsNoClipping);
    using t263 = decltype(goldsrc::cl_enginefunc_t::IsNoClipping);
    constexpr std::size_t o264 = offsetof(goldsrc::cl_enginefunc_t, GetLocalPlayer);
    using t264 = decltype(goldsrc::cl_enginefunc_t::GetLocalPlayer);
    constexpr std::size_t o265 = offsetof(goldsrc::cl_enginefunc_t, GetViewModel);
    using t265 = decltype(goldsrc::cl_enginefunc_t::GetViewModel);
    constexpr std::size_t o266 = offsetof(goldsrc::cl_enginefunc_t, GetEntityByIndex);
    using t266 = decltype(goldsrc::cl_enginefunc_t::GetEntityByIndex);
    constexpr std::size_t o267 = offsetof(goldsrc::cl_enginefunc_t, GetClientTime);
    using t267 = decltype(goldsrc::cl_enginefunc_t::GetClientTime);
    constexpr std::size_t o268 = offsetof(goldsrc::cl_enginefunc_t, V_CalcShake);
    using t268 = decltype(goldsrc::cl_enginefunc_t::V_CalcShake);
    constexpr std::size_t o269 = offsetof(goldsrc::cl_enginefunc_t, V_ApplyShake);
    using t269 = decltype(goldsrc::cl_enginefunc_t::V_ApplyShake);
    constexpr std::size_t o270 = offsetof(goldsrc::cl_enginefunc_t, PM_PointContents);
    using t270 = decltype(goldsrc::cl_enginefunc_t::PM_PointContents);
    constexpr std::size_t o271 = offsetof(goldsrc::cl_enginefunc_t, PM_WaterEntity);
    using t271 = decltype(goldsrc::cl_enginefunc_t::PM_WaterEntity);
    constexpr std::size_t o272 = offsetof(goldsrc::cl_enginefunc_t, PM_TraceLine);
    using t272 = decltype(goldsrc::cl_enginefunc_t::PM_TraceLine);
    constexpr std::size_t o273 = offsetof(goldsrc::cl_enginefunc_t, CL_LoadModel);
    using t273 = decltype(goldsrc::cl_enginefunc_t::CL_LoadModel);
    constexpr std::size_t o274 = offsetof(goldsrc::cl_enginefunc_t, CL_CreateVisibleEntity);
    using t274 = decltype(goldsrc::cl_enginefunc_t::CL_CreateVisibleEntity);
    constexpr std::size_t o275 = offsetof(goldsrc::cl_enginefunc_t, GetSpritePointer);
    using t275 = decltype(goldsrc::cl_enginefunc_t::GetSpritePointer);
    constexpr std::size_t o276 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaySoundByNameAtLocation);
    using t276 = decltype(goldsrc::cl_enginefunc_t::pfnPlaySoundByNameAtLocation);
    constexpr std::size_t o277 = offsetof(goldsrc::cl_enginefunc_t, pfnPrecacheEvent);
    using t277 = decltype(goldsrc::cl_enginefunc_t::pfnPrecacheEvent);
    constexpr std::size_t o278 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaybackEvent);
    using t278 = decltype(goldsrc::cl_enginefunc_t::pfnPlaybackEvent);
    constexpr std::size_t o279 = offsetof(goldsrc::cl_enginefunc_t, pfnWeaponAnim);
    using t279 = decltype(goldsrc::cl_enginefunc_t::pfnWeaponAnim);
    constexpr std::size_t o280 = offsetof(goldsrc::cl_enginefunc_t, pfnRandomFloat);
    using t280 = decltype(goldsrc::cl_enginefunc_t::pfnRandomFloat);
    constexpr std::size_t o281 = offsetof(goldsrc::cl_enginefunc_t, pfnRandomLong);
    using t281 = decltype(goldsrc::cl_enginefunc_t::pfnRandomLong);
    constexpr std::size_t o282 = offsetof(goldsrc::cl_enginefunc_t, pfnHookEvent);
    using t282 = decltype(goldsrc::cl_enginefunc_t::pfnHookEvent);
    constexpr std::size_t o283 = offsetof(goldsrc::cl_enginefunc_t, Con_IsVisible);
    using t283 = decltype(goldsrc::cl_enginefunc_t::Con_IsVisible);
    constexpr std::size_t o284 = offsetof(goldsrc::cl_enginefunc_t, pfnGetGameDirectory);
    using t284 = decltype(goldsrc::cl_enginefunc_t::pfnGetGameDirectory);
    constexpr std::size_t o285 = offsetof(goldsrc::cl_enginefunc_t, pfnGetCvarPointer);
    using t285 = decltype(goldsrc::cl_enginefunc_t::pfnGetCvarPointer);
    constexpr std::size_t o286 = offsetof(goldsrc::cl_enginefunc_t, Key_LookupBinding);
    using t286 = decltype(goldsrc::cl_enginefunc_t::Key_LookupBinding);
    constexpr std::size_t o287 = offsetof(goldsrc::cl_enginefunc_t, pfnGetLevelName);
    using t287 = decltype(goldsrc::cl_enginefunc_t::pfnGetLevelName);
    constexpr std::size_t o288 = offsetof(goldsrc::cl_enginefunc_t, pfnGetScreenFade);
    using t288 = decltype(goldsrc::cl_enginefunc_t::pfnGetScreenFade);
    constexpr std::size_t o289 = offsetof(goldsrc::cl_enginefunc_t, pfnSetScreenFade);
    using t289 = decltype(goldsrc::cl_enginefunc_t::pfnSetScreenFade);
    constexpr std::size_t o290 = offsetof(goldsrc::cl_enginefunc_t, VGui_GetPanel);
    using t290 = decltype(goldsrc::cl_enginefunc_t::VGui_GetPanel);
    constexpr std::size_t o291 = offsetof(goldsrc::cl_enginefunc_t, VGui_ViewportPaintBackground);
    using t291 = decltype(goldsrc::cl_enginefunc_t::VGui_ViewportPaintBackground);
    constexpr std::size_t o292 = offsetof(goldsrc::cl_enginefunc_t, COM_LoadFile);
    using t292 = decltype(goldsrc::cl_enginefunc_t::COM_LoadFile);
    constexpr std::size_t o293 = offsetof(goldsrc::cl_enginefunc_t, COM_ParseFile);
    using t293 = decltype(goldsrc::cl_enginefunc_t::COM_ParseFile);
    constexpr std::size_t o294 = offsetof(goldsrc::cl_enginefunc_t, COM_FreeFile);
    using t294 = decltype(goldsrc::cl_enginefunc_t::COM_FreeFile);
    constexpr std::size_t o295 = offsetof(goldsrc::cl_enginefunc_t, pTriAPI);
    using t295 = decltype(goldsrc::cl_enginefunc_t::pTriAPI);
    constexpr std::size_t o296 = offsetof(goldsrc::cl_enginefunc_t, pEfxAPI);
    using t296 = decltype(goldsrc::cl_enginefunc_t::pEfxAPI);
    constexpr std::size_t o297 = offsetof(goldsrc::cl_enginefunc_t, pEventAPI);
    using t297 = decltype(goldsrc::cl_enginefunc_t::pEventAPI);
    constexpr std::size_t o298 = offsetof(goldsrc::cl_enginefunc_t, pDemoAPI);
    using t298 = decltype(goldsrc::cl_enginefunc_t::pDemoAPI);
    constexpr std::size_t o299 = offsetof(goldsrc::cl_enginefunc_t, pNetAPI);
    using t299 = decltype(goldsrc::cl_enginefunc_t::pNetAPI);
    constexpr std::size_t o300 = offsetof(goldsrc::cl_enginefunc_t, pVoiceTweak);
    using t300 = decltype(goldsrc::cl_enginefunc_t::pVoiceTweak);
    constexpr std::size_t o301 = offsetof(goldsrc::cl_enginefunc_t, IsSpectateOnly);
    using t301 = decltype(goldsrc::cl_enginefunc_t::IsSpectateOnly);
    constexpr std::size_t o302 = offsetof(goldsrc::cl_enginefunc_t, LoadMapSprite);
    using t302 = decltype(goldsrc::cl_enginefunc_t::LoadMapSprite);
    constexpr std::size_t o303 = offsetof(goldsrc::cl_enginefunc_t, COM_AddAppDirectoryToSearchPath);
    using t303 = decltype(goldsrc::cl_enginefunc_t::COM_AddAppDirectoryToSearchPath);
    constexpr std::size_t o304 = offsetof(goldsrc::cl_enginefunc_t, COM_ExpandFilename);
    using t304 = decltype(goldsrc::cl_enginefunc_t::COM_ExpandFilename);
    constexpr std::size_t o305 = offsetof(goldsrc::cl_enginefunc_t, PlayerInfo_ValueForKey);
    using t305 = decltype(goldsrc::cl_enginefunc_t::PlayerInfo_ValueForKey);
    constexpr std::size_t o306 = offsetof(goldsrc::cl_enginefunc_t, PlayerInfo_SetValueForKey);
    using t306 = decltype(goldsrc::cl_enginefunc_t::PlayerInfo_SetValueForKey);
    constexpr std::size_t o307 = offsetof(goldsrc::cl_enginefunc_t, GetPlayerUniqueID);
    using t307 = decltype(goldsrc::cl_enginefunc_t::GetPlayerUniqueID);
    constexpr std::size_t o308 = offsetof(goldsrc::cl_enginefunc_t, GetTrackerIDForPlayer);
    using t308 = decltype(goldsrc::cl_enginefunc_t::GetTrackerIDForPlayer);
    constexpr std::size_t o309 = offsetof(goldsrc::cl_enginefunc_t, GetPlayerForTrackerID);
    using t309 = decltype(goldsrc::cl_enginefunc_t::GetPlayerForTrackerID);
    constexpr std::size_t o310 = offsetof(goldsrc::cl_enginefunc_t, pfnServerCmdUnreliable);
    using t310 = decltype(goldsrc::cl_enginefunc_t::pfnServerCmdUnreliable);
    constexpr std::size_t o311 = offsetof(goldsrc::cl_enginefunc_t, pfnGetMousePos);
    using t311 = decltype(goldsrc::cl_enginefunc_t::pfnGetMousePos);
    constexpr std::size_t o312 = offsetof(goldsrc::cl_enginefunc_t, pfnSetMousePos);
    using t312 = decltype(goldsrc::cl_enginefunc_t::pfnSetMousePos);
    constexpr std::size_t o313 = offsetof(goldsrc::cl_enginefunc_t, pfnSetMouseEnable);
    using t313 = decltype(goldsrc::cl_enginefunc_t::pfnSetMouseEnable);
    constexpr std::size_t o314 = offsetof(goldsrc::cl_enginefunc_t, GetFirstCvarPtr);
    using t314 = decltype(goldsrc::cl_enginefunc_t::GetFirstCvarPtr);
    constexpr std::size_t o315 = offsetof(goldsrc::cl_enginefunc_t, GetFirstCmdFunctionHandle);
    using t315 = decltype(goldsrc::cl_enginefunc_t::GetFirstCmdFunctionHandle);
    constexpr std::size_t o316 = offsetof(goldsrc::cl_enginefunc_t, GetNextCmdFunctionHandle);
    using t316 = decltype(goldsrc::cl_enginefunc_t::GetNextCmdFunctionHandle);
    constexpr std::size_t o317 = offsetof(goldsrc::cl_enginefunc_t, GetCmdFunctionName);
    using t317 = decltype(goldsrc::cl_enginefunc_t::GetCmdFunctionName);
    constexpr std::size_t o318 = offsetof(goldsrc::cl_enginefunc_t, hudGetClientOldTime);
    using t318 = decltype(goldsrc::cl_enginefunc_t::hudGetClientOldTime);
    constexpr std::size_t o319 = offsetof(goldsrc::cl_enginefunc_t, hudGetServerGravityValue);
    using t319 = decltype(goldsrc::cl_enginefunc_t::hudGetServerGravityValue);
    constexpr std::size_t o320 = offsetof(goldsrc::cl_enginefunc_t, hudGetModelByIndex);
    using t320 = decltype(goldsrc::cl_enginefunc_t::hudGetModelByIndex);
    constexpr std::size_t o321 = offsetof(goldsrc::cl_enginefunc_t, pfnSetFilterMode);
    using t321 = decltype(goldsrc::cl_enginefunc_t::pfnSetFilterMode);
    constexpr std::size_t o322 = offsetof(goldsrc::cl_enginefunc_t, pfnSetFilterColor);
    using t322 = decltype(goldsrc::cl_enginefunc_t::pfnSetFilterColor);
    constexpr std::size_t o323 = offsetof(goldsrc::cl_enginefunc_t, pfnSetFilterBrightness);
    using t323 = decltype(goldsrc::cl_enginefunc_t::pfnSetFilterBrightness);
    constexpr std::size_t o324 = offsetof(goldsrc::cl_enginefunc_t, pfnSequenceGet);
    using t324 = decltype(goldsrc::cl_enginefunc_t::pfnSequenceGet);
    constexpr std::size_t o325 = offsetof(goldsrc::cl_enginefunc_t, pfnSPR_DrawGeneric);
    using t325 = decltype(goldsrc::cl_enginefunc_t::pfnSPR_DrawGeneric);
    constexpr std::size_t o326 = offsetof(goldsrc::cl_enginefunc_t, pfnSequencePickSentence);
    using t326 = decltype(goldsrc::cl_enginefunc_t::pfnSequencePickSentence);
    constexpr std::size_t o327 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawString);
    using t327 = decltype(goldsrc::cl_enginefunc_t::pfnDrawString);
    constexpr std::size_t o328 = offsetof(goldsrc::cl_enginefunc_t, pfnDrawStringReverse);
    using t328 = decltype(goldsrc::cl_enginefunc_t::pfnDrawStringReverse);
    constexpr std::size_t o329 = offsetof(goldsrc::cl_enginefunc_t, LocalPlayerInfo_ValueForKey);
    using t329 = decltype(goldsrc::cl_enginefunc_t::LocalPlayerInfo_ValueForKey);
    constexpr std::size_t o330 = offsetof(goldsrc::cl_enginefunc_t, pfnVGUI2DrawCharacter);
    using t330 = decltype(goldsrc::cl_enginefunc_t::pfnVGUI2DrawCharacter);
    constexpr std::size_t o331 = offsetof(goldsrc::cl_enginefunc_t, pfnVGUI2DrawCharacterAdd);
    using t331 = decltype(goldsrc::cl_enginefunc_t::pfnVGUI2DrawCharacterAdd);
    constexpr std::size_t o332 = offsetof(goldsrc::cl_enginefunc_t, COM_GetApproxWavePlayLength);
    using t332 = decltype(goldsrc::cl_enginefunc_t::COM_GetApproxWavePlayLength);
    constexpr std::size_t o333 = offsetof(goldsrc::cl_enginefunc_t, pfnGetCareerUI);
    using t333 = decltype(goldsrc::cl_enginefunc_t::pfnGetCareerUI);
    constexpr std::size_t o334 = offsetof(goldsrc::cl_enginefunc_t, Cvar_Set);
    using t334 = decltype(goldsrc::cl_enginefunc_t::Cvar_Set);
    constexpr std::size_t o335 = offsetof(goldsrc::cl_enginefunc_t, pfnIsCareerMatch);
    using t335 = decltype(goldsrc::cl_enginefunc_t::pfnIsCareerMatch);
    constexpr std::size_t o336 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaySoundVoiceByName);
    using t336 = decltype(goldsrc::cl_enginefunc_t::pfnPlaySoundVoiceByName);
    constexpr std::size_t o337 = offsetof(goldsrc::cl_enginefunc_t, pfnPrimeMusicStream);
    using t337 = decltype(goldsrc::cl_enginefunc_t::pfnPrimeMusicStream);
    constexpr std::size_t o338 = offsetof(goldsrc::cl_enginefunc_t, GetAbsoluteTime);
    using t338 = decltype(goldsrc::cl_enginefunc_t::GetAbsoluteTime);
    constexpr std::size_t o339 = offsetof(goldsrc::cl_enginefunc_t, pfnProcessTutorMessageDecayBuffer);
    using t339 = decltype(goldsrc::cl_enginefunc_t::pfnProcessTutorMessageDecayBuffer);
    constexpr std::size_t o340 = offsetof(goldsrc::cl_enginefunc_t, pfnConstructTutorMessageDecayBuffer);
    using t340 = decltype(goldsrc::cl_enginefunc_t::pfnConstructTutorMessageDecayBuffer);
    constexpr std::size_t o341 = offsetof(goldsrc::cl_enginefunc_t, pfnResetTutorMessageDecayData);
    using t341 = decltype(goldsrc::cl_enginefunc_t::pfnResetTutorMessageDecayData);
    constexpr std::size_t o342 = offsetof(goldsrc::cl_enginefunc_t, pfnPlaySoundByNameAtPitch);
    using t342 = decltype(goldsrc::cl_enginefunc_t::pfnPlaySoundByNameAtPitch);
    constexpr std::size_t o343 = offsetof(goldsrc::cl_enginefunc_t, pfnFillRGBABlend);
    using t343 = decltype(goldsrc::cl_enginefunc_t::pfnFillRGBABlend);
    constexpr std::size_t o344 = offsetof(goldsrc::cl_enginefunc_t, pfnGetAppID);
    using t344 = decltype(goldsrc::cl_enginefunc_t::pfnGetAppID);
    constexpr std::size_t o345 = offsetof(goldsrc::cl_enginefunc_t, pfnGetAliasList);
    using t345 = decltype(goldsrc::cl_enginefunc_t::pfnGetAliasList);
    constexpr std::size_t o346 = offsetof(goldsrc::cl_enginefunc_t, pfnVguiWrap2_GetMouseDelta);
    using t346 = decltype(goldsrc::cl_enginefunc_t::pfnVguiWrap2_GetMouseDelta);
    constexpr std::size_t s7 = sizeof(goldsrc::cstrike::CHud);
    constexpr std::size_t o347 = offsetof(goldsrc::cstrike::CHud, m_pHudList);
    using t347 = decltype(goldsrc::cstrike::CHud::m_pHudList);
    constexpr std::size_t o348 = offsetof(goldsrc::cstrike::CHud, m_hsprLogo);
    using t348 = decltype(goldsrc::cstrike::CHud::m_hsprLogo);
    constexpr std::size_t o349 = offsetof(goldsrc::cstrike::CHud, m_iLogo);
    using t349 = decltype(goldsrc::cstrike::CHud::m_iLogo);
    constexpr std::size_t o350 = offsetof(goldsrc::cstrike::CHud, m_pSpriteList);
    using t350 = decltype(goldsrc::cstrike::CHud::m_pSpriteList);
    constexpr std::size_t o351 = offsetof(goldsrc::cstrike::CHud, m_iSpriteCount);
    using t351 = decltype(goldsrc::cstrike::CHud::m_iSpriteCount);
    constexpr std::size_t o352 = offsetof(goldsrc::cstrike::CHud, m_iSpriteCountAllRes);
    using t352 = decltype(goldsrc::cstrike::CHud::m_iSpriteCountAllRes);
    constexpr std::size_t o353 = offsetof(goldsrc::cstrike::CHud, m_flMouseSensitivity);
    using t353 = decltype(goldsrc::cstrike::CHud::m_flMouseSensitivity);
    constexpr std::size_t o354 = offsetof(goldsrc::cstrike::CHud, m_iConcussionEffect);
    using t354 = decltype(goldsrc::cstrike::CHud::m_iConcussionEffect);
    constexpr std::size_t o355 = offsetof(goldsrc::cstrike::CHud, m_hsprCursor);
    using t355 = decltype(goldsrc::cstrike::CHud::m_hsprCursor);
    constexpr std::size_t o356 = offsetof(goldsrc::cstrike::CHud, m_flTime);
    using t356 = decltype(goldsrc::cstrike::CHud::m_flTime);
    constexpr std::size_t o357 = offsetof(goldsrc::cstrike::CHud, m_fOldTime);
    using t357 = decltype(goldsrc::cstrike::CHud::m_fOldTime);
    constexpr std::size_t o358 = offsetof(goldsrc::cstrike::CHud, m_flTimeDelta);
    using t358 = decltype(goldsrc::cstrike::CHud::m_flTimeDelta);
    constexpr std::size_t o359 = offsetof(goldsrc::cstrike::CHud, m_vecOrigin);
    constexpr std::size_t o360 = offsetof(goldsrc::cstrike::CHud, m_vecAngles);
    constexpr std::size_t o361 = offsetof(goldsrc::cstrike::CHud, m_iKeyBits);
    using t359 = decltype(goldsrc::cstrike::CHud::m_iKeyBits);
    constexpr std::size_t o362 = offsetof(goldsrc::cstrike::CHud, m_iHideHUDDisplay);
    using t360 = decltype(goldsrc::cstrike::CHud::m_iHideHUDDisplay);
    constexpr std::size_t o363 = offsetof(goldsrc::cstrike::CHud, m_iFOV);
    using t361 = decltype(goldsrc::cstrike::CHud::m_iFOV);
    constexpr std::size_t o364 = offsetof(goldsrc::cstrike::CHud, m_Teamplay);
    using t362 = decltype(goldsrc::cstrike::CHud::m_Teamplay);
    constexpr std::size_t o365 = offsetof(goldsrc::cstrike::CHud, m_iRes);
    using t363 = decltype(goldsrc::cstrike::CHud::m_iRes);
    constexpr std::size_t o366 = offsetof(goldsrc::cstrike::CHud, m_pCvarStealMouse);
    using t364 = decltype(goldsrc::cstrike::CHud::m_pCvarStealMouse);
    constexpr std::size_t o367 = offsetof(goldsrc::cstrike::CHud, m_pCvarDraw);
    using t365 = decltype(goldsrc::cstrike::CHud::m_pCvarDraw);
    constexpr std::size_t o368 = offsetof(goldsrc::cstrike::CHud, m_PlayerFOV);
    using t366 = decltype(goldsrc::cstrike::CHud::m_PlayerFOV);
    constexpr std::size_t o369 = offsetof(goldsrc::cstrike::CHud, m_iFontHeight);
    using t367 = decltype(goldsrc::cstrike::CHud::m_iFontHeight);
    constexpr std::size_t o370 = offsetof(goldsrc::cstrike::CHud, m_iFontEngineHeight);
    using t368 = decltype(goldsrc::cstrike::CHud::m_iFontEngineHeight);
    constexpr std::size_t o371 = offsetof(goldsrc::cstrike::CHud, default_fov);
    using t369 = decltype(goldsrc::cstrike::CHud::default_fov);
    constexpr std::size_t o372 = offsetof(goldsrc::cstrike::CHud, m_rghSprites);
    using t370 = decltype(goldsrc::cstrike::CHud::m_rghSprites);
    constexpr std::size_t o373 = offsetof(goldsrc::cstrike::CHud, m_rgrcRects);
    using t371 = decltype(goldsrc::cstrike::CHud::m_rgrcRects);
    constexpr std::size_t s8 = sizeof(goldsrc::cstrike::CHudBase);
    constexpr std::size_t o374 = offsetof(goldsrc::cstrike::CHudBase, m_pos);
    using t372 = decltype(goldsrc::cstrike::CHudBase::m_pos);
    constexpr std::size_t o375 = offsetof(goldsrc::cstrike::CHudBase, m_type);
    using t373 = decltype(goldsrc::cstrike::CHudBase::m_type);
    constexpr std::size_t o376 = offsetof(goldsrc::cstrike::CHudBase, m_iFlags);
    using t374 = decltype(goldsrc::cstrike::CHudBase::m_iFlags);
    constexpr std::size_t s9 = sizeof(goldsrc::cstrike::CHudBase::POSITION);
    constexpr std::size_t o377 = offsetof(goldsrc::cstrike::CHudBase::POSITION, x);
    using t375 = decltype(goldsrc::cstrike::CHudBase::POSITION::x);
    constexpr std::size_t o378 = offsetof(goldsrc::cstrike::CHudBase::POSITION, y);
    using t376 = decltype(goldsrc::cstrike::CHudBase::POSITION::y);
    constexpr std::size_t s10 = sizeof(goldsrc::cstrike::extra_player_info_t);
    constexpr std::size_t o379 = offsetof(goldsrc::cstrike::extra_player_info_t, frags);
    using t377 = decltype(goldsrc::cstrike::extra_player_info_t::frags);
    constexpr std::size_t o380 = offsetof(goldsrc::cstrike::extra_player_info_t, deaths);
    using t378 = decltype(goldsrc::cstrike::extra_player_info_t::deaths);
    constexpr std::size_t o381 = offsetof(goldsrc::cstrike::extra_player_info_t, team_id);
    using t379 = decltype(goldsrc::cstrike::extra_player_info_t::team_id);
    constexpr std::size_t o382 = offsetof(goldsrc::cstrike::extra_player_info_t, has_c4);
    using t380 = decltype(goldsrc::cstrike::extra_player_info_t::has_c4);
    constexpr std::size_t o383 = offsetof(goldsrc::cstrike::extra_player_info_t, vip);
    using t381 = decltype(goldsrc::cstrike::extra_player_info_t::vip);
    constexpr std::size_t o384 = offsetof(goldsrc::cstrike::extra_player_info_t, origin);
    using t382 = decltype(goldsrc::cstrike::extra_player_info_t::origin);
    constexpr std::size_t o385 = offsetof(goldsrc::cstrike::extra_player_info_t, radarflash);
    using t383 = decltype(goldsrc::cstrike::extra_player_info_t::radarflash);
    constexpr std::size_t o386 = offsetof(goldsrc::cstrike::extra_player_info_t, radarflashon);
    using t384 = decltype(goldsrc::cstrike::extra_player_info_t::radarflashon);
    constexpr std::size_t o387 = offsetof(goldsrc::cstrike::extra_player_info_t, radarflashes);
    using t385 = decltype(goldsrc::cstrike::extra_player_info_t::radarflashes);
    constexpr std::size_t o388 = offsetof(goldsrc::cstrike::extra_player_info_t, playerclass);
    using t386 = decltype(goldsrc::cstrike::extra_player_info_t::playerclass);
    constexpr std::size_t o389 = offsetof(goldsrc::cstrike::extra_player_info_t, teamnumber);
    using t387 = decltype(goldsrc::cstrike::extra_player_info_t::teamnumber);
    constexpr std::size_t o390 = offsetof(goldsrc::cstrike::extra_player_info_t, teamname);
    using t388 = decltype(goldsrc::cstrike::extra_player_info_t::teamname);
    constexpr std::size_t o391 = offsetof(goldsrc::cstrike::extra_player_info_t, dead);
    using t389 = decltype(goldsrc::cstrike::extra_player_info_t::dead);
    constexpr std::size_t o392 = offsetof(goldsrc::cstrike::extra_player_info_t, showhealth);
    using t390 = decltype(goldsrc::cstrike::extra_player_info_t::showhealth);
    constexpr std::size_t o393 = offsetof(goldsrc::cstrike::extra_player_info_t, health);
    using t391 = decltype(goldsrc::cstrike::extra_player_info_t::health);
    constexpr std::size_t o394 = offsetof(goldsrc::cstrike::extra_player_info_t, location);
    using t392 = decltype(goldsrc::cstrike::extra_player_info_t::location);
    constexpr std::size_t o395 = offsetof(goldsrc::cstrike::extra_player_info_t, sb_health);
    using t393 = decltype(goldsrc::cstrike::extra_player_info_t::sb_health);
    constexpr std::size_t s11 = sizeof(goldsrc::cstrike::CHudAmmo);
    constexpr std::size_t s12 = sizeof(goldsrc::cstrike::CHudAmmo::RGBA);
    constexpr std::size_t o396 = offsetof(goldsrc::cstrike::CHudAmmo::RGBA, r);
    using t394 = decltype(goldsrc::cstrike::CHudAmmo::RGBA::r);
    constexpr std::size_t o397 = offsetof(goldsrc::cstrike::CHudAmmo::RGBA, g);
    using t395 = decltype(goldsrc::cstrike::CHudAmmo::RGBA::g);
    constexpr std::size_t o398 = offsetof(goldsrc::cstrike::CHudAmmo::RGBA, b);
    using t396 = decltype(goldsrc::cstrike::CHudAmmo::RGBA::b);
    constexpr std::size_t o399 = offsetof(goldsrc::cstrike::CHudAmmo::RGBA, a);
    using t397 = decltype(goldsrc::cstrike::CHudAmmo::RGBA::a);
    constexpr std::size_t s13 = sizeof(goldsrc::cstrike::CHudAmmoSecondary);
    constexpr std::size_t s14 = sizeof(goldsrc::cstrike::CHudMessage);
    constexpr std::size_t s15 = sizeof(goldsrc::cstrike::CHudMessage::client_message_t);
    constexpr std::size_t o400 = offsetof(goldsrc::cstrike::CHudMessage::client_message_t, pMessage);
    using t398 = decltype(goldsrc::cstrike::CHudMessage::client_message_t::pMessage);
    constexpr std::size_t o401 = offsetof(goldsrc::cstrike::CHudMessage::client_message_t, font);
    constexpr std::size_t o402 = offsetof(goldsrc::cstrike::CHudMessage::client_message_t, args);
    using t399 = decltype(goldsrc::cstrike::CHudMessage::client_message_t::args);
    constexpr std::size_t o403 = offsetof(goldsrc::cstrike::CHudMessage::client_message_t, numArgs);
    using t400 = decltype(goldsrc::cstrike::CHudMessage::client_message_t::numArgs);
    constexpr std::size_t s16 = sizeof(goldsrc::cstrike::CHudNightVision);
    constexpr std::size_t o404 = offsetof(goldsrc::cstrike::CHudNightVision, m_fOn);
    using t401 = decltype(goldsrc::cstrike::CHudNightVision::m_fOn);
    constexpr std::size_t s17 = sizeof(goldsrc::cstrike::CHudRoundTimer);
    constexpr std::size_t o405 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_flTimeEnd);
    using t402 = decltype(goldsrc::cstrike::CHudRoundTimer::m_flTimeEnd);
    constexpr std::size_t o406 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_HUD_stopwatch);
    using t403 = decltype(goldsrc::cstrike::CHudRoundTimer::m_HUD_stopwatch);
    constexpr std::size_t o407 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_fFade);
    using t404 = decltype(goldsrc::cstrike::CHudRoundTimer::m_fFade);
    constexpr std::size_t o408 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_flNewPeriod);
    using t405 = decltype(goldsrc::cstrike::CHudRoundTimer::m_flNewPeriod);
    constexpr std::size_t o409 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_flNextToggle);
    using t406 = decltype(goldsrc::cstrike::CHudRoundTimer::m_flNextToggle);
    constexpr std::size_t o410 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_bPanicColor);
    using t407 = decltype(goldsrc::cstrike::CHudRoundTimer::m_bPanicColor);
    constexpr std::size_t o411 = offsetof(goldsrc::cstrike::CHudRoundTimer, m_closestRight);
    using t408 = decltype(goldsrc::cstrike::CHudRoundTimer::m_closestRight);
    constexpr std::size_t s18 = sizeof(goldsrc::cstrike::CHudStatusBar);
    constexpr std::size_t s19 = sizeof(goldsrc::cstrike::CHudStatusIcons);
    constexpr std::size_t s20 = sizeof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t);
    constexpr std::size_t o412 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, szSpriteName);
    using t409 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::szSpriteName);
    constexpr std::size_t o413 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, spr);
    using t410 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::spr);
    constexpr std::size_t o414 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, rc);
    using t411 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::rc);
    constexpr std::size_t o415 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, r);
    using t412 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::r);
    constexpr std::size_t o416 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, g);
    using t413 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::g);
    constexpr std::size_t o417 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, b);
    using t414 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::b);
    constexpr std::size_t o418 = offsetof(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t, bFlash);
    using t415 = decltype(goldsrc::cstrike::CHudStatusIcons::icon_sprite_t::bFlash);
    constexpr std::size_t s21 = sizeof(goldsrc::cstrike::DAMAGE_IMAGE);
    constexpr std::size_t o419 = offsetof(goldsrc::cstrike::DAMAGE_IMAGE, fExpire);
    using t416 = decltype(goldsrc::cstrike::DAMAGE_IMAGE::fExpire);
    constexpr std::size_t o420 = offsetof(goldsrc::cstrike::DAMAGE_IMAGE, fBaseline);
    using t417 = decltype(goldsrc::cstrike::DAMAGE_IMAGE::fBaseline);
    constexpr std::size_t o421 = offsetof(goldsrc::cstrike::DAMAGE_IMAGE, x);
    using t418 = decltype(goldsrc::cstrike::DAMAGE_IMAGE::x);
    constexpr std::size_t o422 = offsetof(goldsrc::cstrike::DAMAGE_IMAGE, y);
    using t419 = decltype(goldsrc::cstrike::DAMAGE_IMAGE::y);
    constexpr std::size_t s22 = sizeof(goldsrc::cstrike::CHudHealth);
    constexpr std::size_t o423 = offsetof(goldsrc::cstrike::CHudHealth, m_iHealth);
    using t420 = decltype(goldsrc::cstrike::CHudHealth::m_iHealth);
    constexpr std::size_t o424 = offsetof(goldsrc::cstrike::CHudHealth, m_HUD_dmg_bio);
    using t421 = decltype(goldsrc::cstrike::CHudHealth::m_HUD_dmg_bio);
    constexpr std::size_t o425 = offsetof(goldsrc::cstrike::CHudHealth, m_HUD_cross);
    using t422 = decltype(goldsrc::cstrike::CHudHealth::m_HUD_cross);
    constexpr std::size_t o426 = offsetof(goldsrc::cstrike::CHudHealth, m_fFade);
    using t423 = decltype(goldsrc::cstrike::CHudHealth::m_fFade);
    constexpr std::size_t o427 = offsetof(goldsrc::cstrike::CHudHealth, m_fAttackFront);
    using t424 = decltype(goldsrc::cstrike::CHudHealth::m_fAttackFront);
    constexpr std::size_t o428 = offsetof(goldsrc::cstrike::CHudHealth, m_fAttackRear);
    using t425 = decltype(goldsrc::cstrike::CHudHealth::m_fAttackRear);
    constexpr std::size_t o429 = offsetof(goldsrc::cstrike::CHudHealth, m_fAttackLeft);
    using t426 = decltype(goldsrc::cstrike::CHudHealth::m_fAttackLeft);
    constexpr std::size_t o430 = offsetof(goldsrc::cstrike::CHudHealth, m_fAttackRight);
    using t427 = decltype(goldsrc::cstrike::CHudHealth::m_fAttackRight);
    constexpr std::size_t o431 = offsetof(goldsrc::cstrike::CHudHealth, m_bTrackArray);
    using t428 = decltype(goldsrc::cstrike::CHudHealth::m_bTrackArray);
    constexpr std::size_t o432 = offsetof(goldsrc::cstrike::CHudHealth, m_iPlayerLastPointedAt);
    using t429 = decltype(goldsrc::cstrike::CHudHealth::m_iPlayerLastPointedAt);
    constexpr std::size_t o433 = offsetof(goldsrc::cstrike::CHudHealth, m_iPlayerNum);
    using t430 = decltype(goldsrc::cstrike::CHudHealth::m_iPlayerNum);
    constexpr std::size_t o434 = offsetof(goldsrc::cstrike::CHudHealth, m_bDrawRadar);
    using t431 = decltype(goldsrc::cstrike::CHudHealth::m_bDrawRadar);
    constexpr std::size_t o435 = offsetof(goldsrc::cstrike::CHudHealth, m_hSprite);
    using t432 = decltype(goldsrc::cstrike::CHudHealth::m_hSprite);
    constexpr std::size_t o436 = offsetof(goldsrc::cstrike::CHudHealth, m_hDamage);
    using t433 = decltype(goldsrc::cstrike::CHudHealth::m_hDamage);
    constexpr std::size_t o437 = offsetof(goldsrc::cstrike::CHudHealth, m_hrad);
    using t434 = decltype(goldsrc::cstrike::CHudHealth::m_hrad);
    constexpr std::size_t o438 = offsetof(goldsrc::cstrike::CHudHealth, m_hradopaque);
    using t435 = decltype(goldsrc::cstrike::CHudHealth::m_hradopaque);
    constexpr std::size_t o439 = offsetof(goldsrc::cstrike::CHudHealth, m_hRadar);
    using t436 = decltype(goldsrc::cstrike::CHudHealth::m_hRadar);
    constexpr std::size_t o440 = offsetof(goldsrc::cstrike::CHudHealth, m_hRadaropaque);
    using t437 = decltype(goldsrc::cstrike::CHudHealth::m_hRadaropaque);
    constexpr std::size_t o441 = offsetof(goldsrc::cstrike::CHudHealth, m_dmg);
    using t438 = decltype(goldsrc::cstrike::CHudHealth::m_dmg);
    constexpr std::size_t o442 = offsetof(goldsrc::cstrike::CHudHealth, m_bitsDamage);
    using t439 = decltype(goldsrc::cstrike::CHudHealth::m_bitsDamage);
    constexpr std::size_t s23 = sizeof(goldsrc::cvar_t);
    constexpr std::size_t o443 = offsetof(goldsrc::cvar_t, name);
    using t440 = decltype(goldsrc::cvar_t::name);
    constexpr std::size_t o444 = offsetof(goldsrc::cvar_t, string);
    using t441 = decltype(goldsrc::cvar_t::string);
    constexpr std::size_t o445 = offsetof(goldsrc::cvar_t, flags);
    using t442 = decltype(goldsrc::cvar_t::flags);
    constexpr std::size_t o446 = offsetof(goldsrc::cvar_t, value);
    using t443 = decltype(goldsrc::cvar_t::value);
    constexpr std::size_t o447 = offsetof(goldsrc::cvar_t, next);
    using t444 = decltype(goldsrc::cvar_t::next);
    constexpr std::size_t s24 = sizeof(goldsrc::cmd_t);
    constexpr std::size_t o448 = offsetof(goldsrc::cmd_t, cmd);
    using t445 = decltype(goldsrc::cmd_t::cmd);
    constexpr std::size_t o449 = offsetof(goldsrc::cmd_t, senttime);
    using t446 = decltype(goldsrc::cmd_t::senttime);
    constexpr std::size_t o450 = offsetof(goldsrc::cmd_t, receivedtime);
    using t447 = decltype(goldsrc::cmd_t::receivedtime);
    constexpr std::size_t o451 = offsetof(goldsrc::cmd_t, frame_lerp);
    using t448 = decltype(goldsrc::cmd_t::frame_lerp);
    constexpr std::size_t o452 = offsetof(goldsrc::cmd_t, processedfuncs);
    using t449 = decltype(goldsrc::cmd_t::processedfuncs);
    constexpr std::size_t o453 = offsetof(goldsrc::cmd_t, heldback);
    using t450 = decltype(goldsrc::cmd_t::heldback);
    constexpr std::size_t o454 = offsetof(goldsrc::cmd_t, sendsize);
    using t451 = decltype(goldsrc::cmd_t::sendsize);
    constexpr std::size_t s25 = sizeof(goldsrc::consistency_t);
    constexpr std::size_t o455 = offsetof(goldsrc::consistency_t, filename);
    using t452 = decltype(goldsrc::consistency_t::filename);
    constexpr std::size_t o456 = offsetof(goldsrc::consistency_t, issound);
    using t453 = decltype(goldsrc::consistency_t::issound);
    constexpr std::size_t o457 = offsetof(goldsrc::consistency_t, orig_index);
    using t454 = decltype(goldsrc::consistency_t::orig_index);
    constexpr std::size_t o458 = offsetof(goldsrc::consistency_t, value);
    using t455 = decltype(goldsrc::consistency_t::value);
    constexpr std::size_t o459 = offsetof(goldsrc::consistency_t, check_type);
    using t456 = decltype(goldsrc::consistency_t::check_type);
    constexpr std::size_t o460 = offsetof(goldsrc::consistency_t, mins);
    using t457 = decltype(goldsrc::consistency_t::mins);
    constexpr std::size_t o461 = offsetof(goldsrc::consistency_t, maxs);
    using t458 = decltype(goldsrc::consistency_t::maxs);
    constexpr std::size_t s26 = sizeof(goldsrc::packet_entities_t);
    constexpr std::size_t o462 = offsetof(goldsrc::packet_entities_t, num_entities);
    using t459 = decltype(goldsrc::packet_entities_t::num_entities);
    constexpr std::size_t o463 = offsetof(goldsrc::packet_entities_t, flags);
    using t460 = decltype(goldsrc::packet_entities_t::flags);
    constexpr std::size_t o464 = offsetof(goldsrc::packet_entities_t, entities);
    using t461 = decltype(goldsrc::packet_entities_t::entities);
    constexpr std::size_t s27 = sizeof(goldsrc::player_info_t);
    constexpr std::size_t o465 = offsetof(goldsrc::player_info_t, userid);
    using t462 = decltype(goldsrc::player_info_t::userid);
    constexpr std::size_t o466 = offsetof(goldsrc::player_info_t, userinfo);
    using t463 = decltype(goldsrc::player_info_t::userinfo);
    constexpr std::size_t o467 = offsetof(goldsrc::player_info_t, name);
    using t464 = decltype(goldsrc::player_info_t::name);
    constexpr std::size_t o468 = offsetof(goldsrc::player_info_t, spectator);
    using t465 = decltype(goldsrc::player_info_t::spectator);
    constexpr std::size_t o469 = offsetof(goldsrc::player_info_t, ping);
    using t466 = decltype(goldsrc::player_info_t::ping);
    constexpr std::size_t o470 = offsetof(goldsrc::player_info_t, packet_loss);
    using t467 = decltype(goldsrc::player_info_t::packet_loss);
    constexpr std::size_t o471 = offsetof(goldsrc::player_info_t, model);
    using t468 = decltype(goldsrc::player_info_t::model);
    constexpr std::size_t o472 = offsetof(goldsrc::player_info_t, topcolor);
    using t469 = decltype(goldsrc::player_info_t::topcolor);
    constexpr std::size_t o473 = offsetof(goldsrc::player_info_t, bottomcolor);
    using t470 = decltype(goldsrc::player_info_t::bottomcolor);
    constexpr std::size_t o474 = offsetof(goldsrc::player_info_t, renderframe);
    using t471 = decltype(goldsrc::player_info_t::renderframe);
    constexpr std::size_t o475 = offsetof(goldsrc::player_info_t, gaitsequence);
    using t472 = decltype(goldsrc::player_info_t::gaitsequence);
    constexpr std::size_t o476 = offsetof(goldsrc::player_info_t, gaitframe);
    using t473 = decltype(goldsrc::player_info_t::gaitframe);
    constexpr std::size_t o477 = offsetof(goldsrc::player_info_t, gaityaw);
    using t474 = decltype(goldsrc::player_info_t::gaityaw);
    constexpr std::size_t o478 = offsetof(goldsrc::player_info_t, prevgaitorigin);
    using t475 = decltype(goldsrc::player_info_t::prevgaitorigin);
    constexpr std::size_t o479 = offsetof(goldsrc::player_info_t, customdata);
    using t476 = decltype(goldsrc::player_info_t::customdata);
    constexpr std::size_t o480 = offsetof(goldsrc::player_info_t, hashedcdkey);
    using t477 = decltype(goldsrc::player_info_t::hashedcdkey);
    constexpr std::size_t o481 = offsetof(goldsrc::player_info_t, m_nSteamID);
    using t478 = decltype(goldsrc::player_info_t::m_nSteamID);
    constexpr std::size_t s28 = sizeof(goldsrc::soundfade_t);
    constexpr std::size_t o482 = offsetof(goldsrc::soundfade_t, nStartPercent);
    using t479 = decltype(goldsrc::soundfade_t::nStartPercent);
    constexpr std::size_t o483 = offsetof(goldsrc::soundfade_t, nClientSoundFadePercent);
    using t480 = decltype(goldsrc::soundfade_t::nClientSoundFadePercent);
    constexpr std::size_t o484 = offsetof(goldsrc::soundfade_t, soundFadeStartTime);
    using t481 = decltype(goldsrc::soundfade_t::soundFadeStartTime);
    constexpr std::size_t o485 = offsetof(goldsrc::soundfade_t, soundFadeOutTime);
    using t482 = decltype(goldsrc::soundfade_t::soundFadeOutTime);
    constexpr std::size_t o486 = offsetof(goldsrc::soundfade_t, soundFadeHoldTime);
    using t483 = decltype(goldsrc::soundfade_t::soundFadeHoldTime);
    constexpr std::size_t o487 = offsetof(goldsrc::soundfade_t, soundFadeInTime);
    using t484 = decltype(goldsrc::soundfade_t::soundFadeInTime);
    constexpr std::size_t s29 = sizeof(goldsrc::client_static_t);
    constexpr std::size_t o488 = offsetof(goldsrc::client_static_t, state);
    using t485 = decltype(goldsrc::client_static_t::state);
    constexpr std::size_t o489 = offsetof(goldsrc::client_static_t, netchan);
    using t486 = decltype(goldsrc::client_static_t::netchan);
    constexpr std::size_t o490 = offsetof(goldsrc::client_static_t, datagram);
    using t487 = decltype(goldsrc::client_static_t::datagram);
    constexpr std::size_t o491 = offsetof(goldsrc::client_static_t, datagram_buf);
    using t488 = decltype(goldsrc::client_static_t::datagram_buf);
    constexpr std::size_t o492 = offsetof(goldsrc::client_static_t, connect_time);
    using t489 = decltype(goldsrc::client_static_t::connect_time);
    constexpr std::size_t o493 = offsetof(goldsrc::client_static_t, connect_retry);
    using t490 = decltype(goldsrc::client_static_t::connect_retry);
    constexpr std::size_t o494 = offsetof(goldsrc::client_static_t, challenge);
    using t491 = decltype(goldsrc::client_static_t::challenge);
    constexpr std::size_t o495 = offsetof(goldsrc::client_static_t, authprotocol);
    using t492 = decltype(goldsrc::client_static_t::authprotocol);
    constexpr std::size_t o496 = offsetof(goldsrc::client_static_t, userid);
    using t493 = decltype(goldsrc::client_static_t::userid);
    constexpr std::size_t o497 = offsetof(goldsrc::client_static_t, trueaddress);
    using t494 = decltype(goldsrc::client_static_t::trueaddress);
    constexpr std::size_t o498 = offsetof(goldsrc::client_static_t, slist_time);
    using t495 = decltype(goldsrc::client_static_t::slist_time);
    constexpr std::size_t o499 = offsetof(goldsrc::client_static_t, signon);
    using t496 = decltype(goldsrc::client_static_t::signon);
    constexpr std::size_t o500 = offsetof(goldsrc::client_static_t, servername);
    using t497 = decltype(goldsrc::client_static_t::servername);
    constexpr std::size_t o501 = offsetof(goldsrc::client_static_t, mapstring);
    using t498 = decltype(goldsrc::client_static_t::mapstring);
    constexpr std::size_t o502 = offsetof(goldsrc::client_static_t, spawnparms);
    using t499 = decltype(goldsrc::client_static_t::spawnparms);
    constexpr std::size_t o503 = offsetof(goldsrc::client_static_t, userinfo);
    using t500 = decltype(goldsrc::client_static_t::userinfo);
    constexpr std::size_t o504 = offsetof(goldsrc::client_static_t, nextcmdtime);
    using t501 = decltype(goldsrc::client_static_t::nextcmdtime);
    constexpr std::size_t o505 = offsetof(goldsrc::client_static_t, lastoutgoingcommand);
    using t502 = decltype(goldsrc::client_static_t::lastoutgoingcommand);
    constexpr std::size_t o506 = offsetof(goldsrc::client_static_t, demonum);
    using t503 = decltype(goldsrc::client_static_t::demonum);
    constexpr std::size_t o507 = offsetof(goldsrc::client_static_t, demos);
    using t504 = decltype(goldsrc::client_static_t::demos);
    constexpr std::size_t o508 = offsetof(goldsrc::client_static_t, demorecording);
    using t505 = decltype(goldsrc::client_static_t::demorecording);
    constexpr std::size_t o509 = offsetof(goldsrc::client_static_t, demoplayback);
    using t506 = decltype(goldsrc::client_static_t::demoplayback);
    constexpr std::size_t o510 = offsetof(goldsrc::client_static_t, timedemo);
    using t507 = decltype(goldsrc::client_static_t::timedemo);
    constexpr std::size_t o511 = offsetof(goldsrc::client_static_t, demostarttime);
    using t508 = decltype(goldsrc::client_static_t::demostarttime);
    constexpr std::size_t o512 = offsetof(goldsrc::client_static_t, demostartframe);
    using t509 = decltype(goldsrc::client_static_t::demostartframe);
    constexpr std::size_t o513 = offsetof(goldsrc::client_static_t, forcetrack);
    using t510 = decltype(goldsrc::client_static_t::forcetrack);
    constexpr std::size_t o514 = offsetof(goldsrc::client_static_t, demofile);
    constexpr std::size_t o515 = offsetof(goldsrc::client_static_t, demoheader);
    constexpr std::size_t o516 = offsetof(goldsrc::client_static_t, demowaiting);
    using t511 = decltype(goldsrc::client_static_t::demowaiting);
    constexpr std::size_t o517 = offsetof(goldsrc::client_static_t, demoappending);
    using t512 = decltype(goldsrc::client_static_t::demoappending);
    constexpr std::size_t o518 = offsetof(goldsrc::client_static_t, demofilename);
    using t513 = decltype(goldsrc::client_static_t::demofilename);
    constexpr std::size_t o519 = offsetof(goldsrc::client_static_t, demoframecount);
    using t514 = decltype(goldsrc::client_static_t::demoframecount);
    constexpr std::size_t o520 = offsetof(goldsrc::client_static_t, td_lastframe);
    using t515 = decltype(goldsrc::client_static_t::td_lastframe);
    constexpr std::size_t o521 = offsetof(goldsrc::client_static_t, td_startframe);
    using t516 = decltype(goldsrc::client_static_t::td_startframe);
    constexpr std::size_t o522 = offsetof(goldsrc::client_static_t, td_starttime);
    using t517 = decltype(goldsrc::client_static_t::td_starttime);
    constexpr std::size_t o523 = offsetof(goldsrc::client_static_t, dl);
    using t518 = decltype(goldsrc::client_static_t::dl);
    constexpr std::size_t o524 = offsetof(goldsrc::client_static_t, packet_loss);
    using t519 = decltype(goldsrc::client_static_t::packet_loss);
    constexpr std::size_t o525 = offsetof(goldsrc::client_static_t, packet_loss_recalc_time);
    using t520 = decltype(goldsrc::client_static_t::packet_loss_recalc_time);
    constexpr std::size_t o526 = offsetof(goldsrc::client_static_t, playerbits);
    using t521 = decltype(goldsrc::client_static_t::playerbits);
    constexpr std::size_t o527 = offsetof(goldsrc::client_static_t, soundfade);
    using t522 = decltype(goldsrc::client_static_t::soundfade);
    constexpr std::size_t o528 = offsetof(goldsrc::client_static_t, physinfo);
    using t523 = decltype(goldsrc::client_static_t::physinfo);
    constexpr std::size_t o529 = offsetof(goldsrc::client_static_t, md5_clientdll);
    using t524 = decltype(goldsrc::client_static_t::md5_clientdll);
    constexpr std::size_t o530 = offsetof(goldsrc::client_static_t, game_stream);
    using t525 = decltype(goldsrc::client_static_t::game_stream);
    constexpr std::size_t o531 = offsetof(goldsrc::client_static_t, connect_stream);
    using t526 = decltype(goldsrc::client_static_t::connect_stream);
    constexpr std::size_t o532 = offsetof(goldsrc::client_static_t, passive);
    using t527 = decltype(goldsrc::client_static_t::passive);
    constexpr std::size_t o533 = offsetof(goldsrc::client_static_t, spectator);
    using t528 = decltype(goldsrc::client_static_t::spectator);
    constexpr std::size_t o534 = offsetof(goldsrc::client_static_t, director);
    using t529 = decltype(goldsrc::client_static_t::director);
    constexpr std::size_t o535 = offsetof(goldsrc::client_static_t, fSecureClient);
    using t530 = decltype(goldsrc::client_static_t::fSecureClient);
    constexpr std::size_t o536 = offsetof(goldsrc::client_static_t, isVAC2Secure);
    using t531 = decltype(goldsrc::client_static_t::isVAC2Secure);
    constexpr std::size_t o537 = offsetof(goldsrc::client_static_t, GameServerSteamID);
    using t532 = decltype(goldsrc::client_static_t::GameServerSteamID);
    constexpr std::size_t o538 = offsetof(goldsrc::client_static_t, build_num);
    using t533 = decltype(goldsrc::client_static_t::build_num);
    constexpr std::size_t s30 = sizeof(goldsrc::frame_t);
    constexpr std::size_t o539 = offsetof(goldsrc::frame_t, receivedtime);
    using t534 = decltype(goldsrc::frame_t::receivedtime);
    constexpr std::size_t o540 = offsetof(goldsrc::frame_t, latency);
    using t535 = decltype(goldsrc::frame_t::latency);
    constexpr std::size_t o541 = offsetof(goldsrc::frame_t, invalid);
    using t536 = decltype(goldsrc::frame_t::invalid);
    constexpr std::size_t o542 = offsetof(goldsrc::frame_t, choked);
    using t537 = decltype(goldsrc::frame_t::choked);
    constexpr std::size_t o543 = offsetof(goldsrc::frame_t, playerstate);
    using t538 = decltype(goldsrc::frame_t::playerstate);
    constexpr std::size_t o544 = offsetof(goldsrc::frame_t, time);
    using t539 = decltype(goldsrc::frame_t::time);
    constexpr std::size_t o545 = offsetof(goldsrc::frame_t, clientdata);
    using t540 = decltype(goldsrc::frame_t::clientdata);
    constexpr std::size_t o546 = offsetof(goldsrc::frame_t, weapondata);
    using t541 = decltype(goldsrc::frame_t::weapondata);
    constexpr std::size_t o547 = offsetof(goldsrc::frame_t, packet_entities);
    using t542 = decltype(goldsrc::frame_t::packet_entities);
    constexpr std::size_t o548 = offsetof(goldsrc::frame_t, clientbytes);
    using t543 = decltype(goldsrc::frame_t::clientbytes);
    constexpr std::size_t o549 = offsetof(goldsrc::frame_t, playerinfobytes);
    using t544 = decltype(goldsrc::frame_t::playerinfobytes);
    constexpr std::size_t o550 = offsetof(goldsrc::frame_t, packetentitybytes);
    using t545 = decltype(goldsrc::frame_t::packetentitybytes);
    constexpr std::size_t o551 = offsetof(goldsrc::frame_t, tentitybytes);
    using t546 = decltype(goldsrc::frame_t::tentitybytes);
    constexpr std::size_t o552 = offsetof(goldsrc::frame_t, soundbytes);
    using t547 = decltype(goldsrc::frame_t::soundbytes);
    constexpr std::size_t o553 = offsetof(goldsrc::frame_t, eventbytes);
    using t548 = decltype(goldsrc::frame_t::eventbytes);
    constexpr std::size_t o554 = offsetof(goldsrc::frame_t, usrbytes);
    using t549 = decltype(goldsrc::frame_t::usrbytes);
    constexpr std::size_t o555 = offsetof(goldsrc::frame_t, voicebytes);
    using t550 = decltype(goldsrc::frame_t::voicebytes);
    constexpr std::size_t o556 = offsetof(goldsrc::frame_t, msgbytes);
    using t551 = decltype(goldsrc::frame_t::msgbytes);
    constexpr std::size_t s31 = sizeof(goldsrc::client_state_t);
    constexpr std::size_t o557 = offsetof(goldsrc::client_state_t, max_edicts);
    using t552 = decltype(goldsrc::client_state_t::max_edicts);
    constexpr std::size_t o558 = offsetof(goldsrc::client_state_t, resourcesonhand);
    using t553 = decltype(goldsrc::client_state_t::resourcesonhand);
    constexpr std::size_t o559 = offsetof(goldsrc::client_state_t, resourcesneeded);
    using t554 = decltype(goldsrc::client_state_t::resourcesneeded);
    constexpr std::size_t o560 = offsetof(goldsrc::client_state_t, resourcelist);
    using t555 = decltype(goldsrc::client_state_t::resourcelist);
    constexpr std::size_t o561 = offsetof(goldsrc::client_state_t, num_resources);
    using t556 = decltype(goldsrc::client_state_t::num_resources);
    constexpr std::size_t o562 = offsetof(goldsrc::client_state_t, need_force_consistency_response);
    using t557 = decltype(goldsrc::client_state_t::need_force_consistency_response);
    constexpr std::size_t o563 = offsetof(goldsrc::client_state_t, serverinfo);
    using t558 = decltype(goldsrc::client_state_t::serverinfo);
    constexpr std::size_t o564 = offsetof(goldsrc::client_state_t, servercount);
    using t559 = decltype(goldsrc::client_state_t::servercount);
    constexpr std::size_t o565 = offsetof(goldsrc::client_state_t, validsequence);
    using t560 = decltype(goldsrc::client_state_t::validsequence);
    constexpr std::size_t o566 = offsetof(goldsrc::client_state_t, parsecount);
    using t561 = decltype(goldsrc::client_state_t::parsecount);
    constexpr std::size_t o567 = offsetof(goldsrc::client_state_t, parsecountmod);
    using t562 = decltype(goldsrc::client_state_t::parsecountmod);
    constexpr std::size_t o568 = offsetof(goldsrc::client_state_t, stats);
    using t563 = decltype(goldsrc::client_state_t::stats);
    constexpr std::size_t o569 = offsetof(goldsrc::client_state_t, weapons);
    using t564 = decltype(goldsrc::client_state_t::weapons);
    constexpr std::size_t o570 = offsetof(goldsrc::client_state_t, cmd);
    using t565 = decltype(goldsrc::client_state_t::cmd);
    constexpr std::size_t o571 = offsetof(goldsrc::client_state_t, viewangles);
    using t566 = decltype(goldsrc::client_state_t::viewangles);
    constexpr std::size_t o572 = offsetof(goldsrc::client_state_t, punchangle);
    using t567 = decltype(goldsrc::client_state_t::punchangle);
    constexpr std::size_t o573 = offsetof(goldsrc::client_state_t, crosshairangle);
    using t568 = decltype(goldsrc::client_state_t::crosshairangle);
    constexpr std::size_t o574 = offsetof(goldsrc::client_state_t, simorg);
    using t569 = decltype(goldsrc::client_state_t::simorg);
    constexpr std::size_t o575 = offsetof(goldsrc::client_state_t, simvel);
    using t570 = decltype(goldsrc::client_state_t::simvel);
    constexpr std::size_t o576 = offsetof(goldsrc::client_state_t, simangles);
    using t571 = decltype(goldsrc::client_state_t::simangles);
    constexpr std::size_t o577 = offsetof(goldsrc::client_state_t, predicted_origins);
    using t572 = decltype(goldsrc::client_state_t::predicted_origins);
    constexpr std::size_t o578 = offsetof(goldsrc::client_state_t, prediction_error);
    using t573 = decltype(goldsrc::client_state_t::prediction_error);
    constexpr std::size_t o579 = offsetof(goldsrc::client_state_t, idealpitch);
    using t574 = decltype(goldsrc::client_state_t::idealpitch);
    constexpr std::size_t o580 = offsetof(goldsrc::client_state_t, viewheight);
    using t575 = decltype(goldsrc::client_state_t::viewheight);
    constexpr std::size_t o581 = offsetof(goldsrc::client_state_t, sf);
    using t576 = decltype(goldsrc::client_state_t::sf);
    constexpr std::size_t o582 = offsetof(goldsrc::client_state_t, paused);
    using t577 = decltype(goldsrc::client_state_t::paused);
    constexpr std::size_t o583 = offsetof(goldsrc::client_state_t, onground);
    using t578 = decltype(goldsrc::client_state_t::onground);
    constexpr std::size_t o584 = offsetof(goldsrc::client_state_t, moving);
    using t579 = decltype(goldsrc::client_state_t::moving);
    constexpr std::size_t o585 = offsetof(goldsrc::client_state_t, waterlevel);
    using t580 = decltype(goldsrc::client_state_t::waterlevel);
    constexpr std::size_t o586 = offsetof(goldsrc::client_state_t, usehull);
    using t581 = decltype(goldsrc::client_state_t::usehull);
    constexpr std::size_t o587 = offsetof(goldsrc::client_state_t, maxspeed);
    using t582 = decltype(goldsrc::client_state_t::maxspeed);
    constexpr std::size_t o588 = offsetof(goldsrc::client_state_t, pushmsec);
    using t583 = decltype(goldsrc::client_state_t::pushmsec);
    constexpr std::size_t o589 = offsetof(goldsrc::client_state_t, light_level);
    using t584 = decltype(goldsrc::client_state_t::light_level);
    constexpr std::size_t o590 = offsetof(goldsrc::client_state_t, intermission);
    using t585 = decltype(goldsrc::client_state_t::intermission);
    constexpr std::size_t o591 = offsetof(goldsrc::client_state_t, mtime);
    using t586 = decltype(goldsrc::client_state_t::mtime);
    constexpr std::size_t o592 = offsetof(goldsrc::client_state_t, time);
    using t587 = decltype(goldsrc::client_state_t::time);
    constexpr std::size_t o593 = offsetof(goldsrc::client_state_t, oldtime);
    using t588 = decltype(goldsrc::client_state_t::oldtime);
    constexpr std::size_t o594 = offsetof(goldsrc::client_state_t, frames);
    using t589 = decltype(goldsrc::client_state_t::frames);
    constexpr std::size_t o595 = offsetof(goldsrc::client_state_t, commands);
    using t590 = decltype(goldsrc::client_state_t::commands);
    constexpr std::size_t o596 = offsetof(goldsrc::client_state_t, predicted_frames);
    using t591 = decltype(goldsrc::client_state_t::predicted_frames);
    constexpr std::size_t o597 = offsetof(goldsrc::client_state_t, delta_sequence);
    using t592 = decltype(goldsrc::client_state_t::delta_sequence);
    constexpr std::size_t o598 = offsetof(goldsrc::client_state_t, playernum);
    using t593 = decltype(goldsrc::client_state_t::playernum);
    constexpr std::size_t o599 = offsetof(goldsrc::client_state_t, event_precache);
    using t594 = decltype(goldsrc::client_state_t::event_precache);
    constexpr std::size_t o600 = offsetof(goldsrc::client_state_t, model_precache);
    using t595 = decltype(goldsrc::client_state_t::model_precache);
    constexpr std::size_t o601 = offsetof(goldsrc::client_state_t, model_precache_count);
    using t596 = decltype(goldsrc::client_state_t::model_precache_count);
    constexpr std::size_t o602 = offsetof(goldsrc::client_state_t, sound_precache);
    using t597 = decltype(goldsrc::client_state_t::sound_precache);
    constexpr std::size_t o603 = offsetof(goldsrc::client_state_t, consistency_list);
    using t598 = decltype(goldsrc::client_state_t::consistency_list);
    constexpr std::size_t o604 = offsetof(goldsrc::client_state_t, num_consistency);
    using t599 = decltype(goldsrc::client_state_t::num_consistency);
    constexpr std::size_t o605 = offsetof(goldsrc::client_state_t, highentity);
    using t600 = decltype(goldsrc::client_state_t::highentity);
    constexpr std::size_t o606 = offsetof(goldsrc::client_state_t, levelname);
    using t601 = decltype(goldsrc::client_state_t::levelname);
    constexpr std::size_t o607 = offsetof(goldsrc::client_state_t, maxclients);
    using t602 = decltype(goldsrc::client_state_t::maxclients);
    constexpr std::size_t o608 = offsetof(goldsrc::client_state_t, gametype);
    using t603 = decltype(goldsrc::client_state_t::gametype);
    constexpr std::size_t o609 = offsetof(goldsrc::client_state_t, viewentity);
    using t604 = decltype(goldsrc::client_state_t::viewentity);
    constexpr std::size_t o610 = offsetof(goldsrc::client_state_t, worldmodel);
    using t605 = decltype(goldsrc::client_state_t::worldmodel);
    constexpr std::size_t o611 = offsetof(goldsrc::client_state_t, free_efrags);
    using t606 = decltype(goldsrc::client_state_t::free_efrags);
    constexpr std::size_t o612 = offsetof(goldsrc::client_state_t, num_entities);
    using t607 = decltype(goldsrc::client_state_t::num_entities);
    constexpr std::size_t o613 = offsetof(goldsrc::client_state_t, num_statics);
    using t608 = decltype(goldsrc::client_state_t::num_statics);
    constexpr std::size_t o614 = offsetof(goldsrc::client_state_t, viewent);
    using t609 = decltype(goldsrc::client_state_t::viewent);
    constexpr std::size_t o615 = offsetof(goldsrc::client_state_t, cdtrack);
    using t610 = decltype(goldsrc::client_state_t::cdtrack);
    constexpr std::size_t o616 = offsetof(goldsrc::client_state_t, looptrack);
    using t611 = decltype(goldsrc::client_state_t::looptrack);
    constexpr std::size_t o617 = offsetof(goldsrc::client_state_t, serverCRC);
    using t612 = decltype(goldsrc::client_state_t::serverCRC);
    constexpr std::size_t o618 = offsetof(goldsrc::client_state_t, clientdllmd5);
    using t613 = decltype(goldsrc::client_state_t::clientdllmd5);
    constexpr std::size_t o619 = offsetof(goldsrc::client_state_t, weaponstarttime);
    using t614 = decltype(goldsrc::client_state_t::weaponstarttime);
    constexpr std::size_t o620 = offsetof(goldsrc::client_state_t, weaponsequence);
    using t615 = decltype(goldsrc::client_state_t::weaponsequence);
    constexpr std::size_t o621 = offsetof(goldsrc::client_state_t, fPrecaching);
    using t616 = decltype(goldsrc::client_state_t::fPrecaching);
    constexpr std::size_t o622 = offsetof(goldsrc::client_state_t, pLight);
    using t617 = decltype(goldsrc::client_state_t::pLight);
    constexpr std::size_t o623 = offsetof(goldsrc::client_state_t, players);
    using t618 = decltype(goldsrc::client_state_t::players);
    constexpr std::size_t o624 = offsetof(goldsrc::client_state_t, instanced_baseline);
    using t619 = decltype(goldsrc::client_state_t::instanced_baseline);
    constexpr std::size_t o625 = offsetof(goldsrc::client_state_t, instanced_baseline_number);
    using t620 = decltype(goldsrc::client_state_t::instanced_baseline_number);
    constexpr std::size_t o626 = offsetof(goldsrc::client_state_t, mapCRC);
    using t621 = decltype(goldsrc::client_state_t::mapCRC);
    constexpr std::size_t o627 = offsetof(goldsrc::client_state_t, events);
    using t622 = decltype(goldsrc::client_state_t::events);
    constexpr std::size_t o628 = offsetof(goldsrc::client_state_t, downloadUrl);
    using t623 = decltype(goldsrc::client_state_t::downloadUrl);
    constexpr std::size_t s32 = sizeof(goldsrc::event_info_s);
    constexpr std::size_t o629 = offsetof(goldsrc::event_info_s, index);
    using t624 = decltype(goldsrc::event_info_s::index);
    constexpr std::size_t o630 = offsetof(goldsrc::event_info_s, packet_index);
    using t625 = decltype(goldsrc::event_info_s::packet_index);
    constexpr std::size_t o631 = offsetof(goldsrc::event_info_s, entity_index);
    using t626 = decltype(goldsrc::event_info_s::entity_index);
    constexpr std::size_t o632 = offsetof(goldsrc::event_info_s, fire_time);
    using t627 = decltype(goldsrc::event_info_s::fire_time);
    constexpr std::size_t o633 = offsetof(goldsrc::event_info_s, args);
    using t628 = decltype(goldsrc::event_info_s::args);
    constexpr std::size_t o634 = offsetof(goldsrc::event_info_s, flags);
    using t629 = decltype(goldsrc::event_info_s::flags);
    constexpr std::size_t s33 = sizeof(goldsrc::event_t);
    constexpr std::size_t o635 = offsetof(goldsrc::event_t, index);
    using t630 = decltype(goldsrc::event_t::index);
    constexpr std::size_t o636 = offsetof(goldsrc::event_t, filename);
    using t631 = decltype(goldsrc::event_t::filename);
    constexpr std::size_t o637 = offsetof(goldsrc::event_t, filesize);
    using t632 = decltype(goldsrc::event_t::filesize);
    constexpr std::size_t o638 = offsetof(goldsrc::event_t, pszScript);
    using t633 = decltype(goldsrc::event_t::pszScript);
    constexpr std::size_t s34 = sizeof(goldsrc::event_state_t);
    constexpr std::size_t o639 = offsetof(goldsrc::event_state_t, ei);
    using t634 = decltype(goldsrc::event_state_t::ei);
    constexpr std::size_t s35 = sizeof(goldsrc::cacheentry_t);
    constexpr std::size_t o640 = offsetof(goldsrc::cacheentry_t, name);
    using t635 = decltype(goldsrc::cacheentry_t::name);
    constexpr std::size_t o641 = offsetof(goldsrc::cacheentry_t, cache);
    using t636 = decltype(goldsrc::cacheentry_t::cache);
    constexpr std::size_t s36 = sizeof(goldsrc::glpoly_t);
    constexpr std::size_t o642 = offsetof(goldsrc::glpoly_t, next);
    using t637 = decltype(goldsrc::glpoly_t::next);
    constexpr std::size_t o643 = offsetof(goldsrc::glpoly_t, chain);
    using t638 = decltype(goldsrc::glpoly_t::chain);
    constexpr std::size_t o644 = offsetof(goldsrc::glpoly_t, numverts);
    using t639 = decltype(goldsrc::glpoly_t::numverts);
    constexpr std::size_t o645 = offsetof(goldsrc::glpoly_t, flags);
    using t640 = decltype(goldsrc::glpoly_t::flags);
    constexpr std::size_t o646 = offsetof(goldsrc::glpoly_t, verts);
    using t641 = decltype(goldsrc::glpoly_t::verts);
    constexpr std::size_t s37 = sizeof(goldsrc::hull_t);
    constexpr std::size_t o647 = offsetof(goldsrc::hull_t, clipnodes);
    using t642 = decltype(goldsrc::hull_t::clipnodes);
    constexpr std::size_t o648 = offsetof(goldsrc::hull_t, planes);
    using t643 = decltype(goldsrc::hull_t::planes);
    constexpr std::size_t o649 = offsetof(goldsrc::hull_t, firstclipnode);
    using t644 = decltype(goldsrc::hull_t::firstclipnode);
    constexpr std::size_t o650 = offsetof(goldsrc::hull_t, lastclipnode);
    using t645 = decltype(goldsrc::hull_t::lastclipnode);
    constexpr std::size_t o651 = offsetof(goldsrc::hull_t, clip_mins);
    using t646 = decltype(goldsrc::hull_t::clip_mins);
    constexpr std::size_t o652 = offsetof(goldsrc::hull_t, clip_maxs);
    using t647 = decltype(goldsrc::hull_t::clip_maxs);
    constexpr std::size_t s38 = sizeof(goldsrc::medge_t);
    constexpr std::size_t o653 = offsetof(goldsrc::medge_t, v);
    using t648 = decltype(goldsrc::medge_t::v);
    constexpr std::size_t s39 = sizeof(goldsrc::mod_known_info_t);
    constexpr std::size_t o654 = offsetof(goldsrc::mod_known_info_t, shouldCRC);
    using t649 = decltype(goldsrc::mod_known_info_t::shouldCRC);
    constexpr std::size_t o655 = offsetof(goldsrc::mod_known_info_t, firstCRCDone);
    using t650 = decltype(goldsrc::mod_known_info_t::firstCRCDone);
    constexpr std::size_t o656 = offsetof(goldsrc::mod_known_info_t, initialCRC);
    using t651 = decltype(goldsrc::mod_known_info_t::initialCRC);
    constexpr std::size_t s40 = sizeof(goldsrc::mplane_t);
    constexpr std::size_t o657 = offsetof(goldsrc::mplane_t, normal);
    using t652 = decltype(goldsrc::mplane_t::normal);
    constexpr std::size_t s41 = sizeof(goldsrc::mspriteframe_t);
    constexpr std::size_t o658 = offsetof(goldsrc::mspriteframe_t, width);
    using t653 = decltype(goldsrc::mspriteframe_t::width);
    constexpr std::size_t o659 = offsetof(goldsrc::mspriteframe_t, height);
    using t654 = decltype(goldsrc::mspriteframe_t::height);
    constexpr std::size_t o660 = offsetof(goldsrc::mspriteframe_t, up);
    using t655 = decltype(goldsrc::mspriteframe_t::up);
    constexpr std::size_t o661 = offsetof(goldsrc::mspriteframe_t, down);
    using t656 = decltype(goldsrc::mspriteframe_t::down);
    constexpr std::size_t o662 = offsetof(goldsrc::mspriteframe_t, left);
    using t657 = decltype(goldsrc::mspriteframe_t::left);
    constexpr std::size_t o663 = offsetof(goldsrc::mspriteframe_t, right);
    using t658 = decltype(goldsrc::mspriteframe_t::right);
    constexpr std::size_t o664 = offsetof(goldsrc::mspriteframe_t, gl_texturenum);
    using t659 = decltype(goldsrc::mspriteframe_t::gl_texturenum);
    constexpr std::size_t s42 = sizeof(goldsrc::mspriteframedesc_t);
    constexpr std::size_t o665 = offsetof(goldsrc::mspriteframedesc_t, type);
    using t660 = decltype(goldsrc::mspriteframedesc_t::type);
    constexpr std::size_t o666 = offsetof(goldsrc::mspriteframedesc_t, frameptr);
    using t661 = decltype(goldsrc::mspriteframedesc_t::frameptr);
    constexpr std::size_t s43 = sizeof(goldsrc::mspritegroup_t);
    constexpr std::size_t o667 = offsetof(goldsrc::mspritegroup_t, numframes);
    using t662 = decltype(goldsrc::mspritegroup_t::numframes);
    constexpr std::size_t o668 = offsetof(goldsrc::mspritegroup_t, intervals);
    using t663 = decltype(goldsrc::mspritegroup_t::intervals);
    constexpr std::size_t o669 = offsetof(goldsrc::mspritegroup_t, frames);
    using t664 = decltype(goldsrc::mspritegroup_t::frames);
    constexpr std::size_t s44 = sizeof(goldsrc::msurface_t);
    constexpr std::size_t o670 = offsetof(goldsrc::msurface_t, visframe);
    using t665 = decltype(goldsrc::msurface_t::visframe);
    constexpr std::size_t o671 = offsetof(goldsrc::msurface_t, plane);
    using t666 = decltype(goldsrc::msurface_t::plane);
    constexpr std::size_t o672 = offsetof(goldsrc::msurface_t, flags);
    using t667 = decltype(goldsrc::msurface_t::flags);
    constexpr std::size_t o673 = offsetof(goldsrc::msurface_t, firstedge);
    using t668 = decltype(goldsrc::msurface_t::firstedge);
    constexpr std::size_t o674 = offsetof(goldsrc::msurface_t, numedges);
    using t669 = decltype(goldsrc::msurface_t::numedges);
    constexpr std::size_t o675 = offsetof(goldsrc::msurface_t, texturemins);
    using t670 = decltype(goldsrc::msurface_t::texturemins);
    constexpr std::size_t o676 = offsetof(goldsrc::msurface_t, extents);
    using t671 = decltype(goldsrc::msurface_t::extents);
    constexpr std::size_t o677 = offsetof(goldsrc::msurface_t, light_s);
    using t672 = decltype(goldsrc::msurface_t::light_s);
    constexpr std::size_t o678 = offsetof(goldsrc::msurface_t, light_t);
    using t673 = decltype(goldsrc::msurface_t::light_t);
    constexpr std::size_t o679 = offsetof(goldsrc::msurface_t, polys);
    using t674 = decltype(goldsrc::msurface_t::polys);
    constexpr std::size_t o680 = offsetof(goldsrc::msurface_t, texturechain);
    using t675 = decltype(goldsrc::msurface_t::texturechain);
    constexpr std::size_t o681 = offsetof(goldsrc::msurface_t, texinfo);
    using t676 = decltype(goldsrc::msurface_t::texinfo);
    constexpr std::size_t o682 = offsetof(goldsrc::msurface_t, dlightframe);
    using t677 = decltype(goldsrc::msurface_t::dlightframe);
    constexpr std::size_t o683 = offsetof(goldsrc::msurface_t, dlightbits);
    using t678 = decltype(goldsrc::msurface_t::dlightbits);
    constexpr std::size_t o684 = offsetof(goldsrc::msurface_t, lightmaptexturenum);
    using t679 = decltype(goldsrc::msurface_t::lightmaptexturenum);
    constexpr std::size_t s45 = sizeof(goldsrc::mtexinfo_t);
    constexpr std::size_t o685 = offsetof(goldsrc::mtexinfo_t, vecs);
    using t680 = decltype(goldsrc::mtexinfo_t::vecs);
    constexpr std::size_t o686 = offsetof(goldsrc::mtexinfo_t, mipadjust);
    using t681 = decltype(goldsrc::mtexinfo_t::mipadjust);
    constexpr std::size_t o687 = offsetof(goldsrc::mtexinfo_t, texture);
    using t682 = decltype(goldsrc::mtexinfo_t::texture);
    constexpr std::size_t s46 = sizeof(goldsrc::mvertex_t);
    constexpr std::size_t o688 = offsetof(goldsrc::mvertex_t, position);
    using t683 = decltype(goldsrc::mvertex_t::position);
    constexpr std::size_t s47 = sizeof(goldsrc::skin_t);
    constexpr std::size_t o689 = offsetof(goldsrc::skin_t, keynum);
    using t684 = decltype(goldsrc::skin_t::keynum);
    constexpr std::size_t o690 = offsetof(goldsrc::skin_t, topcolor);
    using t685 = decltype(goldsrc::skin_t::topcolor);
    constexpr std::size_t o691 = offsetof(goldsrc::skin_t, bottomcolor);
    using t686 = decltype(goldsrc::skin_t::bottomcolor);
    constexpr std::size_t o692 = offsetof(goldsrc::skin_t, model);
    using t687 = decltype(goldsrc::skin_t::model);
    constexpr std::size_t o693 = offsetof(goldsrc::skin_t, name);
    using t688 = decltype(goldsrc::skin_t::name);
    constexpr std::size_t o694 = offsetof(goldsrc::skin_t, index);
    using t689 = decltype(goldsrc::skin_t::index);
    constexpr std::size_t o695 = offsetof(goldsrc::skin_t, source);
    using t690 = decltype(goldsrc::skin_t::source);
    constexpr std::size_t o696 = offsetof(goldsrc::skin_t, width);
    using t691 = decltype(goldsrc::skin_t::width);
    constexpr std::size_t o697 = offsetof(goldsrc::skin_t, height);
    using t692 = decltype(goldsrc::skin_t::height);
    constexpr std::size_t s48 = sizeof(goldsrc::texture_t);
    constexpr std::size_t o698 = offsetof(goldsrc::texture_t, name);
    using t693 = decltype(goldsrc::texture_t::name);
    constexpr std::size_t o699 = offsetof(goldsrc::texture_t, width);
    using t694 = decltype(goldsrc::texture_t::width);
    constexpr std::size_t o700 = offsetof(goldsrc::texture_t, height);
    using t695 = decltype(goldsrc::texture_t::height);
    constexpr std::size_t o701 = offsetof(goldsrc::texture_t, gl_texturenum);
    using t696 = decltype(goldsrc::texture_t::gl_texturenum);
    constexpr std::size_t s49 = sizeof(goldsrc::cachewad_t);
    constexpr std::size_t o702 = offsetof(goldsrc::cachewad_t, name);
    using t697 = decltype(goldsrc::cachewad_t::name);
    constexpr std::size_t o703 = offsetof(goldsrc::cachewad_t, cache);
    using t698 = decltype(goldsrc::cachewad_t::cache);
    constexpr std::size_t o704 = offsetof(goldsrc::cachewad_t, cacheCount);
    using t699 = decltype(goldsrc::cachewad_t::cacheCount);
    constexpr std::size_t o705 = offsetof(goldsrc::cachewad_t, cacheMax);
    using t700 = decltype(goldsrc::cachewad_t::cacheMax);
    constexpr std::size_t o706 = offsetof(goldsrc::cachewad_t, lumps);
    using t701 = decltype(goldsrc::cachewad_t::lumps);
    constexpr std::size_t o707 = offsetof(goldsrc::cachewad_t, lumpCount);
    using t702 = decltype(goldsrc::cachewad_t::lumpCount);
    constexpr std::size_t o708 = offsetof(goldsrc::cachewad_t, cacheExtra);
    using t703 = decltype(goldsrc::cachewad_t::cacheExtra);
    constexpr std::size_t o709 = offsetof(goldsrc::cachewad_t, pfnCacheBuild);
    using t704 = decltype(goldsrc::cachewad_t::pfnCacheBuild);
    constexpr std::size_t o710 = offsetof(goldsrc::cachewad_t, numpaths);
    using t705 = decltype(goldsrc::cachewad_t::numpaths);
    constexpr std::size_t o711 = offsetof(goldsrc::cachewad_t, basedirs);
    using t706 = decltype(goldsrc::cachewad_t::basedirs);
    constexpr std::size_t o712 = offsetof(goldsrc::cachewad_t, lumppathindices);
    using t707 = decltype(goldsrc::cachewad_t::lumppathindices);
    constexpr std::size_t o713 = offsetof(goldsrc::cachewad_t, tempWad);
    using t708 = decltype(goldsrc::cachewad_t::tempWad);
    constexpr std::size_t s50 = sizeof(goldsrc::model_t);
    constexpr std::size_t o714 = offsetof(goldsrc::model_t, name);
    using t709 = decltype(goldsrc::model_t::name);
    constexpr std::size_t o715 = offsetof(goldsrc::model_t, needload);
    using t710 = decltype(goldsrc::model_t::needload);
    constexpr std::size_t o716 = offsetof(goldsrc::model_t, type);
    using t711 = decltype(goldsrc::model_t::type);
    constexpr std::size_t o717 = offsetof(goldsrc::model_t, numframes);
    using t712 = decltype(goldsrc::model_t::numframes);
    constexpr std::size_t o718 = offsetof(goldsrc::model_t, synctype);
    using t713 = decltype(goldsrc::model_t::synctype);
    constexpr std::size_t o719 = offsetof(goldsrc::model_t, flags);
    using t714 = decltype(goldsrc::model_t::flags);
    constexpr std::size_t o720 = offsetof(goldsrc::model_t, mins);
    using t715 = decltype(goldsrc::model_t::mins);
    constexpr std::size_t o721 = offsetof(goldsrc::model_t, maxs);
    using t716 = decltype(goldsrc::model_t::maxs);
    constexpr std::size_t o722 = offsetof(goldsrc::model_t, radius);
    using t717 = decltype(goldsrc::model_t::radius);
    constexpr std::size_t o723 = offsetof(goldsrc::model_t, firstmodelsurface);
    using t718 = decltype(goldsrc::model_t::firstmodelsurface);
    constexpr std::size_t o724 = offsetof(goldsrc::model_t, nummodelsurfaces);
    using t719 = decltype(goldsrc::model_t::nummodelsurfaces);
    constexpr std::size_t o725 = offsetof(goldsrc::model_t, numsubmodels);
    using t720 = decltype(goldsrc::model_t::numsubmodels);
    constexpr std::size_t o726 = offsetof(goldsrc::model_t, submodels);
    using t721 = decltype(goldsrc::model_t::submodels);
    constexpr std::size_t o727 = offsetof(goldsrc::model_t, numplanes);
    using t722 = decltype(goldsrc::model_t::numplanes);
    constexpr std::size_t o728 = offsetof(goldsrc::model_t, planes);
    using t723 = decltype(goldsrc::model_t::planes);
    constexpr std::size_t o729 = offsetof(goldsrc::model_t, numleafs);
    using t724 = decltype(goldsrc::model_t::numleafs);
    constexpr std::size_t o730 = offsetof(goldsrc::model_t, leafs);
    using t725 = decltype(goldsrc::model_t::leafs);
    constexpr std::size_t o731 = offsetof(goldsrc::model_t, numvertexes);
    using t726 = decltype(goldsrc::model_t::numvertexes);
    constexpr std::size_t o732 = offsetof(goldsrc::model_t, vertexes);
    using t727 = decltype(goldsrc::model_t::vertexes);
    constexpr std::size_t o733 = offsetof(goldsrc::model_t, numedges);
    using t728 = decltype(goldsrc::model_t::numedges);
    constexpr std::size_t o734 = offsetof(goldsrc::model_t, edges);
    using t729 = decltype(goldsrc::model_t::edges);
    constexpr std::size_t o735 = offsetof(goldsrc::model_t, numnodes);
    using t730 = decltype(goldsrc::model_t::numnodes);
    constexpr std::size_t o736 = offsetof(goldsrc::model_t, nodes);
    using t731 = decltype(goldsrc::model_t::nodes);
    constexpr std::size_t o737 = offsetof(goldsrc::model_t, numtexinfo);
    using t732 = decltype(goldsrc::model_t::numtexinfo);
    constexpr std::size_t o738 = offsetof(goldsrc::model_t, texinfo);
    using t733 = decltype(goldsrc::model_t::texinfo);
    constexpr std::size_t o739 = offsetof(goldsrc::model_t, numsurfaces);
    using t734 = decltype(goldsrc::model_t::numsurfaces);
    constexpr std::size_t o740 = offsetof(goldsrc::model_t, surfaces);
    using t735 = decltype(goldsrc::model_t::surfaces);
    constexpr std::size_t o741 = offsetof(goldsrc::model_t, numsurfedges);
    using t736 = decltype(goldsrc::model_t::numsurfedges);
    constexpr std::size_t o742 = offsetof(goldsrc::model_t, surfedges);
    using t737 = decltype(goldsrc::model_t::surfedges);
    constexpr std::size_t o743 = offsetof(goldsrc::model_t, numclipnodes);
    using t738 = decltype(goldsrc::model_t::numclipnodes);
    constexpr std::size_t o744 = offsetof(goldsrc::model_t, clipnodes);
    using t739 = decltype(goldsrc::model_t::clipnodes);
    constexpr std::size_t o745 = offsetof(goldsrc::model_t, nummarksurfaces);
    using t740 = decltype(goldsrc::model_t::nummarksurfaces);
    constexpr std::size_t o746 = offsetof(goldsrc::model_t, marksurfaces);
    using t741 = decltype(goldsrc::model_t::marksurfaces);
    constexpr std::size_t o747 = offsetof(goldsrc::model_t, hulls);
    using t742 = decltype(goldsrc::model_t::hulls);
    constexpr std::size_t o748 = offsetof(goldsrc::model_t, numtextures);
    using t743 = decltype(goldsrc::model_t::numtextures);
    constexpr std::size_t o749 = offsetof(goldsrc::model_t, textures);
    using t744 = decltype(goldsrc::model_t::textures);
    constexpr std::size_t o750 = offsetof(goldsrc::model_t, visdata);
    using t745 = decltype(goldsrc::model_t::visdata);
    constexpr std::size_t o751 = offsetof(goldsrc::model_t, lightdata);
    using t746 = decltype(goldsrc::model_t::lightdata);
    constexpr std::size_t o752 = offsetof(goldsrc::model_t, entities);
    using t747 = decltype(goldsrc::model_t::entities);
    constexpr std::size_t o753 = offsetof(goldsrc::model_t, cache);
    using t748 = decltype(goldsrc::model_t::cache);
    constexpr std::size_t s51 = sizeof(goldsrc::msprite_t);
    constexpr std::size_t o754 = offsetof(goldsrc::msprite_t, type);
    using t749 = decltype(goldsrc::msprite_t::type);
    constexpr std::size_t o755 = offsetof(goldsrc::msprite_t, texFormat);
    using t750 = decltype(goldsrc::msprite_t::texFormat);
    constexpr std::size_t o756 = offsetof(goldsrc::msprite_t, maxwidth);
    using t751 = decltype(goldsrc::msprite_t::maxwidth);
    constexpr std::size_t o757 = offsetof(goldsrc::msprite_t, maxheight);
    using t752 = decltype(goldsrc::msprite_t::maxheight);
    constexpr std::size_t o758 = offsetof(goldsrc::msprite_t, numframes);
    using t753 = decltype(goldsrc::msprite_t::numframes);
    constexpr std::size_t o759 = offsetof(goldsrc::msprite_t, paloffset);
    using t754 = decltype(goldsrc::msprite_t::paloffset);
    constexpr std::size_t o760 = offsetof(goldsrc::msprite_t, beamlength);
    using t755 = decltype(goldsrc::msprite_t::beamlength);
    constexpr std::size_t o761 = offsetof(goldsrc::msprite_t, cachespot);
    using t756 = decltype(goldsrc::msprite_t::cachespot);
    constexpr std::size_t o762 = offsetof(goldsrc::msprite_t, frames);
    using t757 = decltype(goldsrc::msprite_t::frames);
    constexpr std::size_t s52 = sizeof(goldsrc::downloadtime_t);
    constexpr std::size_t o763 = offsetof(goldsrc::downloadtime_t, bUsed);
    using t758 = decltype(goldsrc::downloadtime_t::bUsed);
    constexpr std::size_t o764 = offsetof(goldsrc::downloadtime_t, fTime);
    using t759 = decltype(goldsrc::downloadtime_t::fTime);
    constexpr std::size_t o765 = offsetof(goldsrc::downloadtime_t, nBytesRemaining);
    using t760 = decltype(goldsrc::downloadtime_t::nBytesRemaining);
    constexpr std::size_t s53 = sizeof(goldsrc::flowstats_t);
    constexpr std::size_t o766 = offsetof(goldsrc::flowstats_t, size);
    using t761 = decltype(goldsrc::flowstats_t::size);
    constexpr std::size_t o767 = offsetof(goldsrc::flowstats_t, time);
    using t762 = decltype(goldsrc::flowstats_t::time);
    constexpr std::size_t s54 = sizeof(goldsrc::sizebuf_t);
    constexpr std::size_t o768 = offsetof(goldsrc::sizebuf_t, buffername);
    using t763 = decltype(goldsrc::sizebuf_t::buffername);
    constexpr std::size_t o769 = offsetof(goldsrc::sizebuf_t, flags);
    using t764 = decltype(goldsrc::sizebuf_t::flags);
    constexpr std::size_t o770 = offsetof(goldsrc::sizebuf_t, data);
    using t765 = decltype(goldsrc::sizebuf_t::data);
    constexpr std::size_t o771 = offsetof(goldsrc::sizebuf_t, maxsize);
    using t766 = decltype(goldsrc::sizebuf_t::maxsize);
    constexpr std::size_t o772 = offsetof(goldsrc::sizebuf_t, cursize);
    using t767 = decltype(goldsrc::sizebuf_t::cursize);
    constexpr std::size_t s55 = sizeof(goldsrc::flow_t);
    constexpr std::size_t o773 = offsetof(goldsrc::flow_t, stats);
    using t768 = decltype(goldsrc::flow_t::stats);
    constexpr std::size_t o774 = offsetof(goldsrc::flow_t, current);
    using t769 = decltype(goldsrc::flow_t::current);
    constexpr std::size_t o775 = offsetof(goldsrc::flow_t, nextcompute);
    using t770 = decltype(goldsrc::flow_t::nextcompute);
    constexpr std::size_t o776 = offsetof(goldsrc::flow_t, kbytespersec);
    using t771 = decltype(goldsrc::flow_t::kbytespersec);
    constexpr std::size_t o777 = offsetof(goldsrc::flow_t, avgkbytespersec);
    using t772 = decltype(goldsrc::flow_t::avgkbytespersec);
    constexpr std::size_t s56 = sizeof(goldsrc::fragbuf_t);
    constexpr std::size_t o778 = offsetof(goldsrc::fragbuf_t, next);
    using t773 = decltype(goldsrc::fragbuf_t::next);
    constexpr std::size_t o779 = offsetof(goldsrc::fragbuf_t, bufferid);
    using t774 = decltype(goldsrc::fragbuf_t::bufferid);
    constexpr std::size_t o780 = offsetof(goldsrc::fragbuf_t, frag_message);
    using t775 = decltype(goldsrc::fragbuf_t::frag_message);
    constexpr std::size_t s57 = sizeof(goldsrc::incomingtransfer_t);
    constexpr std::size_t o781 = offsetof(goldsrc::incomingtransfer_t, doneregistering);
    using t776 = decltype(goldsrc::incomingtransfer_t::doneregistering);
    constexpr std::size_t o782 = offsetof(goldsrc::incomingtransfer_t, percent);
    using t777 = decltype(goldsrc::incomingtransfer_t::percent);
    constexpr std::size_t o783 = offsetof(goldsrc::incomingtransfer_t, downloadrequested);
    using t778 = decltype(goldsrc::incomingtransfer_t::downloadrequested);
    constexpr std::size_t o784 = offsetof(goldsrc::incomingtransfer_t, rgStats);
    using t779 = decltype(goldsrc::incomingtransfer_t::rgStats);
    constexpr std::size_t o785 = offsetof(goldsrc::incomingtransfer_t, nCurStat);
    using t780 = decltype(goldsrc::incomingtransfer_t::nCurStat);
    constexpr std::size_t o786 = offsetof(goldsrc::incomingtransfer_t, nTotalSize);
    using t781 = decltype(goldsrc::incomingtransfer_t::nTotalSize);
    constexpr std::size_t o787 = offsetof(goldsrc::incomingtransfer_t, nTotalToTransfer);
    using t782 = decltype(goldsrc::incomingtransfer_t::nTotalToTransfer);
    constexpr std::size_t o788 = offsetof(goldsrc::incomingtransfer_t, nRemainingToTransfer);
    using t783 = decltype(goldsrc::incomingtransfer_t::nRemainingToTransfer);
    constexpr std::size_t o789 = offsetof(goldsrc::incomingtransfer_t, fLastStatusUpdate);
    using t784 = decltype(goldsrc::incomingtransfer_t::fLastStatusUpdate);
    constexpr std::size_t o790 = offsetof(goldsrc::incomingtransfer_t, custom);
    using t785 = decltype(goldsrc::incomingtransfer_t::custom);
    constexpr std::size_t s58 = sizeof(goldsrc::netadr_t);
    constexpr std::size_t s59 = sizeof(goldsrc::netchan_t);
    constexpr std::size_t o791 = offsetof(goldsrc::netchan_t, sock);
    using t786 = decltype(goldsrc::netchan_t::sock);
    constexpr std::size_t o792 = offsetof(goldsrc::netchan_t, remote_address);
    using t787 = decltype(goldsrc::netchan_t::remote_address);
    constexpr std::size_t o793 = offsetof(goldsrc::netchan_t, player_slot);
    using t788 = decltype(goldsrc::netchan_t::player_slot);
    constexpr std::size_t o794 = offsetof(goldsrc::netchan_t, last_received);
    using t789 = decltype(goldsrc::netchan_t::last_received);
    constexpr std::size_t o795 = offsetof(goldsrc::netchan_t, connect_time);
    using t790 = decltype(goldsrc::netchan_t::connect_time);
    constexpr std::size_t o796 = offsetof(goldsrc::netchan_t, rate);
    using t791 = decltype(goldsrc::netchan_t::rate);
    constexpr std::size_t o797 = offsetof(goldsrc::netchan_t, cleartime);
    using t792 = decltype(goldsrc::netchan_t::cleartime);
    constexpr std::size_t o798 = offsetof(goldsrc::netchan_t, incoming_sequence);
    using t793 = decltype(goldsrc::netchan_t::incoming_sequence);
    constexpr std::size_t o799 = offsetof(goldsrc::netchan_t, incoming_acknowledged);
    using t794 = decltype(goldsrc::netchan_t::incoming_acknowledged);
    constexpr std::size_t o800 = offsetof(goldsrc::netchan_t, incoming_reliable_acknowledged);
    using t795 = decltype(goldsrc::netchan_t::incoming_reliable_acknowledged);
    constexpr std::size_t o801 = offsetof(goldsrc::netchan_t, incoming_reliable_sequence);
    using t796 = decltype(goldsrc::netchan_t::incoming_reliable_sequence);
    constexpr std::size_t o802 = offsetof(goldsrc::netchan_t, outgoing_sequence);
    using t797 = decltype(goldsrc::netchan_t::outgoing_sequence);
    constexpr std::size_t o803 = offsetof(goldsrc::netchan_t, reliable_sequence);
    using t798 = decltype(goldsrc::netchan_t::reliable_sequence);
    constexpr std::size_t o804 = offsetof(goldsrc::netchan_t, last_reliable_sequence);
    using t799 = decltype(goldsrc::netchan_t::last_reliable_sequence);
    constexpr std::size_t o805 = offsetof(goldsrc::netchan_t, connection_status);
    using t800 = decltype(goldsrc::netchan_t::connection_status);
    constexpr std::size_t o806 = offsetof(goldsrc::netchan_t, pfnNetchan_Blocksize);
    using t801 = decltype(goldsrc::netchan_t::pfnNetchan_Blocksize);
    constexpr std::size_t o807 = offsetof(goldsrc::netchan_t, message);
    using t802 = decltype(goldsrc::netchan_t::message);
    constexpr std::size_t o808 = offsetof(goldsrc::netchan_t, message_buf);
    using t803 = decltype(goldsrc::netchan_t::message_buf);
    constexpr std::size_t o809 = offsetof(goldsrc::netchan_t, reliable_length);
    using t804 = decltype(goldsrc::netchan_t::reliable_length);
    constexpr std::size_t o810 = offsetof(goldsrc::netchan_t, reliable_buf);
    using t805 = decltype(goldsrc::netchan_t::reliable_buf);
    constexpr std::size_t o811 = offsetof(goldsrc::netchan_t, waitlist);
    using t806 = decltype(goldsrc::netchan_t::waitlist);
    constexpr std::size_t o812 = offsetof(goldsrc::netchan_t, reliable_fragment);
    using t807 = decltype(goldsrc::netchan_t::reliable_fragment);
    constexpr std::size_t o813 = offsetof(goldsrc::netchan_t, reliable_fragid);
    using t808 = decltype(goldsrc::netchan_t::reliable_fragid);
    constexpr std::size_t o814 = offsetof(goldsrc::netchan_t, fragbufs);
    using t809 = decltype(goldsrc::netchan_t::fragbufs);
    constexpr std::size_t o815 = offsetof(goldsrc::netchan_t, fragbufcount);
    using t810 = decltype(goldsrc::netchan_t::fragbufcount);
    constexpr std::size_t o816 = offsetof(goldsrc::netchan_t, frag_startpos);
    using t811 = decltype(goldsrc::netchan_t::frag_startpos);
    constexpr std::size_t o817 = offsetof(goldsrc::netchan_t, frag_length);
    using t812 = decltype(goldsrc::netchan_t::frag_length);
    constexpr std::size_t o818 = offsetof(goldsrc::netchan_t, incomingbufs);
    using t813 = decltype(goldsrc::netchan_t::incomingbufs);
    constexpr std::size_t o819 = offsetof(goldsrc::netchan_t, incomingready);
    using t814 = decltype(goldsrc::netchan_t::incomingready);
    constexpr std::size_t o820 = offsetof(goldsrc::netchan_t, incomingfilename);
    using t815 = decltype(goldsrc::netchan_t::incomingfilename);
    constexpr std::size_t o821 = offsetof(goldsrc::netchan_t, tempbuffer);
    using t816 = decltype(goldsrc::netchan_t::tempbuffer);
    constexpr std::size_t o822 = offsetof(goldsrc::netchan_t, tempbuffersize);
    using t817 = decltype(goldsrc::netchan_t::tempbuffersize);
    constexpr std::size_t o823 = offsetof(goldsrc::netchan_t, flow);
    using t818 = decltype(goldsrc::netchan_t::flow);
    constexpr std::size_t s60 = sizeof(goldsrc::_resourceinfo_t);
    constexpr std::size_t o824 = offsetof(goldsrc::_resourceinfo_t, size);
    using t819 = decltype(goldsrc::_resourceinfo_t::size);
    constexpr std::size_t s61 = sizeof(goldsrc::resource_t);
    constexpr std::size_t o825 = offsetof(goldsrc::resource_t, szFileName);
    using t820 = decltype(goldsrc::resource_t::szFileName);
    constexpr std::size_t o826 = offsetof(goldsrc::resource_t, type);
    using t821 = decltype(goldsrc::resource_t::type);
    constexpr std::size_t o827 = offsetof(goldsrc::resource_t, nIndex);
    using t822 = decltype(goldsrc::resource_t::nIndex);
    constexpr std::size_t o828 = offsetof(goldsrc::resource_t, nDownloadSize);
    using t823 = decltype(goldsrc::resource_t::nDownloadSize);
    constexpr std::size_t o829 = offsetof(goldsrc::resource_t, ucFlags);
    using t824 = decltype(goldsrc::resource_t::ucFlags);
    constexpr std::size_t o830 = offsetof(goldsrc::resource_t, rgucMD5_hash);
    using t825 = decltype(goldsrc::resource_t::rgucMD5_hash);
    constexpr std::size_t o831 = offsetof(goldsrc::resource_t, playernum);
    using t826 = decltype(goldsrc::resource_t::playernum);
    constexpr std::size_t o832 = offsetof(goldsrc::resource_t, rguc_reserved);
    using t827 = decltype(goldsrc::resource_t::rguc_reserved);
    constexpr std::size_t o833 = offsetof(goldsrc::resource_t, pNext);
    using t828 = decltype(goldsrc::resource_t::pNext);
    constexpr std::size_t o834 = offsetof(goldsrc::resource_t, pPrev);
    using t829 = decltype(goldsrc::resource_t::pPrev);
    constexpr std::size_t s62 = sizeof(goldsrc::resourceinfo_t);
    constexpr std::size_t o835 = offsetof(goldsrc::resourceinfo_t, info);
    using t830 = decltype(goldsrc::resourceinfo_t::info);
    constexpr std::size_t s63 = sizeof(goldsrc::customization_t);
    constexpr std::size_t o836 = offsetof(goldsrc::customization_t, bInUse);
    using t831 = decltype(goldsrc::customization_t::bInUse);
    constexpr std::size_t o837 = offsetof(goldsrc::customization_t, resource);
    using t832 = decltype(goldsrc::customization_t::resource);
    constexpr std::size_t o838 = offsetof(goldsrc::customization_t, bTranslated);
    using t833 = decltype(goldsrc::customization_t::bTranslated);
    constexpr std::size_t o839 = offsetof(goldsrc::customization_t, nUserData1);
    using t834 = decltype(goldsrc::customization_t::nUserData1);
    constexpr std::size_t o840 = offsetof(goldsrc::customization_t, nUserData2);
    using t835 = decltype(goldsrc::customization_t::nUserData2);
    constexpr std::size_t o841 = offsetof(goldsrc::customization_t, pInfo);
    using t836 = decltype(goldsrc::customization_t::pInfo);
    constexpr std::size_t o842 = offsetof(goldsrc::customization_t, pBuffer);
    using t837 = decltype(goldsrc::customization_t::pBuffer);
    constexpr std::size_t o843 = offsetof(goldsrc::customization_t, pNext);
    using t838 = decltype(goldsrc::customization_t::pNext);
    constexpr std::size_t s64 = sizeof(goldsrc::server_static_t);
    constexpr std::size_t o844 = offsetof(goldsrc::server_static_t, dll_initialized);
    using t839 = decltype(goldsrc::server_static_t::dll_initialized);
    constexpr std::size_t o845 = offsetof(goldsrc::server_static_t, clients);
    using t840 = decltype(goldsrc::server_static_t::clients);
    constexpr std::size_t o846 = offsetof(goldsrc::server_static_t, maxclients);
    using t841 = decltype(goldsrc::server_static_t::maxclients);
    constexpr std::size_t s65 = sizeof(goldsrc::server_t);
    constexpr std::size_t o847 = offsetof(goldsrc::server_t, active);
    using t842 = decltype(goldsrc::server_t::active);
    constexpr std::size_t s66 = sizeof(goldsrc::sfx_t);
    constexpr std::size_t o848 = offsetof(goldsrc::sfx_t, name);
    using t843 = decltype(goldsrc::sfx_t::name);
    constexpr std::size_t o849 = offsetof(goldsrc::sfx_t, cache);
    using t844 = decltype(goldsrc::sfx_t::cache);
    constexpr std::size_t s67 = sizeof(goldsrc::EngineMsg);
    constexpr std::size_t o850 = offsetof(goldsrc::EngineMsg, number);
    using t845 = decltype(goldsrc::EngineMsg::number);
    constexpr std::size_t o851 = offsetof(goldsrc::EngineMsg, name);
    using t846 = decltype(goldsrc::EngineMsg::name);
    constexpr std::size_t o852 = offsetof(goldsrc::EngineMsg, pfn);
    using t847 = decltype(goldsrc::EngineMsg::pfn);
    constexpr std::size_t s68 = sizeof(goldsrc::transObjRef);
    constexpr std::size_t o853 = offsetof(goldsrc::transObjRef, pEnt);
    using t848 = decltype(goldsrc::transObjRef::pEnt);
    constexpr std::size_t s69 = sizeof(goldsrc::viddef_t);
    constexpr std::size_t o854 = offsetof(goldsrc::viddef_t, buffer);
    using t849 = decltype(goldsrc::viddef_t::buffer);
    constexpr std::size_t o855 = offsetof(goldsrc::viddef_t, colormap);
    using t850 = decltype(goldsrc::viddef_t::colormap);
    constexpr std::size_t o856 = offsetof(goldsrc::viddef_t, colormap16);
    using t851 = decltype(goldsrc::viddef_t::colormap16);
    constexpr std::size_t o857 = offsetof(goldsrc::viddef_t, fullbright);
    using t852 = decltype(goldsrc::viddef_t::fullbright);
    constexpr std::size_t o858 = offsetof(goldsrc::viddef_t, bits);
    using t853 = decltype(goldsrc::viddef_t::bits);
    constexpr std::size_t o859 = offsetof(goldsrc::viddef_t, is15bit);
    using t854 = decltype(goldsrc::viddef_t::is15bit);
    constexpr std::size_t o860 = offsetof(goldsrc::viddef_t, rowbytes);
    using t855 = decltype(goldsrc::viddef_t::rowbytes);
    constexpr std::size_t o861 = offsetof(goldsrc::viddef_t, width);
    using t856 = decltype(goldsrc::viddef_t::width);
    constexpr std::size_t o862 = offsetof(goldsrc::viddef_t, height);
    using t857 = decltype(goldsrc::viddef_t::height);
    constexpr std::size_t o863 = offsetof(goldsrc::viddef_t, aspect);
    using t858 = decltype(goldsrc::viddef_t::aspect);
    constexpr std::size_t o864 = offsetof(goldsrc::viddef_t, numpages);
    using t859 = decltype(goldsrc::viddef_t::numpages);
    constexpr std::size_t o865 = offsetof(goldsrc::viddef_t, recalc_refdef);
    using t860 = decltype(goldsrc::viddef_t::recalc_refdef);
    constexpr std::size_t s70 = sizeof(goldsrc::clientdata_t);
    constexpr std::size_t o866 = offsetof(goldsrc::clientdata_t, origin);
    using t861 = decltype(goldsrc::clientdata_t::origin);
    constexpr std::size_t o867 = offsetof(goldsrc::clientdata_t, velocity);
    using t862 = decltype(goldsrc::clientdata_t::velocity);
    constexpr std::size_t o868 = offsetof(goldsrc::clientdata_t, viewmodel);
    using t863 = decltype(goldsrc::clientdata_t::viewmodel);
    constexpr std::size_t o869 = offsetof(goldsrc::clientdata_t, punchangle);
    using t864 = decltype(goldsrc::clientdata_t::punchangle);
    constexpr std::size_t o870 = offsetof(goldsrc::clientdata_t, flags);
    using t865 = decltype(goldsrc::clientdata_t::flags);
    constexpr std::size_t o871 = offsetof(goldsrc::clientdata_t, waterlevel);
    using t866 = decltype(goldsrc::clientdata_t::waterlevel);
    constexpr std::size_t o872 = offsetof(goldsrc::clientdata_t, watertype);
    using t867 = decltype(goldsrc::clientdata_t::watertype);
    constexpr std::size_t o873 = offsetof(goldsrc::clientdata_t, view_ofs);
    using t868 = decltype(goldsrc::clientdata_t::view_ofs);
    constexpr std::size_t o874 = offsetof(goldsrc::clientdata_t, health);
    using t869 = decltype(goldsrc::clientdata_t::health);
    constexpr std::size_t o875 = offsetof(goldsrc::clientdata_t, bInDuck);
    using t870 = decltype(goldsrc::clientdata_t::bInDuck);
    constexpr std::size_t o876 = offsetof(goldsrc::clientdata_t, weapons);
    using t871 = decltype(goldsrc::clientdata_t::weapons);
    constexpr std::size_t o877 = offsetof(goldsrc::clientdata_t, flTimeStepSound);
    using t872 = decltype(goldsrc::clientdata_t::flTimeStepSound);
    constexpr std::size_t o878 = offsetof(goldsrc::clientdata_t, flDuckTime);
    using t873 = decltype(goldsrc::clientdata_t::flDuckTime);
    constexpr std::size_t o879 = offsetof(goldsrc::clientdata_t, flSwimTime);
    using t874 = decltype(goldsrc::clientdata_t::flSwimTime);
    constexpr std::size_t o880 = offsetof(goldsrc::clientdata_t, waterjumptime);
    using t875 = decltype(goldsrc::clientdata_t::waterjumptime);
    constexpr std::size_t o881 = offsetof(goldsrc::clientdata_t, maxspeed);
    using t876 = decltype(goldsrc::clientdata_t::maxspeed);
    constexpr std::size_t o882 = offsetof(goldsrc::clientdata_t, fov);
    using t877 = decltype(goldsrc::clientdata_t::fov);
    constexpr std::size_t o883 = offsetof(goldsrc::clientdata_t, weaponanim);
    using t878 = decltype(goldsrc::clientdata_t::weaponanim);
    constexpr std::size_t o884 = offsetof(goldsrc::clientdata_t, m_iId);
    using t879 = decltype(goldsrc::clientdata_t::m_iId);
    constexpr std::size_t o885 = offsetof(goldsrc::clientdata_t, ammo_shells);
    using t880 = decltype(goldsrc::clientdata_t::ammo_shells);
    constexpr std::size_t o886 = offsetof(goldsrc::clientdata_t, ammo_nails);
    using t881 = decltype(goldsrc::clientdata_t::ammo_nails);
    constexpr std::size_t o887 = offsetof(goldsrc::clientdata_t, ammo_cells);
    using t882 = decltype(goldsrc::clientdata_t::ammo_cells);
    constexpr std::size_t o888 = offsetof(goldsrc::clientdata_t, ammo_rockets);
    using t883 = decltype(goldsrc::clientdata_t::ammo_rockets);
    constexpr std::size_t o889 = offsetof(goldsrc::clientdata_t, m_flNextAttack);
    using t884 = decltype(goldsrc::clientdata_t::m_flNextAttack);
    constexpr std::size_t o890 = offsetof(goldsrc::clientdata_t, tfstate);
    using t885 = decltype(goldsrc::clientdata_t::tfstate);
    constexpr std::size_t o891 = offsetof(goldsrc::clientdata_t, pushmsec);
    using t886 = decltype(goldsrc::clientdata_t::pushmsec);
    constexpr std::size_t o892 = offsetof(goldsrc::clientdata_t, deadflag);
    using t887 = decltype(goldsrc::clientdata_t::deadflag);
    constexpr std::size_t o893 = offsetof(goldsrc::clientdata_t, physinfo);
    using t888 = decltype(goldsrc::clientdata_t::physinfo);
    constexpr std::size_t o894 = offsetof(goldsrc::clientdata_t, iuser1);
    using t889 = decltype(goldsrc::clientdata_t::iuser1);
    constexpr std::size_t o895 = offsetof(goldsrc::clientdata_t, iuser2);
    using t890 = decltype(goldsrc::clientdata_t::iuser2);
    constexpr std::size_t o896 = offsetof(goldsrc::clientdata_t, iuser3);
    using t891 = decltype(goldsrc::clientdata_t::iuser3);
    constexpr std::size_t o897 = offsetof(goldsrc::clientdata_t, iuser4);
    using t892 = decltype(goldsrc::clientdata_t::iuser4);
    constexpr std::size_t o898 = offsetof(goldsrc::clientdata_t, fuser1);
    using t893 = decltype(goldsrc::clientdata_t::fuser1);
    constexpr std::size_t o899 = offsetof(goldsrc::clientdata_t, fuser2);
    using t894 = decltype(goldsrc::clientdata_t::fuser2);
    constexpr std::size_t o900 = offsetof(goldsrc::clientdata_t, fuser3);
    using t895 = decltype(goldsrc::clientdata_t::fuser3);
    constexpr std::size_t o901 = offsetof(goldsrc::clientdata_t, fuser4);
    using t896 = decltype(goldsrc::clientdata_t::fuser4);
    constexpr std::size_t o902 = offsetof(goldsrc::clientdata_t, vuser1);
    using t897 = decltype(goldsrc::clientdata_t::vuser1);
    constexpr std::size_t o903 = offsetof(goldsrc::clientdata_t, vuser2);
    using t898 = decltype(goldsrc::clientdata_t::vuser2);
    constexpr std::size_t o904 = offsetof(goldsrc::clientdata_t, vuser3);
    using t899 = decltype(goldsrc::clientdata_t::vuser3);
    constexpr std::size_t o905 = offsetof(goldsrc::clientdata_t, vuser4);
    using t900 = decltype(goldsrc::clientdata_t::vuser4);
    constexpr std::size_t s71 = sizeof(goldsrc::entity_state_t);
    constexpr std::size_t o906 = offsetof(goldsrc::entity_state_t, entityType);
    using t901 = decltype(goldsrc::entity_state_t::entityType);
    constexpr std::size_t o907 = offsetof(goldsrc::entity_state_t, number);
    using t902 = decltype(goldsrc::entity_state_t::number);
    constexpr std::size_t o908 = offsetof(goldsrc::entity_state_t, msg_time);
    using t903 = decltype(goldsrc::entity_state_t::msg_time);
    constexpr std::size_t o909 = offsetof(goldsrc::entity_state_t, messagenum);
    using t904 = decltype(goldsrc::entity_state_t::messagenum);
    constexpr std::size_t o910 = offsetof(goldsrc::entity_state_t, origin);
    using t905 = decltype(goldsrc::entity_state_t::origin);
    constexpr std::size_t o911 = offsetof(goldsrc::entity_state_t, angles);
    using t906 = decltype(goldsrc::entity_state_t::angles);
    constexpr std::size_t o912 = offsetof(goldsrc::entity_state_t, modelindex);
    using t907 = decltype(goldsrc::entity_state_t::modelindex);
    constexpr std::size_t o913 = offsetof(goldsrc::entity_state_t, sequence);
    using t908 = decltype(goldsrc::entity_state_t::sequence);
    constexpr std::size_t o914 = offsetof(goldsrc::entity_state_t, frame);
    using t909 = decltype(goldsrc::entity_state_t::frame);
    constexpr std::size_t o915 = offsetof(goldsrc::entity_state_t, colormap);
    using t910 = decltype(goldsrc::entity_state_t::colormap);
    constexpr std::size_t o916 = offsetof(goldsrc::entity_state_t, skin);
    using t911 = decltype(goldsrc::entity_state_t::skin);
    constexpr std::size_t o917 = offsetof(goldsrc::entity_state_t, solid);
    using t912 = decltype(goldsrc::entity_state_t::solid);
    constexpr std::size_t o918 = offsetof(goldsrc::entity_state_t, effects);
    using t913 = decltype(goldsrc::entity_state_t::effects);
    constexpr std::size_t o919 = offsetof(goldsrc::entity_state_t, scale);
    using t914 = decltype(goldsrc::entity_state_t::scale);
    constexpr std::size_t o920 = offsetof(goldsrc::entity_state_t, eflags);
    using t915 = decltype(goldsrc::entity_state_t::eflags);
    constexpr std::size_t o921 = offsetof(goldsrc::entity_state_t, rendermode);
    using t916 = decltype(goldsrc::entity_state_t::rendermode);
    constexpr std::size_t o922 = offsetof(goldsrc::entity_state_t, renderamt);
    using t917 = decltype(goldsrc::entity_state_t::renderamt);
    constexpr std::size_t o923 = offsetof(goldsrc::entity_state_t, rendercolor);
    using t918 = decltype(goldsrc::entity_state_t::rendercolor);
    constexpr std::size_t o924 = offsetof(goldsrc::entity_state_t, renderfx);
    using t919 = decltype(goldsrc::entity_state_t::renderfx);
    constexpr std::size_t o925 = offsetof(goldsrc::entity_state_t, movetype);
    using t920 = decltype(goldsrc::entity_state_t::movetype);
    constexpr std::size_t o926 = offsetof(goldsrc::entity_state_t, animtime);
    using t921 = decltype(goldsrc::entity_state_t::animtime);
    constexpr std::size_t o927 = offsetof(goldsrc::entity_state_t, framerate);
    using t922 = decltype(goldsrc::entity_state_t::framerate);
    constexpr std::size_t o928 = offsetof(goldsrc::entity_state_t, body);
    using t923 = decltype(goldsrc::entity_state_t::body);
    constexpr std::size_t o929 = offsetof(goldsrc::entity_state_t, controller);
    using t924 = decltype(goldsrc::entity_state_t::controller);
    constexpr std::size_t o930 = offsetof(goldsrc::entity_state_t, blending);
    using t925 = decltype(goldsrc::entity_state_t::blending);
    constexpr std::size_t o931 = offsetof(goldsrc::entity_state_t, velocity);
    using t926 = decltype(goldsrc::entity_state_t::velocity);
    constexpr std::size_t o932 = offsetof(goldsrc::entity_state_t, mins);
    using t927 = decltype(goldsrc::entity_state_t::mins);
    constexpr std::size_t o933 = offsetof(goldsrc::entity_state_t, maxs);
    using t928 = decltype(goldsrc::entity_state_t::maxs);
    constexpr std::size_t o934 = offsetof(goldsrc::entity_state_t, aiment);
    using t929 = decltype(goldsrc::entity_state_t::aiment);
    constexpr std::size_t o935 = offsetof(goldsrc::entity_state_t, owner);
    using t930 = decltype(goldsrc::entity_state_t::owner);
    constexpr std::size_t o936 = offsetof(goldsrc::entity_state_t, friction);
    using t931 = decltype(goldsrc::entity_state_t::friction);
    constexpr std::size_t o937 = offsetof(goldsrc::entity_state_t, gravity);
    using t932 = decltype(goldsrc::entity_state_t::gravity);
    constexpr std::size_t o938 = offsetof(goldsrc::entity_state_t, team);
    using t933 = decltype(goldsrc::entity_state_t::team);
    constexpr std::size_t o939 = offsetof(goldsrc::entity_state_t, playerclass);
    using t934 = decltype(goldsrc::entity_state_t::playerclass);
    constexpr std::size_t o940 = offsetof(goldsrc::entity_state_t, health);
    using t935 = decltype(goldsrc::entity_state_t::health);
    constexpr std::size_t o941 = offsetof(goldsrc::entity_state_t, spectator);
    using t936 = decltype(goldsrc::entity_state_t::spectator);
    constexpr std::size_t o942 = offsetof(goldsrc::entity_state_t, weaponmodel);
    using t937 = decltype(goldsrc::entity_state_t::weaponmodel);
    constexpr std::size_t o943 = offsetof(goldsrc::entity_state_t, gaitsequence);
    using t938 = decltype(goldsrc::entity_state_t::gaitsequence);
    constexpr std::size_t o944 = offsetof(goldsrc::entity_state_t, basevelocity);
    using t939 = decltype(goldsrc::entity_state_t::basevelocity);
    constexpr std::size_t o945 = offsetof(goldsrc::entity_state_t, usehull);
    using t940 = decltype(goldsrc::entity_state_t::usehull);
    constexpr std::size_t o946 = offsetof(goldsrc::entity_state_t, oldbuttons);
    using t941 = decltype(goldsrc::entity_state_t::oldbuttons);
    constexpr std::size_t o947 = offsetof(goldsrc::entity_state_t, onground);
    using t942 = decltype(goldsrc::entity_state_t::onground);
    constexpr std::size_t o948 = offsetof(goldsrc::entity_state_t, iStepLeft);
    using t943 = decltype(goldsrc::entity_state_t::iStepLeft);
    constexpr std::size_t o949 = offsetof(goldsrc::entity_state_t, flFallVelocity);
    using t944 = decltype(goldsrc::entity_state_t::flFallVelocity);
    constexpr std::size_t o950 = offsetof(goldsrc::entity_state_t, fov);
    using t945 = decltype(goldsrc::entity_state_t::fov);
    constexpr std::size_t o951 = offsetof(goldsrc::entity_state_t, weaponanim);
    using t946 = decltype(goldsrc::entity_state_t::weaponanim);
    constexpr std::size_t o952 = offsetof(goldsrc::entity_state_t, startpos);
    using t947 = decltype(goldsrc::entity_state_t::startpos);
    constexpr std::size_t o953 = offsetof(goldsrc::entity_state_t, endpos);
    using t948 = decltype(goldsrc::entity_state_t::endpos);
    constexpr std::size_t o954 = offsetof(goldsrc::entity_state_t, impacttime);
    using t949 = decltype(goldsrc::entity_state_t::impacttime);
    constexpr std::size_t o955 = offsetof(goldsrc::entity_state_t, starttime);
    using t950 = decltype(goldsrc::entity_state_t::starttime);
    constexpr std::size_t o956 = offsetof(goldsrc::entity_state_t, iuser1);
    using t951 = decltype(goldsrc::entity_state_t::iuser1);
    constexpr std::size_t o957 = offsetof(goldsrc::entity_state_t, iuser2);
    using t952 = decltype(goldsrc::entity_state_t::iuser2);
    constexpr std::size_t o958 = offsetof(goldsrc::entity_state_t, iuser3);
    using t953 = decltype(goldsrc::entity_state_t::iuser3);
    constexpr std::size_t o959 = offsetof(goldsrc::entity_state_t, iuser4);
    using t954 = decltype(goldsrc::entity_state_t::iuser4);
    constexpr std::size_t o960 = offsetof(goldsrc::entity_state_t, fuser1);
    using t955 = decltype(goldsrc::entity_state_t::fuser1);
    constexpr std::size_t o961 = offsetof(goldsrc::entity_state_t, fuser2);
    using t956 = decltype(goldsrc::entity_state_t::fuser2);
    constexpr std::size_t o962 = offsetof(goldsrc::entity_state_t, fuser3);
    using t957 = decltype(goldsrc::entity_state_t::fuser3);
    constexpr std::size_t o963 = offsetof(goldsrc::entity_state_t, fuser4);
    using t958 = decltype(goldsrc::entity_state_t::fuser4);
    constexpr std::size_t o964 = offsetof(goldsrc::entity_state_t, vuser1);
    using t959 = decltype(goldsrc::entity_state_t::vuser1);
    constexpr std::size_t o965 = offsetof(goldsrc::entity_state_t, vuser2);
    using t960 = decltype(goldsrc::entity_state_t::vuser2);
    constexpr std::size_t o966 = offsetof(goldsrc::entity_state_t, vuser3);
    using t961 = decltype(goldsrc::entity_state_t::vuser3);
    constexpr std::size_t o967 = offsetof(goldsrc::entity_state_t, vuser4);
    using t962 = decltype(goldsrc::entity_state_t::vuser4);
    constexpr std::size_t s72 = sizeof(goldsrc::latchedvars_t);
    constexpr std::size_t o968 = offsetof(goldsrc::latchedvars_t, prevanimtime);
    using t963 = decltype(goldsrc::latchedvars_t::prevanimtime);
    constexpr std::size_t o969 = offsetof(goldsrc::latchedvars_t, sequencetime);
    using t964 = decltype(goldsrc::latchedvars_t::sequencetime);
    constexpr std::size_t o970 = offsetof(goldsrc::latchedvars_t, prevseqblending);
    using t965 = decltype(goldsrc::latchedvars_t::prevseqblending);
    constexpr std::size_t o971 = offsetof(goldsrc::latchedvars_t, prevorigin);
    using t966 = decltype(goldsrc::latchedvars_t::prevorigin);
    constexpr std::size_t o972 = offsetof(goldsrc::latchedvars_t, prevangles);
    using t967 = decltype(goldsrc::latchedvars_t::prevangles);
    constexpr std::size_t o973 = offsetof(goldsrc::latchedvars_t, prevsequence);
    using t968 = decltype(goldsrc::latchedvars_t::prevsequence);
    constexpr std::size_t o974 = offsetof(goldsrc::latchedvars_t, prevframe);
    using t969 = decltype(goldsrc::latchedvars_t::prevframe);
    constexpr std::size_t o975 = offsetof(goldsrc::latchedvars_t, prevcontroller);
    using t970 = decltype(goldsrc::latchedvars_t::prevcontroller);
    constexpr std::size_t o976 = offsetof(goldsrc::latchedvars_t, prevblending);
    using t971 = decltype(goldsrc::latchedvars_t::prevblending);
    constexpr std::size_t s73 = sizeof(goldsrc::mouth_t);
    constexpr std::size_t o977 = offsetof(goldsrc::mouth_t, mouthopen);
    using t972 = decltype(goldsrc::mouth_t::mouthopen);
    constexpr std::size_t o978 = offsetof(goldsrc::mouth_t, sndcount);
    using t973 = decltype(goldsrc::mouth_t::sndcount);
    constexpr std::size_t o979 = offsetof(goldsrc::mouth_t, sndavg);
    using t974 = decltype(goldsrc::mouth_t::sndavg);
    constexpr std::size_t s74 = sizeof(goldsrc::position_history_t);
    constexpr std::size_t o980 = offsetof(goldsrc::position_history_t, animtime);
    using t975 = decltype(goldsrc::position_history_t::animtime);
    constexpr std::size_t o981 = offsetof(goldsrc::position_history_t, origin);
    using t976 = decltype(goldsrc::position_history_t::origin);
    constexpr std::size_t o982 = offsetof(goldsrc::position_history_t, angles);
    using t977 = decltype(goldsrc::position_history_t::angles);
    constexpr std::size_t s75 = sizeof(goldsrc::weapon_data_t);
    constexpr std::size_t o983 = offsetof(goldsrc::weapon_data_t, m_iId);
    using t978 = decltype(goldsrc::weapon_data_t::m_iId);
    constexpr std::size_t o984 = offsetof(goldsrc::weapon_data_t, m_iClip);
    using t979 = decltype(goldsrc::weapon_data_t::m_iClip);
    constexpr std::size_t o985 = offsetof(goldsrc::weapon_data_t, m_flNextPrimaryAttack);
    using t980 = decltype(goldsrc::weapon_data_t::m_flNextPrimaryAttack);
    constexpr std::size_t o986 = offsetof(goldsrc::weapon_data_t, m_flNextSecondaryAttack);
    using t981 = decltype(goldsrc::weapon_data_t::m_flNextSecondaryAttack);
    constexpr std::size_t o987 = offsetof(goldsrc::weapon_data_t, m_flTimeWeaponIdle);
    using t982 = decltype(goldsrc::weapon_data_t::m_flTimeWeaponIdle);
    constexpr std::size_t o988 = offsetof(goldsrc::weapon_data_t, m_fInReload);
    using t983 = decltype(goldsrc::weapon_data_t::m_fInReload);
    constexpr std::size_t o989 = offsetof(goldsrc::weapon_data_t, m_fInSpecialReload);
    using t984 = decltype(goldsrc::weapon_data_t::m_fInSpecialReload);
    constexpr std::size_t o990 = offsetof(goldsrc::weapon_data_t, m_flNextReload);
    using t985 = decltype(goldsrc::weapon_data_t::m_flNextReload);
    constexpr std::size_t o991 = offsetof(goldsrc::weapon_data_t, m_flPumpTime);
    using t986 = decltype(goldsrc::weapon_data_t::m_flPumpTime);
    constexpr std::size_t o992 = offsetof(goldsrc::weapon_data_t, m_fReloadTime);
    using t987 = decltype(goldsrc::weapon_data_t::m_fReloadTime);
    constexpr std::size_t o993 = offsetof(goldsrc::weapon_data_t, m_fAimedDamage);
    using t988 = decltype(goldsrc::weapon_data_t::m_fAimedDamage);
    constexpr std::size_t o994 = offsetof(goldsrc::weapon_data_t, m_fNextAimBonus);
    using t989 = decltype(goldsrc::weapon_data_t::m_fNextAimBonus);
    constexpr std::size_t o995 = offsetof(goldsrc::weapon_data_t, m_fInZoom);
    using t990 = decltype(goldsrc::weapon_data_t::m_fInZoom);
    constexpr std::size_t o996 = offsetof(goldsrc::weapon_data_t, m_iWeaponState);
    using t991 = decltype(goldsrc::weapon_data_t::m_iWeaponState);
    constexpr std::size_t o997 = offsetof(goldsrc::weapon_data_t, iuser1);
    using t992 = decltype(goldsrc::weapon_data_t::iuser1);
    constexpr std::size_t o998 = offsetof(goldsrc::weapon_data_t, iuser2);
    using t993 = decltype(goldsrc::weapon_data_t::iuser2);
    constexpr std::size_t o999 = offsetof(goldsrc::weapon_data_t, iuser3);
    using t994 = decltype(goldsrc::weapon_data_t::iuser3);
    constexpr std::size_t o1000 = offsetof(goldsrc::weapon_data_t, iuser4);
    using t995 = decltype(goldsrc::weapon_data_t::iuser4);
    constexpr std::size_t o1001 = offsetof(goldsrc::weapon_data_t, fuser1);
    using t996 = decltype(goldsrc::weapon_data_t::fuser1);
    constexpr std::size_t o1002 = offsetof(goldsrc::weapon_data_t, fuser2);
    using t997 = decltype(goldsrc::weapon_data_t::fuser2);
    constexpr std::size_t o1003 = offsetof(goldsrc::weapon_data_t, fuser3);
    using t998 = decltype(goldsrc::weapon_data_t::fuser3);
    constexpr std::size_t o1004 = offsetof(goldsrc::weapon_data_t, fuser4);
    using t999 = decltype(goldsrc::weapon_data_t::fuser4);
    constexpr std::size_t s76 = sizeof(goldsrc::cl_entity_t);
    constexpr std::size_t o1005 = offsetof(goldsrc::cl_entity_t, index);
    using t1000 = decltype(goldsrc::cl_entity_t::index);
    constexpr std::size_t o1006 = offsetof(goldsrc::cl_entity_t, player);
    using t1001 = decltype(goldsrc::cl_entity_t::player);
    constexpr std::size_t o1007 = offsetof(goldsrc::cl_entity_t, baseline);
    using t1002 = decltype(goldsrc::cl_entity_t::baseline);
    constexpr std::size_t o1008 = offsetof(goldsrc::cl_entity_t, prevstate);
    using t1003 = decltype(goldsrc::cl_entity_t::prevstate);
    constexpr std::size_t o1009 = offsetof(goldsrc::cl_entity_t, curstate);
    using t1004 = decltype(goldsrc::cl_entity_t::curstate);
    constexpr std::size_t o1010 = offsetof(goldsrc::cl_entity_t, current_position);
    using t1005 = decltype(goldsrc::cl_entity_t::current_position);
    constexpr std::size_t o1011 = offsetof(goldsrc::cl_entity_t, ph);
    using t1006 = decltype(goldsrc::cl_entity_t::ph);
    constexpr std::size_t o1012 = offsetof(goldsrc::cl_entity_t, mouth);
    using t1007 = decltype(goldsrc::cl_entity_t::mouth);
    constexpr std::size_t o1013 = offsetof(goldsrc::cl_entity_t, latched);
    using t1008 = decltype(goldsrc::cl_entity_t::latched);
    constexpr std::size_t o1014 = offsetof(goldsrc::cl_entity_t, lastmove);
    using t1009 = decltype(goldsrc::cl_entity_t::lastmove);
    constexpr std::size_t o1015 = offsetof(goldsrc::cl_entity_t, origin);
    using t1010 = decltype(goldsrc::cl_entity_t::origin);
    constexpr std::size_t o1016 = offsetof(goldsrc::cl_entity_t, angles);
    using t1011 = decltype(goldsrc::cl_entity_t::angles);
    constexpr std::size_t o1017 = offsetof(goldsrc::cl_entity_t, attachment);
    using t1012 = decltype(goldsrc::cl_entity_t::attachment);
    constexpr std::size_t o1018 = offsetof(goldsrc::cl_entity_t, trivial_accept);
    using t1013 = decltype(goldsrc::cl_entity_t::trivial_accept);
    constexpr std::size_t o1019 = offsetof(goldsrc::cl_entity_t, model);
    using t1014 = decltype(goldsrc::cl_entity_t::model);
    constexpr std::size_t o1020 = offsetof(goldsrc::cl_entity_t, efrag);
    using t1015 = decltype(goldsrc::cl_entity_t::efrag);
    constexpr std::size_t o1021 = offsetof(goldsrc::cl_entity_t, topnode);
    using t1016 = decltype(goldsrc::cl_entity_t::topnode);
    constexpr std::size_t o1022 = offsetof(goldsrc::cl_entity_t, syncbase);
    using t1017 = decltype(goldsrc::cl_entity_t::syncbase);
    constexpr std::size_t o1023 = offsetof(goldsrc::cl_entity_t, visframe);
    using t1018 = decltype(goldsrc::cl_entity_t::visframe);
    constexpr std::size_t o1024 = offsetof(goldsrc::cl_entity_t, cvFloorColor);
    using t1019 = decltype(goldsrc::cl_entity_t::cvFloorColor);
    constexpr std::size_t s77 = sizeof(goldsrc::local_state_t);
    constexpr std::size_t o1025 = offsetof(goldsrc::local_state_t, playerstate);
    using t1020 = decltype(goldsrc::local_state_t::playerstate);
    constexpr std::size_t o1026 = offsetof(goldsrc::local_state_t, client);
    using t1021 = decltype(goldsrc::local_state_t::client);
    constexpr std::size_t o1027 = offsetof(goldsrc::local_state_t, weapondata);
    using t1022 = decltype(goldsrc::local_state_t::weapondata);
    constexpr std::size_t s78 = sizeof(goldsrc::event_api_s);
    constexpr std::size_t o1028 = offsetof(goldsrc::event_api_s, version);
    using t1023 = decltype(goldsrc::event_api_s::version);
    constexpr std::size_t o1029 = offsetof(goldsrc::event_api_s, EV_PlaySound);
    using t1024 = decltype(goldsrc::event_api_s::EV_PlaySound);
    constexpr std::size_t o1030 = offsetof(goldsrc::event_api_s, EV_StopSound);
    using t1025 = decltype(goldsrc::event_api_s::EV_StopSound);
    constexpr std::size_t o1031 = offsetof(goldsrc::event_api_s, EV_FindModelIndex);
    using t1026 = decltype(goldsrc::event_api_s::EV_FindModelIndex);
    constexpr std::size_t o1032 = offsetof(goldsrc::event_api_s, EV_IsLocal);
    using t1027 = decltype(goldsrc::event_api_s::EV_IsLocal);
    constexpr std::size_t o1033 = offsetof(goldsrc::event_api_s, EV_LocalPlayerDucking);
    using t1028 = decltype(goldsrc::event_api_s::EV_LocalPlayerDucking);
    constexpr std::size_t o1034 = offsetof(goldsrc::event_api_s, EV_LocalPlayerViewheight);
    using t1029 = decltype(goldsrc::event_api_s::EV_LocalPlayerViewheight);
    constexpr std::size_t o1035 = offsetof(goldsrc::event_api_s, EV_LocalPlayerBounds);
    using t1030 = decltype(goldsrc::event_api_s::EV_LocalPlayerBounds);
    constexpr std::size_t o1036 = offsetof(goldsrc::event_api_s, EV_IndexFromTrace);
    using t1031 = decltype(goldsrc::event_api_s::EV_IndexFromTrace);
    constexpr std::size_t o1037 = offsetof(goldsrc::event_api_s, EV_GetPhysent);
    using t1032 = decltype(goldsrc::event_api_s::EV_GetPhysent);
    constexpr std::size_t o1038 = offsetof(goldsrc::event_api_s, EV_SetUpPlayerPrediction);
    using t1033 = decltype(goldsrc::event_api_s::EV_SetUpPlayerPrediction);
    constexpr std::size_t o1039 = offsetof(goldsrc::event_api_s, EV_PushPMStates);
    using t1034 = decltype(goldsrc::event_api_s::EV_PushPMStates);
    constexpr std::size_t o1040 = offsetof(goldsrc::event_api_s, EV_PopPMStates);
    using t1035 = decltype(goldsrc::event_api_s::EV_PopPMStates);
    constexpr std::size_t o1041 = offsetof(goldsrc::event_api_s, EV_SetSolidPlayers);
    using t1036 = decltype(goldsrc::event_api_s::EV_SetSolidPlayers);
    constexpr std::size_t o1042 = offsetof(goldsrc::event_api_s, EV_SetTraceHull);
    using t1037 = decltype(goldsrc::event_api_s::EV_SetTraceHull);
    constexpr std::size_t o1043 = offsetof(goldsrc::event_api_s, EV_PlayerTrace);
    using t1038 = decltype(goldsrc::event_api_s::EV_PlayerTrace);
    constexpr std::size_t o1044 = offsetof(goldsrc::event_api_s, EV_WeaponAnimation);
    using t1039 = decltype(goldsrc::event_api_s::EV_WeaponAnimation);
    constexpr std::size_t o1045 = offsetof(goldsrc::event_api_s, EV_PrecacheEvent);
    using t1040 = decltype(goldsrc::event_api_s::EV_PrecacheEvent);
    constexpr std::size_t o1046 = offsetof(goldsrc::event_api_s, EV_PlaybackEvent);
    using t1041 = decltype(goldsrc::event_api_s::EV_PlaybackEvent);
    constexpr std::size_t o1047 = offsetof(goldsrc::event_api_s, EV_TraceTexture);
    using t1042 = decltype(goldsrc::event_api_s::EV_TraceTexture);
    constexpr std::size_t o1048 = offsetof(goldsrc::event_api_s, EV_StopAllSounds);
    using t1043 = decltype(goldsrc::event_api_s::EV_StopAllSounds);
    constexpr std::size_t o1049 = offsetof(goldsrc::event_api_s, EV_KillEvents);
    using t1044 = decltype(goldsrc::event_api_s::EV_KillEvents);
    constexpr std::size_t s79 = sizeof(goldsrc::event_args_t);
    constexpr std::size_t o1050 = offsetof(goldsrc::event_args_t, flags);
    using t1045 = decltype(goldsrc::event_args_t::flags);
    constexpr std::size_t o1051 = offsetof(goldsrc::event_args_t, entindex);
    using t1046 = decltype(goldsrc::event_args_t::entindex);
    constexpr std::size_t o1052 = offsetof(goldsrc::event_args_t, origin);
    using t1047 = decltype(goldsrc::event_args_t::origin);
    constexpr std::size_t o1053 = offsetof(goldsrc::event_args_t, angles);
    using t1048 = decltype(goldsrc::event_args_t::angles);
    constexpr std::size_t o1054 = offsetof(goldsrc::event_args_t, velocity);
    using t1049 = decltype(goldsrc::event_args_t::velocity);
    constexpr std::size_t o1055 = offsetof(goldsrc::event_args_t, ducking);
    using t1050 = decltype(goldsrc::event_args_t::ducking);
    constexpr std::size_t o1056 = offsetof(goldsrc::event_args_t, fparam1);
    using t1051 = decltype(goldsrc::event_args_t::fparam1);
    constexpr std::size_t o1057 = offsetof(goldsrc::event_args_t, fparam2);
    using t1052 = decltype(goldsrc::event_args_t::fparam2);
    constexpr std::size_t o1058 = offsetof(goldsrc::event_args_t, iparam1);
    using t1053 = decltype(goldsrc::event_args_t::iparam1);
    constexpr std::size_t o1059 = offsetof(goldsrc::event_args_t, iparam2);
    using t1054 = decltype(goldsrc::event_args_t::iparam2);
    constexpr std::size_t o1060 = offsetof(goldsrc::event_args_t, bparam1);
    using t1055 = decltype(goldsrc::event_args_t::bparam1);
    constexpr std::size_t o1061 = offsetof(goldsrc::event_args_t, bparam2);
    using t1056 = decltype(goldsrc::event_args_t::bparam2);
    constexpr std::size_t s80 = sizeof(goldsrc::dmodel_t);
    constexpr std::size_t o1062 = offsetof(goldsrc::dmodel_t, mins);
    using t1057 = decltype(goldsrc::dmodel_t::mins);
    constexpr std::size_t o1063 = offsetof(goldsrc::dmodel_t, maxs);
    using t1058 = decltype(goldsrc::dmodel_t::maxs);
    constexpr std::size_t o1064 = offsetof(goldsrc::dmodel_t, origin);
    using t1059 = decltype(goldsrc::dmodel_t::origin);
    constexpr std::size_t o1065 = offsetof(goldsrc::dmodel_t, headnode);
    using t1060 = decltype(goldsrc::dmodel_t::headnode);
    constexpr std::size_t o1066 = offsetof(goldsrc::dmodel_t, visleafs);
    using t1061 = decltype(goldsrc::dmodel_t::visleafs);
    constexpr std::size_t o1067 = offsetof(goldsrc::dmodel_t, firstface);
    using t1062 = decltype(goldsrc::dmodel_t::firstface);
    constexpr std::size_t o1068 = offsetof(goldsrc::dmodel_t, numfaces);
    using t1063 = decltype(goldsrc::dmodel_t::numfaces);
    constexpr std::size_t s81 = sizeof(goldsrc::dspriteframe_t);
    constexpr std::size_t o1069 = offsetof(goldsrc::dspriteframe_t, origin);
    using t1064 = decltype(goldsrc::dspriteframe_t::origin);
    constexpr std::size_t o1070 = offsetof(goldsrc::dspriteframe_t, width);
    using t1065 = decltype(goldsrc::dspriteframe_t::width);
    constexpr std::size_t o1071 = offsetof(goldsrc::dspriteframe_t, height);
    using t1066 = decltype(goldsrc::dspriteframe_t::height);
    constexpr std::size_t s82 = sizeof(goldsrc::dspritegroup_t);
    constexpr std::size_t o1072 = offsetof(goldsrc::dspritegroup_t, numframes);
    using t1067 = decltype(goldsrc::dspritegroup_t::numframes);
    constexpr std::size_t s83 = sizeof(goldsrc::dspriteinterval_t);
    constexpr std::size_t o1073 = offsetof(goldsrc::dspriteinterval_t, interval);
    using t1068 = decltype(goldsrc::dspriteinterval_t::interval);
    constexpr std::size_t s84 = sizeof(goldsrc::dsprite_t);
    constexpr std::size_t o1074 = offsetof(goldsrc::dsprite_t, ident);
    using t1069 = decltype(goldsrc::dsprite_t::ident);
    constexpr std::size_t o1075 = offsetof(goldsrc::dsprite_t, version);
    using t1070 = decltype(goldsrc::dsprite_t::version);
    constexpr std::size_t o1076 = offsetof(goldsrc::dsprite_t, type);
    using t1071 = decltype(goldsrc::dsprite_t::type);
    constexpr std::size_t o1077 = offsetof(goldsrc::dsprite_t, texFormat);
    using t1072 = decltype(goldsrc::dsprite_t::texFormat);
    constexpr std::size_t o1078 = offsetof(goldsrc::dsprite_t, boundingradius);
    using t1073 = decltype(goldsrc::dsprite_t::boundingradius);
    constexpr std::size_t o1079 = offsetof(goldsrc::dsprite_t, width);
    using t1074 = decltype(goldsrc::dsprite_t::width);
    constexpr std::size_t o1080 = offsetof(goldsrc::dsprite_t, height);
    using t1075 = decltype(goldsrc::dsprite_t::height);
    constexpr std::size_t o1081 = offsetof(goldsrc::dsprite_t, numframes);
    using t1076 = decltype(goldsrc::dsprite_t::numframes);
    constexpr std::size_t o1082 = offsetof(goldsrc::dsprite_t, beamlength);
    using t1077 = decltype(goldsrc::dsprite_t::beamlength);
    constexpr std::size_t o1083 = offsetof(goldsrc::dsprite_t, synctype);
    using t1078 = decltype(goldsrc::dsprite_t::synctype);
    constexpr std::size_t s85 = sizeof(goldsrc::dspriteframetype_t);
    constexpr std::size_t o1084 = offsetof(goldsrc::dspriteframetype_t, type);
    using t1079 = decltype(goldsrc::dspriteframetype_t::type);
    constexpr std::size_t s86 = sizeof(goldsrc::mstudioanim_t);
    constexpr std::size_t o1085 = offsetof(goldsrc::mstudioanim_t, offset);
    using t1080 = decltype(goldsrc::mstudioanim_t::offset);
    constexpr std::size_t s87 = sizeof(goldsrc::mstudioanimvalue_t);
    constexpr std::size_t o1086 = offsetof(goldsrc::mstudioanimvalue_t, num);
    constexpr std::size_t o1087 = offsetof(goldsrc::mstudioanimvalue_t, value);
    using t1081 = decltype(goldsrc::mstudioanimvalue_t::value);
    constexpr std::size_t s88 = sizeof(goldsrc::mstudioattachment_t);
    constexpr std::size_t o1088 = offsetof(goldsrc::mstudioattachment_t, name);
    using t1082 = decltype(goldsrc::mstudioattachment_t::name);
    constexpr std::size_t o1089 = offsetof(goldsrc::mstudioattachment_t, type);
    using t1083 = decltype(goldsrc::mstudioattachment_t::type);
    constexpr std::size_t o1090 = offsetof(goldsrc::mstudioattachment_t, bone);
    using t1084 = decltype(goldsrc::mstudioattachment_t::bone);
    constexpr std::size_t o1091 = offsetof(goldsrc::mstudioattachment_t, org);
    using t1085 = decltype(goldsrc::mstudioattachment_t::org);
    constexpr std::size_t o1092 = offsetof(goldsrc::mstudioattachment_t, vectors);
    using t1086 = decltype(goldsrc::mstudioattachment_t::vectors);
    constexpr std::size_t s89 = sizeof(goldsrc::mstudiobodyparts_t);
    constexpr std::size_t o1093 = offsetof(goldsrc::mstudiobodyparts_t, name);
    using t1087 = decltype(goldsrc::mstudiobodyparts_t::name);
    constexpr std::size_t o1094 = offsetof(goldsrc::mstudiobodyparts_t, nummodels);
    using t1088 = decltype(goldsrc::mstudiobodyparts_t::nummodels);
    constexpr std::size_t o1095 = offsetof(goldsrc::mstudiobodyparts_t, base);
    using t1089 = decltype(goldsrc::mstudiobodyparts_t::base);
    constexpr std::size_t o1096 = offsetof(goldsrc::mstudiobodyparts_t, modelindex);
    using t1090 = decltype(goldsrc::mstudiobodyparts_t::modelindex);
    constexpr std::size_t s90 = sizeof(goldsrc::mstudiobone_t);
    constexpr std::size_t o1097 = offsetof(goldsrc::mstudiobone_t, name);
    using t1091 = decltype(goldsrc::mstudiobone_t::name);
    constexpr std::size_t o1098 = offsetof(goldsrc::mstudiobone_t, parent);
    using t1092 = decltype(goldsrc::mstudiobone_t::parent);
    constexpr std::size_t o1099 = offsetof(goldsrc::mstudiobone_t, flags);
    using t1093 = decltype(goldsrc::mstudiobone_t::flags);
    constexpr std::size_t o1100 = offsetof(goldsrc::mstudiobone_t, bonecontroller);
    using t1094 = decltype(goldsrc::mstudiobone_t::bonecontroller);
    constexpr std::size_t o1101 = offsetof(goldsrc::mstudiobone_t, value);
    using t1095 = decltype(goldsrc::mstudiobone_t::value);
    constexpr std::size_t o1102 = offsetof(goldsrc::mstudiobone_t, scale);
    using t1096 = decltype(goldsrc::mstudiobone_t::scale);
    constexpr std::size_t s91 = sizeof(goldsrc::mstudioevent_s);
    constexpr std::size_t o1103 = offsetof(goldsrc::mstudioevent_s, frame);
    using t1097 = decltype(goldsrc::mstudioevent_s::frame);
    constexpr std::size_t o1104 = offsetof(goldsrc::mstudioevent_s, event);
    using t1098 = decltype(goldsrc::mstudioevent_s::event);
    constexpr std::size_t o1105 = offsetof(goldsrc::mstudioevent_s, type);
    using t1099 = decltype(goldsrc::mstudioevent_s::type);
    constexpr std::size_t o1106 = offsetof(goldsrc::mstudioevent_s, options);
    using t1100 = decltype(goldsrc::mstudioevent_s::options);
    constexpr std::size_t s92 = sizeof(goldsrc::mstudioseqdesc_t);
    constexpr std::size_t o1107 = offsetof(goldsrc::mstudioseqdesc_t, label);
    using t1101 = decltype(goldsrc::mstudioseqdesc_t::label);
    constexpr std::size_t o1108 = offsetof(goldsrc::mstudioseqdesc_t, fps);
    using t1102 = decltype(goldsrc::mstudioseqdesc_t::fps);
    constexpr std::size_t o1109 = offsetof(goldsrc::mstudioseqdesc_t, flags);
    using t1103 = decltype(goldsrc::mstudioseqdesc_t::flags);
    constexpr std::size_t o1110 = offsetof(goldsrc::mstudioseqdesc_t, activity);
    using t1104 = decltype(goldsrc::mstudioseqdesc_t::activity);
    constexpr std::size_t o1111 = offsetof(goldsrc::mstudioseqdesc_t, actweight);
    using t1105 = decltype(goldsrc::mstudioseqdesc_t::actweight);
    constexpr std::size_t o1112 = offsetof(goldsrc::mstudioseqdesc_t, numevents);
    using t1106 = decltype(goldsrc::mstudioseqdesc_t::numevents);
    constexpr std::size_t o1113 = offsetof(goldsrc::mstudioseqdesc_t, eventindex);
    using t1107 = decltype(goldsrc::mstudioseqdesc_t::eventindex);
    constexpr std::size_t o1114 = offsetof(goldsrc::mstudioseqdesc_t, numframes);
    using t1108 = decltype(goldsrc::mstudioseqdesc_t::numframes);
    constexpr std::size_t o1115 = offsetof(goldsrc::mstudioseqdesc_t, numpivots);
    using t1109 = decltype(goldsrc::mstudioseqdesc_t::numpivots);
    constexpr std::size_t o1116 = offsetof(goldsrc::mstudioseqdesc_t, pivotindex);
    using t1110 = decltype(goldsrc::mstudioseqdesc_t::pivotindex);
    constexpr std::size_t o1117 = offsetof(goldsrc::mstudioseqdesc_t, motiontype);
    using t1111 = decltype(goldsrc::mstudioseqdesc_t::motiontype);
    constexpr std::size_t o1118 = offsetof(goldsrc::mstudioseqdesc_t, motionbone);
    using t1112 = decltype(goldsrc::mstudioseqdesc_t::motionbone);
    constexpr std::size_t o1119 = offsetof(goldsrc::mstudioseqdesc_t, linearmovement);
    using t1113 = decltype(goldsrc::mstudioseqdesc_t::linearmovement);
    constexpr std::size_t o1120 = offsetof(goldsrc::mstudioseqdesc_t, automoveposindex);
    using t1114 = decltype(goldsrc::mstudioseqdesc_t::automoveposindex);
    constexpr std::size_t o1121 = offsetof(goldsrc::mstudioseqdesc_t, automoveangleindex);
    using t1115 = decltype(goldsrc::mstudioseqdesc_t::automoveangleindex);
    constexpr std::size_t o1122 = offsetof(goldsrc::mstudioseqdesc_t, bbmin);
    using t1116 = decltype(goldsrc::mstudioseqdesc_t::bbmin);
    constexpr std::size_t o1123 = offsetof(goldsrc::mstudioseqdesc_t, bbmax);
    using t1117 = decltype(goldsrc::mstudioseqdesc_t::bbmax);
    constexpr std::size_t o1124 = offsetof(goldsrc::mstudioseqdesc_t, numblends);
    using t1118 = decltype(goldsrc::mstudioseqdesc_t::numblends);
    constexpr std::size_t o1125 = offsetof(goldsrc::mstudioseqdesc_t, animindex);
    using t1119 = decltype(goldsrc::mstudioseqdesc_t::animindex);
    constexpr std::size_t o1126 = offsetof(goldsrc::mstudioseqdesc_t, blendtype);
    using t1120 = decltype(goldsrc::mstudioseqdesc_t::blendtype);
    constexpr std::size_t o1127 = offsetof(goldsrc::mstudioseqdesc_t, blendstart);
    using t1121 = decltype(goldsrc::mstudioseqdesc_t::blendstart);
    constexpr std::size_t o1128 = offsetof(goldsrc::mstudioseqdesc_t, blendend);
    using t1122 = decltype(goldsrc::mstudioseqdesc_t::blendend);
    constexpr std::size_t o1129 = offsetof(goldsrc::mstudioseqdesc_t, blendparent);
    using t1123 = decltype(goldsrc::mstudioseqdesc_t::blendparent);
    constexpr std::size_t o1130 = offsetof(goldsrc::mstudioseqdesc_t, seqgroup);
    using t1124 = decltype(goldsrc::mstudioseqdesc_t::seqgroup);
    constexpr std::size_t o1131 = offsetof(goldsrc::mstudioseqdesc_t, entrynode);
    using t1125 = decltype(goldsrc::mstudioseqdesc_t::entrynode);
    constexpr std::size_t o1132 = offsetof(goldsrc::mstudioseqdesc_t, exitnode);
    using t1126 = decltype(goldsrc::mstudioseqdesc_t::exitnode);
    constexpr std::size_t o1133 = offsetof(goldsrc::mstudioseqdesc_t, nodeflags);
    using t1127 = decltype(goldsrc::mstudioseqdesc_t::nodeflags);
    constexpr std::size_t o1134 = offsetof(goldsrc::mstudioseqdesc_t, nextseq);
    using t1128 = decltype(goldsrc::mstudioseqdesc_t::nextseq);
    constexpr std::size_t s93 = sizeof(goldsrc::mstudiotexture_t);
    constexpr std::size_t o1135 = offsetof(goldsrc::mstudiotexture_t, name);
    using t1129 = decltype(goldsrc::mstudiotexture_t::name);
    constexpr std::size_t o1136 = offsetof(goldsrc::mstudiotexture_t, flags);
    using t1130 = decltype(goldsrc::mstudiotexture_t::flags);
    constexpr std::size_t o1137 = offsetof(goldsrc::mstudiotexture_t, width);
    using t1131 = decltype(goldsrc::mstudiotexture_t::width);
    constexpr std::size_t o1138 = offsetof(goldsrc::mstudiotexture_t, height);
    using t1132 = decltype(goldsrc::mstudiotexture_t::height);
    constexpr std::size_t o1139 = offsetof(goldsrc::mstudiotexture_t, index);
    using t1133 = decltype(goldsrc::mstudiotexture_t::index);
    constexpr std::size_t s94 = sizeof(goldsrc::studiohdr_t);
    constexpr std::size_t o1140 = offsetof(goldsrc::studiohdr_t, id);
    using t1134 = decltype(goldsrc::studiohdr_t::id);
    constexpr std::size_t o1141 = offsetof(goldsrc::studiohdr_t, version);
    using t1135 = decltype(goldsrc::studiohdr_t::version);
    constexpr std::size_t o1142 = offsetof(goldsrc::studiohdr_t, name);
    using t1136 = decltype(goldsrc::studiohdr_t::name);
    constexpr std::size_t o1143 = offsetof(goldsrc::studiohdr_t, length);
    using t1137 = decltype(goldsrc::studiohdr_t::length);
    constexpr std::size_t o1144 = offsetof(goldsrc::studiohdr_t, eyeposition);
    using t1138 = decltype(goldsrc::studiohdr_t::eyeposition);
    constexpr std::size_t o1145 = offsetof(goldsrc::studiohdr_t, min);
    using t1139 = decltype(goldsrc::studiohdr_t::min);
    constexpr std::size_t o1146 = offsetof(goldsrc::studiohdr_t, max);
    using t1140 = decltype(goldsrc::studiohdr_t::max);
    constexpr std::size_t o1147 = offsetof(goldsrc::studiohdr_t, bbmin);
    using t1141 = decltype(goldsrc::studiohdr_t::bbmin);
    constexpr std::size_t o1148 = offsetof(goldsrc::studiohdr_t, bbmax);
    using t1142 = decltype(goldsrc::studiohdr_t::bbmax);
    constexpr std::size_t o1149 = offsetof(goldsrc::studiohdr_t, flags);
    using t1143 = decltype(goldsrc::studiohdr_t::flags);
    constexpr std::size_t o1150 = offsetof(goldsrc::studiohdr_t, numbones);
    using t1144 = decltype(goldsrc::studiohdr_t::numbones);
    constexpr std::size_t o1151 = offsetof(goldsrc::studiohdr_t, boneindex);
    using t1145 = decltype(goldsrc::studiohdr_t::boneindex);
    constexpr std::size_t o1152 = offsetof(goldsrc::studiohdr_t, numbonecontrollers);
    using t1146 = decltype(goldsrc::studiohdr_t::numbonecontrollers);
    constexpr std::size_t o1153 = offsetof(goldsrc::studiohdr_t, bonecontrollerindex);
    using t1147 = decltype(goldsrc::studiohdr_t::bonecontrollerindex);
    constexpr std::size_t o1154 = offsetof(goldsrc::studiohdr_t, numhitboxes);
    using t1148 = decltype(goldsrc::studiohdr_t::numhitboxes);
    constexpr std::size_t o1155 = offsetof(goldsrc::studiohdr_t, hitboxindex);
    using t1149 = decltype(goldsrc::studiohdr_t::hitboxindex);
    constexpr std::size_t o1156 = offsetof(goldsrc::studiohdr_t, numseq);
    using t1150 = decltype(goldsrc::studiohdr_t::numseq);
    constexpr std::size_t o1157 = offsetof(goldsrc::studiohdr_t, seqindex);
    using t1151 = decltype(goldsrc::studiohdr_t::seqindex);
    constexpr std::size_t o1158 = offsetof(goldsrc::studiohdr_t, numseqgroups);
    using t1152 = decltype(goldsrc::studiohdr_t::numseqgroups);
    constexpr std::size_t o1159 = offsetof(goldsrc::studiohdr_t, seqgroupindex);
    using t1153 = decltype(goldsrc::studiohdr_t::seqgroupindex);
    constexpr std::size_t o1160 = offsetof(goldsrc::studiohdr_t, numtextures);
    using t1154 = decltype(goldsrc::studiohdr_t::numtextures);
    constexpr std::size_t o1161 = offsetof(goldsrc::studiohdr_t, textureindex);
    using t1155 = decltype(goldsrc::studiohdr_t::textureindex);
    constexpr std::size_t o1162 = offsetof(goldsrc::studiohdr_t, texturedataindex);
    using t1156 = decltype(goldsrc::studiohdr_t::texturedataindex);
    constexpr std::size_t o1163 = offsetof(goldsrc::studiohdr_t, numskinref);
    using t1157 = decltype(goldsrc::studiohdr_t::numskinref);
    constexpr std::size_t o1164 = offsetof(goldsrc::studiohdr_t, numskinfamilies);
    using t1158 = decltype(goldsrc::studiohdr_t::numskinfamilies);
    constexpr std::size_t o1165 = offsetof(goldsrc::studiohdr_t, skinindex);
    using t1159 = decltype(goldsrc::studiohdr_t::skinindex);
    constexpr std::size_t o1166 = offsetof(goldsrc::studiohdr_t, numbodyparts);
    using t1160 = decltype(goldsrc::studiohdr_t::numbodyparts);
    constexpr std::size_t o1167 = offsetof(goldsrc::studiohdr_t, bodypartindex);
    using t1161 = decltype(goldsrc::studiohdr_t::bodypartindex);
    constexpr std::size_t o1168 = offsetof(goldsrc::studiohdr_t, numattachments);
    using t1162 = decltype(goldsrc::studiohdr_t::numattachments);
    constexpr std::size_t o1169 = offsetof(goldsrc::studiohdr_t, attachmentindex);
    using t1163 = decltype(goldsrc::studiohdr_t::attachmentindex);
    constexpr std::size_t o1170 = offsetof(goldsrc::studiohdr_t, soundtable);
    using t1164 = decltype(goldsrc::studiohdr_t::soundtable);
    constexpr std::size_t o1171 = offsetof(goldsrc::studiohdr_t, soundindex);
    using t1165 = decltype(goldsrc::studiohdr_t::soundindex);
    constexpr std::size_t o1172 = offsetof(goldsrc::studiohdr_t, soundgroups);
    using t1166 = decltype(goldsrc::studiohdr_t::soundgroups);
    constexpr std::size_t o1173 = offsetof(goldsrc::studiohdr_t, soundgroupindex);
    using t1167 = decltype(goldsrc::studiohdr_t::soundgroupindex);
    constexpr std::size_t o1174 = offsetof(goldsrc::studiohdr_t, numtransitions);
    using t1168 = decltype(goldsrc::studiohdr_t::numtransitions);
    constexpr std::size_t o1175 = offsetof(goldsrc::studiohdr_t, transitionindex);
    using t1169 = decltype(goldsrc::studiohdr_t::transitionindex);
    constexpr std::size_t s95 = sizeof(goldsrc::movevars_s);
    constexpr std::size_t o1176 = offsetof(goldsrc::movevars_s, gravity);
    using t1170 = decltype(goldsrc::movevars_s::gravity);
    constexpr std::size_t o1177 = offsetof(goldsrc::movevars_s, stopspeed);
    using t1171 = decltype(goldsrc::movevars_s::stopspeed);
    constexpr std::size_t o1178 = offsetof(goldsrc::movevars_s, maxspeed);
    using t1172 = decltype(goldsrc::movevars_s::maxspeed);
    constexpr std::size_t o1179 = offsetof(goldsrc::movevars_s, spectatormaxspeed);
    using t1173 = decltype(goldsrc::movevars_s::spectatormaxspeed);
    constexpr std::size_t o1180 = offsetof(goldsrc::movevars_s, accelerate);
    using t1174 = decltype(goldsrc::movevars_s::accelerate);
    constexpr std::size_t o1181 = offsetof(goldsrc::movevars_s, airaccelerate);
    using t1175 = decltype(goldsrc::movevars_s::airaccelerate);
    constexpr std::size_t o1182 = offsetof(goldsrc::movevars_s, wateraccelerate);
    using t1176 = decltype(goldsrc::movevars_s::wateraccelerate);
    constexpr std::size_t o1183 = offsetof(goldsrc::movevars_s, friction);
    using t1177 = decltype(goldsrc::movevars_s::friction);
    constexpr std::size_t o1184 = offsetof(goldsrc::movevars_s, edgefriction);
    using t1178 = decltype(goldsrc::movevars_s::edgefriction);
    constexpr std::size_t o1185 = offsetof(goldsrc::movevars_s, waterfriction);
    using t1179 = decltype(goldsrc::movevars_s::waterfriction);
    constexpr std::size_t o1186 = offsetof(goldsrc::movevars_s, entgravity);
    using t1180 = decltype(goldsrc::movevars_s::entgravity);
    constexpr std::size_t o1187 = offsetof(goldsrc::movevars_s, bounce);
    using t1181 = decltype(goldsrc::movevars_s::bounce);
    constexpr std::size_t o1188 = offsetof(goldsrc::movevars_s, stepsize);
    using t1182 = decltype(goldsrc::movevars_s::stepsize);
    constexpr std::size_t o1189 = offsetof(goldsrc::movevars_s, maxvelocity);
    using t1183 = decltype(goldsrc::movevars_s::maxvelocity);
    constexpr std::size_t o1190 = offsetof(goldsrc::movevars_s, zmax);
    using t1184 = decltype(goldsrc::movevars_s::zmax);
    constexpr std::size_t o1191 = offsetof(goldsrc::movevars_s, waveHeight);
    using t1185 = decltype(goldsrc::movevars_s::waveHeight);
    constexpr std::size_t o1192 = offsetof(goldsrc::movevars_s, footsteps);
    using t1186 = decltype(goldsrc::movevars_s::footsteps);
    constexpr std::size_t o1193 = offsetof(goldsrc::movevars_s, skyName);
    using t1187 = decltype(goldsrc::movevars_s::skyName);
    constexpr std::size_t o1194 = offsetof(goldsrc::movevars_s, rollangle);
    using t1188 = decltype(goldsrc::movevars_s::rollangle);
    constexpr std::size_t o1195 = offsetof(goldsrc::movevars_s, rollspeed);
    using t1189 = decltype(goldsrc::movevars_s::rollspeed);
    constexpr std::size_t o1196 = offsetof(goldsrc::movevars_s, skycolor_r);
    using t1190 = decltype(goldsrc::movevars_s::skycolor_r);
    constexpr std::size_t o1197 = offsetof(goldsrc::movevars_s, skycolor_g);
    using t1191 = decltype(goldsrc::movevars_s::skycolor_g);
    constexpr std::size_t o1198 = offsetof(goldsrc::movevars_s, skycolor_b);
    using t1192 = decltype(goldsrc::movevars_s::skycolor_b);
    constexpr std::size_t o1199 = offsetof(goldsrc::movevars_s, skyvec_x);
    using t1193 = decltype(goldsrc::movevars_s::skyvec_x);
    constexpr std::size_t o1200 = offsetof(goldsrc::movevars_s, skyvec_y);
    using t1194 = decltype(goldsrc::movevars_s::skyvec_y);
    constexpr std::size_t o1201 = offsetof(goldsrc::movevars_s, skyvec_z);
    using t1195 = decltype(goldsrc::movevars_s::skyvec_z);
    constexpr std::size_t s96 = sizeof(goldsrc::physent_t);
    constexpr std::size_t o1202 = offsetof(goldsrc::physent_t, name);
    using t1196 = decltype(goldsrc::physent_t::name);
    constexpr std::size_t o1203 = offsetof(goldsrc::physent_t, player);
    using t1197 = decltype(goldsrc::physent_t::player);
    constexpr std::size_t o1204 = offsetof(goldsrc::physent_t, origin);
    using t1198 = decltype(goldsrc::physent_t::origin);
    constexpr std::size_t o1205 = offsetof(goldsrc::physent_t, model);
    using t1199 = decltype(goldsrc::physent_t::model);
    constexpr std::size_t o1206 = offsetof(goldsrc::physent_t, studiomodel);
    using t1200 = decltype(goldsrc::physent_t::studiomodel);
    constexpr std::size_t o1207 = offsetof(goldsrc::physent_t, mins);
    using t1201 = decltype(goldsrc::physent_t::mins);
    constexpr std::size_t o1208 = offsetof(goldsrc::physent_t, maxs);
    using t1202 = decltype(goldsrc::physent_t::maxs);
    constexpr std::size_t o1209 = offsetof(goldsrc::physent_t, info);
    using t1203 = decltype(goldsrc::physent_t::info);
    constexpr std::size_t o1210 = offsetof(goldsrc::physent_t, angles);
    using t1204 = decltype(goldsrc::physent_t::angles);
    constexpr std::size_t o1211 = offsetof(goldsrc::physent_t, solid);
    using t1205 = decltype(goldsrc::physent_t::solid);
    constexpr std::size_t o1212 = offsetof(goldsrc::physent_t, skin);
    using t1206 = decltype(goldsrc::physent_t::skin);
    constexpr std::size_t o1213 = offsetof(goldsrc::physent_t, rendermode);
    using t1207 = decltype(goldsrc::physent_t::rendermode);
    constexpr std::size_t o1214 = offsetof(goldsrc::physent_t, frame);
    using t1208 = decltype(goldsrc::physent_t::frame);
    constexpr std::size_t o1215 = offsetof(goldsrc::physent_t, sequence);
    using t1209 = decltype(goldsrc::physent_t::sequence);
    constexpr std::size_t o1216 = offsetof(goldsrc::physent_t, controller);
    using t1210 = decltype(goldsrc::physent_t::controller);
    constexpr std::size_t o1217 = offsetof(goldsrc::physent_t, blending);
    using t1211 = decltype(goldsrc::physent_t::blending);
    constexpr std::size_t o1218 = offsetof(goldsrc::physent_t, movetype);
    using t1212 = decltype(goldsrc::physent_t::movetype);
    constexpr std::size_t o1219 = offsetof(goldsrc::physent_t, takedamage);
    using t1213 = decltype(goldsrc::physent_t::takedamage);
    constexpr std::size_t o1220 = offsetof(goldsrc::physent_t, blooddecal);
    using t1214 = decltype(goldsrc::physent_t::blooddecal);
    constexpr std::size_t o1221 = offsetof(goldsrc::physent_t, team);
    using t1215 = decltype(goldsrc::physent_t::team);
    constexpr std::size_t o1222 = offsetof(goldsrc::physent_t, classnumber);
    using t1216 = decltype(goldsrc::physent_t::classnumber);
    constexpr std::size_t o1223 = offsetof(goldsrc::physent_t, iuser1);
    using t1217 = decltype(goldsrc::physent_t::iuser1);
    constexpr std::size_t o1224 = offsetof(goldsrc::physent_t, iuser2);
    using t1218 = decltype(goldsrc::physent_t::iuser2);
    constexpr std::size_t o1225 = offsetof(goldsrc::physent_t, iuser3);
    using t1219 = decltype(goldsrc::physent_t::iuser3);
    constexpr std::size_t o1226 = offsetof(goldsrc::physent_t, iuser4);
    using t1220 = decltype(goldsrc::physent_t::iuser4);
    constexpr std::size_t o1227 = offsetof(goldsrc::physent_t, fuser1);
    using t1221 = decltype(goldsrc::physent_t::fuser1);
    constexpr std::size_t o1228 = offsetof(goldsrc::physent_t, fuser2);
    using t1222 = decltype(goldsrc::physent_t::fuser2);
    constexpr std::size_t o1229 = offsetof(goldsrc::physent_t, fuser3);
    using t1223 = decltype(goldsrc::physent_t::fuser3);
    constexpr std::size_t o1230 = offsetof(goldsrc::physent_t, fuser4);
    using t1224 = decltype(goldsrc::physent_t::fuser4);
    constexpr std::size_t o1231 = offsetof(goldsrc::physent_t, vuser1);
    using t1225 = decltype(goldsrc::physent_t::vuser1);
    constexpr std::size_t o1232 = offsetof(goldsrc::physent_t, vuser2);
    using t1226 = decltype(goldsrc::physent_t::vuser2);
    constexpr std::size_t o1233 = offsetof(goldsrc::physent_t, vuser3);
    using t1227 = decltype(goldsrc::physent_t::vuser3);
    constexpr std::size_t o1234 = offsetof(goldsrc::physent_t, vuser4);
    using t1228 = decltype(goldsrc::physent_t::vuser4);
    constexpr std::size_t s97 = sizeof(goldsrc::pmplane_t);
    constexpr std::size_t o1235 = offsetof(goldsrc::pmplane_t, normal);
    using t1229 = decltype(goldsrc::pmplane_t::normal);
    constexpr std::size_t o1236 = offsetof(goldsrc::pmplane_t, dist);
    using t1230 = decltype(goldsrc::pmplane_t::dist);
    constexpr std::size_t s98 = sizeof(goldsrc::pmtrace_t);
    constexpr std::size_t o1237 = offsetof(goldsrc::pmtrace_t, allsolid);
    using t1231 = decltype(goldsrc::pmtrace_t::allsolid);
    constexpr std::size_t o1238 = offsetof(goldsrc::pmtrace_t, startsolid);
    using t1232 = decltype(goldsrc::pmtrace_t::startsolid);
    constexpr std::size_t o1239 = offsetof(goldsrc::pmtrace_t, inopen);
    using t1233 = decltype(goldsrc::pmtrace_t::inopen);
    constexpr std::size_t o1240 = offsetof(goldsrc::pmtrace_t, inwater);
    using t1234 = decltype(goldsrc::pmtrace_t::inwater);
    constexpr std::size_t o1241 = offsetof(goldsrc::pmtrace_t, fraction);
    using t1235 = decltype(goldsrc::pmtrace_t::fraction);
    constexpr std::size_t o1242 = offsetof(goldsrc::pmtrace_t, endpos);
    using t1236 = decltype(goldsrc::pmtrace_t::endpos);
    constexpr std::size_t o1243 = offsetof(goldsrc::pmtrace_t, plane);
    using t1237 = decltype(goldsrc::pmtrace_t::plane);
    constexpr std::size_t o1244 = offsetof(goldsrc::pmtrace_t, ent);
    using t1238 = decltype(goldsrc::pmtrace_t::ent);
    constexpr std::size_t o1245 = offsetof(goldsrc::pmtrace_t, deltavelocity);
    using t1239 = decltype(goldsrc::pmtrace_t::deltavelocity);
    constexpr std::size_t o1246 = offsetof(goldsrc::pmtrace_t, hitgroup);
    using t1240 = decltype(goldsrc::pmtrace_t::hitgroup);
    constexpr std::size_t s99 = sizeof(goldsrc::playermove_t);
    constexpr std::size_t o1247 = offsetof(goldsrc::playermove_t, player_index);
    using t1241 = decltype(goldsrc::playermove_t::player_index);
    constexpr std::size_t o1248 = offsetof(goldsrc::playermove_t, server);
    using t1242 = decltype(goldsrc::playermove_t::server);
    constexpr std::size_t o1249 = offsetof(goldsrc::playermove_t, multiplayer);
    using t1243 = decltype(goldsrc::playermove_t::multiplayer);
    constexpr std::size_t o1250 = offsetof(goldsrc::playermove_t, time);
    using t1244 = decltype(goldsrc::playermove_t::time);
    constexpr std::size_t o1251 = offsetof(goldsrc::playermove_t, frametime);
    using t1245 = decltype(goldsrc::playermove_t::frametime);
    constexpr std::size_t o1252 = offsetof(goldsrc::playermove_t, forward);
    using t1246 = decltype(goldsrc::playermove_t::forward);
    constexpr std::size_t o1253 = offsetof(goldsrc::playermove_t, right);
    using t1247 = decltype(goldsrc::playermove_t::right);
    constexpr std::size_t o1254 = offsetof(goldsrc::playermove_t, up);
    using t1248 = decltype(goldsrc::playermove_t::up);
    constexpr std::size_t o1255 = offsetof(goldsrc::playermove_t, origin);
    using t1249 = decltype(goldsrc::playermove_t::origin);
    constexpr std::size_t o1256 = offsetof(goldsrc::playermove_t, angles);
    using t1250 = decltype(goldsrc::playermove_t::angles);
    constexpr std::size_t o1257 = offsetof(goldsrc::playermove_t, oldangles);
    using t1251 = decltype(goldsrc::playermove_t::oldangles);
    constexpr std::size_t o1258 = offsetof(goldsrc::playermove_t, velocity);
    using t1252 = decltype(goldsrc::playermove_t::velocity);
    constexpr std::size_t o1259 = offsetof(goldsrc::playermove_t, movedir);
    using t1253 = decltype(goldsrc::playermove_t::movedir);
    constexpr std::size_t o1260 = offsetof(goldsrc::playermove_t, basevelocity);
    using t1254 = decltype(goldsrc::playermove_t::basevelocity);
    constexpr std::size_t o1261 = offsetof(goldsrc::playermove_t, view_ofs);
    using t1255 = decltype(goldsrc::playermove_t::view_ofs);
    constexpr std::size_t o1262 = offsetof(goldsrc::playermove_t, flDuckTime);
    using t1256 = decltype(goldsrc::playermove_t::flDuckTime);
    constexpr std::size_t o1263 = offsetof(goldsrc::playermove_t, bInDuck);
    using t1257 = decltype(goldsrc::playermove_t::bInDuck);
    constexpr std::size_t o1264 = offsetof(goldsrc::playermove_t, flTimeStepSound);
    using t1258 = decltype(goldsrc::playermove_t::flTimeStepSound);
    constexpr std::size_t o1265 = offsetof(goldsrc::playermove_t, iStepLeft);
    using t1259 = decltype(goldsrc::playermove_t::iStepLeft);
    constexpr std::size_t o1266 = offsetof(goldsrc::playermove_t, flFallVelocity);
    using t1260 = decltype(goldsrc::playermove_t::flFallVelocity);
    constexpr std::size_t o1267 = offsetof(goldsrc::playermove_t, punchangle);
    using t1261 = decltype(goldsrc::playermove_t::punchangle);
    constexpr std::size_t o1268 = offsetof(goldsrc::playermove_t, flSwimTime);
    using t1262 = decltype(goldsrc::playermove_t::flSwimTime);
    constexpr std::size_t o1269 = offsetof(goldsrc::playermove_t, flNextPrimaryAttack);
    using t1263 = decltype(goldsrc::playermove_t::flNextPrimaryAttack);
    constexpr std::size_t o1270 = offsetof(goldsrc::playermove_t, effects);
    using t1264 = decltype(goldsrc::playermove_t::effects);
    constexpr std::size_t o1271 = offsetof(goldsrc::playermove_t, flags);
    using t1265 = decltype(goldsrc::playermove_t::flags);
    constexpr std::size_t o1272 = offsetof(goldsrc::playermove_t, usehull);
    using t1266 = decltype(goldsrc::playermove_t::usehull);
    constexpr std::size_t o1273 = offsetof(goldsrc::playermove_t, gravity);
    using t1267 = decltype(goldsrc::playermove_t::gravity);
    constexpr std::size_t o1274 = offsetof(goldsrc::playermove_t, friction);
    using t1268 = decltype(goldsrc::playermove_t::friction);
    constexpr std::size_t o1275 = offsetof(goldsrc::playermove_t, oldbuttons);
    using t1269 = decltype(goldsrc::playermove_t::oldbuttons);
    constexpr std::size_t o1276 = offsetof(goldsrc::playermove_t, waterjumptime);
    using t1270 = decltype(goldsrc::playermove_t::waterjumptime);
    constexpr std::size_t o1277 = offsetof(goldsrc::playermove_t, dead);
    using t1271 = decltype(goldsrc::playermove_t::dead);
    constexpr std::size_t o1278 = offsetof(goldsrc::playermove_t, deadflag);
    using t1272 = decltype(goldsrc::playermove_t::deadflag);
    constexpr std::size_t o1279 = offsetof(goldsrc::playermove_t, spectator);
    using t1273 = decltype(goldsrc::playermove_t::spectator);
    constexpr std::size_t o1280 = offsetof(goldsrc::playermove_t, movetype);
    using t1274 = decltype(goldsrc::playermove_t::movetype);
    constexpr std::size_t o1281 = offsetof(goldsrc::playermove_t, onground);
    using t1275 = decltype(goldsrc::playermove_t::onground);
    constexpr std::size_t o1282 = offsetof(goldsrc::playermove_t, waterlevel);
    using t1276 = decltype(goldsrc::playermove_t::waterlevel);
    constexpr std::size_t o1283 = offsetof(goldsrc::playermove_t, watertype);
    using t1277 = decltype(goldsrc::playermove_t::watertype);
    constexpr std::size_t o1284 = offsetof(goldsrc::playermove_t, oldwaterlevel);
    using t1278 = decltype(goldsrc::playermove_t::oldwaterlevel);
    constexpr std::size_t o1285 = offsetof(goldsrc::playermove_t, sztexturename);
    using t1279 = decltype(goldsrc::playermove_t::sztexturename);
    constexpr std::size_t o1286 = offsetof(goldsrc::playermove_t, chtexturetype);
    using t1280 = decltype(goldsrc::playermove_t::chtexturetype);
    constexpr std::size_t o1287 = offsetof(goldsrc::playermove_t, maxspeed);
    using t1281 = decltype(goldsrc::playermove_t::maxspeed);
    constexpr std::size_t o1288 = offsetof(goldsrc::playermove_t, clientmaxspeed);
    using t1282 = decltype(goldsrc::playermove_t::clientmaxspeed);
    constexpr std::size_t o1289 = offsetof(goldsrc::playermove_t, iuser1);
    using t1283 = decltype(goldsrc::playermove_t::iuser1);
    constexpr std::size_t o1290 = offsetof(goldsrc::playermove_t, iuser2);
    using t1284 = decltype(goldsrc::playermove_t::iuser2);
    constexpr std::size_t o1291 = offsetof(goldsrc::playermove_t, iuser3);
    using t1285 = decltype(goldsrc::playermove_t::iuser3);
    constexpr std::size_t o1292 = offsetof(goldsrc::playermove_t, iuser4);
    using t1286 = decltype(goldsrc::playermove_t::iuser4);
    constexpr std::size_t o1293 = offsetof(goldsrc::playermove_t, fuser1);
    using t1287 = decltype(goldsrc::playermove_t::fuser1);
    constexpr std::size_t o1294 = offsetof(goldsrc::playermove_t, fuser2);
    using t1288 = decltype(goldsrc::playermove_t::fuser2);
    constexpr std::size_t o1295 = offsetof(goldsrc::playermove_t, fuser3);
    using t1289 = decltype(goldsrc::playermove_t::fuser3);
    constexpr std::size_t o1296 = offsetof(goldsrc::playermove_t, fuser4);
    using t1290 = decltype(goldsrc::playermove_t::fuser4);
    constexpr std::size_t o1297 = offsetof(goldsrc::playermove_t, vuser1);
    using t1291 = decltype(goldsrc::playermove_t::vuser1);
    constexpr std::size_t o1298 = offsetof(goldsrc::playermove_t, vuser2);
    using t1292 = decltype(goldsrc::playermove_t::vuser2);
    constexpr std::size_t o1299 = offsetof(goldsrc::playermove_t, vuser3);
    using t1293 = decltype(goldsrc::playermove_t::vuser3);
    constexpr std::size_t o1300 = offsetof(goldsrc::playermove_t, vuser4);
    using t1294 = decltype(goldsrc::playermove_t::vuser4);
    constexpr std::size_t o1301 = offsetof(goldsrc::playermove_t, numphysent);
    using t1295 = decltype(goldsrc::playermove_t::numphysent);
    constexpr std::size_t o1302 = offsetof(goldsrc::playermove_t, physents);
    using t1296 = decltype(goldsrc::playermove_t::physents);
    constexpr std::size_t o1303 = offsetof(goldsrc::playermove_t, nummoveent);
    using t1297 = decltype(goldsrc::playermove_t::nummoveent);
    constexpr std::size_t o1304 = offsetof(goldsrc::playermove_t, moveents);
    using t1298 = decltype(goldsrc::playermove_t::moveents);
    constexpr std::size_t o1305 = offsetof(goldsrc::playermove_t, numvisent);
    using t1299 = decltype(goldsrc::playermove_t::numvisent);
    constexpr std::size_t o1306 = offsetof(goldsrc::playermove_t, visents);
    using t1300 = decltype(goldsrc::playermove_t::visents);
    constexpr std::size_t o1307 = offsetof(goldsrc::playermove_t, cmd);
    using t1301 = decltype(goldsrc::playermove_t::cmd);
    constexpr std::size_t o1308 = offsetof(goldsrc::playermove_t, numtouch);
    using t1302 = decltype(goldsrc::playermove_t::numtouch);
    constexpr std::size_t o1309 = offsetof(goldsrc::playermove_t, touchindex);
    using t1303 = decltype(goldsrc::playermove_t::touchindex);
    constexpr std::size_t o1310 = offsetof(goldsrc::playermove_t, physinfo);
    using t1304 = decltype(goldsrc::playermove_t::physinfo);
    constexpr std::size_t o1311 = offsetof(goldsrc::playermove_t, movevars);
    using t1305 = decltype(goldsrc::playermove_t::movevars);
    constexpr std::size_t o1312 = offsetof(goldsrc::playermove_t, _player_mins);
    using t1306 = decltype(goldsrc::playermove_t::_player_mins);
    constexpr std::size_t o1313 = offsetof(goldsrc::playermove_t, _player_maxs);
    using t1307 = decltype(goldsrc::playermove_t::_player_maxs);
    constexpr std::size_t o1314 = offsetof(goldsrc::playermove_t, PM_Info_ValueForKey);
    using t1308 = decltype(goldsrc::playermove_t::PM_Info_ValueForKey);
    constexpr std::size_t o1315 = offsetof(goldsrc::playermove_t, PM_Particle);
    using t1309 = decltype(goldsrc::playermove_t::PM_Particle);
    constexpr std::size_t o1316 = offsetof(goldsrc::playermove_t, PM_TestPlayerPosition);
    using t1310 = decltype(goldsrc::playermove_t::PM_TestPlayerPosition);
    constexpr std::size_t o1317 = offsetof(goldsrc::playermove_t, Con_NPrintf);
    using t1311 = decltype(goldsrc::playermove_t::Con_NPrintf);
    constexpr std::size_t o1318 = offsetof(goldsrc::playermove_t, Con_DPrintf);
    using t1312 = decltype(goldsrc::playermove_t::Con_DPrintf);
    constexpr std::size_t o1319 = offsetof(goldsrc::playermove_t, Con_Printf);
    using t1313 = decltype(goldsrc::playermove_t::Con_Printf);
    constexpr std::size_t o1320 = offsetof(goldsrc::playermove_t, Sys_FloatTime);
    using t1314 = decltype(goldsrc::playermove_t::Sys_FloatTime);
    constexpr std::size_t o1321 = offsetof(goldsrc::playermove_t, PM_StuckTouch);
    using t1315 = decltype(goldsrc::playermove_t::PM_StuckTouch);
    constexpr std::size_t o1322 = offsetof(goldsrc::playermove_t, PM_PointContents);
    using t1316 = decltype(goldsrc::playermove_t::PM_PointContents);
    constexpr std::size_t o1323 = offsetof(goldsrc::playermove_t, PM_TruePointContents);
    using t1317 = decltype(goldsrc::playermove_t::PM_TruePointContents);
    constexpr std::size_t o1324 = offsetof(goldsrc::playermove_t, PM_HullPointContents);
    using t1318 = decltype(goldsrc::playermove_t::PM_HullPointContents);
    constexpr std::size_t o1325 = offsetof(goldsrc::playermove_t, PM_PlayerTrace);
    using t1319 = decltype(goldsrc::playermove_t::PM_PlayerTrace);
    constexpr std::size_t o1326 = offsetof(goldsrc::playermove_t, PM_TraceLine);
    using t1320 = decltype(goldsrc::playermove_t::PM_TraceLine);
    constexpr std::size_t o1327 = offsetof(goldsrc::playermove_t, RandomLong);
    using t1321 = decltype(goldsrc::playermove_t::RandomLong);
    constexpr std::size_t o1328 = offsetof(goldsrc::playermove_t, RandomFloat);
    using t1322 = decltype(goldsrc::playermove_t::RandomFloat);
    constexpr std::size_t o1329 = offsetof(goldsrc::playermove_t, PM_GetModelType);
    using t1323 = decltype(goldsrc::playermove_t::PM_GetModelType);
    constexpr std::size_t o1330 = offsetof(goldsrc::playermove_t, PM_GetModelBounds);
    using t1324 = decltype(goldsrc::playermove_t::PM_GetModelBounds);
    constexpr std::size_t o1331 = offsetof(goldsrc::playermove_t, PM_HullForBsp);
    using t1325 = decltype(goldsrc::playermove_t::PM_HullForBsp);
    constexpr std::size_t o1332 = offsetof(goldsrc::playermove_t, PM_TraceModel);
    using t1326 = decltype(goldsrc::playermove_t::PM_TraceModel);
    constexpr std::size_t o1333 = offsetof(goldsrc::playermove_t, COM_FileSize);
    using t1327 = decltype(goldsrc::playermove_t::COM_FileSize);
    constexpr std::size_t o1334 = offsetof(goldsrc::playermove_t, COM_LoadFile);
    using t1328 = decltype(goldsrc::playermove_t::COM_LoadFile);
    constexpr std::size_t o1335 = offsetof(goldsrc::playermove_t, COM_FreeFile);
    using t1329 = decltype(goldsrc::playermove_t::COM_FreeFile);
    constexpr std::size_t o1336 = offsetof(goldsrc::playermove_t, memfgets);
    using t1330 = decltype(goldsrc::playermove_t::memfgets);
    constexpr std::size_t o1337 = offsetof(goldsrc::playermove_t, runfuncs);
    using t1331 = decltype(goldsrc::playermove_t::runfuncs);
    constexpr std::size_t o1338 = offsetof(goldsrc::playermove_t, PM_PlaySound);
    using t1332 = decltype(goldsrc::playermove_t::PM_PlaySound);
    constexpr std::size_t o1339 = offsetof(goldsrc::playermove_t, PM_TraceTexture);
    using t1333 = decltype(goldsrc::playermove_t::PM_TraceTexture);
    constexpr std::size_t o1340 = offsetof(goldsrc::playermove_t, PM_PlaybackEventFull);
    using t1334 = decltype(goldsrc::playermove_t::PM_PlaybackEventFull);
    constexpr std::size_t o1341 = offsetof(goldsrc::playermove_t, PM_PlayerTraceEx);
    using t1335 = decltype(goldsrc::playermove_t::PM_PlayerTraceEx);
    constexpr std::size_t o1342 = offsetof(goldsrc::playermove_t, PM_TestPlayerPositionEx);
    using t1336 = decltype(goldsrc::playermove_t::PM_TestPlayerPositionEx);
    constexpr std::size_t o1343 = offsetof(goldsrc::playermove_t, PM_TraceLineEx);
    using t1337 = decltype(goldsrc::playermove_t::PM_TraceLineEx);
    constexpr std::size_t s100 = sizeof(goldsrc::enginefuncs_t);
    constexpr std::size_t o1344 = offsetof(goldsrc::enginefuncs_t, pfnPrecacheModel);
    using t1338 = decltype(goldsrc::enginefuncs_t::pfnPrecacheModel);
    constexpr std::size_t o1345 = offsetof(goldsrc::enginefuncs_t, pfnPrecacheSound);
    using t1339 = decltype(goldsrc::enginefuncs_t::pfnPrecacheSound);
    constexpr std::size_t o1346 = offsetof(goldsrc::enginefuncs_t, pfnSetModel);
    using t1340 = decltype(goldsrc::enginefuncs_t::pfnSetModel);
    constexpr std::size_t o1347 = offsetof(goldsrc::enginefuncs_t, pfnModelIndex);
    using t1341 = decltype(goldsrc::enginefuncs_t::pfnModelIndex);
    constexpr std::size_t o1348 = offsetof(goldsrc::enginefuncs_t, pfnModelFrames);
    using t1342 = decltype(goldsrc::enginefuncs_t::pfnModelFrames);
    constexpr std::size_t o1349 = offsetof(goldsrc::enginefuncs_t, pfnSetSize);
    using t1343 = decltype(goldsrc::enginefuncs_t::pfnSetSize);
    constexpr std::size_t o1350 = offsetof(goldsrc::enginefuncs_t, pfnChangeLevel);
    using t1344 = decltype(goldsrc::enginefuncs_t::pfnChangeLevel);
    constexpr std::size_t o1351 = offsetof(goldsrc::enginefuncs_t, pfnGetSpawnParms);
    using t1345 = decltype(goldsrc::enginefuncs_t::pfnGetSpawnParms);
    constexpr std::size_t o1352 = offsetof(goldsrc::enginefuncs_t, pfnSaveSpawnParms);
    using t1346 = decltype(goldsrc::enginefuncs_t::pfnSaveSpawnParms);
    constexpr std::size_t o1353 = offsetof(goldsrc::enginefuncs_t, pfnVecToYaw);
    using t1347 = decltype(goldsrc::enginefuncs_t::pfnVecToYaw);
    constexpr std::size_t o1354 = offsetof(goldsrc::enginefuncs_t, pfnVecToAngles);
    using t1348 = decltype(goldsrc::enginefuncs_t::pfnVecToAngles);
    constexpr std::size_t o1355 = offsetof(goldsrc::enginefuncs_t, pfnMoveToOrigin);
    using t1349 = decltype(goldsrc::enginefuncs_t::pfnMoveToOrigin);
    constexpr std::size_t o1356 = offsetof(goldsrc::enginefuncs_t, pfnChangeYaw);
    using t1350 = decltype(goldsrc::enginefuncs_t::pfnChangeYaw);
    constexpr std::size_t o1357 = offsetof(goldsrc::enginefuncs_t, pfnChangePitch);
    using t1351 = decltype(goldsrc::enginefuncs_t::pfnChangePitch);
    constexpr std::size_t o1358 = offsetof(goldsrc::enginefuncs_t, pfnFindEntityByString);
    using t1352 = decltype(goldsrc::enginefuncs_t::pfnFindEntityByString);
    constexpr std::size_t o1359 = offsetof(goldsrc::enginefuncs_t, pfnGetEntityIllum);
    using t1353 = decltype(goldsrc::enginefuncs_t::pfnGetEntityIllum);
    constexpr std::size_t o1360 = offsetof(goldsrc::enginefuncs_t, pfnFindEntityInSphere);
    using t1354 = decltype(goldsrc::enginefuncs_t::pfnFindEntityInSphere);
    constexpr std::size_t o1361 = offsetof(goldsrc::enginefuncs_t, pfnFindClientInPVS);
    using t1355 = decltype(goldsrc::enginefuncs_t::pfnFindClientInPVS);
    constexpr std::size_t o1362 = offsetof(goldsrc::enginefuncs_t, pfnEntitiesInPVS);
    using t1356 = decltype(goldsrc::enginefuncs_t::pfnEntitiesInPVS);
    constexpr std::size_t o1363 = offsetof(goldsrc::enginefuncs_t, pfnMakeVectors);
    using t1357 = decltype(goldsrc::enginefuncs_t::pfnMakeVectors);
    constexpr std::size_t o1364 = offsetof(goldsrc::enginefuncs_t, pfnAngleVectors);
    using t1358 = decltype(goldsrc::enginefuncs_t::pfnAngleVectors);
    constexpr std::size_t o1365 = offsetof(goldsrc::enginefuncs_t, pfnCreateEntity);
    using t1359 = decltype(goldsrc::enginefuncs_t::pfnCreateEntity);
    constexpr std::size_t o1366 = offsetof(goldsrc::enginefuncs_t, pfnRemoveEntity);
    using t1360 = decltype(goldsrc::enginefuncs_t::pfnRemoveEntity);
    constexpr std::size_t o1367 = offsetof(goldsrc::enginefuncs_t, pfnCreateNamedEntity);
    using t1361 = decltype(goldsrc::enginefuncs_t::pfnCreateNamedEntity);
    constexpr std::size_t o1368 = offsetof(goldsrc::enginefuncs_t, pfnMakeStatic);
    using t1362 = decltype(goldsrc::enginefuncs_t::pfnMakeStatic);
    constexpr std::size_t o1369 = offsetof(goldsrc::enginefuncs_t, pfnEntIsOnFloor);
    using t1363 = decltype(goldsrc::enginefuncs_t::pfnEntIsOnFloor);
    constexpr std::size_t o1370 = offsetof(goldsrc::enginefuncs_t, pfnDropToFloor);
    using t1364 = decltype(goldsrc::enginefuncs_t::pfnDropToFloor);
    constexpr std::size_t o1371 = offsetof(goldsrc::enginefuncs_t, pfnWalkMove);
    using t1365 = decltype(goldsrc::enginefuncs_t::pfnWalkMove);
    constexpr std::size_t o1372 = offsetof(goldsrc::enginefuncs_t, pfnSetOrigin);
    using t1366 = decltype(goldsrc::enginefuncs_t::pfnSetOrigin);
    constexpr std::size_t o1373 = offsetof(goldsrc::enginefuncs_t, pfnEmitSound);
    using t1367 = decltype(goldsrc::enginefuncs_t::pfnEmitSound);
    constexpr std::size_t o1374 = offsetof(goldsrc::enginefuncs_t, pfnEmitAmbientSound);
    using t1368 = decltype(goldsrc::enginefuncs_t::pfnEmitAmbientSound);
    constexpr std::size_t o1375 = offsetof(goldsrc::enginefuncs_t, pfnTraceLine);
    using t1369 = decltype(goldsrc::enginefuncs_t::pfnTraceLine);
    constexpr std::size_t o1376 = offsetof(goldsrc::enginefuncs_t, pfnTraceToss);
    using t1370 = decltype(goldsrc::enginefuncs_t::pfnTraceToss);
    constexpr std::size_t o1377 = offsetof(goldsrc::enginefuncs_t, pfnTraceMonsterHull);
    using t1371 = decltype(goldsrc::enginefuncs_t::pfnTraceMonsterHull);
    constexpr std::size_t o1378 = offsetof(goldsrc::enginefuncs_t, pfnTraceHull);
    using t1372 = decltype(goldsrc::enginefuncs_t::pfnTraceHull);
    constexpr std::size_t o1379 = offsetof(goldsrc::enginefuncs_t, pfnTraceModel);
    using t1373 = decltype(goldsrc::enginefuncs_t::pfnTraceModel);
    constexpr std::size_t o1380 = offsetof(goldsrc::enginefuncs_t, pfnTraceTexture);
    using t1374 = decltype(goldsrc::enginefuncs_t::pfnTraceTexture);
    constexpr std::size_t o1381 = offsetof(goldsrc::enginefuncs_t, pfnTraceSphere);
    using t1375 = decltype(goldsrc::enginefuncs_t::pfnTraceSphere);
    constexpr std::size_t o1382 = offsetof(goldsrc::enginefuncs_t, pfnGetAimVector);
    using t1376 = decltype(goldsrc::enginefuncs_t::pfnGetAimVector);
    constexpr std::size_t o1383 = offsetof(goldsrc::enginefuncs_t, pfnServerCommand);
    using t1377 = decltype(goldsrc::enginefuncs_t::pfnServerCommand);
    constexpr std::size_t o1384 = offsetof(goldsrc::enginefuncs_t, pfnServerExecute);
    using t1378 = decltype(goldsrc::enginefuncs_t::pfnServerExecute);
    constexpr std::size_t o1385 = offsetof(goldsrc::enginefuncs_t, pfnClientCommand);
    using t1379 = decltype(goldsrc::enginefuncs_t::pfnClientCommand);
    constexpr std::size_t o1386 = offsetof(goldsrc::enginefuncs_t, pfnParticleEffect);
    using t1380 = decltype(goldsrc::enginefuncs_t::pfnParticleEffect);
    constexpr std::size_t o1387 = offsetof(goldsrc::enginefuncs_t, pfnLightStyle);
    using t1381 = decltype(goldsrc::enginefuncs_t::pfnLightStyle);
    constexpr std::size_t o1388 = offsetof(goldsrc::enginefuncs_t, pfnDecalIndex);
    using t1382 = decltype(goldsrc::enginefuncs_t::pfnDecalIndex);
    constexpr std::size_t o1389 = offsetof(goldsrc::enginefuncs_t, pfnPointContents);
    using t1383 = decltype(goldsrc::enginefuncs_t::pfnPointContents);
    constexpr std::size_t o1390 = offsetof(goldsrc::enginefuncs_t, pfnMessageBegin);
    using t1384 = decltype(goldsrc::enginefuncs_t::pfnMessageBegin);
    constexpr std::size_t o1391 = offsetof(goldsrc::enginefuncs_t, pfnMessageEnd);
    using t1385 = decltype(goldsrc::enginefuncs_t::pfnMessageEnd);
    constexpr std::size_t o1392 = offsetof(goldsrc::enginefuncs_t, pfnWriteByte);
    using t1386 = decltype(goldsrc::enginefuncs_t::pfnWriteByte);
    constexpr std::size_t o1393 = offsetof(goldsrc::enginefuncs_t, pfnWriteChar);
    using t1387 = decltype(goldsrc::enginefuncs_t::pfnWriteChar);
    constexpr std::size_t o1394 = offsetof(goldsrc::enginefuncs_t, pfnWriteShort);
    using t1388 = decltype(goldsrc::enginefuncs_t::pfnWriteShort);
    constexpr std::size_t o1395 = offsetof(goldsrc::enginefuncs_t, pfnWriteLong);
    using t1389 = decltype(goldsrc::enginefuncs_t::pfnWriteLong);
    constexpr std::size_t o1396 = offsetof(goldsrc::enginefuncs_t, pfnWriteAngle);
    using t1390 = decltype(goldsrc::enginefuncs_t::pfnWriteAngle);
    constexpr std::size_t o1397 = offsetof(goldsrc::enginefuncs_t, pfnWriteCoord);
    using t1391 = decltype(goldsrc::enginefuncs_t::pfnWriteCoord);
    constexpr std::size_t o1398 = offsetof(goldsrc::enginefuncs_t, pfnWriteString);
    using t1392 = decltype(goldsrc::enginefuncs_t::pfnWriteString);
    constexpr std::size_t o1399 = offsetof(goldsrc::enginefuncs_t, pfnWriteEntity);
    using t1393 = decltype(goldsrc::enginefuncs_t::pfnWriteEntity);
    constexpr std::size_t o1400 = offsetof(goldsrc::enginefuncs_t, pfnCVarRegister);
    using t1394 = decltype(goldsrc::enginefuncs_t::pfnCVarRegister);
    constexpr std::size_t o1401 = offsetof(goldsrc::enginefuncs_t, pfnCVarGetFloat);
    using t1395 = decltype(goldsrc::enginefuncs_t::pfnCVarGetFloat);
    constexpr std::size_t o1402 = offsetof(goldsrc::enginefuncs_t, pfnCVarGetString);
    using t1396 = decltype(goldsrc::enginefuncs_t::pfnCVarGetString);
    constexpr std::size_t o1403 = offsetof(goldsrc::enginefuncs_t, pfnCVarSetFloat);
    using t1397 = decltype(goldsrc::enginefuncs_t::pfnCVarSetFloat);
    constexpr std::size_t o1404 = offsetof(goldsrc::enginefuncs_t, pfnCVarSetString);
    using t1398 = decltype(goldsrc::enginefuncs_t::pfnCVarSetString);
    constexpr std::size_t o1405 = offsetof(goldsrc::enginefuncs_t, pfnAlertMessage);
    using t1399 = decltype(goldsrc::enginefuncs_t::pfnAlertMessage);
    constexpr std::size_t o1406 = offsetof(goldsrc::enginefuncs_t, pfnEngineFprintf);
    using t1400 = decltype(goldsrc::enginefuncs_t::pfnEngineFprintf);
    constexpr std::size_t o1407 = offsetof(goldsrc::enginefuncs_t, pfnPvAllocEntPrivateData);
    using t1401 = decltype(goldsrc::enginefuncs_t::pfnPvAllocEntPrivateData);
    constexpr std::size_t o1408 = offsetof(goldsrc::enginefuncs_t, pfnPvEntPrivateData);
    using t1402 = decltype(goldsrc::enginefuncs_t::pfnPvEntPrivateData);
    constexpr std::size_t o1409 = offsetof(goldsrc::enginefuncs_t, pfnFreeEntPrivateData);
    using t1403 = decltype(goldsrc::enginefuncs_t::pfnFreeEntPrivateData);
    constexpr std::size_t o1410 = offsetof(goldsrc::enginefuncs_t, pfnSzFromIndex);
    using t1404 = decltype(goldsrc::enginefuncs_t::pfnSzFromIndex);
    constexpr std::size_t o1411 = offsetof(goldsrc::enginefuncs_t, pfnAllocString);
    using t1405 = decltype(goldsrc::enginefuncs_t::pfnAllocString);
    constexpr std::size_t o1412 = offsetof(goldsrc::enginefuncs_t, pfnGetVarsOfEnt);
    using t1406 = decltype(goldsrc::enginefuncs_t::pfnGetVarsOfEnt);
    constexpr std::size_t o1413 = offsetof(goldsrc::enginefuncs_t, pfnPEntityOfEntOffset);
    using t1407 = decltype(goldsrc::enginefuncs_t::pfnPEntityOfEntOffset);
    constexpr std::size_t o1414 = offsetof(goldsrc::enginefuncs_t, pfnEntOffsetOfPEntity);
    using t1408 = decltype(goldsrc::enginefuncs_t::pfnEntOffsetOfPEntity);
    constexpr std::size_t o1415 = offsetof(goldsrc::enginefuncs_t, pfnIndexOfEdict);
    using t1409 = decltype(goldsrc::enginefuncs_t::pfnIndexOfEdict);
    constexpr std::size_t o1416 = offsetof(goldsrc::enginefuncs_t, pfnPEntityOfEntIndex);
    using t1410 = decltype(goldsrc::enginefuncs_t::pfnPEntityOfEntIndex);
    constexpr std::size_t o1417 = offsetof(goldsrc::enginefuncs_t, pfnFindEntityByVars);
    using t1411 = decltype(goldsrc::enginefuncs_t::pfnFindEntityByVars);
    constexpr std::size_t o1418 = offsetof(goldsrc::enginefuncs_t, pfnGetModelPtr);
    using t1412 = decltype(goldsrc::enginefuncs_t::pfnGetModelPtr);
    constexpr std::size_t o1419 = offsetof(goldsrc::enginefuncs_t, pfnRegUserMsg);
    using t1413 = decltype(goldsrc::enginefuncs_t::pfnRegUserMsg);
    constexpr std::size_t o1420 = offsetof(goldsrc::enginefuncs_t, pfnAnimationAutomove);
    using t1414 = decltype(goldsrc::enginefuncs_t::pfnAnimationAutomove);
    constexpr std::size_t o1421 = offsetof(goldsrc::enginefuncs_t, pfnGetBonePosition);
    using t1415 = decltype(goldsrc::enginefuncs_t::pfnGetBonePosition);
    constexpr std::size_t o1422 = offsetof(goldsrc::enginefuncs_t, pfnFunctionFromName);
    using t1416 = decltype(goldsrc::enginefuncs_t::pfnFunctionFromName);
    constexpr std::size_t o1423 = offsetof(goldsrc::enginefuncs_t, pfnNameForFunction);
    using t1417 = decltype(goldsrc::enginefuncs_t::pfnNameForFunction);
    constexpr std::size_t o1424 = offsetof(goldsrc::enginefuncs_t, pfnClientPrintf);
    using t1418 = decltype(goldsrc::enginefuncs_t::pfnClientPrintf);
    constexpr std::size_t o1425 = offsetof(goldsrc::enginefuncs_t, pfnServerPrint);
    using t1419 = decltype(goldsrc::enginefuncs_t::pfnServerPrint);
    constexpr std::size_t o1426 = offsetof(goldsrc::enginefuncs_t, pfnCmd_Args);
    using t1420 = decltype(goldsrc::enginefuncs_t::pfnCmd_Args);
    constexpr std::size_t o1427 = offsetof(goldsrc::enginefuncs_t, pfnCmd_Argv);
    using t1421 = decltype(goldsrc::enginefuncs_t::pfnCmd_Argv);
    constexpr std::size_t o1428 = offsetof(goldsrc::enginefuncs_t, pfnCmd_Argc);
    using t1422 = decltype(goldsrc::enginefuncs_t::pfnCmd_Argc);
    constexpr std::size_t o1429 = offsetof(goldsrc::enginefuncs_t, pfnGetAttachment);
    using t1423 = decltype(goldsrc::enginefuncs_t::pfnGetAttachment);
    constexpr std::size_t o1430 = offsetof(goldsrc::enginefuncs_t, pfnCRC32_Init);
    using t1424 = decltype(goldsrc::enginefuncs_t::pfnCRC32_Init);
    constexpr std::size_t o1431 = offsetof(goldsrc::enginefuncs_t, pfnCRC32_ProcessBuffer);
    using t1425 = decltype(goldsrc::enginefuncs_t::pfnCRC32_ProcessBuffer);
    constexpr std::size_t o1432 = offsetof(goldsrc::enginefuncs_t, pfnCRC32_ProcessByte);
    using t1426 = decltype(goldsrc::enginefuncs_t::pfnCRC32_ProcessByte);
    constexpr std::size_t o1433 = offsetof(goldsrc::enginefuncs_t, pfnCRC32_Final);
    using t1427 = decltype(goldsrc::enginefuncs_t::pfnCRC32_Final);
    constexpr std::size_t o1434 = offsetof(goldsrc::enginefuncs_t, pfnRandomLong);
    using t1428 = decltype(goldsrc::enginefuncs_t::pfnRandomLong);
    constexpr std::size_t o1435 = offsetof(goldsrc::enginefuncs_t, pfnRandomFloat);
    using t1429 = decltype(goldsrc::enginefuncs_t::pfnRandomFloat);
    constexpr std::size_t o1436 = offsetof(goldsrc::enginefuncs_t, pfnSetView);
    using t1430 = decltype(goldsrc::enginefuncs_t::pfnSetView);
    constexpr std::size_t o1437 = offsetof(goldsrc::enginefuncs_t, pfnTime);
    using t1431 = decltype(goldsrc::enginefuncs_t::pfnTime);
    constexpr std::size_t o1438 = offsetof(goldsrc::enginefuncs_t, pfnCrosshairAngle);
    using t1432 = decltype(goldsrc::enginefuncs_t::pfnCrosshairAngle);
    constexpr std::size_t o1439 = offsetof(goldsrc::enginefuncs_t, pfnLoadFileForMe);
    using t1433 = decltype(goldsrc::enginefuncs_t::pfnLoadFileForMe);
    constexpr std::size_t o1440 = offsetof(goldsrc::enginefuncs_t, pfnFreeFile);
    using t1434 = decltype(goldsrc::enginefuncs_t::pfnFreeFile);
    constexpr std::size_t o1441 = offsetof(goldsrc::enginefuncs_t, pfnEndSection);
    using t1435 = decltype(goldsrc::enginefuncs_t::pfnEndSection);
    constexpr std::size_t o1442 = offsetof(goldsrc::enginefuncs_t, pfnCompareFileTime);
    using t1436 = decltype(goldsrc::enginefuncs_t::pfnCompareFileTime);
    constexpr std::size_t o1443 = offsetof(goldsrc::enginefuncs_t, pfnGetGameDir);
    using t1437 = decltype(goldsrc::enginefuncs_t::pfnGetGameDir);
    constexpr std::size_t o1444 = offsetof(goldsrc::enginefuncs_t, pfnCvar_RegisterVariable);
    using t1438 = decltype(goldsrc::enginefuncs_t::pfnCvar_RegisterVariable);
    constexpr std::size_t o1445 = offsetof(goldsrc::enginefuncs_t, pfnFadeClientVolume);
    using t1439 = decltype(goldsrc::enginefuncs_t::pfnFadeClientVolume);
    constexpr std::size_t o1446 = offsetof(goldsrc::enginefuncs_t, pfnSetClientMaxspeed);
    using t1440 = decltype(goldsrc::enginefuncs_t::pfnSetClientMaxspeed);
    constexpr std::size_t o1447 = offsetof(goldsrc::enginefuncs_t, pfnCreateFakeClient);
    using t1441 = decltype(goldsrc::enginefuncs_t::pfnCreateFakeClient);
    constexpr std::size_t o1448 = offsetof(goldsrc::enginefuncs_t, pfnRunPlayerMove);
    using t1442 = decltype(goldsrc::enginefuncs_t::pfnRunPlayerMove);
    constexpr std::size_t o1449 = offsetof(goldsrc::enginefuncs_t, pfnNumberOfEntities);
    using t1443 = decltype(goldsrc::enginefuncs_t::pfnNumberOfEntities);
    constexpr std::size_t o1450 = offsetof(goldsrc::enginefuncs_t, pfnGetInfoKeyBuffer);
    using t1444 = decltype(goldsrc::enginefuncs_t::pfnGetInfoKeyBuffer);
    constexpr std::size_t o1451 = offsetof(goldsrc::enginefuncs_t, pfnInfoKeyValue);
    using t1445 = decltype(goldsrc::enginefuncs_t::pfnInfoKeyValue);
    constexpr std::size_t o1452 = offsetof(goldsrc::enginefuncs_t, pfnSetKeyValue);
    using t1446 = decltype(goldsrc::enginefuncs_t::pfnSetKeyValue);
    constexpr std::size_t o1453 = offsetof(goldsrc::enginefuncs_t, pfnSetClientKeyValue);
    using t1447 = decltype(goldsrc::enginefuncs_t::pfnSetClientKeyValue);
    constexpr std::size_t o1454 = offsetof(goldsrc::enginefuncs_t, pfnIsMapValid);
    using t1448 = decltype(goldsrc::enginefuncs_t::pfnIsMapValid);
    constexpr std::size_t o1455 = offsetof(goldsrc::enginefuncs_t, pfnStaticDecal);
    using t1449 = decltype(goldsrc::enginefuncs_t::pfnStaticDecal);
    constexpr std::size_t o1456 = offsetof(goldsrc::enginefuncs_t, pfnPrecacheGeneric);
    using t1450 = decltype(goldsrc::enginefuncs_t::pfnPrecacheGeneric);
    constexpr std::size_t o1457 = offsetof(goldsrc::enginefuncs_t, pfnGetPlayerUserId);
    using t1451 = decltype(goldsrc::enginefuncs_t::pfnGetPlayerUserId);
    constexpr std::size_t o1458 = offsetof(goldsrc::enginefuncs_t, pfnBuildSoundMsg);
    using t1452 = decltype(goldsrc::enginefuncs_t::pfnBuildSoundMsg);
    constexpr std::size_t o1459 = offsetof(goldsrc::enginefuncs_t, pfnIsDedicatedServer);
    using t1453 = decltype(goldsrc::enginefuncs_t::pfnIsDedicatedServer);
    constexpr std::size_t o1460 = offsetof(goldsrc::enginefuncs_t, pfnCVarGetPointer);
    using t1454 = decltype(goldsrc::enginefuncs_t::pfnCVarGetPointer);
    constexpr std::size_t o1461 = offsetof(goldsrc::enginefuncs_t, pfnGetPlayerWONId);
    using t1455 = decltype(goldsrc::enginefuncs_t::pfnGetPlayerWONId);
    constexpr std::size_t o1462 = offsetof(goldsrc::enginefuncs_t, pfnInfo_RemoveKey);
    using t1456 = decltype(goldsrc::enginefuncs_t::pfnInfo_RemoveKey);
    constexpr std::size_t o1463 = offsetof(goldsrc::enginefuncs_t, pfnGetPhysicsKeyValue);
    using t1457 = decltype(goldsrc::enginefuncs_t::pfnGetPhysicsKeyValue);
    constexpr std::size_t o1464 = offsetof(goldsrc::enginefuncs_t, pfnSetPhysicsKeyValue);
    using t1458 = decltype(goldsrc::enginefuncs_t::pfnSetPhysicsKeyValue);
    constexpr std::size_t o1465 = offsetof(goldsrc::enginefuncs_t, pfnGetPhysicsInfoString);
    using t1459 = decltype(goldsrc::enginefuncs_t::pfnGetPhysicsInfoString);
    constexpr std::size_t o1466 = offsetof(goldsrc::enginefuncs_t, pfnPrecacheEvent);
    using t1460 = decltype(goldsrc::enginefuncs_t::pfnPrecacheEvent);
    constexpr std::size_t o1467 = offsetof(goldsrc::enginefuncs_t, pfnPlaybackEvent);
    using t1461 = decltype(goldsrc::enginefuncs_t::pfnPlaybackEvent);
    constexpr std::size_t o1468 = offsetof(goldsrc::enginefuncs_t, pfnSetFatPVS);
    using t1462 = decltype(goldsrc::enginefuncs_t::pfnSetFatPVS);
    constexpr std::size_t o1469 = offsetof(goldsrc::enginefuncs_t, pfnSetFatPAS);
    using t1463 = decltype(goldsrc::enginefuncs_t::pfnSetFatPAS);
    constexpr std::size_t o1470 = offsetof(goldsrc::enginefuncs_t, pfnCheckVisibility);
    using t1464 = decltype(goldsrc::enginefuncs_t::pfnCheckVisibility);
    constexpr std::size_t o1471 = offsetof(goldsrc::enginefuncs_t, pfnDeltaSetField);
    using t1465 = decltype(goldsrc::enginefuncs_t::pfnDeltaSetField);
    constexpr std::size_t o1472 = offsetof(goldsrc::enginefuncs_t, pfnDeltaUnsetField);
    using t1466 = decltype(goldsrc::enginefuncs_t::pfnDeltaUnsetField);
    constexpr std::size_t o1473 = offsetof(goldsrc::enginefuncs_t, pfnDeltaAddEncoder);
    using t1467 = decltype(goldsrc::enginefuncs_t::pfnDeltaAddEncoder);
    constexpr std::size_t o1474 = offsetof(goldsrc::enginefuncs_t, pfnGetCurrentPlayer);
    using t1468 = decltype(goldsrc::enginefuncs_t::pfnGetCurrentPlayer);
    constexpr std::size_t o1475 = offsetof(goldsrc::enginefuncs_t, pfnCanSkipPlayer);
    using t1469 = decltype(goldsrc::enginefuncs_t::pfnCanSkipPlayer);
    constexpr std::size_t o1476 = offsetof(goldsrc::enginefuncs_t, pfnDeltaFindField);
    using t1470 = decltype(goldsrc::enginefuncs_t::pfnDeltaFindField);
    constexpr std::size_t o1477 = offsetof(goldsrc::enginefuncs_t, pfnDeltaSetFieldByIndex);
    using t1471 = decltype(goldsrc::enginefuncs_t::pfnDeltaSetFieldByIndex);
    constexpr std::size_t o1478 = offsetof(goldsrc::enginefuncs_t, pfnDeltaUnsetFieldByIndex);
    using t1472 = decltype(goldsrc::enginefuncs_t::pfnDeltaUnsetFieldByIndex);
    constexpr std::size_t o1479 = offsetof(goldsrc::enginefuncs_t, pfnSetGroupMask);
    using t1473 = decltype(goldsrc::enginefuncs_t::pfnSetGroupMask);
    constexpr std::size_t o1480 = offsetof(goldsrc::enginefuncs_t, pfnCreateInstancedBaseline);
    using t1474 = decltype(goldsrc::enginefuncs_t::pfnCreateInstancedBaseline);
    constexpr std::size_t o1481 = offsetof(goldsrc::enginefuncs_t, pfnCvar_DirectSet);
    using t1475 = decltype(goldsrc::enginefuncs_t::pfnCvar_DirectSet);
    constexpr std::size_t o1482 = offsetof(goldsrc::enginefuncs_t, pfnForceUnmodified);
    using t1476 = decltype(goldsrc::enginefuncs_t::pfnForceUnmodified);
    constexpr std::size_t o1483 = offsetof(goldsrc::enginefuncs_t, pfnGetPlayerStats);
    using t1477 = decltype(goldsrc::enginefuncs_t::pfnGetPlayerStats);
    constexpr std::size_t o1484 = offsetof(goldsrc::enginefuncs_t, pfnAddServerCommand);
    using t1478 = decltype(goldsrc::enginefuncs_t::pfnAddServerCommand);
    constexpr std::size_t o1485 = offsetof(goldsrc::enginefuncs_t, pfnVoice_GetClientListening);
    using t1479 = decltype(goldsrc::enginefuncs_t::pfnVoice_GetClientListening);
    constexpr std::size_t o1486 = offsetof(goldsrc::enginefuncs_t, pfnVoice_SetClientListening);
    using t1480 = decltype(goldsrc::enginefuncs_t::pfnVoice_SetClientListening);
    constexpr std::size_t o1487 = offsetof(goldsrc::enginefuncs_t, pfnGetPlayerAuthId);
    using t1481 = decltype(goldsrc::enginefuncs_t::pfnGetPlayerAuthId);
    constexpr std::size_t o1488 = offsetof(goldsrc::enginefuncs_t, pfnSequenceGet);
    using t1482 = decltype(goldsrc::enginefuncs_t::pfnSequenceGet);
    constexpr std::size_t o1489 = offsetof(goldsrc::enginefuncs_t, pfnSequencePickSentence);
    using t1483 = decltype(goldsrc::enginefuncs_t::pfnSequencePickSentence);
    constexpr std::size_t o1490 = offsetof(goldsrc::enginefuncs_t, pfnGetFileSize);
    using t1484 = decltype(goldsrc::enginefuncs_t::pfnGetFileSize);
    constexpr std::size_t o1491 = offsetof(goldsrc::enginefuncs_t, pfnGetApproxWavePlayLen);
    using t1485 = decltype(goldsrc::enginefuncs_t::pfnGetApproxWavePlayLen);
    constexpr std::size_t o1492 = offsetof(goldsrc::enginefuncs_t, pfnIsCareerMatch);
    using t1486 = decltype(goldsrc::enginefuncs_t::pfnIsCareerMatch);
    constexpr std::size_t o1493 = offsetof(goldsrc::enginefuncs_t, pfnGetLocalizedStringLength);
    using t1487 = decltype(goldsrc::enginefuncs_t::pfnGetLocalizedStringLength);
    constexpr std::size_t o1494 = offsetof(goldsrc::enginefuncs_t, pfnRegisterTutorMessageShown);
    using t1488 = decltype(goldsrc::enginefuncs_t::pfnRegisterTutorMessageShown);
    constexpr std::size_t o1495 = offsetof(goldsrc::enginefuncs_t, pfnGetTimesTutorMessageShown);
    using t1489 = decltype(goldsrc::enginefuncs_t::pfnGetTimesTutorMessageShown);
    constexpr std::size_t o1496 = offsetof(goldsrc::enginefuncs_t, pfnProcessTutorMessageDecayBuffer);
    using t1490 = decltype(goldsrc::enginefuncs_t::pfnProcessTutorMessageDecayBuffer);
    constexpr std::size_t o1497 = offsetof(goldsrc::enginefuncs_t, pfnConstructTutorMessageDecayBuffer);
    using t1491 = decltype(goldsrc::enginefuncs_t::pfnConstructTutorMessageDecayBuffer);
    constexpr std::size_t o1498 = offsetof(goldsrc::enginefuncs_t, pfnResetTutorMessageDecayData);
    using t1492 = decltype(goldsrc::enginefuncs_t::pfnResetTutorMessageDecayData);
    constexpr std::size_t o1499 = offsetof(goldsrc::enginefuncs_t, pfnQueryClientCvarValue);
    using t1493 = decltype(goldsrc::enginefuncs_t::pfnQueryClientCvarValue);
    constexpr std::size_t o1500 = offsetof(goldsrc::enginefuncs_t, pfnQueryClientCvarValue2);
    using t1494 = decltype(goldsrc::enginefuncs_t::pfnQueryClientCvarValue2);
    constexpr std::size_t o1501 = offsetof(goldsrc::enginefuncs_t, pfnEngCheckParm);
    using t1495 = decltype(goldsrc::enginefuncs_t::pfnEngCheckParm);
    constexpr std::size_t s101 = sizeof(goldsrc::engine_studio_api_t);
    constexpr std::size_t o1502 = offsetof(goldsrc::engine_studio_api_t, Mem_Calloc);
    using t1496 = decltype(goldsrc::engine_studio_api_t::Mem_Calloc);
    constexpr std::size_t o1503 = offsetof(goldsrc::engine_studio_api_t, Cache_Check);
    using t1497 = decltype(goldsrc::engine_studio_api_t::Cache_Check);
    constexpr std::size_t o1504 = offsetof(goldsrc::engine_studio_api_t, LoadCacheFile);
    using t1498 = decltype(goldsrc::engine_studio_api_t::LoadCacheFile);
    constexpr std::size_t o1505 = offsetof(goldsrc::engine_studio_api_t, Mod_ForName);
    using t1499 = decltype(goldsrc::engine_studio_api_t::Mod_ForName);
    constexpr std::size_t o1506 = offsetof(goldsrc::engine_studio_api_t, Mod_Extradata);
    using t1500 = decltype(goldsrc::engine_studio_api_t::Mod_Extradata);
    constexpr std::size_t o1507 = offsetof(goldsrc::engine_studio_api_t, GetModelByIndex);
    using t1501 = decltype(goldsrc::engine_studio_api_t::GetModelByIndex);
    constexpr std::size_t o1508 = offsetof(goldsrc::engine_studio_api_t, GetCurrentEntity);
    using t1502 = decltype(goldsrc::engine_studio_api_t::GetCurrentEntity);
    constexpr std::size_t o1509 = offsetof(goldsrc::engine_studio_api_t, PlayerInfo);
    using t1503 = decltype(goldsrc::engine_studio_api_t::PlayerInfo);
    constexpr std::size_t o1510 = offsetof(goldsrc::engine_studio_api_t, GetPlayerState);
    using t1504 = decltype(goldsrc::engine_studio_api_t::GetPlayerState);
    constexpr std::size_t o1511 = offsetof(goldsrc::engine_studio_api_t, GetViewEntity);
    using t1505 = decltype(goldsrc::engine_studio_api_t::GetViewEntity);
    constexpr std::size_t o1512 = offsetof(goldsrc::engine_studio_api_t, GetTimes);
    using t1506 = decltype(goldsrc::engine_studio_api_t::GetTimes);
    constexpr std::size_t o1513 = offsetof(goldsrc::engine_studio_api_t, GetCvar);
    using t1507 = decltype(goldsrc::engine_studio_api_t::GetCvar);
    constexpr std::size_t o1514 = offsetof(goldsrc::engine_studio_api_t, GetViewInfo);
    using t1508 = decltype(goldsrc::engine_studio_api_t::GetViewInfo);
    constexpr std::size_t o1515 = offsetof(goldsrc::engine_studio_api_t, GetChromeSprite);
    using t1509 = decltype(goldsrc::engine_studio_api_t::GetChromeSprite);
    constexpr std::size_t o1516 = offsetof(goldsrc::engine_studio_api_t, GetModelCounters);
    using t1510 = decltype(goldsrc::engine_studio_api_t::GetModelCounters);
    constexpr std::size_t o1517 = offsetof(goldsrc::engine_studio_api_t, GetAliasScale);
    using t1511 = decltype(goldsrc::engine_studio_api_t::GetAliasScale);
    constexpr std::size_t o1518 = offsetof(goldsrc::engine_studio_api_t, StudioGetBoneTransform);
    using t1512 = decltype(goldsrc::engine_studio_api_t::StudioGetBoneTransform);
    constexpr std::size_t o1519 = offsetof(goldsrc::engine_studio_api_t, StudioGetLightTransform);
    using t1513 = decltype(goldsrc::engine_studio_api_t::StudioGetLightTransform);
    constexpr std::size_t o1520 = offsetof(goldsrc::engine_studio_api_t, StudioGetAliasTransform);
    using t1514 = decltype(goldsrc::engine_studio_api_t::StudioGetAliasTransform);
    constexpr std::size_t o1521 = offsetof(goldsrc::engine_studio_api_t, StudioGetRotationMatrix);
    using t1515 = decltype(goldsrc::engine_studio_api_t::StudioGetRotationMatrix);
    constexpr std::size_t o1522 = offsetof(goldsrc::engine_studio_api_t, StudioSetupModel);
    using t1516 = decltype(goldsrc::engine_studio_api_t::StudioSetupModel);
    constexpr std::size_t o1523 = offsetof(goldsrc::engine_studio_api_t, StudioCheckBBox);
    using t1517 = decltype(goldsrc::engine_studio_api_t::StudioCheckBBox);
    constexpr std::size_t o1524 = offsetof(goldsrc::engine_studio_api_t, StudioDynamicLight);
    using t1518 = decltype(goldsrc::engine_studio_api_t::StudioDynamicLight);
    constexpr std::size_t o1525 = offsetof(goldsrc::engine_studio_api_t, StudioEntityLight);
    using t1519 = decltype(goldsrc::engine_studio_api_t::StudioEntityLight);
    constexpr std::size_t o1526 = offsetof(goldsrc::engine_studio_api_t, StudioSetupLighting);
    using t1520 = decltype(goldsrc::engine_studio_api_t::StudioSetupLighting);
    constexpr std::size_t o1527 = offsetof(goldsrc::engine_studio_api_t, StudioDrawPoints);
    using t1521 = decltype(goldsrc::engine_studio_api_t::StudioDrawPoints);
    constexpr std::size_t o1528 = offsetof(goldsrc::engine_studio_api_t, StudioDrawHulls);
    using t1522 = decltype(goldsrc::engine_studio_api_t::StudioDrawHulls);
    constexpr std::size_t o1529 = offsetof(goldsrc::engine_studio_api_t, StudioDrawAbsBBox);
    using t1523 = decltype(goldsrc::engine_studio_api_t::StudioDrawAbsBBox);
    constexpr std::size_t o1530 = offsetof(goldsrc::engine_studio_api_t, StudioDrawBones);
    using t1524 = decltype(goldsrc::engine_studio_api_t::StudioDrawBones);
    constexpr std::size_t o1531 = offsetof(goldsrc::engine_studio_api_t, StudioSetupSkin);
    using t1525 = decltype(goldsrc::engine_studio_api_t::StudioSetupSkin);
    constexpr std::size_t o1532 = offsetof(goldsrc::engine_studio_api_t, StudioSetRemapColors);
    using t1526 = decltype(goldsrc::engine_studio_api_t::StudioSetRemapColors);
    constexpr std::size_t o1533 = offsetof(goldsrc::engine_studio_api_t, SetupPlayerModel);
    using t1527 = decltype(goldsrc::engine_studio_api_t::SetupPlayerModel);
    constexpr std::size_t o1534 = offsetof(goldsrc::engine_studio_api_t, StudioClientEvents);
    using t1528 = decltype(goldsrc::engine_studio_api_t::StudioClientEvents);
    constexpr std::size_t o1535 = offsetof(goldsrc::engine_studio_api_t, GetForceFaceFlags);
    using t1529 = decltype(goldsrc::engine_studio_api_t::GetForceFaceFlags);
    constexpr std::size_t o1536 = offsetof(goldsrc::engine_studio_api_t, SetForceFaceFlags);
    using t1530 = decltype(goldsrc::engine_studio_api_t::SetForceFaceFlags);
    constexpr std::size_t o1537 = offsetof(goldsrc::engine_studio_api_t, StudioSetHeader);
    using t1531 = decltype(goldsrc::engine_studio_api_t::StudioSetHeader);
    constexpr std::size_t o1538 = offsetof(goldsrc::engine_studio_api_t, SetRenderModel);
    using t1532 = decltype(goldsrc::engine_studio_api_t::SetRenderModel);
    constexpr std::size_t o1539 = offsetof(goldsrc::engine_studio_api_t, SetupRenderer);
    using t1533 = decltype(goldsrc::engine_studio_api_t::SetupRenderer);
    constexpr std::size_t o1540 = offsetof(goldsrc::engine_studio_api_t, RestoreRenderer);
    using t1534 = decltype(goldsrc::engine_studio_api_t::RestoreRenderer);
    constexpr std::size_t o1541 = offsetof(goldsrc::engine_studio_api_t, SetChromeOrigin);
    using t1535 = decltype(goldsrc::engine_studio_api_t::SetChromeOrigin);
    constexpr std::size_t o1542 = offsetof(goldsrc::engine_studio_api_t, IsHardware);
    using t1536 = decltype(goldsrc::engine_studio_api_t::IsHardware);
    constexpr std::size_t o1543 = offsetof(goldsrc::engine_studio_api_t, GL_StudioDrawShadow);
    using t1537 = decltype(goldsrc::engine_studio_api_t::GL_StudioDrawShadow);
    constexpr std::size_t o1544 = offsetof(goldsrc::engine_studio_api_t, GL_SetRenderMode);
    using t1538 = decltype(goldsrc::engine_studio_api_t::GL_SetRenderMode);
    constexpr std::size_t o1545 = offsetof(goldsrc::engine_studio_api_t, StudioSetRenderamt);
    using t1539 = decltype(goldsrc::engine_studio_api_t::StudioSetRenderamt);
    constexpr std::size_t o1546 = offsetof(goldsrc::engine_studio_api_t, StudioSetCullState);
    using t1540 = decltype(goldsrc::engine_studio_api_t::StudioSetCullState);
    constexpr std::size_t o1547 = offsetof(goldsrc::engine_studio_api_t, StudioRenderShadow);
    using t1541 = decltype(goldsrc::engine_studio_api_t::StudioRenderShadow);
    constexpr std::size_t s102 = sizeof(goldsrc::r_studio_interface_t);
    constexpr std::size_t o1548 = offsetof(goldsrc::r_studio_interface_t, version);
    using t1542 = decltype(goldsrc::r_studio_interface_t::version);
    constexpr std::size_t o1549 = offsetof(goldsrc::r_studio_interface_t, StudioDrawModel);
    using t1543 = decltype(goldsrc::r_studio_interface_t::StudioDrawModel);
    constexpr std::size_t o1550 = offsetof(goldsrc::r_studio_interface_t, StudioDrawPlayer);
    using t1544 = decltype(goldsrc::r_studio_interface_t::StudioDrawPlayer);
    constexpr std::size_t s103 = sizeof(goldsrc::triangleapi_t);
    constexpr std::size_t o1551 = offsetof(goldsrc::triangleapi_t, version);
    using t1545 = decltype(goldsrc::triangleapi_t::version);
    constexpr std::size_t o1552 = offsetof(goldsrc::triangleapi_t, RenderMode);
    using t1546 = decltype(goldsrc::triangleapi_t::RenderMode);
    constexpr std::size_t o1553 = offsetof(goldsrc::triangleapi_t, Begin);
    using t1547 = decltype(goldsrc::triangleapi_t::Begin);
    constexpr std::size_t o1554 = offsetof(goldsrc::triangleapi_t, End);
    using t1548 = decltype(goldsrc::triangleapi_t::End);
    constexpr std::size_t o1555 = offsetof(goldsrc::triangleapi_t, Color4f);
    using t1549 = decltype(goldsrc::triangleapi_t::Color4f);
    constexpr std::size_t o1556 = offsetof(goldsrc::triangleapi_t, Color4ub);
    using t1550 = decltype(goldsrc::triangleapi_t::Color4ub);
    constexpr std::size_t o1557 = offsetof(goldsrc::triangleapi_t, TexCoord2f);
    using t1551 = decltype(goldsrc::triangleapi_t::TexCoord2f);
    constexpr std::size_t o1558 = offsetof(goldsrc::triangleapi_t, Vertex3fv);
    using t1552 = decltype(goldsrc::triangleapi_t::Vertex3fv);
    constexpr std::size_t o1559 = offsetof(goldsrc::triangleapi_t, Vertex3f);
    using t1553 = decltype(goldsrc::triangleapi_t::Vertex3f);
    constexpr std::size_t o1560 = offsetof(goldsrc::triangleapi_t, Brightness);
    using t1554 = decltype(goldsrc::triangleapi_t::Brightness);
    constexpr std::size_t o1561 = offsetof(goldsrc::triangleapi_t, CullFace);
    using t1555 = decltype(goldsrc::triangleapi_t::CullFace);
    constexpr std::size_t o1562 = offsetof(goldsrc::triangleapi_t, SpriteTexture);
    using t1556 = decltype(goldsrc::triangleapi_t::SpriteTexture);
    constexpr std::size_t o1563 = offsetof(goldsrc::triangleapi_t, WorldToScreen);
    using t1557 = decltype(goldsrc::triangleapi_t::WorldToScreen);
    constexpr std::size_t o1564 = offsetof(goldsrc::triangleapi_t, Fog);
    using t1558 = decltype(goldsrc::triangleapi_t::Fog);
    constexpr std::size_t o1565 = offsetof(goldsrc::triangleapi_t, ScreenToWorld);
    using t1559 = decltype(goldsrc::triangleapi_t::ScreenToWorld);
    constexpr std::size_t o1566 = offsetof(goldsrc::triangleapi_t, GetMatrix);
    using t1560 = decltype(goldsrc::triangleapi_t::GetMatrix);
    constexpr std::size_t o1567 = offsetof(goldsrc::triangleapi_t, BoxInPVS);
    using t1561 = decltype(goldsrc::triangleapi_t::BoxInPVS);
    constexpr std::size_t o1568 = offsetof(goldsrc::triangleapi_t, LightAtPoint);
    using t1562 = decltype(goldsrc::triangleapi_t::LightAtPoint);
    constexpr std::size_t o1569 = offsetof(goldsrc::triangleapi_t, Color4fRendermode);
    using t1563 = decltype(goldsrc::triangleapi_t::Color4fRendermode);
    constexpr std::size_t o1570 = offsetof(goldsrc::triangleapi_t, FogParams);
    using t1564 = decltype(goldsrc::triangleapi_t::FogParams);
    constexpr std::size_t s104 = sizeof(goldsrc::cache_user_t);
    constexpr std::size_t o1571 = offsetof(goldsrc::cache_user_t, data);
    using t1565 = decltype(goldsrc::cache_user_t::data);
    constexpr std::size_t s105 = sizeof(goldsrc::color24);
    constexpr std::size_t o1572 = offsetof(goldsrc::color24, r);
    using t1566 = decltype(goldsrc::color24::r);
    constexpr std::size_t o1573 = offsetof(goldsrc::color24, g);
    using t1567 = decltype(goldsrc::color24::g);
    constexpr std::size_t o1574 = offsetof(goldsrc::color24, b);
    using t1568 = decltype(goldsrc::color24::b);
    constexpr std::size_t s106 = sizeof(goldsrc::colorVec);
    constexpr std::size_t o1575 = offsetof(goldsrc::colorVec, r);
    using t1569 = decltype(goldsrc::colorVec::r);
    constexpr std::size_t o1576 = offsetof(goldsrc::colorVec, g);
    using t1570 = decltype(goldsrc::colorVec::g);
    constexpr std::size_t o1577 = offsetof(goldsrc::colorVec, b);
    using t1571 = decltype(goldsrc::colorVec::b);
    constexpr std::size_t o1578 = offsetof(goldsrc::colorVec, a);
    using t1572 = decltype(goldsrc::colorVec::a);
    constexpr std::size_t s107 = sizeof(goldsrc::vrect_t);
    constexpr std::size_t o1579 = offsetof(goldsrc::vrect_t, x);
    using t1573 = decltype(goldsrc::vrect_t::x);
    constexpr std::size_t o1580 = offsetof(goldsrc::vrect_t, y);
    using t1574 = decltype(goldsrc::vrect_t::y);
    constexpr std::size_t o1581 = offsetof(goldsrc::vrect_t, width);
    using t1575 = decltype(goldsrc::vrect_t::width);
    constexpr std::size_t o1582 = offsetof(goldsrc::vrect_t, height);
    using t1576 = decltype(goldsrc::vrect_t::height);
    constexpr std::size_t o1583 = offsetof(goldsrc::vrect_t, pnext);
    using t1577 = decltype(goldsrc::vrect_t::pnext);
    constexpr std::size_t s108 = sizeof(goldsrc::wrect_t);
    constexpr std::size_t o1584 = offsetof(goldsrc::wrect_t, left);
    using t1578 = decltype(goldsrc::wrect_t::left);
    constexpr std::size_t o1585 = offsetof(goldsrc::wrect_t, right);
    using t1579 = decltype(goldsrc::wrect_t::right);
    constexpr std::size_t o1586 = offsetof(goldsrc::wrect_t, top);
    using t1580 = decltype(goldsrc::wrect_t::top);
    constexpr std::size_t o1587 = offsetof(goldsrc::wrect_t, bottom);
    using t1581 = decltype(goldsrc::wrect_t::bottom);
    constexpr std::size_t s109 = sizeof(goldsrc::kbutton_t);
    constexpr std::size_t o1588 = offsetof(goldsrc::kbutton_t, down);
    using t1582 = decltype(goldsrc::kbutton_t::down);
    constexpr std::size_t o1589 = offsetof(goldsrc::kbutton_t, state);
    using t1583 = decltype(goldsrc::kbutton_t::state);
    constexpr std::size_t s110 = sizeof(goldsrc::usercmd_t);
    constexpr std::size_t o1590 = offsetof(goldsrc::usercmd_t, lerp_msec);
    using t1584 = decltype(goldsrc::usercmd_t::lerp_msec);
    constexpr std::size_t o1591 = offsetof(goldsrc::usercmd_t, msec);
    using t1585 = decltype(goldsrc::usercmd_t::msec);
    constexpr std::size_t o1592 = offsetof(goldsrc::usercmd_t, viewangles);
    using t1586 = decltype(goldsrc::usercmd_t::viewangles);
    constexpr std::size_t o1593 = offsetof(goldsrc::usercmd_t, forwardmove);
    using t1587 = decltype(goldsrc::usercmd_t::forwardmove);
    constexpr std::size_t o1594 = offsetof(goldsrc::usercmd_t, sidemove);
    using t1588 = decltype(goldsrc::usercmd_t::sidemove);
    constexpr std::size_t o1595 = offsetof(goldsrc::usercmd_t, upmove);
    using t1589 = decltype(goldsrc::usercmd_t::upmove);
    constexpr std::size_t o1596 = offsetof(goldsrc::usercmd_t, lightlevel);
    using t1590 = decltype(goldsrc::usercmd_t::lightlevel);
    constexpr std::size_t o1597 = offsetof(goldsrc::usercmd_t, buttons);
    using t1591 = decltype(goldsrc::usercmd_t::buttons);
    constexpr std::size_t o1598 = offsetof(goldsrc::usercmd_t, impulse);
    using t1592 = decltype(goldsrc::usercmd_t::impulse);
    constexpr std::size_t o1599 = offsetof(goldsrc::usercmd_t, weaponselect);
    using t1593 = decltype(goldsrc::usercmd_t::weaponselect);
    constexpr std::size_t o1600 = offsetof(goldsrc::usercmd_t, impact_index);
    using t1594 = decltype(goldsrc::usercmd_t::impact_index);
    constexpr std::size_t o1601 = offsetof(goldsrc::usercmd_t, impact_position);
    using t1595 = decltype(goldsrc::usercmd_t::impact_position);
    constexpr std::size_t s111 = sizeof(goldsrc::ref_params_t);
    constexpr std::size_t o1602 = offsetof(goldsrc::ref_params_t, vieworg);
    using t1596 = decltype(goldsrc::ref_params_t::vieworg);
    constexpr std::size_t o1603 = offsetof(goldsrc::ref_params_t, viewangles);
    using t1597 = decltype(goldsrc::ref_params_t::viewangles);
    constexpr std::size_t o1604 = offsetof(goldsrc::ref_params_t, forward);
    using t1598 = decltype(goldsrc::ref_params_t::forward);
    constexpr std::size_t o1605 = offsetof(goldsrc::ref_params_t, right);
    using t1599 = decltype(goldsrc::ref_params_t::right);
    constexpr std::size_t o1606 = offsetof(goldsrc::ref_params_t, up);
    using t1600 = decltype(goldsrc::ref_params_t::up);
    constexpr std::size_t o1607 = offsetof(goldsrc::ref_params_t, frametime);
    using t1601 = decltype(goldsrc::ref_params_t::frametime);
    constexpr std::size_t o1608 = offsetof(goldsrc::ref_params_t, time);
    using t1602 = decltype(goldsrc::ref_params_t::time);
    constexpr std::size_t o1609 = offsetof(goldsrc::ref_params_t, intermission);
    using t1603 = decltype(goldsrc::ref_params_t::intermission);
    constexpr std::size_t o1610 = offsetof(goldsrc::ref_params_t, paused);
    using t1604 = decltype(goldsrc::ref_params_t::paused);
    constexpr std::size_t o1611 = offsetof(goldsrc::ref_params_t, spectator);
    using t1605 = decltype(goldsrc::ref_params_t::spectator);
    constexpr std::size_t o1612 = offsetof(goldsrc::ref_params_t, onground);
    using t1606 = decltype(goldsrc::ref_params_t::onground);
    constexpr std::size_t o1613 = offsetof(goldsrc::ref_params_t, waterlevel);
    using t1607 = decltype(goldsrc::ref_params_t::waterlevel);
    constexpr std::size_t o1614 = offsetof(goldsrc::ref_params_t, simvel);
    using t1608 = decltype(goldsrc::ref_params_t::simvel);
    constexpr std::size_t o1615 = offsetof(goldsrc::ref_params_t, simorg);
    using t1609 = decltype(goldsrc::ref_params_t::simorg);
    constexpr std::size_t o1616 = offsetof(goldsrc::ref_params_t, viewheight);
    using t1610 = decltype(goldsrc::ref_params_t::viewheight);
    constexpr std::size_t o1617 = offsetof(goldsrc::ref_params_t, idealpitch);
    using t1611 = decltype(goldsrc::ref_params_t::idealpitch);
    constexpr std::size_t o1618 = offsetof(goldsrc::ref_params_t, cl_viewangles);
    using t1612 = decltype(goldsrc::ref_params_t::cl_viewangles);
    constexpr std::size_t o1619 = offsetof(goldsrc::ref_params_t, health);
    using t1613 = decltype(goldsrc::ref_params_t::health);
    constexpr std::size_t o1620 = offsetof(goldsrc::ref_params_t, crosshairangle);
    using t1614 = decltype(goldsrc::ref_params_t::crosshairangle);
    constexpr std::size_t o1621 = offsetof(goldsrc::ref_params_t, viewsize);
    using t1615 = decltype(goldsrc::ref_params_t::viewsize);
    constexpr std::size_t o1622 = offsetof(goldsrc::ref_params_t, punchangle);
    using t1616 = decltype(goldsrc::ref_params_t::punchangle);
    constexpr std::size_t o1623 = offsetof(goldsrc::ref_params_t, maxclients);
    using t1617 = decltype(goldsrc::ref_params_t::maxclients);
    constexpr std::size_t o1624 = offsetof(goldsrc::ref_params_t, viewentity);
    using t1618 = decltype(goldsrc::ref_params_t::viewentity);
    constexpr std::size_t o1625 = offsetof(goldsrc::ref_params_t, playernum);
    using t1619 = decltype(goldsrc::ref_params_t::playernum);
    constexpr std::size_t o1626 = offsetof(goldsrc::ref_params_t, max_entities);
    using t1620 = decltype(goldsrc::ref_params_t::max_entities);
    constexpr std::size_t o1627 = offsetof(goldsrc::ref_params_t, demoplayback);
    using t1621 = decltype(goldsrc::ref_params_t::demoplayback);
    constexpr std::size_t o1628 = offsetof(goldsrc::ref_params_t, hardware);
    using t1622 = decltype(goldsrc::ref_params_t::hardware);
    constexpr std::size_t o1629 = offsetof(goldsrc::ref_params_t, smoothing);
    using t1623 = decltype(goldsrc::ref_params_t::smoothing);
    constexpr std::size_t o1630 = offsetof(goldsrc::ref_params_t, cmd);
    using t1624 = decltype(goldsrc::ref_params_t::cmd);
    constexpr std::size_t o1631 = offsetof(goldsrc::ref_params_t, movevars);
    using t1625 = decltype(goldsrc::ref_params_t::movevars);
    constexpr std::size_t o1632 = offsetof(goldsrc::ref_params_t, viewport);
    using t1626 = decltype(goldsrc::ref_params_t::viewport);
    constexpr std::size_t o1633 = offsetof(goldsrc::ref_params_t, nextView);
    using t1627 = decltype(goldsrc::ref_params_t::nextView);
    constexpr std::size_t o1634 = offsetof(goldsrc::ref_params_t, onlyClientDraw);
    using t1628 = decltype(goldsrc::ref_params_t::onlyClientDraw);
    constexpr std::size_t s112 = sizeof(goldsrc::refdef_t);
    constexpr std::size_t o1635 = offsetof(goldsrc::refdef_t, vrect);
    using t1629 = decltype(goldsrc::refdef_t::vrect);
    constexpr std::size_t o1636 = offsetof(goldsrc::refdef_t, aliasvrect);
    using t1630 = decltype(goldsrc::refdef_t::aliasvrect);
    constexpr std::size_t o1637 = offsetof(goldsrc::refdef_t, vrectright);
    using t1631 = decltype(goldsrc::refdef_t::vrectright);
    constexpr std::size_t o1638 = offsetof(goldsrc::refdef_t, vrectbottom);
    using t1632 = decltype(goldsrc::refdef_t::vrectbottom);
    constexpr std::size_t o1639 = offsetof(goldsrc::refdef_t, aliasvrectright);
    using t1633 = decltype(goldsrc::refdef_t::aliasvrectright);
    constexpr std::size_t o1640 = offsetof(goldsrc::refdef_t, aliasvrectbottom);
    using t1634 = decltype(goldsrc::refdef_t::aliasvrectbottom);
    constexpr std::size_t o1641 = offsetof(goldsrc::refdef_t, vrectrightedge);
    using t1635 = decltype(goldsrc::refdef_t::vrectrightedge);
    constexpr std::size_t o1642 = offsetof(goldsrc::refdef_t, fvrectx);
    using t1636 = decltype(goldsrc::refdef_t::fvrectx);
    constexpr std::size_t o1643 = offsetof(goldsrc::refdef_t, fvrecty);
    using t1637 = decltype(goldsrc::refdef_t::fvrecty);
    constexpr std::size_t o1644 = offsetof(goldsrc::refdef_t, fvrectx_adj);
    using t1638 = decltype(goldsrc::refdef_t::fvrectx_adj);
    constexpr std::size_t o1645 = offsetof(goldsrc::refdef_t, fvrecty_adj);
    using t1639 = decltype(goldsrc::refdef_t::fvrecty_adj);
    constexpr std::size_t o1646 = offsetof(goldsrc::refdef_t, vrect_x_adj_shift20);
    using t1640 = decltype(goldsrc::refdef_t::vrect_x_adj_shift20);
    constexpr std::size_t o1647 = offsetof(goldsrc::refdef_t, vrectright_adj_shift20);
    using t1641 = decltype(goldsrc::refdef_t::vrectright_adj_shift20);
    constexpr std::size_t o1648 = offsetof(goldsrc::refdef_t, fvrectright_adj);
    using t1642 = decltype(goldsrc::refdef_t::fvrectright_adj);
    constexpr std::size_t o1649 = offsetof(goldsrc::refdef_t, fvrectbottom_adj);
    using t1643 = decltype(goldsrc::refdef_t::fvrectbottom_adj);
    constexpr std::size_t o1650 = offsetof(goldsrc::refdef_t, fvrectright);
    using t1644 = decltype(goldsrc::refdef_t::fvrectright);
    constexpr std::size_t o1651 = offsetof(goldsrc::refdef_t, fvrectbottom);
    using t1645 = decltype(goldsrc::refdef_t::fvrectbottom);
    constexpr std::size_t o1652 = offsetof(goldsrc::refdef_t, horizontalFieldOfView);
    using t1646 = decltype(goldsrc::refdef_t::horizontalFieldOfView);
    constexpr std::size_t o1653 = offsetof(goldsrc::refdef_t, xOrigin);
    using t1647 = decltype(goldsrc::refdef_t::xOrigin);
    constexpr std::size_t o1654 = offsetof(goldsrc::refdef_t, yOrigin);
    using t1648 = decltype(goldsrc::refdef_t::yOrigin);
    constexpr std::size_t o1655 = offsetof(goldsrc::refdef_t, vieworg);
    using t1649 = decltype(goldsrc::refdef_t::vieworg);
    constexpr std::size_t o1656 = offsetof(goldsrc::refdef_t, viewangles);
    using t1650 = decltype(goldsrc::refdef_t::viewangles);
    constexpr std::size_t o1657 = offsetof(goldsrc::refdef_t, ambientlight);
    using t1651 = decltype(goldsrc::refdef_t::ambientlight);
    constexpr std::size_t o1658 = offsetof(goldsrc::refdef_t, onlyClientDraws);
    using t1652 = decltype(goldsrc::refdef_t::onlyClientDraws);
    constexpr std::size_t s113 = sizeof(goldsrc::screenfade_t);
    constexpr std::size_t o1659 = offsetof(goldsrc::screenfade_t, fadeSpeed);
    using t1653 = decltype(goldsrc::screenfade_t::fadeSpeed);
    constexpr std::size_t o1660 = offsetof(goldsrc::screenfade_t, fadeEnd);
    using t1654 = decltype(goldsrc::screenfade_t::fadeEnd);
    constexpr std::size_t o1661 = offsetof(goldsrc::screenfade_t, fadeTotalEnd);
    using t1655 = decltype(goldsrc::screenfade_t::fadeTotalEnd);
    constexpr std::size_t o1662 = offsetof(goldsrc::screenfade_t, fadeReset);
    using t1656 = decltype(goldsrc::screenfade_t::fadeReset);
    constexpr std::size_t o1663 = offsetof(goldsrc::screenfade_t, fader);
    using t1657 = decltype(goldsrc::screenfade_t::fader);
    constexpr std::size_t o1664 = offsetof(goldsrc::screenfade_t, fadeg);
    using t1658 = decltype(goldsrc::screenfade_t::fadeg);
    constexpr std::size_t o1665 = offsetof(goldsrc::screenfade_t, fadeb);
    using t1659 = decltype(goldsrc::screenfade_t::fadeb);
    constexpr std::size_t o1666 = offsetof(goldsrc::screenfade_t, fadealpha);
    using t1660 = decltype(goldsrc::screenfade_t::fadealpha);
    constexpr std::size_t o1667 = offsetof(goldsrc::screenfade_t, fadeFlags);
    using t1661 = decltype(goldsrc::screenfade_t::fadeFlags);
    constexpr std::size_t s114 = sizeof(goldsrc::IVoiceTweak);
    constexpr std::size_t o1668 = offsetof(goldsrc::IVoiceTweak, StartVoiceTweakMode);
    using t1662 = decltype(goldsrc::IVoiceTweak::StartVoiceTweakMode);
    constexpr std::size_t o1669 = offsetof(goldsrc::IVoiceTweak, EndVoiceTweakMode);
    using t1663 = decltype(goldsrc::IVoiceTweak::EndVoiceTweakMode);
    constexpr std::size_t o1670 = offsetof(goldsrc::IVoiceTweak, SetControlFloat);
    using t1664 = decltype(goldsrc::IVoiceTweak::SetControlFloat);
    constexpr std::size_t o1671 = offsetof(goldsrc::IVoiceTweak, GetControlFloat);
    using t1665 = decltype(goldsrc::IVoiceTweak::GetControlFloat);
    constexpr std::size_t o1672 = offsetof(goldsrc::IVoiceTweak, GetSpeakingVolume);
    using t1666 = decltype(goldsrc::IVoiceTweak::GetSpeakingVolume);
    constexpr auto c0 = goldsrc::cstrike::DMG_HALLUC;
    constexpr auto c1 = goldsrc::cstrike::DMG_GENERIC;
    constexpr auto c2 = goldsrc::cstrike::DMG_CRUSH;
    constexpr auto c3 = goldsrc::cstrike::DMG_BULLET;
    constexpr auto c4 = goldsrc::cstrike::DMG_NAIL;
    constexpr auto c5 = goldsrc::cstrike::DMG_SLASH;
    constexpr auto c6 = goldsrc::cstrike::DMG_BURN;
    constexpr auto c7 = goldsrc::cstrike::DMG_FREEZE;
    constexpr auto c8 = goldsrc::cstrike::DMG_NOT_SELF;
    constexpr auto c9 = goldsrc::cstrike::DMG_FALL;
    constexpr auto c10 = goldsrc::cstrike::DMG_BLAST;
    constexpr auto c11 = goldsrc::cstrike::DMG_CLUB;
    constexpr auto c12 = goldsrc::cstrike::DMG_SHOCK;
    constexpr auto c13 = goldsrc::cstrike::DMG_CONCUSS;
    constexpr auto c14 = goldsrc::cstrike::DMG_SONIC;
    constexpr auto c15 = goldsrc::cstrike::DMG_GIB_CORPSE;
    constexpr auto c16 = goldsrc::cstrike::DMG_ENERGYBEAM;
    constexpr auto c17 = goldsrc::cstrike::DMG_NEVERGIB;
    constexpr auto c18 = goldsrc::cstrike::DMG_ALWAYSGIB;
    constexpr auto c19 = goldsrc::cstrike::DMG_DROWN;
    constexpr auto c20 = goldsrc::cstrike::DMG_FIRSTTIMEBASED;
    constexpr auto c21 = goldsrc::cstrike::DMG_PARALYZE;
    constexpr auto c22 = goldsrc::cstrike::DMG_NERVEGAS;
    constexpr auto c23 = goldsrc::cstrike::DMG_POISON;
    constexpr auto c24 = goldsrc::cstrike::DMG_RADIATION;
    constexpr auto c25 = goldsrc::cstrike::DMG_DROWNRECOVER;
    constexpr auto c26 = goldsrc::cstrike::DMG_ACID;
    constexpr auto c27 = goldsrc::cstrike::DMG_SLOWBURN;
    constexpr auto c28 = goldsrc::cstrike::DMG_SLOWFREEZE;
    constexpr auto c29 = goldsrc::cstrike::DMG_SHOWNHUD;
    constexpr auto c30 = goldsrc::cstrike::DMG_MORTAR;
    constexpr auto c31 = goldsrc::cstrike::DMG_TRANQ;
    constexpr auto c32 = goldsrc::cstrike::DMG_TIMEBASED;
    constexpr auto c33 = goldsrc::cstrike::DMG_EXPLOSION;
    constexpr auto c34 = goldsrc::cstrike::DMG_IGNITE;
    constexpr auto c35 = goldsrc::cstrike::DMG_IGNORE_MAXHEALTH;
    constexpr auto c36 = goldsrc::cstrike::DMG_RADIUS_MAX;
    constexpr auto c37 = goldsrc::cstrike::DMG_RADIUS_QUAKE;
    constexpr auto c38 = goldsrc::cstrike::DMG_IGNOREARMOR;
    constexpr auto c39 = goldsrc::cstrike::DMG_AIMED;
    constexpr auto c40 = goldsrc::cstrike::DMG_WALLPIERCING;
    constexpr auto c41 = goldsrc::cstrike::DMG_CALTROP;
    constexpr auto c42 = goldsrc::cstrike::HIDEHUD_WEAPONS;
    constexpr auto c43 = goldsrc::cstrike::HIDEHUD_FLASHLIGHT;
    constexpr auto c44 = goldsrc::cstrike::HIDEHUD_ALL;
    constexpr auto c45 = goldsrc::cstrike::HIDEHUD_HEALTH;
    constexpr auto c46 = goldsrc::cstrike::HIDEHUD_TIMER;
    constexpr auto c47 = goldsrc::cstrike::HIDEHUD_MONEY;
    constexpr auto c48 = goldsrc::cstrike::HIDEHUD_CROSSHAIR;
    constexpr auto c49 = goldsrc::cstrike::HIDEHUD_OBSERVER_CROSSHAIR;
    constexpr auto c50 = goldsrc::cstrike::TEAM_UNASSIGNED;
    constexpr auto c51 = goldsrc::cstrike::TEAM_TERRORIST;
    constexpr auto c52 = goldsrc::cstrike::TEAM_CT;
    constexpr auto c53 = goldsrc::cstrike::TEAM_SPECTATOR;
    constexpr auto c54 = goldsrc::cstrike::WEAPON_SUIT;
    constexpr auto c55 = goldsrc::cstrike::DHN_DRAWZERO;
    constexpr auto c56 = goldsrc::cstrike::DHN_2DIGITS;
    constexpr auto c57 = goldsrc::cstrike::DHN_3DIGITS;
    constexpr auto c58 = goldsrc::cstrike::DHN_PADZERO;
    constexpr auto c59 = goldsrc::cstrike::DHN_4DIGITS;
    constexpr auto c60 = goldsrc::cstrike::DHN_5DIGITS;
    constexpr auto c61 = goldsrc::cstrike::DMG_IMAGE_POISON;
    constexpr auto c62 = goldsrc::cstrike::DMG_IMAGE_ACID;
    constexpr auto c63 = goldsrc::cstrike::DMG_IMAGE_COLD;
    constexpr auto c64 = goldsrc::cstrike::DMG_IMAGE_LIFE;
    constexpr auto c65 = goldsrc::cstrike::DMG_IMAGE_DROWN;
    constexpr auto c66 = goldsrc::cstrike::DMG_IMAGE_BURN;
    constexpr auto c67 = goldsrc::cstrike::DMG_IMAGE_NERVE;
    constexpr auto c68 = goldsrc::cstrike::DMG_IMAGE_RAD;
    constexpr auto c69 = goldsrc::cstrike::DMG_IMAGE_SHOCK;
    constexpr auto c70 = goldsrc::cstrike::DMG_IMAGE_CALTROP;
    constexpr auto c71 = goldsrc::cstrike::DMG_IMAGE_TRANQ;
    constexpr auto c72 = goldsrc::cstrike::DMG_IMAGE_CONCUSS;
    constexpr auto c73 = goldsrc::cstrike::DMG_IMAGE_HALLUC;
    constexpr auto c74 = goldsrc::cstrike::FADE_TIME;
    constexpr auto c75 = goldsrc::cstrike::MIN_ALPHA;
    constexpr auto c76 = goldsrc::cstrike::NUM_DMG_TYPES;
    constexpr auto c77 = goldsrc::cstrike::RGB_GREENISH;
    constexpr auto c78 = goldsrc::cstrike::RGB_REDISH;
    constexpr auto c79 = goldsrc::cstrike::RGB_YELLOWISH;
    constexpr auto c80 = goldsrc::cstrike::MAX_SPRITE_NAME_LENGTH;
    constexpr auto c81 = goldsrc::cstrike::INSET_OFF;
    constexpr auto c82 = goldsrc::cstrike::INSET_CHASE_FREE;
    constexpr auto c83 = goldsrc::cstrike::INSET_IN_EYE;
    constexpr auto c84 = goldsrc::cstrike::INSET_MAP_FREE;
    constexpr auto c85 = goldsrc::cstrike::INSET_MAP_CHASE;
    constexpr auto c86 = goldsrc::cstrike::WEAPON_IS_ONTARGET;
    constexpr auto c87 = goldsrc::FCVAR_ARCHIVE;
    constexpr auto c88 = goldsrc::FCVAR_USERINFO;
    constexpr auto c89 = goldsrc::FCVAR_SERVER;
    constexpr auto c90 = goldsrc::FCVAR_EXTDLL;
    constexpr auto c91 = goldsrc::FCVAR_CLIENTDLL;
    constexpr auto c92 = goldsrc::FCVAR_PROTECTED;
    constexpr auto c93 = goldsrc::FCVAR_SPONLY;
    constexpr auto c94 = goldsrc::FCVAR_PRINTABLEONLY;
    constexpr auto c95 = goldsrc::FCVAR_UNLOGGED;
    constexpr auto c96 = goldsrc::FCVAR_NOEXTRAWHITEPACE;
    constexpr auto c97 = goldsrc::FCVAR_PRIVILEGED;
    constexpr auto c98 = goldsrc::FCVAR_FILTERABLE;
    constexpr auto c99 = goldsrc::FCVAR_ISEXECUTED;
    constexpr auto c100 = goldsrc::FCVAR_ISPATH;
    constexpr auto c101 = goldsrc::FCVAR_CHEAT;
    constexpr auto c102 = goldsrc::COM_MAX_CMD_LINE;
    constexpr auto c103 = goldsrc::NUM_SAFE_ARGVS;
    constexpr auto c104 = goldsrc::IDPOLYHEADER;
    constexpr auto c105 = goldsrc::MAX_KNOWN_MODELS;
    constexpr auto c106 = goldsrc::SURF_PLANEBACK;
    constexpr auto c107 = goldsrc::SURF_DRAWSKY;
    constexpr auto c108 = goldsrc::SURF_DRAWSPRITE;
    constexpr auto c109 = goldsrc::SURF_DRAWTURB;
    constexpr auto c110 = goldsrc::SURF_DRAWTILED;
    constexpr auto c111 = goldsrc::SURF_DRAWBACKGROUND;
    constexpr auto c112 = goldsrc::SURF_UNDERWATER;
    constexpr auto c113 = goldsrc::MAX_MODEL_NAME;
    constexpr auto c114 = goldsrc::SIZEBUF_CHECK_OVERFLOW;
    constexpr auto c115 = goldsrc::SIZEBUF_ALLOW_OVERFLOW;
    constexpr auto c116 = goldsrc::SIZEBUF_OVERFLOWED;
    constexpr auto c117 = goldsrc::FCUST_FROMHPAK;
    constexpr auto c118 = goldsrc::FCUST_WIPEDATA;
    constexpr auto c119 = goldsrc::FCUST_IGNOREINIT;
    constexpr auto c120 = goldsrc::MAX_RESOURCE_LIST;
    constexpr auto c121 = goldsrc::RES_FATALIFMISSING;
    constexpr auto c122 = goldsrc::RES_WASMISSING;
    constexpr auto c123 = goldsrc::RES_CUSTOM;
    constexpr auto c124 = goldsrc::RES_REQUESTED;
    constexpr auto c125 = goldsrc::RES_PRECACHED;
    constexpr auto c126 = goldsrc::RES_ALWAYS;
    constexpr auto c127 = goldsrc::RES_UNK_6;
    constexpr auto c128 = goldsrc::RES_CHECKFILE;
    constexpr auto c129 = goldsrc::CHAN_AUTO;
    constexpr auto c130 = goldsrc::CHAN_WEAPON;
    constexpr auto c131 = goldsrc::CHAN_VOICE;
    constexpr auto c132 = goldsrc::CHAN_ITEM;
    constexpr auto c133 = goldsrc::CHAN_BODY;
    constexpr auto c134 = goldsrc::CHAN_STREAM;
    constexpr auto c135 = goldsrc::CHAN_STATIC;
    constexpr auto c136 = goldsrc::CHAN_NETWORKVOICE_BASE;
    constexpr auto c137 = goldsrc::CHAN_NETWORKVOICE_END;
    constexpr auto c138 = goldsrc::CHAN_BOT;
    constexpr auto c139 = goldsrc::CVOXFILESENTENCEMAX;
    constexpr auto c140 = goldsrc::OFF_SVC_MESSAGES_P;
    constexpr auto c141 = goldsrc::EF_BRIGHTFIELD;
    constexpr auto c142 = goldsrc::EF_MUZZLEFLASH;
    constexpr auto c143 = goldsrc::EF_BRIGHTLIGHT;
    constexpr auto c144 = goldsrc::EF_DIMLIGHT;
    constexpr auto c145 = goldsrc::EF_INVLIGHT;
    constexpr auto c146 = goldsrc::EF_NOINTERP;
    constexpr auto c147 = goldsrc::EF_LIGHT;
    constexpr auto c148 = goldsrc::EF_NODRAW;
    constexpr auto c149 = goldsrc::EF_NIGHTVISION;
    constexpr auto c150 = goldsrc::EF_SNIPERLASER;
    constexpr auto c151 = goldsrc::EF_FIBERCAMERA;
    constexpr auto c152 = goldsrc::FL_DORMANT;
    constexpr auto c153 = goldsrc::FL_FLY;
    constexpr auto c154 = goldsrc::FL_SWIM;
    constexpr auto c155 = goldsrc::FL_CONVEYOR;
    constexpr auto c156 = goldsrc::FL_CLIENT;
    constexpr auto c157 = goldsrc::FL_INWATER;
    constexpr auto c158 = goldsrc::FL_MONSTER;
    constexpr auto c159 = goldsrc::FL_GODMODE;
    constexpr auto c160 = goldsrc::FL_NOTARGET;
    constexpr auto c161 = goldsrc::FL_SKIPLOCALHOST;
    constexpr auto c162 = goldsrc::FL_ONGROUND;
    constexpr auto c163 = goldsrc::FL_PARTIALGROUND;
    constexpr auto c164 = goldsrc::FL_WATERJUMP;
    constexpr auto c165 = goldsrc::FL_FROZEN;
    constexpr auto c166 = goldsrc::FL_FAKECLIENT;
    constexpr auto c167 = goldsrc::FL_DUCKING;
    constexpr auto c168 = goldsrc::FL_FLOAT;
    constexpr auto c169 = goldsrc::FL_GRAPHED;
    constexpr auto c170 = goldsrc::FL_IMMUNE_WATER;
    constexpr auto c171 = goldsrc::FL_IMMUNE_SLIME;
    constexpr auto c172 = goldsrc::FL_IMMUNE_LAVA;
    constexpr auto c173 = goldsrc::FL_PROXY;
    constexpr auto c174 = goldsrc::FL_ALWAYSTHINK;
    constexpr auto c175 = goldsrc::FL_BASEVELOCITY;
    constexpr auto c176 = goldsrc::FL_MONSTERCLIP;
    constexpr auto c177 = goldsrc::FL_ONTRAIN;
    constexpr auto c178 = goldsrc::FL_WORLDBRUSH;
    constexpr auto c179 = goldsrc::FL_SPECTATOR;
    constexpr auto c180 = goldsrc::FL_CUSTOMENTITY;
    constexpr auto c181 = goldsrc::FL_KILLME;
    constexpr auto c182 = goldsrc::HISTORY_MASK;
    constexpr auto c183 = goldsrc::HISTORY_MAX;
    constexpr auto c184 = goldsrc::MOVETYPE_NONE;
    constexpr auto c185 = goldsrc::MOVETYPE_WALK;
    constexpr auto c186 = goldsrc::MOVETYPE_STEP;
    constexpr auto c187 = goldsrc::MOVETYPE_FLY;
    constexpr auto c188 = goldsrc::MOVETYPE_TOSS;
    constexpr auto c189 = goldsrc::MOVETYPE_PUSH;
    constexpr auto c190 = goldsrc::MOVETYPE_NOCLIP;
    constexpr auto c191 = goldsrc::MOVETYPE_FLYMISSILE;
    constexpr auto c192 = goldsrc::MOVETYPE_BOUNCE;
    constexpr auto c193 = goldsrc::MOVETYPE_BOUNCEMISSILE;
    constexpr auto c194 = goldsrc::MOVETYPE_FOLLOW;
    constexpr auto c195 = goldsrc::MOVETYPE_PUSHSTEP;
    constexpr auto c196 = goldsrc::HLBSP_VERSION;
    constexpr auto c197 = goldsrc::Q1BSP_VERSION;
    constexpr auto c198 = goldsrc::IDSPRITEHEADER;
    constexpr auto c199 = goldsrc::SPRITE_VERSION;
    constexpr auto c200 = goldsrc::MAXSTUDIOSKINS;
    constexpr auto c201 = goldsrc::STUDIO_HAS_NORMALS;
    constexpr auto c202 = goldsrc::STUDIO_LOOPING;
    constexpr auto c203 = goldsrc::STUDIO_NF_FLATSHADE;
    constexpr auto c204 = goldsrc::STUDIO_X;
    constexpr auto c205 = goldsrc::STUDIO_HAS_VERTICES;
    constexpr auto c206 = goldsrc::STUDIO_NF_CHROME;
    constexpr auto c207 = goldsrc::STUDIO_Y;
    constexpr auto c208 = goldsrc::STUDIO_HAS_BBOX;
    constexpr auto c209 = goldsrc::STUDIO_NF_FULLBRIGHT;
    constexpr auto c210 = goldsrc::STUDIO_Z;
    constexpr auto c211 = goldsrc::STUDIO_HAS_CHROME;
    constexpr auto c212 = goldsrc::STUDIO_NF_NOMIPS;
    constexpr auto c213 = goldsrc::STUDIO_XR;
    constexpr auto c214 = goldsrc::STUDIO_NF_ALPHA;
    constexpr auto c215 = goldsrc::STUDIO_YR;
    constexpr auto c216 = goldsrc::STUDIO_NF_ADDITIVE;
    constexpr auto c217 = goldsrc::STUDIO_ZR;
    constexpr auto c218 = goldsrc::STUDIO_LX;
    constexpr auto c219 = goldsrc::STUDIO_NF_MASKED;
    constexpr auto c220 = goldsrc::STUDIO_LY;
    constexpr auto c221 = goldsrc::STUDIO_DYNAMIC_LIGHT;
    constexpr auto c222 = goldsrc::STUDIO_LZ;
    constexpr auto c223 = goldsrc::STUDIO_AX;
    constexpr auto c224 = goldsrc::STUDIO_TRACE_HITBOX;
    constexpr auto c225 = goldsrc::STUDIO_AY;
    constexpr auto c226 = goldsrc::STUDIO_AZ;
    constexpr auto c227 = goldsrc::STUDIO_AXR;
    constexpr auto c228 = goldsrc::STUDIO_AYR;
    constexpr auto c229 = goldsrc::STUDIO_AZR;
    constexpr auto c230 = goldsrc::STUDIO_TYPES;
    constexpr auto c231 = goldsrc::STUDIO_RLOOP;
    constexpr auto c232 = goldsrc::STUDIO_CACHE_SIZE;
    constexpr auto c233 = goldsrc::STUDIO_MOUTH;
    constexpr auto c234 = goldsrc::STUDIO_NUM_HULLS;
    constexpr auto c235 = goldsrc::STUDIO_NUM_PLANES;
    constexpr auto c236 = goldsrc::STUDIO_TO_RAD;
    constexpr auto c237 = goldsrc::DRC_CMD_NONE;
    constexpr auto c238 = goldsrc::DRC_CMD_START;
    constexpr auto c239 = goldsrc::DRC_CMD_EVENT;
    constexpr auto c240 = goldsrc::DRC_CMD_MODE;
    constexpr auto c241 = goldsrc::DRC_CMD_CAMERA;
    constexpr auto c242 = goldsrc::DRC_CMD_TIMESCALE;
    constexpr auto c243 = goldsrc::DRC_CMD_MESSAGE;
    constexpr auto c244 = goldsrc::DRC_CMD_SOUND;
    constexpr auto c245 = goldsrc::DRC_CMD_STATUS;
    constexpr auto c246 = goldsrc::DRC_CMD_BANNER;
    constexpr auto c247 = goldsrc::DRC_CMD_STUFFTEXT;
    constexpr auto c248 = goldsrc::DRC_CMD_CHASE;
    constexpr auto c249 = goldsrc::DRC_CMD_INEYE;
    constexpr auto c250 = goldsrc::DRC_CMD_MAP;
    constexpr auto c251 = goldsrc::DRC_CMD_CAMPATH;
    constexpr auto c252 = goldsrc::DRC_CMD_LAST;
    constexpr auto c253 = goldsrc::DRC_CMD_WAYPOINTS;
    constexpr auto c254 = goldsrc::DRC_FLAG_STARTPATH;
    constexpr auto c255 = goldsrc::DRC_FLAG_SLOWSTART;
    constexpr auto c256 = goldsrc::DRC_FLAG_SLOWEND;
    constexpr auto c257 = goldsrc::DRC_FLAG_PRIO_MASK;
    constexpr auto c258 = goldsrc::DRC_FLAG_SIDE;
    constexpr auto c259 = goldsrc::DRC_FLAG_DRAMATIC;
    constexpr auto c260 = goldsrc::DRC_FLAG_SLOWMOTION;
    constexpr auto c261 = goldsrc::DRC_FLAG_FACEPLAYER;
    constexpr auto c262 = goldsrc::DRC_FLAG_INTRO;
    constexpr auto c263 = goldsrc::DRC_FLAG_FINAL;
    constexpr auto c264 = goldsrc::DRC_FLAG_NO_RANDOM;
    constexpr auto c265 = goldsrc::HLTV_ACTIVE;
    constexpr auto c266 = goldsrc::HLTV_STATUS;
    constexpr auto c267 = goldsrc::HLTV_LISTEN;
    constexpr auto c268 = goldsrc::TYPE_CLIENT;
    constexpr auto c269 = goldsrc::TYPE_PROXY;
    constexpr auto c270 = goldsrc::TYPE_DIRECTOR;
    constexpr auto c271 = goldsrc::TYPE_COMMENTATOR;
    constexpr auto c272 = goldsrc::TYPE_DEMO;
    constexpr auto c273 = goldsrc::K_TAB;
    constexpr auto c274 = goldsrc::K_ENTER;
    constexpr auto c275 = goldsrc::K_ESCAPE;
    constexpr auto c276 = goldsrc::K_SPACE;
    constexpr auto c277 = goldsrc::K_BACKSPACE;
    constexpr auto c278 = goldsrc::K_UPARROW;
    constexpr auto c279 = goldsrc::K_DOWNARROW;
    constexpr auto c280 = goldsrc::K_LEFTARROW;
    constexpr auto c281 = goldsrc::K_RIGHTARROW;
    constexpr auto c282 = goldsrc::K_ALT;
    constexpr auto c283 = goldsrc::K_CTRL;
    constexpr auto c284 = goldsrc::K_SHIFT;
    constexpr auto c285 = goldsrc::K_F1;
    constexpr auto c286 = goldsrc::K_F2;
    constexpr auto c287 = goldsrc::K_F3;
    constexpr auto c288 = goldsrc::K_F4;
    constexpr auto c289 = goldsrc::K_F5;
    constexpr auto c290 = goldsrc::K_F6;
    constexpr auto c291 = goldsrc::K_F7;
    constexpr auto c292 = goldsrc::K_F8;
    constexpr auto c293 = goldsrc::K_F9;
    constexpr auto c294 = goldsrc::K_F10;
    constexpr auto c295 = goldsrc::K_F11;
    constexpr auto c296 = goldsrc::K_F12;
    constexpr auto c297 = goldsrc::K_INS;
    constexpr auto c298 = goldsrc::K_DEL;
    constexpr auto c299 = goldsrc::K_PGDN;
    constexpr auto c300 = goldsrc::K_PGUP;
    constexpr auto c301 = goldsrc::K_HOME;
    constexpr auto c302 = goldsrc::K_END;
    constexpr auto c303 = goldsrc::K_KP_HOME;
    constexpr auto c304 = goldsrc::K_KP_UPARROW;
    constexpr auto c305 = goldsrc::K_KP_PGUP;
    constexpr auto c306 = goldsrc::K_KP_LEFTARROW;
    constexpr auto c307 = goldsrc::K_KP_5;
    constexpr auto c308 = goldsrc::K_KP_RIGHTARROW;
    constexpr auto c309 = goldsrc::K_KP_END;
    constexpr auto c310 = goldsrc::K_KP_DOWNARROW;
    constexpr auto c311 = goldsrc::K_KP_PGDN;
    constexpr auto c312 = goldsrc::K_KP_ENTER;
    constexpr auto c313 = goldsrc::K_KP_INS;
    constexpr auto c314 = goldsrc::K_KP_DEL;
    constexpr auto c315 = goldsrc::K_KP_SLASH;
    constexpr auto c316 = goldsrc::K_KP_MINUS;
    constexpr auto c317 = goldsrc::K_KP_PLUS;
    constexpr auto c318 = goldsrc::K_CAPSLOCK;
    constexpr auto c319 = goldsrc::K_JOY1;
    constexpr auto c320 = goldsrc::K_JOY2;
    constexpr auto c321 = goldsrc::K_JOY3;
    constexpr auto c322 = goldsrc::K_JOY4;
    constexpr auto c323 = goldsrc::K_AUX1;
    constexpr auto c324 = goldsrc::K_AUX2;
    constexpr auto c325 = goldsrc::K_AUX3;
    constexpr auto c326 = goldsrc::K_AUX4;
    constexpr auto c327 = goldsrc::K_AUX5;
    constexpr auto c328 = goldsrc::K_AUX6;
    constexpr auto c329 = goldsrc::K_AUX7;
    constexpr auto c330 = goldsrc::K_AUX8;
    constexpr auto c331 = goldsrc::K_AUX9;
    constexpr auto c332 = goldsrc::K_AUX10;
    constexpr auto c333 = goldsrc::K_AUX11;
    constexpr auto c334 = goldsrc::K_AUX12;
    constexpr auto c335 = goldsrc::K_AUX13;
    constexpr auto c336 = goldsrc::K_AUX14;
    constexpr auto c337 = goldsrc::K_AUX15;
    constexpr auto c338 = goldsrc::K_AUX16;
    constexpr auto c339 = goldsrc::K_AUX17;
    constexpr auto c340 = goldsrc::K_AUX18;
    constexpr auto c341 = goldsrc::K_AUX19;
    constexpr auto c342 = goldsrc::K_AUX20;
    constexpr auto c343 = goldsrc::K_AUX21;
    constexpr auto c344 = goldsrc::K_AUX22;
    constexpr auto c345 = goldsrc::K_AUX23;
    constexpr auto c346 = goldsrc::K_AUX24;
    constexpr auto c347 = goldsrc::K_AUX25;
    constexpr auto c348 = goldsrc::K_AUX26;
    constexpr auto c349 = goldsrc::K_AUX27;
    constexpr auto c350 = goldsrc::K_AUX28;
    constexpr auto c351 = goldsrc::K_AUX29;
    constexpr auto c352 = goldsrc::K_AUX30;
    constexpr auto c353 = goldsrc::K_AUX31;
    constexpr auto c354 = goldsrc::K_AUX32;
    constexpr auto c355 = goldsrc::K_MWHEELDOWN;
    constexpr auto c356 = goldsrc::K_MWHEELUP;
    constexpr auto c357 = goldsrc::K_MOUSE1;
    constexpr auto c358 = goldsrc::K_MOUSE2;
    constexpr auto c359 = goldsrc::K_MOUSE3;
    constexpr auto c360 = goldsrc::K_MOUSE4;
    constexpr auto c361 = goldsrc::K_MOUSE5;
    constexpr auto c362 = goldsrc::K_PAUSE;
    constexpr auto c363 = goldsrc::MAX_LIGHTSTYLE_INDEX_BITS;
    constexpr auto c364 = goldsrc::MAX_DECAL_INDEX_BITS;
    constexpr auto c365 = goldsrc::MAX_GENERIC_INDEX_BITS;
    constexpr auto c366 = goldsrc::MAX_MODEL_INDEX_BITS;
    constexpr auto c367 = goldsrc::MAX_SOUND_INDEX_BITS;
    constexpr auto c368 = goldsrc::MAX_NUM_ARGVS;
    constexpr auto c369 = goldsrc::MAX_LIGHTSTYLES;
    constexpr auto c370 = goldsrc::MAX_QPATH;
    constexpr auto c371 = goldsrc::MAX_EVENTS;
    constexpr auto c372 = goldsrc::MAX_PACKET_ENTITIES;
    constexpr auto c373 = goldsrc::MAX_OSPATH;
    constexpr auto c374 = goldsrc::MAX_BASE_DECALS;
    constexpr auto c375 = goldsrc::MAX_GENERIC;
    constexpr auto c376 = goldsrc::MAX_MODELS;
    constexpr auto c377 = goldsrc::MAX_SOUNDS;
    constexpr auto c378 = goldsrc::MAX_SOUNDS_HASHLOOKUP_SIZE;
    constexpr auto c379 = goldsrc::MAX_CLIENTS;
    constexpr auto c380 = goldsrc::PM_NORMAL;
    constexpr auto c381 = goldsrc::PM_TRACELINE_PHYSENTSONLY;
    constexpr auto c382 = goldsrc::PM_STUDIO_IGNORE;
    constexpr auto c383 = goldsrc::PM_TRACELINE_ANYVISIBLE;
    constexpr auto c384 = goldsrc::PM_STUDIO_BOX;
    constexpr auto c385 = goldsrc::PM_GLASS_IGNORE;
    constexpr auto c386 = goldsrc::PM_WORLD_ONLY;
    constexpr auto c387 = goldsrc::PM_CHECKSTUCK_MINTIME;
    constexpr auto c388 = goldsrc::PM_DEAD_VIEWHEIGHT;
    constexpr auto c389 = goldsrc::PM_PLAYER_FALL_PUNCH_THRESHHOLD;
    constexpr auto c390 = goldsrc::PM_PLAYER_MIN_BOUNCE_SPEED;
    constexpr auto c391 = goldsrc::PM_PLAYER_MAX_SAFE_FALL_SPEED;
    constexpr auto c392 = goldsrc::PM_VEC_HULL_MIN;
    constexpr auto c393 = goldsrc::PM_VEC_DUCK_HULL_MIN;
    constexpr auto c394 = goldsrc::PM_VEC_DUCK_VIEW;
    constexpr auto c395 = goldsrc::PM_VEC_VIEW;
    constexpr auto c396 = goldsrc::SVC_BAD;
    constexpr auto c397 = goldsrc::SVC_NOP;
    constexpr auto c398 = goldsrc::SVC_DISCONNECT;
    constexpr auto c399 = goldsrc::SVC_EVENT;
    constexpr auto c400 = goldsrc::SVC_VERSION;
    constexpr auto c401 = goldsrc::SVC_SETVIEW;
    constexpr auto c402 = goldsrc::SVC_SOUND;
    constexpr auto c403 = goldsrc::SVC_TIME;
    constexpr auto c404 = goldsrc::SVC_PRINT;
    constexpr auto c405 = goldsrc::SVC_STUFFTEXT;
    constexpr auto c406 = goldsrc::SVC_SETANGLE;
    constexpr auto c407 = goldsrc::SVC_SERVERINFO;
    constexpr auto c408 = goldsrc::SVC_LIGHTSTYLE;
    constexpr auto c409 = goldsrc::SVC_UPDATEUSERINFO;
    constexpr auto c410 = goldsrc::SVC_DELTADESCRIPTION;
    constexpr auto c411 = goldsrc::SVC_CLIENTDATA;
    constexpr auto c412 = goldsrc::SVC_STOPSOUND;
    constexpr auto c413 = goldsrc::SVC_PINGS;
    constexpr auto c414 = goldsrc::SVC_PARTICLE;
    constexpr auto c415 = goldsrc::SVC_DAMAGE;
    constexpr auto c416 = goldsrc::SVC_SPAWNSTATIC;
    constexpr auto c417 = goldsrc::SVC_EVENT_RELIABLE;
    constexpr auto c418 = goldsrc::SVC_SPAWNBASELINE;
    constexpr auto c419 = goldsrc::SVC_TEMPENTITY;
    constexpr auto c420 = goldsrc::SVC_SETPAUSE;
    constexpr auto c421 = goldsrc::SVC_SIGNONNUM;
    constexpr auto c422 = goldsrc::SVC_CENTERPRINT;
    constexpr auto c423 = goldsrc::SVC_KILLEDMONSTER;
    constexpr auto c424 = goldsrc::SVC_FOUNDSECRET;
    constexpr auto c425 = goldsrc::SVC_SPAWNSTATICSOUND;
    constexpr auto c426 = goldsrc::SVC_INTERMISSION;
    constexpr auto c427 = goldsrc::SVC_FINALE;
    constexpr auto c428 = goldsrc::SVC_CDTRACK;
    constexpr auto c429 = goldsrc::SVC_RESTORE;
    constexpr auto c430 = goldsrc::SVC_CUTSCENE;
    constexpr auto c431 = goldsrc::SVC_WEAPONANIM;
    constexpr auto c432 = goldsrc::SVC_DECALNAME;
    constexpr auto c433 = goldsrc::SVC_ROOMTYPE;
    constexpr auto c434 = goldsrc::SVC_ADDANGLE;
    constexpr auto c435 = goldsrc::SVC_NEWUSERMSG;
    constexpr auto c436 = goldsrc::SVC_PACKETENTITIES;
    constexpr auto c437 = goldsrc::SVC_DELTAPACKETENTITIES;
    constexpr auto c438 = goldsrc::SVC_CHOKE;
    constexpr auto c439 = goldsrc::SVC_RESOURCELIST;
    constexpr auto c440 = goldsrc::SVC_NEWMOVEVARS;
    constexpr auto c441 = goldsrc::SVC_RESOURCEREQUEST;
    constexpr auto c442 = goldsrc::SVC_CUSTOMIZATION;
    constexpr auto c443 = goldsrc::SVC_CROSSHAIRANGLE;
    constexpr auto c444 = goldsrc::SVC_SOUNDFADE;
    constexpr auto c445 = goldsrc::SVC_FILETXFERFAILED;
    constexpr auto c446 = goldsrc::SVC_HLTV;
    constexpr auto c447 = goldsrc::SVC_DIRECTOR;
    constexpr auto c448 = goldsrc::SVC_VOICEINIT;
    constexpr auto c449 = goldsrc::SVC_VOICEDATA;
    constexpr auto c450 = goldsrc::SVC_SENDEXTRAINFO;
    constexpr auto c451 = goldsrc::SVC_TIMESCALE;
    constexpr auto c452 = goldsrc::SVC_RESOURCELOCATION;
    constexpr auto c453 = goldsrc::SVC_SENDCVARVALUE;
    constexpr auto c454 = goldsrc::SVC_SENDCVARVALUE2;
    constexpr auto c455 = goldsrc::STUDIO_RENDER;
    constexpr auto c456 = goldsrc::STUDIO_EVENTS;
    constexpr auto c457 = goldsrc::TRI_TRIANGLES;
    constexpr auto c458 = goldsrc::TRI_API_VERSION;
    constexpr auto c459 = goldsrc::TRI_TRIANGLE_FAN;
    constexpr auto c460 = goldsrc::TRI_QUADS;
    constexpr auto c461 = goldsrc::TRI_POLYGON;
    constexpr auto c462 = goldsrc::TRI_LINES;
    constexpr auto c463 = goldsrc::TRI_TRIANGLE_STRIP;
    constexpr auto c464 = goldsrc::TRI_QUAD_STRIP;
    constexpr auto c465 = goldsrc::cstrike::MAX_PLAYERS;
    constexpr auto c466 = goldsrc::cstrike::MAX_TEAMS;
    constexpr auto c467 = goldsrc::cstrike::MAX_TEAM_NAME;
    constexpr auto c468 = goldsrc::cstrike::MAX_LOCATION_NAME;
    constexpr auto c469 = goldsrc::cstrike::MAX_HOSTAGES;
    constexpr auto c470 = goldsrc::cstrike::WEAPON_NONE;
    constexpr auto c471 = goldsrc::cstrike::WEAPON_P228;
    constexpr auto c472 = goldsrc::cstrike::WEAPON_GLOCK;
    constexpr auto c473 = goldsrc::cstrike::WEAPON_SCOUT;
    constexpr auto c474 = goldsrc::cstrike::WEAPON_HEGRENADE;
    constexpr auto c475 = goldsrc::cstrike::WEAPON_XM1014;
    constexpr auto c476 = goldsrc::cstrike::WEAPON_C4;
    constexpr auto c477 = goldsrc::cstrike::WEAPON_MAC10;
    constexpr auto c478 = goldsrc::cstrike::WEAPON_AUG;
    constexpr auto c479 = goldsrc::cstrike::WEAPON_SMOKEGRENADE;
    constexpr auto c480 = goldsrc::cstrike::WEAPON_ELITE;
    constexpr auto c481 = goldsrc::cstrike::WEAPON_FIVESEVEN;
    constexpr auto c482 = goldsrc::cstrike::WEAPON_UMP45;
    constexpr auto c483 = goldsrc::cstrike::WEAPON_SG550;
    constexpr auto c484 = goldsrc::cstrike::WEAPON_GALIL;
    constexpr auto c485 = goldsrc::cstrike::WEAPON_FAMAS;
    constexpr auto c486 = goldsrc::cstrike::WEAPON_USP;
    constexpr auto c487 = goldsrc::cstrike::WEAPON_GLOCK18;
    constexpr auto c488 = goldsrc::cstrike::WEAPON_AWP;
    constexpr auto c489 = goldsrc::cstrike::WEAPON_MP5N;
    constexpr auto c490 = goldsrc::cstrike::WEAPON_M249;
    constexpr auto c491 = goldsrc::cstrike::WEAPON_M3;
    constexpr auto c492 = goldsrc::cstrike::WEAPON_M4A1;
    constexpr auto c493 = goldsrc::cstrike::WEAPON_TMP;
    constexpr auto c494 = goldsrc::cstrike::WEAPON_G3SG1;
    constexpr auto c495 = goldsrc::cstrike::WEAPON_FLASHBANG;
    constexpr auto c496 = goldsrc::cstrike::WEAPON_DEAGLE;
    constexpr auto c497 = goldsrc::cstrike::WEAPON_SG552;
    constexpr auto c498 = goldsrc::cstrike::WEAPON_AK47;
    constexpr auto c499 = goldsrc::cstrike::WEAPON_KNIFE;
    constexpr auto c500 = goldsrc::cstrike::WEAPON_P90;
    constexpr auto c501 = goldsrc::cstrike::WEAPON_SHIELDGUN;
    constexpr auto c502 = goldsrc::cstrike::WPNSTATE_USP_SILENCED;
    constexpr auto c503 = goldsrc::cstrike::WPNSTATE_GLOCK18_BURST_MODE;
    constexpr auto c504 = goldsrc::cstrike::WPNSTATE_M4A1_SILENCED;
    constexpr auto c505 = goldsrc::cstrike::WPNSTATE_ELITE_LEFT;
    constexpr auto c506 = goldsrc::cstrike::WPNSTATE_FAMAS_BURST_MODE;
    constexpr auto c507 = goldsrc::cstrike::WPNSTATE_SHIELD_DRAWN;
    constexpr auto c508 = goldsrc::cstrike::ELITE_IDLE;
    constexpr auto c509 = goldsrc::cstrike::ELITE_IDLE_LEFTEMPTY;
    constexpr auto c510 = goldsrc::cstrike::ELITE_SHOOTLEFT1;
    constexpr auto c511 = goldsrc::cstrike::ELITE_SHOOTLEFT2;
    constexpr auto c512 = goldsrc::cstrike::ELITE_SHOOTLEFT3;
    constexpr auto c513 = goldsrc::cstrike::ELITE_SHOOTLEFT4;
    constexpr auto c514 = goldsrc::cstrike::ELITE_SHOOTLEFT5;
    constexpr auto c515 = goldsrc::cstrike::ELITE_SHOOTLEFTLAST;
    constexpr auto c516 = goldsrc::cstrike::ELITE_SHOOTRIGHT1;
    constexpr auto c517 = goldsrc::cstrike::ELITE_SHOOTRIGHT2;
    constexpr auto c518 = goldsrc::cstrike::ELITE_SHOOTRIGHT3;
    constexpr auto c519 = goldsrc::cstrike::ELITE_SHOOTRIGHT4;
    constexpr auto c520 = goldsrc::cstrike::ELITE_SHOOTRIGHT5;
    constexpr auto c521 = goldsrc::cstrike::ELITE_SHOOTRIGHTLAST;
    constexpr auto c522 = goldsrc::cstrike::ELITE_RELOAD;
    constexpr auto c523 = goldsrc::cstrike::ELITE_DRAW;
    constexpr auto c524 = goldsrc::cstrike::GLOCK18_IDLE1;
    constexpr auto c525 = goldsrc::cstrike::GLOCK18_IDLE2;
    constexpr auto c526 = goldsrc::cstrike::GLOCK18_IDLE3;
    constexpr auto c527 = goldsrc::cstrike::GLOCK18_SHOOT;
    constexpr auto c528 = goldsrc::cstrike::GLOCK18_SHOOT2;
    constexpr auto c529 = goldsrc::cstrike::GLOCK18_SHOOT3;
    constexpr auto c530 = goldsrc::cstrike::GLOCK18_SHOOT_EMPTY;
    constexpr auto c531 = goldsrc::cstrike::GLOCK18_RELOAD;
    constexpr auto c532 = goldsrc::cstrike::GLOCK18_DRAW;
    constexpr auto c533 = goldsrc::cstrike::GLOCK18_HOLSTER;
    constexpr auto c534 = goldsrc::cstrike::GLOCK18_ADD_SILENCER;
    constexpr auto c535 = goldsrc::cstrike::GLOCK18_DRAW2;
    constexpr auto c536 = goldsrc::cstrike::GLOCK18_RELOAD2;
    constexpr auto c537 = goldsrc::cstrike::M4A1_IDLE;
    constexpr auto c538 = goldsrc::cstrike::M4A1_SHOOT1;
    constexpr auto c539 = goldsrc::cstrike::M4A1_SHOOT2;
    constexpr auto c540 = goldsrc::cstrike::M4A1_SHOOT3;
    constexpr auto c541 = goldsrc::cstrike::M4A1_RELOAD;
    constexpr auto c542 = goldsrc::cstrike::M4A1_DRAW;
    constexpr auto c543 = goldsrc::cstrike::M4A1_ATTACH_SILENCER;
    constexpr auto c544 = goldsrc::cstrike::M4A1_UNSIL_IDLE;
    constexpr auto c545 = goldsrc::cstrike::M4A1_UNSIL_SHOOT1;
    constexpr auto c546 = goldsrc::cstrike::M4A1_UNSIL_SHOOT2;
    constexpr auto c547 = goldsrc::cstrike::M4A1_UNSIL_SHOOT3;
    constexpr auto c548 = goldsrc::cstrike::M4A1_UNSIL_RELOAD;
    constexpr auto c549 = goldsrc::cstrike::M4A1_UNSIL_DRAW;
    constexpr auto c550 = goldsrc::cstrike::M4A1_DETACH_SILENCER;
    constexpr auto c551 = goldsrc::cstrike::USP_IDLE;
    constexpr auto c552 = goldsrc::cstrike::USP_SHOOT1;
    constexpr auto c553 = goldsrc::cstrike::USP_SHOOT2;
    constexpr auto c554 = goldsrc::cstrike::USP_SHOOT3;
    constexpr auto c555 = goldsrc::cstrike::USP_SHOOT_EMPTY;
    constexpr auto c556 = goldsrc::cstrike::USP_RELOAD;
    constexpr auto c557 = goldsrc::cstrike::USP_DRAW;
    constexpr auto c558 = goldsrc::cstrike::USP_ATTACH_SILENCER;
    constexpr auto c559 = goldsrc::cstrike::USP_UNSIL_IDLE;
    constexpr auto c560 = goldsrc::cstrike::USP_UNSIL_SHOOT1;
    constexpr auto c561 = goldsrc::cstrike::USP_UNSIL_SHOOT2;
    constexpr auto c562 = goldsrc::cstrike::USP_UNSIL_SHOOT3;
    constexpr auto c563 = goldsrc::cstrike::USP_UNSIL_SHOOT_EMPTY;
    constexpr auto c564 = goldsrc::cstrike::USP_UNSIL_RELOAD;
    constexpr auto c565 = goldsrc::cstrike::USP_UNSIL_DRAW;
    constexpr auto c566 = goldsrc::cstrike::USP_DETACH_SILENCER;
    constexpr auto c567 = goldsrc::CAREER_NONE;
    constexpr auto c568 = goldsrc::CAREER_LOADING;
    constexpr auto c569 = goldsrc::CAREER_PLAYING;
    constexpr auto c570 = goldsrc::ca_dedicated;
    constexpr auto c571 = goldsrc::ca_disconnected;
    constexpr auto c572 = goldsrc::ca_connecting;
    constexpr auto c573 = goldsrc::ca_connected;
    constexpr auto c574 = goldsrc::ca_uninitialized;
    constexpr auto c575 = goldsrc::ca_active;
    constexpr auto c576 = goldsrc::src_client;
    constexpr auto c577 = goldsrc::src_command;
    constexpr auto c578 = goldsrc::mod_bad;
    constexpr auto c579 = goldsrc::mod_brush;
    constexpr auto c580 = goldsrc::mod_sprite;
    constexpr auto c581 = goldsrc::mod_alias;
    constexpr auto c582 = goldsrc::mod_studio;
    constexpr auto c583 = goldsrc::clc_bad;
    constexpr auto c584 = goldsrc::clc_nop;
    constexpr auto c585 = goldsrc::clc_move;
    constexpr auto c586 = goldsrc::clc_stringcmd;
    constexpr auto c587 = goldsrc::clc_delta;
    constexpr auto c588 = goldsrc::clc_resourcelist;
    constexpr auto c589 = goldsrc::clc_tmove;
    constexpr auto c590 = goldsrc::clc_fileconsistency;
    constexpr auto c591 = goldsrc::clc_voicedata;
    constexpr auto c592 = goldsrc::clc_hltv;
    constexpr auto c593 = goldsrc::clc_cvarvalue;
    constexpr auto c594 = goldsrc::clc_cvarvalue2;
    constexpr auto c595 = goldsrc::clc_endoflist;
    constexpr auto c596 = goldsrc::NA_UNUSED;
    constexpr auto c597 = goldsrc::NA_LOOPBACK;
    constexpr auto c598 = goldsrc::NA_BROADCAST;
    constexpr auto c599 = goldsrc::NA_IP;
    constexpr auto c600 = goldsrc::NA_IPX;
    constexpr auto c601 = goldsrc::NA_BROADCAST_IPX;
    constexpr auto c602 = goldsrc::NS_CLIENT;
    constexpr auto c603 = goldsrc::NS_SERVER;
    constexpr auto c604 = goldsrc::NS_MULTICAST;
    constexpr auto c605 = goldsrc::NS_MAX;
    constexpr auto c606 = goldsrc::FRAG_NORMAL_STREAM;
    constexpr auto c607 = goldsrc::FRAG_FILE_STREAM;
    constexpr auto c608 = goldsrc::MAX_STREAMS;
    constexpr auto c609 = goldsrc::t_sound;
    constexpr auto c610 = goldsrc::t_skin;
    constexpr auto c611 = goldsrc::t_model;
    constexpr auto c612 = goldsrc::t_decal;
    constexpr auto c613 = goldsrc::t_generic;
    constexpr auto c614 = goldsrc::t_eventscript;
    constexpr auto c615 = goldsrc::t_world;
    constexpr auto c616 = goldsrc::rt_unk;
    constexpr auto c617 = goldsrc::rt_max;
    constexpr auto c618 = goldsrc::VT_None;
    constexpr auto c619 = goldsrc::VT_Software;
    constexpr auto c620 = goldsrc::VT_OpenGL;
    constexpr auto c621 = goldsrc::VT_Direct3D;
    constexpr auto c622 = goldsrc::kRenderNormal;
    constexpr auto c623 = goldsrc::kRenderTransColor;
    constexpr auto c624 = goldsrc::kRenderTransTexture;
    constexpr auto c625 = goldsrc::kRenderGlow;
    constexpr auto c626 = goldsrc::kRenderTransAlpha;
    constexpr auto c627 = goldsrc::kRenderTransAdd;
    constexpr auto c628 = goldsrc::kRenderFxNone;
    constexpr auto c629 = goldsrc::kRenderFxPulseSlow;
    constexpr auto c630 = goldsrc::kRenderFxPulseFast;
    constexpr auto c631 = goldsrc::kRenderFxPulseSlowWide;
    constexpr auto c632 = goldsrc::kRenderFxPulseFastWide;
    constexpr auto c633 = goldsrc::kRenderFxFadeSlow;
    constexpr auto c634 = goldsrc::kRenderFxFadeFast;
    constexpr auto c635 = goldsrc::kRenderFxSolidSlow;
    constexpr auto c636 = goldsrc::kRenderFxSolidFast;
    constexpr auto c637 = goldsrc::kRenderFxStrobeSlow;
    constexpr auto c638 = goldsrc::kRenderFxStrobeFast;
    constexpr auto c639 = goldsrc::kRenderFxStrobeFaster;
    constexpr auto c640 = goldsrc::kRenderFxFlickerSlow;
    constexpr auto c641 = goldsrc::kRenderFxFlickerFast;
    constexpr auto c642 = goldsrc::kRenderFxNoDissipation;
    constexpr auto c643 = goldsrc::kRenderFxDistort;
    constexpr auto c644 = goldsrc::kRenderFxHologram;
    constexpr auto c645 = goldsrc::kRenderFxDeadPlayer;
    constexpr auto c646 = goldsrc::kRenderFxExplode;
    constexpr auto c647 = goldsrc::kRenderFxGlowShell;
    constexpr auto c648 = goldsrc::kRenderFxClampMinScale;
    constexpr auto c649 = goldsrc::kRenderFxLightMultiplier;
    constexpr auto c650 = goldsrc::SPR_SINGLE;
    constexpr auto c651 = goldsrc::SPR_GROUP;
    constexpr auto c652 = goldsrc::SPR_ANGLED;
    constexpr auto c653 = goldsrc::ST_SYNC;
    constexpr auto c654 = goldsrc::ST_RAND;
    constexpr auto c655 = goldsrc::key_game;
    constexpr auto c656 = goldsrc::key_message;
    constexpr auto c657 = goldsrc::key_menu;
    constexpr auto c658 = goldsrc::at_notice;
    constexpr auto c659 = goldsrc::at_console;
    constexpr auto c660 = goldsrc::at_aiconsole;
    constexpr auto c661 = goldsrc::at_warning;
    constexpr auto c662 = goldsrc::at_error;
    constexpr auto c663 = goldsrc::at_logged;
    constexpr auto c664 = goldsrc::force_exactfile;
    constexpr auto c665 = goldsrc::force_model_samebounds;
    constexpr auto c666 = goldsrc::force_model_specifybounds;
    constexpr auto c667 = goldsrc::force_model_specifybounds_if_avail;
    constexpr auto c668 = goldsrc::print_console;
    constexpr auto c669 = goldsrc::print_center;
    constexpr auto c670 = goldsrc::print_chat;
    constexpr auto c671 = goldsrc::TRI_FRONT;
    constexpr auto c672 = goldsrc::TRI_NONE;
    constexpr auto c673 = goldsrc::MicrophoneVolume;
    constexpr auto c674 = goldsrc::OtherSpeakerScale;
    constexpr auto c675 = goldsrc::MicBoost;
    constexpr auto c676 = goldsrc::cstrike::CHudAmmoSecondary::MAX_SEC_AMMO_VALUES;
    constexpr auto c677 = goldsrc::cstrike::CHudMessage::MESSAGE_ARG_LEN;
    constexpr auto c678 = goldsrc::cstrike::CHudMessage::MAX_MESSAGE_ARGS;
    constexpr auto c679 = goldsrc::cstrike::CHudStatusBar::MAX_STATUSTEXT_LENGTH;
    constexpr auto c680 = goldsrc::cstrike::CHudStatusBar::MAX_STATUSBAR_VALUES;
    constexpr auto c681 = goldsrc::cstrike::CHudStatusBar::MAX_STATUSBAR_LINES;
    constexpr auto c682 = goldsrc::cstrike::CHudStatusIcons::MAX_ICONSPRITENAME_LENGTH;
    constexpr auto c683 = goldsrc::cstrike::CHudStatusIcons::MAX_ICONSPRITES;
} // namespace goldsrc_equivalence

struct tagPOINT;
struct alight_s;
struct channel_t;
struct con_nprint_s;
struct delta_s;
struct demo_api_s;
struct dma_t;
struct net_api_s;
struct sfxcache_t;
struct wavstream_t;
struct lumpinfo_s;
struct qpic_t;
struct wadinfo_t;
#include "dep/ncl-hl1-source-sdk/dlls/extdll.h"
#include "dep/ncl-hl1-source-sdk/pm_shared/pm_defs.h"
#include "dep/ncl-hl1-source-sdk/engine/client.h"
#include "dep/ncl-hl1-source-sdk/common/vmodes.h"
#include "dep/ncl-hl1-source-sdk/common/event_api.h"
#include "dep/ncl-hl1-source-sdk/engine/studio.h"
#include "dep/ncl-hl1-source-sdk/dlls/weapontype.h"
#include "dep/ncl-hl1-source-sdk/dlls/cbase.h"
#include "include/nitroapi/modules/engine/structs.h"
#include "include/nitroapi/modules/engine/voice.h"
#include "dep/ncl-hl1-source-sdk/engine/server.h"
#include "dep/ncl-hl1-source-sdk/common/r_efx.h"
#include "dep/ncl-hl1-source-sdk/common/ref_params.h"
#include "dep/ncl-hl1-source-sdk/common/ref_int.h"
#include "dep/ncl-hl1-source-sdk/engine/APIProxy.h"
#include "dep/ncl-hl1-source-sdk/common/r_studioint.h"
#include "dep/ncl-hl1-source-sdk/engine/cdll_int.h"
#include "dep/ncl-hl1-source-sdk/engine/cmd.h"
#include "include/nitroapi/modules/client/cl_dll/hud.h"
#include "dep/ncl-hl1-source-sdk/HLTV/common/BitBuffer.h"
#include "dep/ncl-hl1-source-sdk/public/HLTV/IDemoPlayer.h"
#include "dep/ncl-hl1-source-sdk/common/quakedef.h"
#include "dep/ncl-hl1-source-sdk/engine/common.h"
#include "dep/ncl-hl1-source-sdk/common/cvardef.h"
#include "dep/ncl-hl1-source-sdk/common/kbutton.h"
#include "dep/ncl-hl1-source-sdk/engine/custom.h"
#include "dep/ncl-hl1-source-sdk/pm_shared/pm_movevars.h"
#include "dep/ncl-hl1-source-sdk/public/HLTV/ISystemModule.h"
#include "dep/ncl-hl1-source-sdk/public/HLTV/IEngineWrapper.h"
#include "dep/ncl-hl1-source-sdk/pm_shared/pm_shared.h"
#include "dep/ncl-hl1-source-sdk/common/triangleapi.h"
#include "dep/ncl-hl1-source-sdk/common/ivoicetweak.h"
#include "dep/ncl-hl1-source-sdk/HLTV/common/hltv.h"
#include "dep/ncl-hl1-source-sdk/engine/keydefs.h"
#include "dep/ncl-hl1-source-sdk/public/tier0/basetypes.h"
#include "dep/ncl-hl1-source-sdk/public/tier1/netadr.h"

namespace goldsrc_equivalence
{
    template <>
    struct map<::SCREENINFO_s>
    {
        using type = r0;
    };
    template <>
    struct map<::client_data_s>
    {
        using type = r1;
    };
    template <>
    struct map<::hud_player_info_s>
    {
        using type = r2;
    };
    template <>
    struct map<::cldll_func_t>
    {
        using type = r3;
    };
    template <>
    struct map<::modfuncs_s>
    {
        using type = r4;
    };
    template <>
    struct map<::cl_enginefunc_dst_t>
    {
        using type = r5;
    };
    template <>
    struct map<::cl_enginefuncs_s>
    {
        using type = r6;
    };
    template <>
    struct map<::CHud>
    {
        using type = r7;
    };
    template <>
    struct map<::CHudBase>
    {
        using type = r8;
    };
    template <>
    struct map<::CHudBase::POSITION>
    {
        using type = r9;
    };
    template <>
    struct map<::extra_player_info_t>
    {
        using type = r10;
    };
    template <>
    struct map<::CHudAmmo>
    {
        using type = r11;
    };
    template <>
    struct map<::CHudAmmo::RGBA>
    {
        using type = r12;
    };
    template <>
    struct map<::CHudAmmoSecondary>
    {
        using type = r13;
    };
    template <>
    struct map<::CHudMessage>
    {
        using type = r14;
    };
    template <>
    struct map<::CHudMessage::client_message_t>
    {
        using type = r15;
    };
    template <>
    struct map<::CHudNightVision>
    {
        using type = r16;
    };
    template <>
    struct map<::CHudRoundTimer>
    {
        using type = r17;
    };
    template <>
    struct map<::CHudStatusBar>
    {
        using type = r18;
    };
    template <>
    struct map<::CHudStatusIcons>
    {
        using type = r19;
    };
    template <>
    struct map<::CHudStatusIcons::icon_sprite_t>
    {
        using type = r20;
    };
    template <>
    struct map<::DAMAGE_IMAGE>
    {
        using type = r21;
    };
    template <>
    struct map<::CHudHealth>
    {
        using type = r22;
    };
    template <>
    struct map<::cvar_s>
    {
        using type = r23;
    };
    template <>
    struct map<::cmd_s>
    {
        using type = r24;
    };
    template <>
    struct map<::consistency_s>
    {
        using type = r25;
    };
    template <>
    struct map<::packet_entities_s>
    {
        using type = r26;
    };
    template <>
    struct map<::player_info_s>
    {
        using type = r27;
    };
    template <>
    struct map<::soundfade_s>
    {
        using type = r28;
    };
    template <>
    struct map<::client_static_s>
    {
        using type = r29;
    };
    template <>
    struct map<::frame_s>
    {
        using type = r30;
    };
    template <>
    struct map<::client_state_s>
    {
        using type = r31;
    };
    template <>
    struct map<::event_info_s>
    {
        using type = r32;
    };
    template <>
    struct map<::event_s>
    {
        using type = r33;
    };
    template <>
    struct map<::event_state_s>
    {
        using type = r34;
    };
    template <>
    struct map<::cacheentry_s>
    {
        using type = r35;
    };
    template <>
    struct map<::glpoly_s>
    {
        using type = r36;
    };
    template <>
    struct map<::hull_s>
    {
        using type = r37;
    };
    template <>
    struct map<::medge_s>
    {
        using type = r38;
    };
    template <>
    struct map<::mod_known_info_s>
    {
        using type = r39;
    };
    template <>
    struct map<::mplane_s>
    {
        using type = r40;
    };
    template <>
    struct map<::mspriteframe_s>
    {
        using type = r41;
    };
    template <>
    struct map<::mspriteframedesc_s>
    {
        using type = r42;
    };
    template <>
    struct map<::mspritegroup_s>
    {
        using type = r43;
    };
    template <>
    struct map<::msurface_s>
    {
        using type = r44;
    };
    template <>
    struct map<::mtexinfo_s>
    {
        using type = r45;
    };
    template <>
    struct map<::mvertex_s>
    {
        using type = r46;
    };
    template <>
    struct map<::skin_s>
    {
        using type = r47;
    };
    template <>
    struct map<::texture_s>
    {
        using type = r48;
    };
    template <>
    struct map<::cachewad_s>
    {
        using type = r49;
    };
    template <>
    struct map<::model_s>
    {
        using type = r50;
    };
    template <>
    struct map<::msprite_s>
    {
        using type = r51;
    };
    template <>
    struct map<::downloadtime_s>
    {
        using type = r52;
    };
    template <>
    struct map<::flowstats_s>
    {
        using type = r53;
    };
    template <>
    struct map<::sizebuf_s>
    {
        using type = r54;
    };
    template <>
    struct map<::flow_s>
    {
        using type = r55;
    };
    template <>
    struct map<::fragbuf_s>
    {
        using type = r56;
    };
    template <>
    struct map<::incomingtransfer_s>
    {
        using type = r57;
    };
    template <>
    struct map<::netadr_s>
    {
        using type = r58;
    };
    template <>
    struct map<::netchan_s>
    {
        using type = r59;
    };
    template <>
    struct map<::_resourceinfo_t>
    {
        using type = r60;
    };
    template <>
    struct map<::resource_s>
    {
        using type = r61;
    };
    template <>
    struct map<::resourceinfo_s>
    {
        using type = r62;
    };
    template <>
    struct map<::customization_s>
    {
        using type = r63;
    };
    template <>
    struct map<::server_static_s>
    {
        using type = r64;
    };
    template <>
    struct map<::server_s>
    {
        using type = r65;
    };
    template <>
    struct map<::sfx_t>
    {
        using type = r66;
    };
    template <>
    struct map<::nitroapi::EngineMsg>
    {
        using type = r67;
    };
    template <>
    struct map<::nitroapi::transObjRef>
    {
        using type = r68;
    };
    template <>
    struct map<::nitroapi::viddef_s>
    {
        using type = r69;
    };
    template <>
    struct map<::clientdata_s>
    {
        using type = r70;
    };
    template <>
    struct map<::entity_state_s>
    {
        using type = r71;
    };
    template <>
    struct map<::latchedvars_t>
    {
        using type = r72;
    };
    template <>
    struct map<::mouth_t>
    {
        using type = r73;
    };
    template <>
    struct map<::position_history_t>
    {
        using type = r74;
    };
    template <>
    struct map<::weapon_data_s>
    {
        using type = r75;
    };
    template <>
    struct map<::cl_entity_s>
    {
        using type = r76;
    };
    template <>
    struct map<::local_state_s>
    {
        using type = r77;
    };
    template <>
    struct map<::event_api_s>
    {
        using type = r78;
    };
    template <>
    struct map<::event_args_s>
    {
        using type = r79;
    };
    template <>
    struct map<::dmodel_s>
    {
        using type = r80;
    };
    template <>
    struct map<::dspriteframe_s>
    {
        using type = r81;
    };
    template <>
    struct map<::dspritegroup_s>
    {
        using type = r82;
    };
    template <>
    struct map<::dspriteinterval_s>
    {
        using type = r83;
    };
    template <>
    struct map<::dsprite_s>
    {
        using type = r84;
    };
    template <>
    struct map<::dspriteframetype_s>
    {
        using type = r85;
    };
    template <>
    struct map<::mstudioanim_t>
    {
        using type = r86;
    };
    template <>
    struct map<::mstudioanimvalue_t>
    {
        using type = r87;
    };
    template <>
    struct map<::mstudioattachment_t>
    {
        using type = r88;
    };
    template <>
    struct map<::mstudiobodyparts_t>
    {
        using type = r89;
    };
    template <>
    struct map<::mstudiobone_t>
    {
        using type = r90;
    };
    template <>
    struct map<::mstudioevent_s>
    {
        using type = r91;
    };
    template <>
    struct map<::mstudioseqdesc_t>
    {
        using type = r92;
    };
    template <>
    struct map<::mstudiotexture_t>
    {
        using type = r93;
    };
    template <>
    struct map<::studiohdr_t>
    {
        using type = r94;
    };
    template <>
    struct map<::lumpinfo_s>
    {
        using type = r95;
    };
    template <>
    struct map<::qpic_t>
    {
        using type = r96;
    };
    template <>
    struct map<::wadinfo_t>
    {
        using type = r97;
    };
    template <>
    struct map<::movevars_s>
    {
        using type = r98;
    };
    template <>
    struct map<::physent_s>
    {
        using type = r99;
    };
    template <>
    struct map<::pmplane_t>
    {
        using type = r100;
    };
    template <>
    struct map<::pmtrace_s>
    {
        using type = r101;
    };
    template <>
    struct map<::playermove_s>
    {
        using type = r102;
    };
    template <>
    struct map<::enginefuncs_s>
    {
        using type = r103;
    };
    template <>
    struct map<::engine_studio_api_s>
    {
        using type = r104;
    };
    template <>
    struct map<::r_studio_interface_s>
    {
        using type = r105;
    };
    template <>
    struct map<::triangleapi_s>
    {
        using type = r106;
    };
    template <>
    struct map<::cache_user_s>
    {
        using type = r107;
    };
    template <>
    struct map<::color24>
    {
        using type = r108;
    };
    template <>
    struct map<::colorVec>
    {
        using type = r109;
    };
    template <>
    struct map<::vrect_t>
    {
        using type = r110;
    };
    template <>
    struct map<::rect_s>
    {
        using type = r111;
    };
    template <>
    struct map<::kbutton_s>
    {
        using type = r112;
    };
    template <>
    struct map<::usercmd_s>
    {
        using type = r113;
    };
    template <>
    struct map<::ref_params_s>
    {
        using type = r114;
    };
    template <>
    struct map<::refdef_s>
    {
        using type = r115;
    };
    template <>
    struct map<::screenfade_s>
    {
        using type = r116;
    };
    template <>
    struct map<::IVoiceTweak_s>
    {
        using type = r117;
    };
    template <>
    struct map<::CHudAccountBalance>
    {
        using type = r118;
    };
    template <>
    struct map<::CHudBattery>
    {
        using type = r119;
    };
    template <>
    struct map<::CHudCareerTaskTimer>
    {
        using type = r120;
    };
    template <>
    struct map<::CHudDeathNotice>
    {
        using type = r121;
    };
    template <>
    struct map<::CHudFlashlight>
    {
        using type = r122;
    };
    template <>
    struct map<::CHudGeiger>
    {
        using type = r123;
    };
    template <>
    struct map<::CHudMenu>
    {
        using type = r124;
    };
    template <>
    struct map<::CHudProgressBar>
    {
        using type = r125;
    };
    template <>
    struct map<::CHudSayText>
    {
        using type = r126;
    };
    template <>
    struct map<::CHudScenarioStatus>
    {
        using type = r127;
    };
    template <>
    struct map<::CHudSniperScope>
    {
        using type = r128;
    };
    template <>
    struct map<::CHudSpectator>
    {
        using type = r129;
    };
    template <>
    struct map<::CHudTextMessage>
    {
        using type = r130;
    };
    template <>
    struct map<::CHudTrain>
    {
        using type = r131;
    };
    template <>
    struct map<::CHudVGUI2Print>
    {
        using type = r132;
    };
    template <>
    struct map<::CInterpolation>
    {
        using type = r133;
    };
    template <>
    struct map<::HUDLIST>
    {
        using type = r134;
    };
    template <>
    struct map<::TraceResult>
    {
        using type = r135;
    };
    template <>
    struct map<::WEAPON>
    {
        using type = r136;
    };
    template <>
    struct map<::cameraWayPoint_s>
    {
        using type = r137;
    };
    template <>
    struct map<::cldll_func_dst_t>
    {
        using type = r138;
    };
    template <>
    struct map<::client_s>
    {
        using type = r139;
    };
    template <>
    struct map<::client_sprite_s>
    {
        using type = r140;
    };
    template <>
    struct map<::client_textmessage_s>
    {
        using type = r141;
    };
    template <>
    struct map<::cmdalias_s>
    {
        using type = r142;
    };
    template <>
    struct map<::dclipnode_s>
    {
        using type = r143;
    };
    template <>
    struct map<::dlight_s>
    {
        using type = r144;
    };
    template <>
    struct map<::edict_s>
    {
        using type = r145;
    };
    template <>
    struct map<::efrag_s>
    {
        using type = r146;
    };
    template <>
    struct map<::efx_api_s>
    {
        using type = r147;
    };
    template <>
    struct map<::engdata_s>
    {
        using type = r148;
    };
    template <>
    struct map<::entvars_s>
    {
        using type = r149;
    };
    template <>
    struct map<::fragbufwaiting_s>
    {
        using type = r150;
    };
    template <>
    struct map<::hostage_info_t>
    {
        using type = r151;
    };
    template <>
    struct map<::message_parms_t>
    {
        using type = r152;
    };
    template <>
    struct map<::mleaf_s>
    {
        using type = r153;
    };
    template <>
    struct map<::mnode_s>
    {
        using type = r154;
    };
    template <>
    struct map<::modchelpers_s>
    {
        using type = r155;
    };
    template <>
    struct map<::modshelpers_s>
    {
        using type = r156;
    };
    template <>
    struct map<::nitroapi::UserMsg>
    {
        using type = r157;
    };
    template <>
    struct map<::nitroapi::VoiceRecordInterface>
    {
        using type = r158;
    };
    template <>
    struct map<::overviewEntity_s>
    {
        using type = r159;
    };
    template <>
    struct map<::overviewInfo_s>
    {
        using type = r160;
    };
    template <>
    struct map<::sentenceEntry_>
    {
        using type = r161;
    };
    template <>
    struct map<::sequenceEntry_>
    {
        using type = r162;
    };
    template <>
    struct map<::tempent_s>
    {
        using type = r163;
    };
    template <>
    struct map<::trace_s>
    {
        using type = r164;
    };
    template <>
    struct map<::WeaponIdType>
    {
        using type = r165;
    };
    template <>
    struct map<::WeaponState>
    {
        using type = r166;
    };
    template <>
    struct map<::elite_e>
    {
        using type = r167;
    };
    template <>
    struct map<::glock18_e>
    {
        using type = r168;
    };
    template <>
    struct map<::m4a1_e>
    {
        using type = r169;
    };
    template <>
    struct map<::usp_e>
    {
        using type = r170;
    };
    template <>
    struct map<::CareerStateType_e>
    {
        using type = r171;
    };
    template <>
    struct map<::cactive_e>
    {
        using type = r172;
    };
    template <>
    struct map<::cmd_source_s>
    {
        using type = r173;
    };
    template <>
    struct map<::modtype_e>
    {
        using type = r174;
    };
    template <>
    struct map<::clc_commands_e>
    {
        using type = r175;
    };
    template <>
    struct map<::netadrtype_t>
    {
        using type = r176;
    };
    template <>
    struct map<::netsrc_s>
    {
        using type = r177;
    };
    template <>
    struct map<::resourcetype_t>
    {
        using type = r178;
    };
    template <>
    struct map<::nitroapi::VidTypes>
    {
        using type = r179;
    };
    template <>
    struct map<::spriteframetype_e>
    {
        using type = r180;
    };
    template <>
    struct map<::synctype_e>
    {
        using type = r181;
    };
    template <>
    struct map<::keydest_t>
    {
        using type = r182;
    };
    template <>
    struct map<::ALERT_TYPE>
    {
        using type = r183;
    };
    template <>
    struct map<::FORCE_TYPE>
    {
        using type = r184;
    };
    template <>
    struct map<::PRINT_TYPE>
    {
        using type = r185;
    };
    template <>
    struct map<::TRICULLSTYLE>
    {
        using type = r186;
    };
    template <>
    struct map<::VoiceTweakControl>
    {
        using type = r187;
    };
    template <>
    struct map<::tagPOINT>
    {
        using type = r188;
    };
    template <>
    struct map<::alight_s>
    {
        using type = r189;
    };
    template <>
    struct map<::channel_t>
    {
        using type = r190;
    };
    template <>
    struct map<::con_nprint_s>
    {
        using type = r191;
    };
    template <>
    struct map<::delta_s>
    {
        using type = r192;
    };
    template <>
    struct map<::demo_api_s>
    {
        using type = r193;
    };
    template <>
    struct map<::dma_t>
    {
        using type = r194;
    };
    template <>
    struct map<::net_api_s>
    {
        using type = r195;
    };
    template <>
    struct map<::sfxcache_t>
    {
        using type = r196;
    };
    template <>
    struct map<::wavstream_t>
    {
        using type = r197;
    };

    static_assert(s0 == sizeof(::SCREENINFO_s), "sizeof SCREENINFO_s");
    static_assert(o0 == offsetof(::SCREENINFO_s, iSize), "offsetof SCREENINFO_s.iSize");
    static_assert(std::is_same_v<t0, map_t<decltype(::SCREENINFO_s::iSize)>>, "type SCREENINFO_s.iSize");
    static_assert(o1 == offsetof(::SCREENINFO_s, iWidth), "offsetof SCREENINFO_s.iWidth");
    static_assert(std::is_same_v<t1, map_t<decltype(::SCREENINFO_s::iWidth)>>, "type SCREENINFO_s.iWidth");
    static_assert(o2 == offsetof(::SCREENINFO_s, iHeight), "offsetof SCREENINFO_s.iHeight");
    static_assert(std::is_same_v<t2, map_t<decltype(::SCREENINFO_s::iHeight)>>, "type SCREENINFO_s.iHeight");
    static_assert(o3 == offsetof(::SCREENINFO_s, iFlags), "offsetof SCREENINFO_s.iFlags");
    static_assert(std::is_same_v<t3, map_t<decltype(::SCREENINFO_s::iFlags)>>, "type SCREENINFO_s.iFlags");
    static_assert(o4 == offsetof(::SCREENINFO_s, iCharHeight), "offsetof SCREENINFO_s.iCharHeight");
    static_assert(std::is_same_v<t4, map_t<decltype(::SCREENINFO_s::iCharHeight)>>, "type SCREENINFO_s.iCharHeight");
    static_assert(o5 == offsetof(::SCREENINFO_s, charWidths), "offsetof SCREENINFO_s.charWidths");
    static_assert(std::is_same_v<t5, map_t<decltype(::SCREENINFO_s::charWidths)>>, "type SCREENINFO_s.charWidths");
    static_assert(s1 == sizeof(::client_data_s), "sizeof client_data_s");
    static_assert(o6 == offsetof(::client_data_s, origin), "offsetof client_data_s.origin");
    static_assert(std::is_same_v<t6, map_t<decltype(::client_data_s::origin)>>, "type client_data_s.origin");
    static_assert(o7 == offsetof(::client_data_s, viewangles), "offsetof client_data_s.viewangles");
    static_assert(std::is_same_v<t7, map_t<decltype(::client_data_s::viewangles)>>, "type client_data_s.viewangles");
    static_assert(o8 == offsetof(::client_data_s, iWeaponBits), "offsetof client_data_s.iWeaponBits");
    static_assert(std::is_same_v<t8, map_t<decltype(::client_data_s::iWeaponBits)>>, "type client_data_s.iWeaponBits");
    static_assert(o9 == offsetof(::client_data_s, fov), "offsetof client_data_s.fov");
    static_assert(std::is_same_v<t9, map_t<decltype(::client_data_s::fov)>>, "type client_data_s.fov");
    static_assert(s2 == sizeof(::hud_player_info_s), "sizeof hud_player_info_s");
    static_assert(o10 == offsetof(::hud_player_info_s, name), "offsetof hud_player_info_s.name");
    static_assert(std::is_same_v<t10, map_t<decltype(::hud_player_info_s::name)>>, "type hud_player_info_s.name");
    static_assert(o11 == offsetof(::hud_player_info_s, ping), "offsetof hud_player_info_s.ping");
    static_assert(std::is_same_v<t11, map_t<decltype(::hud_player_info_s::ping)>>, "type hud_player_info_s.ping");
    static_assert(o12 == offsetof(::hud_player_info_s, thisplayer), "offsetof hud_player_info_s.thisplayer");
    static_assert(std::is_same_v<t12, map_t<decltype(::hud_player_info_s::thisplayer)>>, "type hud_player_info_s.thisplayer");
    static_assert(o13 == offsetof(::hud_player_info_s, spectator), "offsetof hud_player_info_s.spectator");
    static_assert(std::is_same_v<t13, map_t<decltype(::hud_player_info_s::spectator)>>, "type hud_player_info_s.spectator");
    static_assert(o14 == offsetof(::hud_player_info_s, packetloss), "offsetof hud_player_info_s.packetloss");
    static_assert(std::is_same_v<t14, map_t<decltype(::hud_player_info_s::packetloss)>>, "type hud_player_info_s.packetloss");
    static_assert(o15 == offsetof(::hud_player_info_s, model), "offsetof hud_player_info_s.model");
    static_assert(std::is_same_v<t15, map_t<decltype(::hud_player_info_s::model)>>, "type hud_player_info_s.model");
    static_assert(o16 == offsetof(::hud_player_info_s, topcolor), "offsetof hud_player_info_s.topcolor");
    static_assert(std::is_same_v<t16, map_t<decltype(::hud_player_info_s::topcolor)>>, "type hud_player_info_s.topcolor");
    static_assert(o17 == offsetof(::hud_player_info_s, bottomcolor), "offsetof hud_player_info_s.bottomcolor");
    static_assert(std::is_same_v<t17, map_t<decltype(::hud_player_info_s::bottomcolor)>>, "type hud_player_info_s.bottomcolor");
    static_assert(o18 == offsetof(::hud_player_info_s, m_nSteamID), "offsetof hud_player_info_s.m_nSteamID");
    static_assert(std::is_same_v<t18, map_t<decltype(::hud_player_info_s::m_nSteamID)>>, "type hud_player_info_s.m_nSteamID");
    static_assert(s3 == sizeof(::cldll_func_t), "sizeof cldll_func_t");
    static_assert(o19 == offsetof(::cldll_func_t, pInitFunc), "offsetof cldll_func_t.pInitFunc");
    static_assert(std::is_same_v<t19, map_t<decltype(::cldll_func_t::pInitFunc)>>, "type cldll_func_t.pInitFunc");
    static_assert(o20 == offsetof(::cldll_func_t, pHudInitFunc), "offsetof cldll_func_t.pHudInitFunc");
    static_assert(std::is_same_v<t20, map_t<decltype(::cldll_func_t::pHudInitFunc)>>, "type cldll_func_t.pHudInitFunc");
    static_assert(o21 == offsetof(::cldll_func_t, pHudVidInitFunc), "offsetof cldll_func_t.pHudVidInitFunc");
    static_assert(std::is_same_v<t21, map_t<decltype(::cldll_func_t::pHudVidInitFunc)>>, "type cldll_func_t.pHudVidInitFunc");
    static_assert(o22 == offsetof(::cldll_func_t, pHudRedrawFunc), "offsetof cldll_func_t.pHudRedrawFunc");
    static_assert(std::is_same_v<t22, map_t<decltype(::cldll_func_t::pHudRedrawFunc)>>, "type cldll_func_t.pHudRedrawFunc");
    static_assert(o23 == offsetof(::cldll_func_t, pHudUpdateClientDataFunc), "offsetof cldll_func_t.pHudUpdateClientDataFunc");
    static_assert(
        std::is_same_v<t23, map_t<decltype(::cldll_func_t::pHudUpdateClientDataFunc)>>,
        "type cldll_func_t.pHudUpdateClientDataFunc"
    );
    static_assert(o24 == offsetof(::cldll_func_t, pHudResetFunc), "offsetof cldll_func_t.pHudResetFunc");
    static_assert(std::is_same_v<t24, map_t<decltype(::cldll_func_t::pHudResetFunc)>>, "type cldll_func_t.pHudResetFunc");
    static_assert(o25 == offsetof(::cldll_func_t, pClientMove), "offsetof cldll_func_t.pClientMove");
    static_assert(std::is_same_v<t25, map_t<decltype(::cldll_func_t::pClientMove)>>, "type cldll_func_t.pClientMove");
    static_assert(o26 == offsetof(::cldll_func_t, pClientMoveInit), "offsetof cldll_func_t.pClientMoveInit");
    static_assert(std::is_same_v<t26, map_t<decltype(::cldll_func_t::pClientMoveInit)>>, "type cldll_func_t.pClientMoveInit");
    static_assert(o27 == offsetof(::cldll_func_t, pClientTextureType), "offsetof cldll_func_t.pClientTextureType");
    static_assert(std::is_same_v<t27, map_t<decltype(::cldll_func_t::pClientTextureType)>>, "type cldll_func_t.pClientTextureType");
    static_assert(o28 == offsetof(::cldll_func_t, pIN_ActivateMouse), "offsetof cldll_func_t.pIN_ActivateMouse");
    static_assert(std::is_same_v<t28, map_t<decltype(::cldll_func_t::pIN_ActivateMouse)>>, "type cldll_func_t.pIN_ActivateMouse");
    static_assert(o29 == offsetof(::cldll_func_t, pIN_DeactivateMouse), "offsetof cldll_func_t.pIN_DeactivateMouse");
    static_assert(std::is_same_v<t29, map_t<decltype(::cldll_func_t::pIN_DeactivateMouse)>>, "type cldll_func_t.pIN_DeactivateMouse");
    static_assert(o30 == offsetof(::cldll_func_t, pIN_MouseEvent), "offsetof cldll_func_t.pIN_MouseEvent");
    static_assert(std::is_same_v<t30, map_t<decltype(::cldll_func_t::pIN_MouseEvent)>>, "type cldll_func_t.pIN_MouseEvent");
    static_assert(o31 == offsetof(::cldll_func_t, pIN_ClearStates), "offsetof cldll_func_t.pIN_ClearStates");
    static_assert(std::is_same_v<t31, map_t<decltype(::cldll_func_t::pIN_ClearStates)>>, "type cldll_func_t.pIN_ClearStates");
    static_assert(o32 == offsetof(::cldll_func_t, pIN_Accumulate), "offsetof cldll_func_t.pIN_Accumulate");
    static_assert(std::is_same_v<t32, map_t<decltype(::cldll_func_t::pIN_Accumulate)>>, "type cldll_func_t.pIN_Accumulate");
    static_assert(o33 == offsetof(::cldll_func_t, pCL_CreateMove), "offsetof cldll_func_t.pCL_CreateMove");
    static_assert(std::is_same_v<t33, map_t<decltype(::cldll_func_t::pCL_CreateMove)>>, "type cldll_func_t.pCL_CreateMove");
    static_assert(o34 == offsetof(::cldll_func_t, pCL_IsThirdPerson), "offsetof cldll_func_t.pCL_IsThirdPerson");
    static_assert(std::is_same_v<t34, map_t<decltype(::cldll_func_t::pCL_IsThirdPerson)>>, "type cldll_func_t.pCL_IsThirdPerson");
    static_assert(o35 == offsetof(::cldll_func_t, pCL_GetCameraOffsets), "offsetof cldll_func_t.pCL_GetCameraOffsets");
    static_assert(std::is_same_v<t35, map_t<decltype(::cldll_func_t::pCL_GetCameraOffsets)>>, "type cldll_func_t.pCL_GetCameraOffsets");
    static_assert(o36 == offsetof(::cldll_func_t, pFindKey), "offsetof cldll_func_t.pFindKey");
    static_assert(std::is_same_v<t36, map_t<decltype(::cldll_func_t::pFindKey)>>, "type cldll_func_t.pFindKey");
    static_assert(o37 == offsetof(::cldll_func_t, pCamThink), "offsetof cldll_func_t.pCamThink");
    static_assert(std::is_same_v<t37, map_t<decltype(::cldll_func_t::pCamThink)>>, "type cldll_func_t.pCamThink");
    static_assert(o38 == offsetof(::cldll_func_t, pCalcRefdef), "offsetof cldll_func_t.pCalcRefdef");
    static_assert(std::is_same_v<t38, map_t<decltype(::cldll_func_t::pCalcRefdef)>>, "type cldll_func_t.pCalcRefdef");
    static_assert(o39 == offsetof(::cldll_func_t, pAddEntity), "offsetof cldll_func_t.pAddEntity");
    static_assert(std::is_same_v<t39, map_t<decltype(::cldll_func_t::pAddEntity)>>, "type cldll_func_t.pAddEntity");
    static_assert(o40 == offsetof(::cldll_func_t, pCreateEntities), "offsetof cldll_func_t.pCreateEntities");
    static_assert(std::is_same_v<t40, map_t<decltype(::cldll_func_t::pCreateEntities)>>, "type cldll_func_t.pCreateEntities");
    static_assert(o41 == offsetof(::cldll_func_t, pDrawNormalTriangles), "offsetof cldll_func_t.pDrawNormalTriangles");
    static_assert(std::is_same_v<t41, map_t<decltype(::cldll_func_t::pDrawNormalTriangles)>>, "type cldll_func_t.pDrawNormalTriangles");
    static_assert(o42 == offsetof(::cldll_func_t, pDrawTransparentTriangles), "offsetof cldll_func_t.pDrawTransparentTriangles");
    static_assert(
        std::is_same_v<t42, map_t<decltype(::cldll_func_t::pDrawTransparentTriangles)>>,
        "type cldll_func_t.pDrawTransparentTriangles"
    );
    static_assert(o43 == offsetof(::cldll_func_t, pStudioEvent), "offsetof cldll_func_t.pStudioEvent");
    static_assert(std::is_same_v<t43, map_t<decltype(::cldll_func_t::pStudioEvent)>>, "type cldll_func_t.pStudioEvent");
    static_assert(o44 == offsetof(::cldll_func_t, pPostRunCmd), "offsetof cldll_func_t.pPostRunCmd");
    static_assert(std::is_same_v<t44, map_t<decltype(::cldll_func_t::pPostRunCmd)>>, "type cldll_func_t.pPostRunCmd");
    static_assert(o45 == offsetof(::cldll_func_t, pShutdown), "offsetof cldll_func_t.pShutdown");
    static_assert(std::is_same_v<t45, map_t<decltype(::cldll_func_t::pShutdown)>>, "type cldll_func_t.pShutdown");
    static_assert(o46 == offsetof(::cldll_func_t, pTxferLocalOverrides), "offsetof cldll_func_t.pTxferLocalOverrides");
    static_assert(std::is_same_v<t46, map_t<decltype(::cldll_func_t::pTxferLocalOverrides)>>, "type cldll_func_t.pTxferLocalOverrides");
    static_assert(o47 == offsetof(::cldll_func_t, pProcessPlayerState), "offsetof cldll_func_t.pProcessPlayerState");
    static_assert(std::is_same_v<t47, map_t<decltype(::cldll_func_t::pProcessPlayerState)>>, "type cldll_func_t.pProcessPlayerState");
    static_assert(o48 == offsetof(::cldll_func_t, pTxferPredictionData), "offsetof cldll_func_t.pTxferPredictionData");
    static_assert(std::is_same_v<t48, map_t<decltype(::cldll_func_t::pTxferPredictionData)>>, "type cldll_func_t.pTxferPredictionData");
    static_assert(o49 == offsetof(::cldll_func_t, pReadDemoBuffer), "offsetof cldll_func_t.pReadDemoBuffer");
    static_assert(std::is_same_v<t49, map_t<decltype(::cldll_func_t::pReadDemoBuffer)>>, "type cldll_func_t.pReadDemoBuffer");
    static_assert(o50 == offsetof(::cldll_func_t, pConnectionlessPacket), "offsetof cldll_func_t.pConnectionlessPacket");
    static_assert(std::is_same_v<t50, map_t<decltype(::cldll_func_t::pConnectionlessPacket)>>, "type cldll_func_t.pConnectionlessPacket");
    static_assert(o51 == offsetof(::cldll_func_t, pGetHullBounds), "offsetof cldll_func_t.pGetHullBounds");
    static_assert(std::is_same_v<t51, map_t<decltype(::cldll_func_t::pGetHullBounds)>>, "type cldll_func_t.pGetHullBounds");
    static_assert(o52 == offsetof(::cldll_func_t, pHudFrame), "offsetof cldll_func_t.pHudFrame");
    static_assert(std::is_same_v<t52, map_t<decltype(::cldll_func_t::pHudFrame)>>, "type cldll_func_t.pHudFrame");
    static_assert(o53 == offsetof(::cldll_func_t, pKeyEvent), "offsetof cldll_func_t.pKeyEvent");
    static_assert(std::is_same_v<t53, map_t<decltype(::cldll_func_t::pKeyEvent)>>, "type cldll_func_t.pKeyEvent");
    static_assert(o54 == offsetof(::cldll_func_t, pTempEntUpdate), "offsetof cldll_func_t.pTempEntUpdate");
    static_assert(std::is_same_v<t54, map_t<decltype(::cldll_func_t::pTempEntUpdate)>>, "type cldll_func_t.pTempEntUpdate");
    static_assert(o55 == offsetof(::cldll_func_t, pGetUserEntity), "offsetof cldll_func_t.pGetUserEntity");
    static_assert(std::is_same_v<t55, map_t<decltype(::cldll_func_t::pGetUserEntity)>>, "type cldll_func_t.pGetUserEntity");
    static_assert(o56 == offsetof(::cldll_func_t, pVoiceStatus), "offsetof cldll_func_t.pVoiceStatus");
    static_assert(std::is_same_v<t56, map_t<decltype(::cldll_func_t::pVoiceStatus)>>, "type cldll_func_t.pVoiceStatus");
    static_assert(o57 == offsetof(::cldll_func_t, pDirectorMessage), "offsetof cldll_func_t.pDirectorMessage");
    static_assert(std::is_same_v<t57, map_t<decltype(::cldll_func_t::pDirectorMessage)>>, "type cldll_func_t.pDirectorMessage");
    static_assert(o58 == offsetof(::cldll_func_t, pStudioInterface), "offsetof cldll_func_t.pStudioInterface");
    static_assert(std::is_same_v<t58, map_t<decltype(::cldll_func_t::pStudioInterface)>>, "type cldll_func_t.pStudioInterface");
    static_assert(o59 == offsetof(::cldll_func_t, pChatInputPosition), "offsetof cldll_func_t.pChatInputPosition");
    static_assert(std::is_same_v<t59, map_t<decltype(::cldll_func_t::pChatInputPosition)>>, "type cldll_func_t.pChatInputPosition");
    static_assert(o60 == offsetof(::cldll_func_t, pGetPlayerTeam), "offsetof cldll_func_t.pGetPlayerTeam");
    static_assert(std::is_same_v<t60, map_t<decltype(::cldll_func_t::pGetPlayerTeam)>>, "type cldll_func_t.pGetPlayerTeam");
    static_assert(o61 == offsetof(::cldll_func_t, pClientFactory), "offsetof cldll_func_t.pClientFactory");
    static_assert(std::is_same_v<t61, map_t<decltype(::cldll_func_t::pClientFactory)>>, "type cldll_func_t.pClientFactory");
    static_assert(s4 == sizeof(::modfuncs_s), "sizeof modfuncs_s");
    static_assert(o62 == offsetof(::modfuncs_s, m_pfnLoadMod), "offsetof modfuncs_s.m_pfnLoadMod");
    static_assert(std::is_same_v<t62, map_t<decltype(::modfuncs_s::m_pfnLoadMod)>>, "type modfuncs_s.m_pfnLoadMod");
    static_assert(o63 == offsetof(::modfuncs_s, m_pfnCloseMod), "offsetof modfuncs_s.m_pfnCloseMod");
    static_assert(std::is_same_v<t63, map_t<decltype(::modfuncs_s::m_pfnCloseMod)>>, "type modfuncs_s.m_pfnCloseMod");
    static_assert(o64 == offsetof(::modfuncs_s, m_pfnNCall), "offsetof modfuncs_s.m_pfnNCall");
    static_assert(std::is_same_v<t64, map_t<decltype(::modfuncs_s::m_pfnNCall)>>, "type modfuncs_s.m_pfnNCall");
    static_assert(o65 == offsetof(::modfuncs_s, m_pfnGetClDstAddrs), "offsetof modfuncs_s.m_pfnGetClDstAddrs");
    static_assert(std::is_same_v<t65, map_t<decltype(::modfuncs_s::m_pfnGetClDstAddrs)>>, "type modfuncs_s.m_pfnGetClDstAddrs");
    static_assert(o66 == offsetof(::modfuncs_s, m_pfnGetEngDstAddrs), "offsetof modfuncs_s.m_pfnGetEngDstAddrs");
    static_assert(std::is_same_v<t66, map_t<decltype(::modfuncs_s::m_pfnGetEngDstAddrs)>>, "type modfuncs_s.m_pfnGetEngDstAddrs");
    static_assert(o67 == offsetof(::modfuncs_s, m_pfnModuleLoaded), "offsetof modfuncs_s.m_pfnModuleLoaded");
    static_assert(std::is_same_v<t67, map_t<decltype(::modfuncs_s::m_pfnModuleLoaded)>>, "type modfuncs_s.m_pfnModuleLoaded");
    static_assert(o68 == offsetof(::modfuncs_s, m_pfnProcessOutgoingNet), "offsetof modfuncs_s.m_pfnProcessOutgoingNet");
    static_assert(std::is_same_v<t68, map_t<decltype(::modfuncs_s::m_pfnProcessOutgoingNet)>>, "type modfuncs_s.m_pfnProcessOutgoingNet");
    static_assert(o69 == offsetof(::modfuncs_s, m_pfnProcessIncomingNet), "offsetof modfuncs_s.m_pfnProcessIncomingNet");
    static_assert(std::is_same_v<t69, map_t<decltype(::modfuncs_s::m_pfnProcessIncomingNet)>>, "type modfuncs_s.m_pfnProcessIncomingNet");
    static_assert(o70 == offsetof(::modfuncs_s, m_pfnTextureLoad), "offsetof modfuncs_s.m_pfnTextureLoad");
    static_assert(std::is_same_v<t70, map_t<decltype(::modfuncs_s::m_pfnTextureLoad)>>, "type modfuncs_s.m_pfnTextureLoad");
    static_assert(o71 == offsetof(::modfuncs_s, m_pfnModelLoad), "offsetof modfuncs_s.m_pfnModelLoad");
    static_assert(std::is_same_v<t71, map_t<decltype(::modfuncs_s::m_pfnModelLoad)>>, "type modfuncs_s.m_pfnModelLoad");
    static_assert(o72 == offsetof(::modfuncs_s, m_pfnFrameBegin), "offsetof modfuncs_s.m_pfnFrameBegin");
    static_assert(std::is_same_v<t72, map_t<decltype(::modfuncs_s::m_pfnFrameBegin)>>, "type modfuncs_s.m_pfnFrameBegin");
    static_assert(o73 == offsetof(::modfuncs_s, m_pfnFrameRender1), "offsetof modfuncs_s.m_pfnFrameRender1");
    static_assert(std::is_same_v<t73, map_t<decltype(::modfuncs_s::m_pfnFrameRender1)>>, "type modfuncs_s.m_pfnFrameRender1");
    static_assert(o74 == offsetof(::modfuncs_s, m_pfnFrameRender2), "offsetof modfuncs_s.m_pfnFrameRender2");
    static_assert(std::is_same_v<t74, map_t<decltype(::modfuncs_s::m_pfnFrameRender2)>>, "type modfuncs_s.m_pfnFrameRender2");
    static_assert(o75 == offsetof(::modfuncs_s, m_pfnSetModSHelpers), "offsetof modfuncs_s.m_pfnSetModSHelpers");
    static_assert(std::is_same_v<t75, map_t<decltype(::modfuncs_s::m_pfnSetModSHelpers)>>, "type modfuncs_s.m_pfnSetModSHelpers");
    static_assert(o76 == offsetof(::modfuncs_s, m_pfnSetModCHelpers), "offsetof modfuncs_s.m_pfnSetModCHelpers");
    static_assert(std::is_same_v<t76, map_t<decltype(::modfuncs_s::m_pfnSetModCHelpers)>>, "type modfuncs_s.m_pfnSetModCHelpers");
    static_assert(o77 == offsetof(::modfuncs_s, m_pfnSetEngData), "offsetof modfuncs_s.m_pfnSetEngData");
    static_assert(std::is_same_v<t77, map_t<decltype(::modfuncs_s::m_pfnSetEngData)>>, "type modfuncs_s.m_pfnSetEngData");
    static_assert(o78 == offsetof(::modfuncs_s, m_nVersion), "offsetof modfuncs_s.m_nVersion");
    static_assert(std::is_same_v<t78, map_t<decltype(::modfuncs_s::m_nVersion)>>, "type modfuncs_s.m_nVersion");
    static_assert(o79 == offsetof(::modfuncs_s, m_pfnConnectClient), "offsetof modfuncs_s.m_pfnConnectClient");
    static_assert(std::is_same_v<t79, map_t<decltype(::modfuncs_s::m_pfnConnectClient)>>, "type modfuncs_s.m_pfnConnectClient");
    static_assert(o80 == offsetof(::modfuncs_s, m_pfnRecordIP), "offsetof modfuncs_s.m_pfnRecordIP");
    static_assert(std::is_same_v<t80, map_t<decltype(::modfuncs_s::m_pfnRecordIP)>>, "type modfuncs_s.m_pfnRecordIP");
    static_assert(o81 == offsetof(::modfuncs_s, m_pfnPlayerStatus), "offsetof modfuncs_s.m_pfnPlayerStatus");
    static_assert(std::is_same_v<t81, map_t<decltype(::modfuncs_s::m_pfnPlayerStatus)>>, "type modfuncs_s.m_pfnPlayerStatus");
    static_assert(o82 == offsetof(::modfuncs_s, m_pfnSetEngineVersion), "offsetof modfuncs_s.m_pfnSetEngineVersion");
    static_assert(std::is_same_v<t82, map_t<decltype(::modfuncs_s::m_pfnSetEngineVersion)>>, "type modfuncs_s.m_pfnSetEngineVersion");
    static_assert(o83 == offsetof(::modfuncs_s, m_nVoid2), "offsetof modfuncs_s.m_nVoid2");
    static_assert(std::is_same_v<t83, map_t<decltype(::modfuncs_s::m_nVoid2)>>, "type modfuncs_s.m_nVoid2");
    static_assert(o84 == offsetof(::modfuncs_s, m_nVoid3), "offsetof modfuncs_s.m_nVoid3");
    static_assert(std::is_same_v<t84, map_t<decltype(::modfuncs_s::m_nVoid3)>>, "type modfuncs_s.m_nVoid3");
    static_assert(o85 == offsetof(::modfuncs_s, m_nVoid4), "offsetof modfuncs_s.m_nVoid4");
    static_assert(std::is_same_v<t85, map_t<decltype(::modfuncs_s::m_nVoid4)>>, "type modfuncs_s.m_nVoid4");
    static_assert(o86 == offsetof(::modfuncs_s, m_nVoid5), "offsetof modfuncs_s.m_nVoid5");
    static_assert(std::is_same_v<t86, map_t<decltype(::modfuncs_s::m_nVoid5)>>, "type modfuncs_s.m_nVoid5");
    static_assert(o87 == offsetof(::modfuncs_s, m_nVoid6), "offsetof modfuncs_s.m_nVoid6");
    static_assert(std::is_same_v<t87, map_t<decltype(::modfuncs_s::m_nVoid6)>>, "type modfuncs_s.m_nVoid6");
    static_assert(o88 == offsetof(::modfuncs_s, m_nVoid7), "offsetof modfuncs_s.m_nVoid7");
    static_assert(std::is_same_v<t88, map_t<decltype(::modfuncs_s::m_nVoid7)>>, "type modfuncs_s.m_nVoid7");
    static_assert(o89 == offsetof(::modfuncs_s, m_nVoid8), "offsetof modfuncs_s.m_nVoid8");
    static_assert(std::is_same_v<t89, map_t<decltype(::modfuncs_s::m_nVoid8)>>, "type modfuncs_s.m_nVoid8");
    static_assert(o90 == offsetof(::modfuncs_s, m_nVoid9), "offsetof modfuncs_s.m_nVoid9");
    static_assert(std::is_same_v<t90, map_t<decltype(::modfuncs_s::m_nVoid9)>>, "type modfuncs_s.m_nVoid9");
    static_assert(s5 == sizeof(::cl_enginefunc_dst_t), "sizeof cl_enginefunc_dst_t");
    static_assert(o91 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Load), "offsetof cl_enginefunc_dst_t.pfnSPR_Load");
    static_assert(std::is_same_v<t91, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Load)>>, "type cl_enginefunc_dst_t.pfnSPR_Load");
    static_assert(o92 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Frames), "offsetof cl_enginefunc_dst_t.pfnSPR_Frames");
    static_assert(std::is_same_v<t92, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Frames)>>, "type cl_enginefunc_dst_t.pfnSPR_Frames");
    static_assert(o93 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Height), "offsetof cl_enginefunc_dst_t.pfnSPR_Height");
    static_assert(std::is_same_v<t93, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Height)>>, "type cl_enginefunc_dst_t.pfnSPR_Height");
    static_assert(o94 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Width), "offsetof cl_enginefunc_dst_t.pfnSPR_Width");
    static_assert(std::is_same_v<t94, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Width)>>, "type cl_enginefunc_dst_t.pfnSPR_Width");
    static_assert(o95 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Set), "offsetof cl_enginefunc_dst_t.pfnSPR_Set");
    static_assert(std::is_same_v<t95, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Set)>>, "type cl_enginefunc_dst_t.pfnSPR_Set");
    static_assert(o96 == offsetof(::cl_enginefunc_dst_t, pfnSPR_Draw), "offsetof cl_enginefunc_dst_t.pfnSPR_Draw");
    static_assert(std::is_same_v<t96, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_Draw)>>, "type cl_enginefunc_dst_t.pfnSPR_Draw");
    static_assert(o97 == offsetof(::cl_enginefunc_dst_t, pfnSPR_DrawHoles), "offsetof cl_enginefunc_dst_t.pfnSPR_DrawHoles");
    static_assert(
        std::is_same_v<t97, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_DrawHoles)>>,
        "type cl_enginefunc_dst_t.pfnSPR_DrawHoles"
    );
    static_assert(o98 == offsetof(::cl_enginefunc_dst_t, pfnSPR_DrawAdditive), "offsetof cl_enginefunc_dst_t.pfnSPR_DrawAdditive");
    static_assert(
        std::is_same_v<t98, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_DrawAdditive)>>,
        "type cl_enginefunc_dst_t.pfnSPR_DrawAdditive"
    );
    static_assert(o99 == offsetof(::cl_enginefunc_dst_t, pfnSPR_EnableScissor), "offsetof cl_enginefunc_dst_t.pfnSPR_EnableScissor");
    static_assert(
        std::is_same_v<t99, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_EnableScissor)>>,
        "type cl_enginefunc_dst_t.pfnSPR_EnableScissor"
    );
    static_assert(o100 == offsetof(::cl_enginefunc_dst_t, pfnSPR_DisableScissor), "offsetof cl_enginefunc_dst_t.pfnSPR_DisableScissor");
    static_assert(
        std::is_same_v<t100, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_DisableScissor)>>,
        "type cl_enginefunc_dst_t.pfnSPR_DisableScissor"
    );
    static_assert(o101 == offsetof(::cl_enginefunc_dst_t, pfnSPR_GetList), "offsetof cl_enginefunc_dst_t.pfnSPR_GetList");
    static_assert(std::is_same_v<t101, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_GetList)>>, "type cl_enginefunc_dst_t.pfnSPR_GetList");
    static_assert(o102 == offsetof(::cl_enginefunc_dst_t, pfnFillRGBA), "offsetof cl_enginefunc_dst_t.pfnFillRGBA");
    static_assert(std::is_same_v<t102, map_t<decltype(::cl_enginefunc_dst_t::pfnFillRGBA)>>, "type cl_enginefunc_dst_t.pfnFillRGBA");
    static_assert(o103 == offsetof(::cl_enginefunc_dst_t, pfnGetScreenInfo), "offsetof cl_enginefunc_dst_t.pfnGetScreenInfo");
    static_assert(
        std::is_same_v<t103, map_t<decltype(::cl_enginefunc_dst_t::pfnGetScreenInfo)>>,
        "type cl_enginefunc_dst_t.pfnGetScreenInfo"
    );
    static_assert(o104 == offsetof(::cl_enginefunc_dst_t, pfnSetCrosshair), "offsetof cl_enginefunc_dst_t.pfnSetCrosshair");
    static_assert(
        std::is_same_v<t104, map_t<decltype(::cl_enginefunc_dst_t::pfnSetCrosshair)>>,
        "type cl_enginefunc_dst_t.pfnSetCrosshair"
    );
    static_assert(o105 == offsetof(::cl_enginefunc_dst_t, pfnRegisterVariable), "offsetof cl_enginefunc_dst_t.pfnRegisterVariable");
    static_assert(
        std::is_same_v<t105, map_t<decltype(::cl_enginefunc_dst_t::pfnRegisterVariable)>>,
        "type cl_enginefunc_dst_t.pfnRegisterVariable"
    );
    static_assert(o106 == offsetof(::cl_enginefunc_dst_t, pfnGetCvarFloat), "offsetof cl_enginefunc_dst_t.pfnGetCvarFloat");
    static_assert(
        std::is_same_v<t106, map_t<decltype(::cl_enginefunc_dst_t::pfnGetCvarFloat)>>,
        "type cl_enginefunc_dst_t.pfnGetCvarFloat"
    );
    static_assert(o107 == offsetof(::cl_enginefunc_dst_t, pfnGetCvarString), "offsetof cl_enginefunc_dst_t.pfnGetCvarString");
    static_assert(
        std::is_same_v<t107, map_t<decltype(::cl_enginefunc_dst_t::pfnGetCvarString)>>,
        "type cl_enginefunc_dst_t.pfnGetCvarString"
    );
    static_assert(o108 == offsetof(::cl_enginefunc_dst_t, pfnAddCommand), "offsetof cl_enginefunc_dst_t.pfnAddCommand");
    static_assert(std::is_same_v<t108, map_t<decltype(::cl_enginefunc_dst_t::pfnAddCommand)>>, "type cl_enginefunc_dst_t.pfnAddCommand");
    static_assert(o109 == offsetof(::cl_enginefunc_dst_t, pfnHookUserMsg), "offsetof cl_enginefunc_dst_t.pfnHookUserMsg");
    static_assert(std::is_same_v<t109, map_t<decltype(::cl_enginefunc_dst_t::pfnHookUserMsg)>>, "type cl_enginefunc_dst_t.pfnHookUserMsg");
    static_assert(o110 == offsetof(::cl_enginefunc_dst_t, pfnServerCmd), "offsetof cl_enginefunc_dst_t.pfnServerCmd");
    static_assert(std::is_same_v<t110, map_t<decltype(::cl_enginefunc_dst_t::pfnServerCmd)>>, "type cl_enginefunc_dst_t.pfnServerCmd");
    static_assert(o111 == offsetof(::cl_enginefunc_dst_t, pfnClientCmd), "offsetof cl_enginefunc_dst_t.pfnClientCmd");
    static_assert(std::is_same_v<t111, map_t<decltype(::cl_enginefunc_dst_t::pfnClientCmd)>>, "type cl_enginefunc_dst_t.pfnClientCmd");
    static_assert(o112 == offsetof(::cl_enginefunc_dst_t, pfnGetPlayerInfo), "offsetof cl_enginefunc_dst_t.pfnGetPlayerInfo");
    static_assert(
        std::is_same_v<t112, map_t<decltype(::cl_enginefunc_dst_t::pfnGetPlayerInfo)>>,
        "type cl_enginefunc_dst_t.pfnGetPlayerInfo"
    );
    static_assert(o113 == offsetof(::cl_enginefunc_dst_t, pfnPlaySoundByName), "offsetof cl_enginefunc_dst_t.pfnPlaySoundByName");
    static_assert(
        std::is_same_v<t113, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaySoundByName)>>,
        "type cl_enginefunc_dst_t.pfnPlaySoundByName"
    );
    static_assert(o114 == offsetof(::cl_enginefunc_dst_t, pfnPlaySoundByIndex), "offsetof cl_enginefunc_dst_t.pfnPlaySoundByIndex");
    static_assert(
        std::is_same_v<t114, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaySoundByIndex)>>,
        "type cl_enginefunc_dst_t.pfnPlaySoundByIndex"
    );
    static_assert(o115 == offsetof(::cl_enginefunc_dst_t, pfnAngleVectors), "offsetof cl_enginefunc_dst_t.pfnAngleVectors");
    static_assert(
        std::is_same_v<t115, map_t<decltype(::cl_enginefunc_dst_t::pfnAngleVectors)>>,
        "type cl_enginefunc_dst_t.pfnAngleVectors"
    );
    static_assert(o116 == offsetof(::cl_enginefunc_dst_t, pfnTextMessageGet), "offsetof cl_enginefunc_dst_t.pfnTextMessageGet");
    static_assert(
        std::is_same_v<t116, map_t<decltype(::cl_enginefunc_dst_t::pfnTextMessageGet)>>,
        "type cl_enginefunc_dst_t.pfnTextMessageGet"
    );
    static_assert(o117 == offsetof(::cl_enginefunc_dst_t, pfnDrawCharacter), "offsetof cl_enginefunc_dst_t.pfnDrawCharacter");
    static_assert(
        std::is_same_v<t117, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawCharacter)>>,
        "type cl_enginefunc_dst_t.pfnDrawCharacter"
    );
    static_assert(o118 == offsetof(::cl_enginefunc_dst_t, pfnDrawConsoleString), "offsetof cl_enginefunc_dst_t.pfnDrawConsoleString");
    static_assert(
        std::is_same_v<t118, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawConsoleString)>>,
        "type cl_enginefunc_dst_t.pfnDrawConsoleString"
    );
    static_assert(o119 == offsetof(::cl_enginefunc_dst_t, pfnDrawSetTextColor), "offsetof cl_enginefunc_dst_t.pfnDrawSetTextColor");
    static_assert(
        std::is_same_v<t119, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawSetTextColor)>>,
        "type cl_enginefunc_dst_t.pfnDrawSetTextColor"
    );
    static_assert(o120 == offsetof(::cl_enginefunc_dst_t, pfnDrawConsoleStringLen), "offsetof cl_enginefunc_dst_t.pfnDrawConsoleStringLen");
    static_assert(
        std::is_same_v<t120, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawConsoleStringLen)>>,
        "type cl_enginefunc_dst_t.pfnDrawConsoleStringLen"
    );
    static_assert(o121 == offsetof(::cl_enginefunc_dst_t, pfnConsolePrint), "offsetof cl_enginefunc_dst_t.pfnConsolePrint");
    static_assert(
        std::is_same_v<t121, map_t<decltype(::cl_enginefunc_dst_t::pfnConsolePrint)>>,
        "type cl_enginefunc_dst_t.pfnConsolePrint"
    );
    static_assert(o122 == offsetof(::cl_enginefunc_dst_t, pfnCenterPrint), "offsetof cl_enginefunc_dst_t.pfnCenterPrint");
    static_assert(std::is_same_v<t122, map_t<decltype(::cl_enginefunc_dst_t::pfnCenterPrint)>>, "type cl_enginefunc_dst_t.pfnCenterPrint");
    static_assert(o123 == offsetof(::cl_enginefunc_dst_t, GetWindowCenterX), "offsetof cl_enginefunc_dst_t.GetWindowCenterX");
    static_assert(
        std::is_same_v<t123, map_t<decltype(::cl_enginefunc_dst_t::GetWindowCenterX)>>,
        "type cl_enginefunc_dst_t.GetWindowCenterX"
    );
    static_assert(o124 == offsetof(::cl_enginefunc_dst_t, GetWindowCenterY), "offsetof cl_enginefunc_dst_t.GetWindowCenterY");
    static_assert(
        std::is_same_v<t124, map_t<decltype(::cl_enginefunc_dst_t::GetWindowCenterY)>>,
        "type cl_enginefunc_dst_t.GetWindowCenterY"
    );
    static_assert(o125 == offsetof(::cl_enginefunc_dst_t, GetViewAngles), "offsetof cl_enginefunc_dst_t.GetViewAngles");
    static_assert(std::is_same_v<t125, map_t<decltype(::cl_enginefunc_dst_t::GetViewAngles)>>, "type cl_enginefunc_dst_t.GetViewAngles");
    static_assert(o126 == offsetof(::cl_enginefunc_dst_t, SetViewAngles), "offsetof cl_enginefunc_dst_t.SetViewAngles");
    static_assert(std::is_same_v<t126, map_t<decltype(::cl_enginefunc_dst_t::SetViewAngles)>>, "type cl_enginefunc_dst_t.SetViewAngles");
    static_assert(o127 == offsetof(::cl_enginefunc_dst_t, GetMaxClients), "offsetof cl_enginefunc_dst_t.GetMaxClients");
    static_assert(std::is_same_v<t127, map_t<decltype(::cl_enginefunc_dst_t::GetMaxClients)>>, "type cl_enginefunc_dst_t.GetMaxClients");
    static_assert(o128 == offsetof(::cl_enginefunc_dst_t, Cvar_SetValue), "offsetof cl_enginefunc_dst_t.Cvar_SetValue");
    static_assert(std::is_same_v<t128, map_t<decltype(::cl_enginefunc_dst_t::Cvar_SetValue)>>, "type cl_enginefunc_dst_t.Cvar_SetValue");
    static_assert(o129 == offsetof(::cl_enginefunc_dst_t, Cmd_Argc), "offsetof cl_enginefunc_dst_t.Cmd_Argc");
    static_assert(std::is_same_v<t129, map_t<decltype(::cl_enginefunc_dst_t::Cmd_Argc)>>, "type cl_enginefunc_dst_t.Cmd_Argc");
    static_assert(o130 == offsetof(::cl_enginefunc_dst_t, Cmd_Argv), "offsetof cl_enginefunc_dst_t.Cmd_Argv");
    static_assert(std::is_same_v<t130, map_t<decltype(::cl_enginefunc_dst_t::Cmd_Argv)>>, "type cl_enginefunc_dst_t.Cmd_Argv");
    static_assert(o131 == offsetof(::cl_enginefunc_dst_t, Con_Printf), "offsetof cl_enginefunc_dst_t.Con_Printf");
    static_assert(std::is_same_v<t131, map_t<decltype(::cl_enginefunc_dst_t::Con_Printf)>>, "type cl_enginefunc_dst_t.Con_Printf");
    static_assert(o132 == offsetof(::cl_enginefunc_dst_t, Con_DPrintf), "offsetof cl_enginefunc_dst_t.Con_DPrintf");
    static_assert(std::is_same_v<t132, map_t<decltype(::cl_enginefunc_dst_t::Con_DPrintf)>>, "type cl_enginefunc_dst_t.Con_DPrintf");
    static_assert(o133 == offsetof(::cl_enginefunc_dst_t, Con_NPrintf), "offsetof cl_enginefunc_dst_t.Con_NPrintf");
    static_assert(std::is_same_v<t133, map_t<decltype(::cl_enginefunc_dst_t::Con_NPrintf)>>, "type cl_enginefunc_dst_t.Con_NPrintf");
    static_assert(o134 == offsetof(::cl_enginefunc_dst_t, Con_NXPrintf), "offsetof cl_enginefunc_dst_t.Con_NXPrintf");
    static_assert(std::is_same_v<t134, map_t<decltype(::cl_enginefunc_dst_t::Con_NXPrintf)>>, "type cl_enginefunc_dst_t.Con_NXPrintf");
    static_assert(o135 == offsetof(::cl_enginefunc_dst_t, PhysInfo_ValueForKey), "offsetof cl_enginefunc_dst_t.PhysInfo_ValueForKey");
    static_assert(
        std::is_same_v<t135, map_t<decltype(::cl_enginefunc_dst_t::PhysInfo_ValueForKey)>>,
        "type cl_enginefunc_dst_t.PhysInfo_ValueForKey"
    );
    static_assert(o136 == offsetof(::cl_enginefunc_dst_t, ServerInfo_ValueForKey), "offsetof cl_enginefunc_dst_t.ServerInfo_ValueForKey");
    static_assert(
        std::is_same_v<t136, map_t<decltype(::cl_enginefunc_dst_t::ServerInfo_ValueForKey)>>,
        "type cl_enginefunc_dst_t.ServerInfo_ValueForKey"
    );
    static_assert(o137 == offsetof(::cl_enginefunc_dst_t, GetClientMaxspeed), "offsetof cl_enginefunc_dst_t.GetClientMaxspeed");
    static_assert(
        std::is_same_v<t137, map_t<decltype(::cl_enginefunc_dst_t::GetClientMaxspeed)>>,
        "type cl_enginefunc_dst_t.GetClientMaxspeed"
    );
    static_assert(o138 == offsetof(::cl_enginefunc_dst_t, CheckParm), "offsetof cl_enginefunc_dst_t.CheckParm");
    static_assert(std::is_same_v<t138, map_t<decltype(::cl_enginefunc_dst_t::CheckParm)>>, "type cl_enginefunc_dst_t.CheckParm");
    static_assert(o139 == offsetof(::cl_enginefunc_dst_t, Key_Event), "offsetof cl_enginefunc_dst_t.Key_Event");
    static_assert(std::is_same_v<t139, map_t<decltype(::cl_enginefunc_dst_t::Key_Event)>>, "type cl_enginefunc_dst_t.Key_Event");
    static_assert(o140 == offsetof(::cl_enginefunc_dst_t, GetMousePosition), "offsetof cl_enginefunc_dst_t.GetMousePosition");
    static_assert(
        std::is_same_v<t140, map_t<decltype(::cl_enginefunc_dst_t::GetMousePosition)>>,
        "type cl_enginefunc_dst_t.GetMousePosition"
    );
    static_assert(o141 == offsetof(::cl_enginefunc_dst_t, IsNoClipping), "offsetof cl_enginefunc_dst_t.IsNoClipping");
    static_assert(std::is_same_v<t141, map_t<decltype(::cl_enginefunc_dst_t::IsNoClipping)>>, "type cl_enginefunc_dst_t.IsNoClipping");
    static_assert(o142 == offsetof(::cl_enginefunc_dst_t, GetLocalPlayer), "offsetof cl_enginefunc_dst_t.GetLocalPlayer");
    static_assert(std::is_same_v<t142, map_t<decltype(::cl_enginefunc_dst_t::GetLocalPlayer)>>, "type cl_enginefunc_dst_t.GetLocalPlayer");
    static_assert(o143 == offsetof(::cl_enginefunc_dst_t, GetViewModel), "offsetof cl_enginefunc_dst_t.GetViewModel");
    static_assert(std::is_same_v<t143, map_t<decltype(::cl_enginefunc_dst_t::GetViewModel)>>, "type cl_enginefunc_dst_t.GetViewModel");
    static_assert(o144 == offsetof(::cl_enginefunc_dst_t, GetEntityByIndex), "offsetof cl_enginefunc_dst_t.GetEntityByIndex");
    static_assert(
        std::is_same_v<t144, map_t<decltype(::cl_enginefunc_dst_t::GetEntityByIndex)>>,
        "type cl_enginefunc_dst_t.GetEntityByIndex"
    );
    static_assert(o145 == offsetof(::cl_enginefunc_dst_t, GetClientTime), "offsetof cl_enginefunc_dst_t.GetClientTime");
    static_assert(std::is_same_v<t145, map_t<decltype(::cl_enginefunc_dst_t::GetClientTime)>>, "type cl_enginefunc_dst_t.GetClientTime");
    static_assert(o146 == offsetof(::cl_enginefunc_dst_t, V_CalcShake), "offsetof cl_enginefunc_dst_t.V_CalcShake");
    static_assert(std::is_same_v<t146, map_t<decltype(::cl_enginefunc_dst_t::V_CalcShake)>>, "type cl_enginefunc_dst_t.V_CalcShake");
    static_assert(o147 == offsetof(::cl_enginefunc_dst_t, V_ApplyShake), "offsetof cl_enginefunc_dst_t.V_ApplyShake");
    static_assert(std::is_same_v<t147, map_t<decltype(::cl_enginefunc_dst_t::V_ApplyShake)>>, "type cl_enginefunc_dst_t.V_ApplyShake");
    static_assert(o148 == offsetof(::cl_enginefunc_dst_t, PM_PointContents), "offsetof cl_enginefunc_dst_t.PM_PointContents");
    static_assert(
        std::is_same_v<t148, map_t<decltype(::cl_enginefunc_dst_t::PM_PointContents)>>,
        "type cl_enginefunc_dst_t.PM_PointContents"
    );
    static_assert(o149 == offsetof(::cl_enginefunc_dst_t, PM_WaterEntity), "offsetof cl_enginefunc_dst_t.PM_WaterEntity");
    static_assert(std::is_same_v<t149, map_t<decltype(::cl_enginefunc_dst_t::PM_WaterEntity)>>, "type cl_enginefunc_dst_t.PM_WaterEntity");
    static_assert(o150 == offsetof(::cl_enginefunc_dst_t, PM_TraceLine), "offsetof cl_enginefunc_dst_t.PM_TraceLine");
    static_assert(std::is_same_v<t150, map_t<decltype(::cl_enginefunc_dst_t::PM_TraceLine)>>, "type cl_enginefunc_dst_t.PM_TraceLine");
    static_assert(o151 == offsetof(::cl_enginefunc_dst_t, CL_LoadModel), "offsetof cl_enginefunc_dst_t.CL_LoadModel");
    static_assert(std::is_same_v<t151, map_t<decltype(::cl_enginefunc_dst_t::CL_LoadModel)>>, "type cl_enginefunc_dst_t.CL_LoadModel");
    static_assert(o152 == offsetof(::cl_enginefunc_dst_t, CL_CreateVisibleEntity), "offsetof cl_enginefunc_dst_t.CL_CreateVisibleEntity");
    static_assert(
        std::is_same_v<t152, map_t<decltype(::cl_enginefunc_dst_t::CL_CreateVisibleEntity)>>,
        "type cl_enginefunc_dst_t.CL_CreateVisibleEntity"
    );
    static_assert(o153 == offsetof(::cl_enginefunc_dst_t, GetSpritePointer), "offsetof cl_enginefunc_dst_t.GetSpritePointer");
    static_assert(
        std::is_same_v<t153, map_t<decltype(::cl_enginefunc_dst_t::GetSpritePointer)>>,
        "type cl_enginefunc_dst_t.GetSpritePointer"
    );
    static_assert(
        o154 == offsetof(::cl_enginefunc_dst_t, pfnPlaySoundByNameAtLocation),
        "offsetof cl_enginefunc_dst_t.pfnPlaySoundByNameAtLocation"
    );
    static_assert(
        std::is_same_v<t154, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaySoundByNameAtLocation)>>,
        "type cl_enginefunc_dst_t.pfnPlaySoundByNameAtLocation"
    );
    static_assert(o155 == offsetof(::cl_enginefunc_dst_t, pfnPrecacheEvent), "offsetof cl_enginefunc_dst_t.pfnPrecacheEvent");
    static_assert(
        std::is_same_v<t155, map_t<decltype(::cl_enginefunc_dst_t::pfnPrecacheEvent)>>,
        "type cl_enginefunc_dst_t.pfnPrecacheEvent"
    );
    static_assert(o156 == offsetof(::cl_enginefunc_dst_t, pfnPlaybackEvent), "offsetof cl_enginefunc_dst_t.pfnPlaybackEvent");
    static_assert(
        std::is_same_v<t156, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaybackEvent)>>,
        "type cl_enginefunc_dst_t.pfnPlaybackEvent"
    );
    static_assert(o157 == offsetof(::cl_enginefunc_dst_t, pfnWeaponAnim), "offsetof cl_enginefunc_dst_t.pfnWeaponAnim");
    static_assert(std::is_same_v<t157, map_t<decltype(::cl_enginefunc_dst_t::pfnWeaponAnim)>>, "type cl_enginefunc_dst_t.pfnWeaponAnim");
    static_assert(o158 == offsetof(::cl_enginefunc_dst_t, pfnRandomFloat), "offsetof cl_enginefunc_dst_t.pfnRandomFloat");
    static_assert(std::is_same_v<t158, map_t<decltype(::cl_enginefunc_dst_t::pfnRandomFloat)>>, "type cl_enginefunc_dst_t.pfnRandomFloat");
    static_assert(o159 == offsetof(::cl_enginefunc_dst_t, pfnRandomLong), "offsetof cl_enginefunc_dst_t.pfnRandomLong");
    static_assert(std::is_same_v<t159, map_t<decltype(::cl_enginefunc_dst_t::pfnRandomLong)>>, "type cl_enginefunc_dst_t.pfnRandomLong");
    static_assert(o160 == offsetof(::cl_enginefunc_dst_t, pfnHookEvent), "offsetof cl_enginefunc_dst_t.pfnHookEvent");
    static_assert(std::is_same_v<t160, map_t<decltype(::cl_enginefunc_dst_t::pfnHookEvent)>>, "type cl_enginefunc_dst_t.pfnHookEvent");
    static_assert(o161 == offsetof(::cl_enginefunc_dst_t, Con_IsVisible), "offsetof cl_enginefunc_dst_t.Con_IsVisible");
    static_assert(std::is_same_v<t161, map_t<decltype(::cl_enginefunc_dst_t::Con_IsVisible)>>, "type cl_enginefunc_dst_t.Con_IsVisible");
    static_assert(o162 == offsetof(::cl_enginefunc_dst_t, pfnGetGameDirectory), "offsetof cl_enginefunc_dst_t.pfnGetGameDirectory");
    static_assert(
        std::is_same_v<t162, map_t<decltype(::cl_enginefunc_dst_t::pfnGetGameDirectory)>>,
        "type cl_enginefunc_dst_t.pfnGetGameDirectory"
    );
    static_assert(o163 == offsetof(::cl_enginefunc_dst_t, pfnGetCvarPointer), "offsetof cl_enginefunc_dst_t.pfnGetCvarPointer");
    static_assert(
        std::is_same_v<t163, map_t<decltype(::cl_enginefunc_dst_t::pfnGetCvarPointer)>>,
        "type cl_enginefunc_dst_t.pfnGetCvarPointer"
    );
    static_assert(o164 == offsetof(::cl_enginefunc_dst_t, Key_LookupBinding), "offsetof cl_enginefunc_dst_t.Key_LookupBinding");
    static_assert(
        std::is_same_v<t164, map_t<decltype(::cl_enginefunc_dst_t::Key_LookupBinding)>>,
        "type cl_enginefunc_dst_t.Key_LookupBinding"
    );
    static_assert(o165 == offsetof(::cl_enginefunc_dst_t, pfnGetLevelName), "offsetof cl_enginefunc_dst_t.pfnGetLevelName");
    static_assert(
        std::is_same_v<t165, map_t<decltype(::cl_enginefunc_dst_t::pfnGetLevelName)>>,
        "type cl_enginefunc_dst_t.pfnGetLevelName"
    );
    static_assert(o166 == offsetof(::cl_enginefunc_dst_t, pfnGetScreenFade), "offsetof cl_enginefunc_dst_t.pfnGetScreenFade");
    static_assert(
        std::is_same_v<t166, map_t<decltype(::cl_enginefunc_dst_t::pfnGetScreenFade)>>,
        "type cl_enginefunc_dst_t.pfnGetScreenFade"
    );
    static_assert(o167 == offsetof(::cl_enginefunc_dst_t, pfnSetScreenFade), "offsetof cl_enginefunc_dst_t.pfnSetScreenFade");
    static_assert(
        std::is_same_v<t167, map_t<decltype(::cl_enginefunc_dst_t::pfnSetScreenFade)>>,
        "type cl_enginefunc_dst_t.pfnSetScreenFade"
    );
    static_assert(o168 == offsetof(::cl_enginefunc_dst_t, VGui_GetPanel), "offsetof cl_enginefunc_dst_t.VGui_GetPanel");
    static_assert(std::is_same_v<t168, map_t<decltype(::cl_enginefunc_dst_t::VGui_GetPanel)>>, "type cl_enginefunc_dst_t.VGui_GetPanel");
    static_assert(
        o169 == offsetof(::cl_enginefunc_dst_t, VGui_ViewportPaintBackground),
        "offsetof cl_enginefunc_dst_t.VGui_ViewportPaintBackground"
    );
    static_assert(
        std::is_same_v<t169, map_t<decltype(::cl_enginefunc_dst_t::VGui_ViewportPaintBackground)>>,
        "type cl_enginefunc_dst_t.VGui_ViewportPaintBackground"
    );
    static_assert(o170 == offsetof(::cl_enginefunc_dst_t, COM_LoadFile), "offsetof cl_enginefunc_dst_t.COM_LoadFile");
    static_assert(std::is_same_v<t170, map_t<decltype(::cl_enginefunc_dst_t::COM_LoadFile)>>, "type cl_enginefunc_dst_t.COM_LoadFile");
    static_assert(o171 == offsetof(::cl_enginefunc_dst_t, COM_ParseFile), "offsetof cl_enginefunc_dst_t.COM_ParseFile");
    static_assert(std::is_same_v<t171, map_t<decltype(::cl_enginefunc_dst_t::COM_ParseFile)>>, "type cl_enginefunc_dst_t.COM_ParseFile");
    static_assert(o172 == offsetof(::cl_enginefunc_dst_t, COM_FreeFile), "offsetof cl_enginefunc_dst_t.COM_FreeFile");
    static_assert(std::is_same_v<t172, map_t<decltype(::cl_enginefunc_dst_t::COM_FreeFile)>>, "type cl_enginefunc_dst_t.COM_FreeFile");
    static_assert(o173 == offsetof(::cl_enginefunc_dst_t, pTriAPI), "offsetof cl_enginefunc_dst_t.pTriAPI");
    static_assert(std::is_same_v<t173, map_t<decltype(::cl_enginefunc_dst_t::pTriAPI)>>, "type cl_enginefunc_dst_t.pTriAPI");
    static_assert(o174 == offsetof(::cl_enginefunc_dst_t, pEfxAPI), "offsetof cl_enginefunc_dst_t.pEfxAPI");
    static_assert(std::is_same_v<t174, map_t<decltype(::cl_enginefunc_dst_t::pEfxAPI)>>, "type cl_enginefunc_dst_t.pEfxAPI");
    static_assert(o175 == offsetof(::cl_enginefunc_dst_t, pEventAPI), "offsetof cl_enginefunc_dst_t.pEventAPI");
    static_assert(std::is_same_v<t175, map_t<decltype(::cl_enginefunc_dst_t::pEventAPI)>>, "type cl_enginefunc_dst_t.pEventAPI");
    static_assert(o176 == offsetof(::cl_enginefunc_dst_t, pDemoAPI), "offsetof cl_enginefunc_dst_t.pDemoAPI");
    static_assert(std::is_same_v<t176, map_t<decltype(::cl_enginefunc_dst_t::pDemoAPI)>>, "type cl_enginefunc_dst_t.pDemoAPI");
    static_assert(o177 == offsetof(::cl_enginefunc_dst_t, pNetAPI), "offsetof cl_enginefunc_dst_t.pNetAPI");
    static_assert(std::is_same_v<t177, map_t<decltype(::cl_enginefunc_dst_t::pNetAPI)>>, "type cl_enginefunc_dst_t.pNetAPI");
    static_assert(o178 == offsetof(::cl_enginefunc_dst_t, pVoiceTweak), "offsetof cl_enginefunc_dst_t.pVoiceTweak");
    static_assert(std::is_same_v<t178, map_t<decltype(::cl_enginefunc_dst_t::pVoiceTweak)>>, "type cl_enginefunc_dst_t.pVoiceTweak");
    static_assert(o179 == offsetof(::cl_enginefunc_dst_t, IsSpectateOnly), "offsetof cl_enginefunc_dst_t.IsSpectateOnly");
    static_assert(std::is_same_v<t179, map_t<decltype(::cl_enginefunc_dst_t::IsSpectateOnly)>>, "type cl_enginefunc_dst_t.IsSpectateOnly");
    static_assert(o180 == offsetof(::cl_enginefunc_dst_t, LoadMapSprite), "offsetof cl_enginefunc_dst_t.LoadMapSprite");
    static_assert(std::is_same_v<t180, map_t<decltype(::cl_enginefunc_dst_t::LoadMapSprite)>>, "type cl_enginefunc_dst_t.LoadMapSprite");
    static_assert(
        o181 == offsetof(::cl_enginefunc_dst_t, COM_AddAppDirectoryToSearchPath),
        "offsetof cl_enginefunc_dst_t.COM_AddAppDirectoryToSearchPath"
    );
    static_assert(
        std::is_same_v<t181, map_t<decltype(::cl_enginefunc_dst_t::COM_AddAppDirectoryToSearchPath)>>,
        "type cl_enginefunc_dst_t.COM_AddAppDirectoryToSearchPath"
    );
    static_assert(o182 == offsetof(::cl_enginefunc_dst_t, COM_ExpandFilename), "offsetof cl_enginefunc_dst_t.COM_ExpandFilename");
    static_assert(
        std::is_same_v<t182, map_t<decltype(::cl_enginefunc_dst_t::COM_ExpandFilename)>>,
        "type cl_enginefunc_dst_t.COM_ExpandFilename"
    );
    static_assert(o183 == offsetof(::cl_enginefunc_dst_t, PlayerInfo_ValueForKey), "offsetof cl_enginefunc_dst_t.PlayerInfo_ValueForKey");
    static_assert(
        std::is_same_v<t183, map_t<decltype(::cl_enginefunc_dst_t::PlayerInfo_ValueForKey)>>,
        "type cl_enginefunc_dst_t.PlayerInfo_ValueForKey"
    );
    static_assert(
        o184 == offsetof(::cl_enginefunc_dst_t, PlayerInfo_SetValueForKey),
        "offsetof cl_enginefunc_dst_t.PlayerInfo_SetValueForKey"
    );
    static_assert(
        std::is_same_v<t184, map_t<decltype(::cl_enginefunc_dst_t::PlayerInfo_SetValueForKey)>>,
        "type cl_enginefunc_dst_t.PlayerInfo_SetValueForKey"
    );
    static_assert(o185 == offsetof(::cl_enginefunc_dst_t, GetPlayerUniqueID), "offsetof cl_enginefunc_dst_t.GetPlayerUniqueID");
    static_assert(
        std::is_same_v<t185, map_t<decltype(::cl_enginefunc_dst_t::GetPlayerUniqueID)>>,
        "type cl_enginefunc_dst_t.GetPlayerUniqueID"
    );
    static_assert(o186 == offsetof(::cl_enginefunc_dst_t, GetTrackerIDForPlayer), "offsetof cl_enginefunc_dst_t.GetTrackerIDForPlayer");
    static_assert(
        std::is_same_v<t186, map_t<decltype(::cl_enginefunc_dst_t::GetTrackerIDForPlayer)>>,
        "type cl_enginefunc_dst_t.GetTrackerIDForPlayer"
    );
    static_assert(o187 == offsetof(::cl_enginefunc_dst_t, GetPlayerForTrackerID), "offsetof cl_enginefunc_dst_t.GetPlayerForTrackerID");
    static_assert(
        std::is_same_v<t187, map_t<decltype(::cl_enginefunc_dst_t::GetPlayerForTrackerID)>>,
        "type cl_enginefunc_dst_t.GetPlayerForTrackerID"
    );
    static_assert(o188 == offsetof(::cl_enginefunc_dst_t, pfnServerCmdUnreliable), "offsetof cl_enginefunc_dst_t.pfnServerCmdUnreliable");
    static_assert(
        std::is_same_v<t188, map_t<decltype(::cl_enginefunc_dst_t::pfnServerCmdUnreliable)>>,
        "type cl_enginefunc_dst_t.pfnServerCmdUnreliable"
    );
    static_assert(o189 == offsetof(::cl_enginefunc_dst_t, pfnGetMousePos), "offsetof cl_enginefunc_dst_t.pfnGetMousePos");
    static_assert(std::is_same_v<t189, map_t<decltype(::cl_enginefunc_dst_t::pfnGetMousePos)>>, "type cl_enginefunc_dst_t.pfnGetMousePos");
    static_assert(o190 == offsetof(::cl_enginefunc_dst_t, pfnSetMousePos), "offsetof cl_enginefunc_dst_t.pfnSetMousePos");
    static_assert(std::is_same_v<t190, map_t<decltype(::cl_enginefunc_dst_t::pfnSetMousePos)>>, "type cl_enginefunc_dst_t.pfnSetMousePos");
    static_assert(o191 == offsetof(::cl_enginefunc_dst_t, pfnSetMouseEnable), "offsetof cl_enginefunc_dst_t.pfnSetMouseEnable");
    static_assert(
        std::is_same_v<t191, map_t<decltype(::cl_enginefunc_dst_t::pfnSetMouseEnable)>>,
        "type cl_enginefunc_dst_t.pfnSetMouseEnable"
    );
    static_assert(o192 == offsetof(::cl_enginefunc_dst_t, pfnSetFilterMode), "offsetof cl_enginefunc_dst_t.pfnSetFilterMode");
    static_assert(
        std::is_same_v<t192, map_t<decltype(::cl_enginefunc_dst_t::pfnSetFilterMode)>>,
        "type cl_enginefunc_dst_t.pfnSetFilterMode"
    );
    static_assert(o193 == offsetof(::cl_enginefunc_dst_t, pfnSetFilterColor), "offsetof cl_enginefunc_dst_t.pfnSetFilterColor");
    static_assert(
        std::is_same_v<t193, map_t<decltype(::cl_enginefunc_dst_t::pfnSetFilterColor)>>,
        "type cl_enginefunc_dst_t.pfnSetFilterColor"
    );
    static_assert(o194 == offsetof(::cl_enginefunc_dst_t, pfnSetFilterBrightness), "offsetof cl_enginefunc_dst_t.pfnSetFilterBrightness");
    static_assert(
        std::is_same_v<t194, map_t<decltype(::cl_enginefunc_dst_t::pfnSetFilterBrightness)>>,
        "type cl_enginefunc_dst_t.pfnSetFilterBrightness"
    );
    static_assert(o195 == offsetof(::cl_enginefunc_dst_t, pfnSequenceGet), "offsetof cl_enginefunc_dst_t.pfnSequenceGet");
    static_assert(std::is_same_v<t195, map_t<decltype(::cl_enginefunc_dst_t::pfnSequenceGet)>>, "type cl_enginefunc_dst_t.pfnSequenceGet");
    static_assert(o196 == offsetof(::cl_enginefunc_dst_t, pfnSPR_DrawGeneric), "offsetof cl_enginefunc_dst_t.pfnSPR_DrawGeneric");
    static_assert(
        std::is_same_v<t196, map_t<decltype(::cl_enginefunc_dst_t::pfnSPR_DrawGeneric)>>,
        "type cl_enginefunc_dst_t.pfnSPR_DrawGeneric"
    );
    static_assert(o197 == offsetof(::cl_enginefunc_dst_t, pfnSequencePickSentence), "offsetof cl_enginefunc_dst_t.pfnSequencePickSentence");
    static_assert(
        std::is_same_v<t197, map_t<decltype(::cl_enginefunc_dst_t::pfnSequencePickSentence)>>,
        "type cl_enginefunc_dst_t.pfnSequencePickSentence"
    );
    static_assert(o198 == offsetof(::cl_enginefunc_dst_t, pfnDrawString), "offsetof cl_enginefunc_dst_t.pfnDrawString");
    static_assert(std::is_same_v<t198, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawString)>>, "type cl_enginefunc_dst_t.pfnDrawString");
    static_assert(o199 == offsetof(::cl_enginefunc_dst_t, pfnDrawStringReverse), "offsetof cl_enginefunc_dst_t.pfnDrawStringReverse");
    static_assert(
        std::is_same_v<t199, map_t<decltype(::cl_enginefunc_dst_t::pfnDrawStringReverse)>>,
        "type cl_enginefunc_dst_t.pfnDrawStringReverse"
    );
    static_assert(
        o200 == offsetof(::cl_enginefunc_dst_t, LocalPlayerInfo_ValueForKey),
        "offsetof cl_enginefunc_dst_t.LocalPlayerInfo_ValueForKey"
    );
    static_assert(
        std::is_same_v<t200, map_t<decltype(::cl_enginefunc_dst_t::LocalPlayerInfo_ValueForKey)>>,
        "type cl_enginefunc_dst_t.LocalPlayerInfo_ValueForKey"
    );
    static_assert(o201 == offsetof(::cl_enginefunc_dst_t, pfnVGUI2DrawCharacter), "offsetof cl_enginefunc_dst_t.pfnVGUI2DrawCharacter");
    static_assert(
        std::is_same_v<t201, map_t<decltype(::cl_enginefunc_dst_t::pfnVGUI2DrawCharacter)>>,
        "type cl_enginefunc_dst_t.pfnVGUI2DrawCharacter"
    );
    static_assert(
        o202 == offsetof(::cl_enginefunc_dst_t, pfnVGUI2DrawCharacterAdd),
        "offsetof cl_enginefunc_dst_t.pfnVGUI2DrawCharacterAdd"
    );
    static_assert(
        std::is_same_v<t202, map_t<decltype(::cl_enginefunc_dst_t::pfnVGUI2DrawCharacterAdd)>>,
        "type cl_enginefunc_dst_t.pfnVGUI2DrawCharacterAdd"
    );
    static_assert(o203 == offsetof(::cl_enginefunc_dst_t, pfnPlaySoundVoiceByName), "offsetof cl_enginefunc_dst_t.pfnPlaySoundVoiceByName");
    static_assert(
        std::is_same_v<t203, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaySoundVoiceByName)>>,
        "type cl_enginefunc_dst_t.pfnPlaySoundVoiceByName"
    );
    static_assert(o204 == offsetof(::cl_enginefunc_dst_t, pfnPrimeMusicStream), "offsetof cl_enginefunc_dst_t.pfnPrimeMusicStream");
    static_assert(
        std::is_same_v<t204, map_t<decltype(::cl_enginefunc_dst_t::pfnPrimeMusicStream)>>,
        "type cl_enginefunc_dst_t.pfnPrimeMusicStream"
    );
    static_assert(
        o205 == offsetof(::cl_enginefunc_dst_t, pfnProcessTutorMessageDecayBuffer),
        "offsetof cl_enginefunc_dst_t.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t205, map_t<decltype(::cl_enginefunc_dst_t::pfnProcessTutorMessageDecayBuffer)>>,
        "type cl_enginefunc_dst_t.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        o206 == offsetof(::cl_enginefunc_dst_t, pfnConstructTutorMessageDecayBuffer),
        "offsetof cl_enginefunc_dst_t.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t206, map_t<decltype(::cl_enginefunc_dst_t::pfnConstructTutorMessageDecayBuffer)>>,
        "type cl_enginefunc_dst_t.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        o207 == offsetof(::cl_enginefunc_dst_t, pfnResetTutorMessageDecayData),
        "offsetof cl_enginefunc_dst_t.pfnResetTutorMessageDecayData"
    );
    static_assert(
        std::is_same_v<t207, map_t<decltype(::cl_enginefunc_dst_t::pfnResetTutorMessageDecayData)>>,
        "type cl_enginefunc_dst_t.pfnResetTutorMessageDecayData"
    );
    static_assert(
        o208 == offsetof(::cl_enginefunc_dst_t, pfnPlaySoundByNameAtPitch),
        "offsetof cl_enginefunc_dst_t.pfnPlaySoundByNameAtPitch"
    );
    static_assert(
        std::is_same_v<t208, map_t<decltype(::cl_enginefunc_dst_t::pfnPlaySoundByNameAtPitch)>>,
        "type cl_enginefunc_dst_t.pfnPlaySoundByNameAtPitch"
    );
    static_assert(o209 == offsetof(::cl_enginefunc_dst_t, pfnFillRGBABlend), "offsetof cl_enginefunc_dst_t.pfnFillRGBABlend");
    static_assert(
        std::is_same_v<t209, map_t<decltype(::cl_enginefunc_dst_t::pfnFillRGBABlend)>>,
        "type cl_enginefunc_dst_t.pfnFillRGBABlend"
    );
    static_assert(o210 == offsetof(::cl_enginefunc_dst_t, pfnGetAppID), "offsetof cl_enginefunc_dst_t.pfnGetAppID");
    static_assert(std::is_same_v<t210, map_t<decltype(::cl_enginefunc_dst_t::pfnGetAppID)>>, "type cl_enginefunc_dst_t.pfnGetAppID");
    static_assert(o211 == offsetof(::cl_enginefunc_dst_t, pfnGetAliasList), "offsetof cl_enginefunc_dst_t.pfnGetAliasList");
    static_assert(
        std::is_same_v<t211, map_t<decltype(::cl_enginefunc_dst_t::pfnGetAliasList)>>,
        "type cl_enginefunc_dst_t.pfnGetAliasList"
    );
    static_assert(
        o212 == offsetof(::cl_enginefunc_dst_t, pfnVguiWrap2_GetMouseDelta),
        "offsetof cl_enginefunc_dst_t.pfnVguiWrap2_GetMouseDelta"
    );
    static_assert(
        std::is_same_v<t212, map_t<decltype(::cl_enginefunc_dst_t::pfnVguiWrap2_GetMouseDelta)>>,
        "type cl_enginefunc_dst_t.pfnVguiWrap2_GetMouseDelta"
    );
    static_assert(s6 == sizeof(::cl_enginefuncs_s), "sizeof cl_enginefuncs_s");
    static_assert(o213 == offsetof(::cl_enginefuncs_s, pfnSPR_Load), "offsetof cl_enginefuncs_s.pfnSPR_Load");
    static_assert(std::is_same_v<t213, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Load)>>, "type cl_enginefuncs_s.pfnSPR_Load");
    static_assert(o214 == offsetof(::cl_enginefuncs_s, pfnSPR_Frames), "offsetof cl_enginefuncs_s.pfnSPR_Frames");
    static_assert(std::is_same_v<t214, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Frames)>>, "type cl_enginefuncs_s.pfnSPR_Frames");
    static_assert(o215 == offsetof(::cl_enginefuncs_s, pfnSPR_Height), "offsetof cl_enginefuncs_s.pfnSPR_Height");
    static_assert(std::is_same_v<t215, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Height)>>, "type cl_enginefuncs_s.pfnSPR_Height");
    static_assert(o216 == offsetof(::cl_enginefuncs_s, pfnSPR_Width), "offsetof cl_enginefuncs_s.pfnSPR_Width");
    static_assert(std::is_same_v<t216, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Width)>>, "type cl_enginefuncs_s.pfnSPR_Width");
    static_assert(o217 == offsetof(::cl_enginefuncs_s, pfnSPR_Set), "offsetof cl_enginefuncs_s.pfnSPR_Set");
    static_assert(std::is_same_v<t217, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Set)>>, "type cl_enginefuncs_s.pfnSPR_Set");
    static_assert(o218 == offsetof(::cl_enginefuncs_s, pfnSPR_Draw), "offsetof cl_enginefuncs_s.pfnSPR_Draw");
    static_assert(std::is_same_v<t218, map_t<decltype(::cl_enginefuncs_s::pfnSPR_Draw)>>, "type cl_enginefuncs_s.pfnSPR_Draw");
    static_assert(o219 == offsetof(::cl_enginefuncs_s, pfnSPR_DrawHoles), "offsetof cl_enginefuncs_s.pfnSPR_DrawHoles");
    static_assert(std::is_same_v<t219, map_t<decltype(::cl_enginefuncs_s::pfnSPR_DrawHoles)>>, "type cl_enginefuncs_s.pfnSPR_DrawHoles");
    static_assert(o220 == offsetof(::cl_enginefuncs_s, pfnSPR_DrawAdditive), "offsetof cl_enginefuncs_s.pfnSPR_DrawAdditive");
    static_assert(
        std::is_same_v<t220, map_t<decltype(::cl_enginefuncs_s::pfnSPR_DrawAdditive)>>,
        "type cl_enginefuncs_s.pfnSPR_DrawAdditive"
    );
    static_assert(o221 == offsetof(::cl_enginefuncs_s, pfnSPR_EnableScissor), "offsetof cl_enginefuncs_s.pfnSPR_EnableScissor");
    static_assert(
        std::is_same_v<t221, map_t<decltype(::cl_enginefuncs_s::pfnSPR_EnableScissor)>>,
        "type cl_enginefuncs_s.pfnSPR_EnableScissor"
    );
    static_assert(o222 == offsetof(::cl_enginefuncs_s, pfnSPR_DisableScissor), "offsetof cl_enginefuncs_s.pfnSPR_DisableScissor");
    static_assert(
        std::is_same_v<t222, map_t<decltype(::cl_enginefuncs_s::pfnSPR_DisableScissor)>>,
        "type cl_enginefuncs_s.pfnSPR_DisableScissor"
    );
    static_assert(o223 == offsetof(::cl_enginefuncs_s, pfnSPR_GetList), "offsetof cl_enginefuncs_s.pfnSPR_GetList");
    static_assert(std::is_same_v<t223, map_t<decltype(::cl_enginefuncs_s::pfnSPR_GetList)>>, "type cl_enginefuncs_s.pfnSPR_GetList");
    static_assert(o224 == offsetof(::cl_enginefuncs_s, pfnFillRGBA), "offsetof cl_enginefuncs_s.pfnFillRGBA");
    static_assert(std::is_same_v<t224, map_t<decltype(::cl_enginefuncs_s::pfnFillRGBA)>>, "type cl_enginefuncs_s.pfnFillRGBA");
    static_assert(o225 == offsetof(::cl_enginefuncs_s, pfnGetScreenInfo), "offsetof cl_enginefuncs_s.pfnGetScreenInfo");
    static_assert(std::is_same_v<t225, map_t<decltype(::cl_enginefuncs_s::pfnGetScreenInfo)>>, "type cl_enginefuncs_s.pfnGetScreenInfo");
    static_assert(o226 == offsetof(::cl_enginefuncs_s, pfnSetCrosshair), "offsetof cl_enginefuncs_s.pfnSetCrosshair");
    static_assert(std::is_same_v<t226, map_t<decltype(::cl_enginefuncs_s::pfnSetCrosshair)>>, "type cl_enginefuncs_s.pfnSetCrosshair");
    static_assert(o227 == offsetof(::cl_enginefuncs_s, pfnRegisterVariable), "offsetof cl_enginefuncs_s.pfnRegisterVariable");
    static_assert(
        std::is_same_v<t227, map_t<decltype(::cl_enginefuncs_s::pfnRegisterVariable)>>,
        "type cl_enginefuncs_s.pfnRegisterVariable"
    );
    static_assert(o228 == offsetof(::cl_enginefuncs_s, pfnGetCvarFloat), "offsetof cl_enginefuncs_s.pfnGetCvarFloat");
    static_assert(std::is_same_v<t228, map_t<decltype(::cl_enginefuncs_s::pfnGetCvarFloat)>>, "type cl_enginefuncs_s.pfnGetCvarFloat");
    static_assert(o229 == offsetof(::cl_enginefuncs_s, pfnGetCvarString), "offsetof cl_enginefuncs_s.pfnGetCvarString");
    static_assert(std::is_same_v<t229, map_t<decltype(::cl_enginefuncs_s::pfnGetCvarString)>>, "type cl_enginefuncs_s.pfnGetCvarString");
    static_assert(o230 == offsetof(::cl_enginefuncs_s, pfnAddCommand), "offsetof cl_enginefuncs_s.pfnAddCommand");
    static_assert(std::is_same_v<t230, map_t<decltype(::cl_enginefuncs_s::pfnAddCommand)>>, "type cl_enginefuncs_s.pfnAddCommand");
    static_assert(o231 == offsetof(::cl_enginefuncs_s, pfnHookUserMsg), "offsetof cl_enginefuncs_s.pfnHookUserMsg");
    static_assert(std::is_same_v<t231, map_t<decltype(::cl_enginefuncs_s::pfnHookUserMsg)>>, "type cl_enginefuncs_s.pfnHookUserMsg");
    static_assert(o232 == offsetof(::cl_enginefuncs_s, pfnServerCmd), "offsetof cl_enginefuncs_s.pfnServerCmd");
    static_assert(std::is_same_v<t232, map_t<decltype(::cl_enginefuncs_s::pfnServerCmd)>>, "type cl_enginefuncs_s.pfnServerCmd");
    static_assert(o233 == offsetof(::cl_enginefuncs_s, pfnClientCmd), "offsetof cl_enginefuncs_s.pfnClientCmd");
    static_assert(std::is_same_v<t233, map_t<decltype(::cl_enginefuncs_s::pfnClientCmd)>>, "type cl_enginefuncs_s.pfnClientCmd");
    static_assert(o234 == offsetof(::cl_enginefuncs_s, pfnGetPlayerInfo), "offsetof cl_enginefuncs_s.pfnGetPlayerInfo");
    static_assert(std::is_same_v<t234, map_t<decltype(::cl_enginefuncs_s::pfnGetPlayerInfo)>>, "type cl_enginefuncs_s.pfnGetPlayerInfo");
    static_assert(o235 == offsetof(::cl_enginefuncs_s, pfnPlaySoundByName), "offsetof cl_enginefuncs_s.pfnPlaySoundByName");
    static_assert(
        std::is_same_v<t235, map_t<decltype(::cl_enginefuncs_s::pfnPlaySoundByName)>>,
        "type cl_enginefuncs_s.pfnPlaySoundByName"
    );
    static_assert(o236 == offsetof(::cl_enginefuncs_s, pfnPlaySoundByIndex), "offsetof cl_enginefuncs_s.pfnPlaySoundByIndex");
    static_assert(
        std::is_same_v<t236, map_t<decltype(::cl_enginefuncs_s::pfnPlaySoundByIndex)>>,
        "type cl_enginefuncs_s.pfnPlaySoundByIndex"
    );
    static_assert(o237 == offsetof(::cl_enginefuncs_s, pfnAngleVectors), "offsetof cl_enginefuncs_s.pfnAngleVectors");
    static_assert(std::is_same_v<t237, map_t<decltype(::cl_enginefuncs_s::pfnAngleVectors)>>, "type cl_enginefuncs_s.pfnAngleVectors");
    static_assert(o238 == offsetof(::cl_enginefuncs_s, pfnTextMessageGet), "offsetof cl_enginefuncs_s.pfnTextMessageGet");
    static_assert(std::is_same_v<t238, map_t<decltype(::cl_enginefuncs_s::pfnTextMessageGet)>>, "type cl_enginefuncs_s.pfnTextMessageGet");
    static_assert(o239 == offsetof(::cl_enginefuncs_s, pfnDrawCharacter), "offsetof cl_enginefuncs_s.pfnDrawCharacter");
    static_assert(std::is_same_v<t239, map_t<decltype(::cl_enginefuncs_s::pfnDrawCharacter)>>, "type cl_enginefuncs_s.pfnDrawCharacter");
    static_assert(o240 == offsetof(::cl_enginefuncs_s, pfnDrawConsoleString), "offsetof cl_enginefuncs_s.pfnDrawConsoleString");
    static_assert(
        std::is_same_v<t240, map_t<decltype(::cl_enginefuncs_s::pfnDrawConsoleString)>>,
        "type cl_enginefuncs_s.pfnDrawConsoleString"
    );
    static_assert(o241 == offsetof(::cl_enginefuncs_s, pfnDrawSetTextColor), "offsetof cl_enginefuncs_s.pfnDrawSetTextColor");
    static_assert(
        std::is_same_v<t241, map_t<decltype(::cl_enginefuncs_s::pfnDrawSetTextColor)>>,
        "type cl_enginefuncs_s.pfnDrawSetTextColor"
    );
    static_assert(o242 == offsetof(::cl_enginefuncs_s, pfnDrawConsoleStringLen), "offsetof cl_enginefuncs_s.pfnDrawConsoleStringLen");
    static_assert(
        std::is_same_v<t242, map_t<decltype(::cl_enginefuncs_s::pfnDrawConsoleStringLen)>>,
        "type cl_enginefuncs_s.pfnDrawConsoleStringLen"
    );
    static_assert(o243 == offsetof(::cl_enginefuncs_s, pfnConsolePrint), "offsetof cl_enginefuncs_s.pfnConsolePrint");
    static_assert(std::is_same_v<t243, map_t<decltype(::cl_enginefuncs_s::pfnConsolePrint)>>, "type cl_enginefuncs_s.pfnConsolePrint");
    static_assert(o244 == offsetof(::cl_enginefuncs_s, pfnCenterPrint), "offsetof cl_enginefuncs_s.pfnCenterPrint");
    static_assert(std::is_same_v<t244, map_t<decltype(::cl_enginefuncs_s::pfnCenterPrint)>>, "type cl_enginefuncs_s.pfnCenterPrint");
    static_assert(o245 == offsetof(::cl_enginefuncs_s, GetWindowCenterX), "offsetof cl_enginefuncs_s.GetWindowCenterX");
    static_assert(std::is_same_v<t245, map_t<decltype(::cl_enginefuncs_s::GetWindowCenterX)>>, "type cl_enginefuncs_s.GetWindowCenterX");
    static_assert(o246 == offsetof(::cl_enginefuncs_s, GetWindowCenterY), "offsetof cl_enginefuncs_s.GetWindowCenterY");
    static_assert(std::is_same_v<t246, map_t<decltype(::cl_enginefuncs_s::GetWindowCenterY)>>, "type cl_enginefuncs_s.GetWindowCenterY");
    static_assert(o247 == offsetof(::cl_enginefuncs_s, GetViewAngles), "offsetof cl_enginefuncs_s.GetViewAngles");
    static_assert(std::is_same_v<t247, map_t<decltype(::cl_enginefuncs_s::GetViewAngles)>>, "type cl_enginefuncs_s.GetViewAngles");
    static_assert(o248 == offsetof(::cl_enginefuncs_s, SetViewAngles), "offsetof cl_enginefuncs_s.SetViewAngles");
    static_assert(std::is_same_v<t248, map_t<decltype(::cl_enginefuncs_s::SetViewAngles)>>, "type cl_enginefuncs_s.SetViewAngles");
    static_assert(o249 == offsetof(::cl_enginefuncs_s, GetMaxClients), "offsetof cl_enginefuncs_s.GetMaxClients");
    static_assert(std::is_same_v<t249, map_t<decltype(::cl_enginefuncs_s::GetMaxClients)>>, "type cl_enginefuncs_s.GetMaxClients");
    static_assert(o250 == offsetof(::cl_enginefuncs_s, Cvar_SetValue), "offsetof cl_enginefuncs_s.Cvar_SetValue");
    static_assert(std::is_same_v<t250, map_t<decltype(::cl_enginefuncs_s::Cvar_SetValue)>>, "type cl_enginefuncs_s.Cvar_SetValue");
    static_assert(o251 == offsetof(::cl_enginefuncs_s, Cmd_Argc), "offsetof cl_enginefuncs_s.Cmd_Argc");
    static_assert(std::is_same_v<t251, map_t<decltype(::cl_enginefuncs_s::Cmd_Argc)>>, "type cl_enginefuncs_s.Cmd_Argc");
    static_assert(o252 == offsetof(::cl_enginefuncs_s, Cmd_Argv), "offsetof cl_enginefuncs_s.Cmd_Argv");
    static_assert(std::is_same_v<t252, map_t<decltype(::cl_enginefuncs_s::Cmd_Argv)>>, "type cl_enginefuncs_s.Cmd_Argv");
    static_assert(o253 == offsetof(::cl_enginefuncs_s, Con_Printf), "offsetof cl_enginefuncs_s.Con_Printf");
    static_assert(std::is_same_v<t253, map_t<decltype(::cl_enginefuncs_s::Con_Printf)>>, "type cl_enginefuncs_s.Con_Printf");
    static_assert(o254 == offsetof(::cl_enginefuncs_s, Con_DPrintf), "offsetof cl_enginefuncs_s.Con_DPrintf");
    static_assert(std::is_same_v<t254, map_t<decltype(::cl_enginefuncs_s::Con_DPrintf)>>, "type cl_enginefuncs_s.Con_DPrintf");
    static_assert(o255 == offsetof(::cl_enginefuncs_s, Con_NPrintf), "offsetof cl_enginefuncs_s.Con_NPrintf");
    static_assert(std::is_same_v<t255, map_t<decltype(::cl_enginefuncs_s::Con_NPrintf)>>, "type cl_enginefuncs_s.Con_NPrintf");
    static_assert(o256 == offsetof(::cl_enginefuncs_s, Con_NXPrintf), "offsetof cl_enginefuncs_s.Con_NXPrintf");
    static_assert(std::is_same_v<t256, map_t<decltype(::cl_enginefuncs_s::Con_NXPrintf)>>, "type cl_enginefuncs_s.Con_NXPrintf");
    static_assert(o257 == offsetof(::cl_enginefuncs_s, PhysInfo_ValueForKey), "offsetof cl_enginefuncs_s.PhysInfo_ValueForKey");
    static_assert(
        std::is_same_v<t257, map_t<decltype(::cl_enginefuncs_s::PhysInfo_ValueForKey)>>,
        "type cl_enginefuncs_s.PhysInfo_ValueForKey"
    );
    static_assert(o258 == offsetof(::cl_enginefuncs_s, ServerInfo_ValueForKey), "offsetof cl_enginefuncs_s.ServerInfo_ValueForKey");
    static_assert(
        std::is_same_v<t258, map_t<decltype(::cl_enginefuncs_s::ServerInfo_ValueForKey)>>,
        "type cl_enginefuncs_s.ServerInfo_ValueForKey"
    );
    static_assert(o259 == offsetof(::cl_enginefuncs_s, GetClientMaxspeed), "offsetof cl_enginefuncs_s.GetClientMaxspeed");
    static_assert(std::is_same_v<t259, map_t<decltype(::cl_enginefuncs_s::GetClientMaxspeed)>>, "type cl_enginefuncs_s.GetClientMaxspeed");
    static_assert(o260 == offsetof(::cl_enginefuncs_s, CheckParm), "offsetof cl_enginefuncs_s.CheckParm");
    static_assert(std::is_same_v<t260, map_t<decltype(::cl_enginefuncs_s::CheckParm)>>, "type cl_enginefuncs_s.CheckParm");
    static_assert(o261 == offsetof(::cl_enginefuncs_s, Key_Event), "offsetof cl_enginefuncs_s.Key_Event");
    static_assert(std::is_same_v<t261, map_t<decltype(::cl_enginefuncs_s::Key_Event)>>, "type cl_enginefuncs_s.Key_Event");
    static_assert(o262 == offsetof(::cl_enginefuncs_s, GetMousePosition), "offsetof cl_enginefuncs_s.GetMousePosition");
    static_assert(std::is_same_v<t262, map_t<decltype(::cl_enginefuncs_s::GetMousePosition)>>, "type cl_enginefuncs_s.GetMousePosition");
    static_assert(o263 == offsetof(::cl_enginefuncs_s, IsNoClipping), "offsetof cl_enginefuncs_s.IsNoClipping");
    static_assert(std::is_same_v<t263, map_t<decltype(::cl_enginefuncs_s::IsNoClipping)>>, "type cl_enginefuncs_s.IsNoClipping");
    static_assert(o264 == offsetof(::cl_enginefuncs_s, GetLocalPlayer), "offsetof cl_enginefuncs_s.GetLocalPlayer");
    static_assert(std::is_same_v<t264, map_t<decltype(::cl_enginefuncs_s::GetLocalPlayer)>>, "type cl_enginefuncs_s.GetLocalPlayer");
    static_assert(o265 == offsetof(::cl_enginefuncs_s, GetViewModel), "offsetof cl_enginefuncs_s.GetViewModel");
    static_assert(std::is_same_v<t265, map_t<decltype(::cl_enginefuncs_s::GetViewModel)>>, "type cl_enginefuncs_s.GetViewModel");
    static_assert(o266 == offsetof(::cl_enginefuncs_s, GetEntityByIndex), "offsetof cl_enginefuncs_s.GetEntityByIndex");
    static_assert(std::is_same_v<t266, map_t<decltype(::cl_enginefuncs_s::GetEntityByIndex)>>, "type cl_enginefuncs_s.GetEntityByIndex");
    static_assert(o267 == offsetof(::cl_enginefuncs_s, GetClientTime), "offsetof cl_enginefuncs_s.GetClientTime");
    static_assert(std::is_same_v<t267, map_t<decltype(::cl_enginefuncs_s::GetClientTime)>>, "type cl_enginefuncs_s.GetClientTime");
    static_assert(o268 == offsetof(::cl_enginefuncs_s, V_CalcShake), "offsetof cl_enginefuncs_s.V_CalcShake");
    static_assert(std::is_same_v<t268, map_t<decltype(::cl_enginefuncs_s::V_CalcShake)>>, "type cl_enginefuncs_s.V_CalcShake");
    static_assert(o269 == offsetof(::cl_enginefuncs_s, V_ApplyShake), "offsetof cl_enginefuncs_s.V_ApplyShake");
    static_assert(std::is_same_v<t269, map_t<decltype(::cl_enginefuncs_s::V_ApplyShake)>>, "type cl_enginefuncs_s.V_ApplyShake");
    static_assert(o270 == offsetof(::cl_enginefuncs_s, PM_PointContents), "offsetof cl_enginefuncs_s.PM_PointContents");
    static_assert(std::is_same_v<t270, map_t<decltype(::cl_enginefuncs_s::PM_PointContents)>>, "type cl_enginefuncs_s.PM_PointContents");
    static_assert(o271 == offsetof(::cl_enginefuncs_s, PM_WaterEntity), "offsetof cl_enginefuncs_s.PM_WaterEntity");
    static_assert(std::is_same_v<t271, map_t<decltype(::cl_enginefuncs_s::PM_WaterEntity)>>, "type cl_enginefuncs_s.PM_WaterEntity");
    static_assert(o272 == offsetof(::cl_enginefuncs_s, PM_TraceLine), "offsetof cl_enginefuncs_s.PM_TraceLine");
    static_assert(std::is_same_v<t272, map_t<decltype(::cl_enginefuncs_s::PM_TraceLine)>>, "type cl_enginefuncs_s.PM_TraceLine");
    static_assert(o273 == offsetof(::cl_enginefuncs_s, CL_LoadModel), "offsetof cl_enginefuncs_s.CL_LoadModel");
    static_assert(std::is_same_v<t273, map_t<decltype(::cl_enginefuncs_s::CL_LoadModel)>>, "type cl_enginefuncs_s.CL_LoadModel");
    static_assert(o274 == offsetof(::cl_enginefuncs_s, CL_CreateVisibleEntity), "offsetof cl_enginefuncs_s.CL_CreateVisibleEntity");
    static_assert(
        std::is_same_v<t274, map_t<decltype(::cl_enginefuncs_s::CL_CreateVisibleEntity)>>,
        "type cl_enginefuncs_s.CL_CreateVisibleEntity"
    );
    static_assert(o275 == offsetof(::cl_enginefuncs_s, GetSpritePointer), "offsetof cl_enginefuncs_s.GetSpritePointer");
    static_assert(std::is_same_v<t275, map_t<decltype(::cl_enginefuncs_s::GetSpritePointer)>>, "type cl_enginefuncs_s.GetSpritePointer");
    static_assert(
        o276 == offsetof(::cl_enginefuncs_s, pfnPlaySoundByNameAtLocation),
        "offsetof cl_enginefuncs_s.pfnPlaySoundByNameAtLocation"
    );
    static_assert(
        std::is_same_v<t276, map_t<decltype(::cl_enginefuncs_s::pfnPlaySoundByNameAtLocation)>>,
        "type cl_enginefuncs_s.pfnPlaySoundByNameAtLocation"
    );
    static_assert(o277 == offsetof(::cl_enginefuncs_s, pfnPrecacheEvent), "offsetof cl_enginefuncs_s.pfnPrecacheEvent");
    static_assert(std::is_same_v<t277, map_t<decltype(::cl_enginefuncs_s::pfnPrecacheEvent)>>, "type cl_enginefuncs_s.pfnPrecacheEvent");
    static_assert(o278 == offsetof(::cl_enginefuncs_s, pfnPlaybackEvent), "offsetof cl_enginefuncs_s.pfnPlaybackEvent");
    static_assert(std::is_same_v<t278, map_t<decltype(::cl_enginefuncs_s::pfnPlaybackEvent)>>, "type cl_enginefuncs_s.pfnPlaybackEvent");
    static_assert(o279 == offsetof(::cl_enginefuncs_s, pfnWeaponAnim), "offsetof cl_enginefuncs_s.pfnWeaponAnim");
    static_assert(std::is_same_v<t279, map_t<decltype(::cl_enginefuncs_s::pfnWeaponAnim)>>, "type cl_enginefuncs_s.pfnWeaponAnim");
    static_assert(o280 == offsetof(::cl_enginefuncs_s, pfnRandomFloat), "offsetof cl_enginefuncs_s.pfnRandomFloat");
    static_assert(std::is_same_v<t280, map_t<decltype(::cl_enginefuncs_s::pfnRandomFloat)>>, "type cl_enginefuncs_s.pfnRandomFloat");
    static_assert(o281 == offsetof(::cl_enginefuncs_s, pfnRandomLong), "offsetof cl_enginefuncs_s.pfnRandomLong");
    static_assert(std::is_same_v<t281, map_t<decltype(::cl_enginefuncs_s::pfnRandomLong)>>, "type cl_enginefuncs_s.pfnRandomLong");
    static_assert(o282 == offsetof(::cl_enginefuncs_s, pfnHookEvent), "offsetof cl_enginefuncs_s.pfnHookEvent");
    static_assert(std::is_same_v<t282, map_t<decltype(::cl_enginefuncs_s::pfnHookEvent)>>, "type cl_enginefuncs_s.pfnHookEvent");
    static_assert(o283 == offsetof(::cl_enginefuncs_s, Con_IsVisible), "offsetof cl_enginefuncs_s.Con_IsVisible");
    static_assert(std::is_same_v<t283, map_t<decltype(::cl_enginefuncs_s::Con_IsVisible)>>, "type cl_enginefuncs_s.Con_IsVisible");
    static_assert(o284 == offsetof(::cl_enginefuncs_s, pfnGetGameDirectory), "offsetof cl_enginefuncs_s.pfnGetGameDirectory");
    static_assert(
        std::is_same_v<t284, map_t<decltype(::cl_enginefuncs_s::pfnGetGameDirectory)>>,
        "type cl_enginefuncs_s.pfnGetGameDirectory"
    );
    static_assert(o285 == offsetof(::cl_enginefuncs_s, pfnGetCvarPointer), "offsetof cl_enginefuncs_s.pfnGetCvarPointer");
    static_assert(std::is_same_v<t285, map_t<decltype(::cl_enginefuncs_s::pfnGetCvarPointer)>>, "type cl_enginefuncs_s.pfnGetCvarPointer");
    static_assert(o286 == offsetof(::cl_enginefuncs_s, Key_LookupBinding), "offsetof cl_enginefuncs_s.Key_LookupBinding");
    static_assert(std::is_same_v<t286, map_t<decltype(::cl_enginefuncs_s::Key_LookupBinding)>>, "type cl_enginefuncs_s.Key_LookupBinding");
    static_assert(o287 == offsetof(::cl_enginefuncs_s, pfnGetLevelName), "offsetof cl_enginefuncs_s.pfnGetLevelName");
    static_assert(std::is_same_v<t287, map_t<decltype(::cl_enginefuncs_s::pfnGetLevelName)>>, "type cl_enginefuncs_s.pfnGetLevelName");
    static_assert(o288 == offsetof(::cl_enginefuncs_s, pfnGetScreenFade), "offsetof cl_enginefuncs_s.pfnGetScreenFade");
    static_assert(std::is_same_v<t288, map_t<decltype(::cl_enginefuncs_s::pfnGetScreenFade)>>, "type cl_enginefuncs_s.pfnGetScreenFade");
    static_assert(o289 == offsetof(::cl_enginefuncs_s, pfnSetScreenFade), "offsetof cl_enginefuncs_s.pfnSetScreenFade");
    static_assert(std::is_same_v<t289, map_t<decltype(::cl_enginefuncs_s::pfnSetScreenFade)>>, "type cl_enginefuncs_s.pfnSetScreenFade");
    static_assert(o290 == offsetof(::cl_enginefuncs_s, VGui_GetPanel), "offsetof cl_enginefuncs_s.VGui_GetPanel");
    static_assert(std::is_same_v<t290, map_t<decltype(::cl_enginefuncs_s::VGui_GetPanel)>>, "type cl_enginefuncs_s.VGui_GetPanel");
    static_assert(
        o291 == offsetof(::cl_enginefuncs_s, VGui_ViewportPaintBackground),
        "offsetof cl_enginefuncs_s.VGui_ViewportPaintBackground"
    );
    static_assert(
        std::is_same_v<t291, map_t<decltype(::cl_enginefuncs_s::VGui_ViewportPaintBackground)>>,
        "type cl_enginefuncs_s.VGui_ViewportPaintBackground"
    );
    static_assert(o292 == offsetof(::cl_enginefuncs_s, COM_LoadFile), "offsetof cl_enginefuncs_s.COM_LoadFile");
    static_assert(std::is_same_v<t292, map_t<decltype(::cl_enginefuncs_s::COM_LoadFile)>>, "type cl_enginefuncs_s.COM_LoadFile");
    static_assert(o293 == offsetof(::cl_enginefuncs_s, COM_ParseFile), "offsetof cl_enginefuncs_s.COM_ParseFile");
    static_assert(std::is_same_v<t293, map_t<decltype(::cl_enginefuncs_s::COM_ParseFile)>>, "type cl_enginefuncs_s.COM_ParseFile");
    static_assert(o294 == offsetof(::cl_enginefuncs_s, COM_FreeFile), "offsetof cl_enginefuncs_s.COM_FreeFile");
    static_assert(std::is_same_v<t294, map_t<decltype(::cl_enginefuncs_s::COM_FreeFile)>>, "type cl_enginefuncs_s.COM_FreeFile");
    static_assert(o295 == offsetof(::cl_enginefuncs_s, pTriAPI), "offsetof cl_enginefuncs_s.pTriAPI");
    static_assert(std::is_same_v<t295, map_t<decltype(::cl_enginefuncs_s::pTriAPI)>>, "type cl_enginefuncs_s.pTriAPI");
    static_assert(o296 == offsetof(::cl_enginefuncs_s, pEfxAPI), "offsetof cl_enginefuncs_s.pEfxAPI");
    static_assert(std::is_same_v<t296, map_t<decltype(::cl_enginefuncs_s::pEfxAPI)>>, "type cl_enginefuncs_s.pEfxAPI");
    static_assert(o297 == offsetof(::cl_enginefuncs_s, pEventAPI), "offsetof cl_enginefuncs_s.pEventAPI");
    static_assert(std::is_same_v<t297, map_t<decltype(::cl_enginefuncs_s::pEventAPI)>>, "type cl_enginefuncs_s.pEventAPI");
    static_assert(o298 == offsetof(::cl_enginefuncs_s, pDemoAPI), "offsetof cl_enginefuncs_s.pDemoAPI");
    static_assert(std::is_same_v<t298, map_t<decltype(::cl_enginefuncs_s::pDemoAPI)>>, "type cl_enginefuncs_s.pDemoAPI");
    static_assert(o299 == offsetof(::cl_enginefuncs_s, pNetAPI), "offsetof cl_enginefuncs_s.pNetAPI");
    static_assert(std::is_same_v<t299, map_t<decltype(::cl_enginefuncs_s::pNetAPI)>>, "type cl_enginefuncs_s.pNetAPI");
    static_assert(o300 == offsetof(::cl_enginefuncs_s, pVoiceTweak), "offsetof cl_enginefuncs_s.pVoiceTweak");
    static_assert(std::is_same_v<t300, map_t<decltype(::cl_enginefuncs_s::pVoiceTweak)>>, "type cl_enginefuncs_s.pVoiceTweak");
    static_assert(o301 == offsetof(::cl_enginefuncs_s, IsSpectateOnly), "offsetof cl_enginefuncs_s.IsSpectateOnly");
    static_assert(std::is_same_v<t301, map_t<decltype(::cl_enginefuncs_s::IsSpectateOnly)>>, "type cl_enginefuncs_s.IsSpectateOnly");
    static_assert(o302 == offsetof(::cl_enginefuncs_s, LoadMapSprite), "offsetof cl_enginefuncs_s.LoadMapSprite");
    static_assert(std::is_same_v<t302, map_t<decltype(::cl_enginefuncs_s::LoadMapSprite)>>, "type cl_enginefuncs_s.LoadMapSprite");
    static_assert(
        o303 == offsetof(::cl_enginefuncs_s, COM_AddAppDirectoryToSearchPath),
        "offsetof cl_enginefuncs_s.COM_AddAppDirectoryToSearchPath"
    );
    static_assert(
        std::is_same_v<t303, map_t<decltype(::cl_enginefuncs_s::COM_AddAppDirectoryToSearchPath)>>,
        "type cl_enginefuncs_s.COM_AddAppDirectoryToSearchPath"
    );
    static_assert(o304 == offsetof(::cl_enginefuncs_s, COM_ExpandFilename), "offsetof cl_enginefuncs_s.COM_ExpandFilename");
    static_assert(
        std::is_same_v<t304, map_t<decltype(::cl_enginefuncs_s::COM_ExpandFilename)>>,
        "type cl_enginefuncs_s.COM_ExpandFilename"
    );
    static_assert(o305 == offsetof(::cl_enginefuncs_s, PlayerInfo_ValueForKey), "offsetof cl_enginefuncs_s.PlayerInfo_ValueForKey");
    static_assert(
        std::is_same_v<t305, map_t<decltype(::cl_enginefuncs_s::PlayerInfo_ValueForKey)>>,
        "type cl_enginefuncs_s.PlayerInfo_ValueForKey"
    );
    static_assert(o306 == offsetof(::cl_enginefuncs_s, PlayerInfo_SetValueForKey), "offsetof cl_enginefuncs_s.PlayerInfo_SetValueForKey");
    static_assert(
        std::is_same_v<t306, map_t<decltype(::cl_enginefuncs_s::PlayerInfo_SetValueForKey)>>,
        "type cl_enginefuncs_s.PlayerInfo_SetValueForKey"
    );
    static_assert(o307 == offsetof(::cl_enginefuncs_s, GetPlayerUniqueID), "offsetof cl_enginefuncs_s.GetPlayerUniqueID");
    static_assert(std::is_same_v<t307, map_t<decltype(::cl_enginefuncs_s::GetPlayerUniqueID)>>, "type cl_enginefuncs_s.GetPlayerUniqueID");
    static_assert(o308 == offsetof(::cl_enginefuncs_s, GetTrackerIDForPlayer), "offsetof cl_enginefuncs_s.GetTrackerIDForPlayer");
    static_assert(
        std::is_same_v<t308, map_t<decltype(::cl_enginefuncs_s::GetTrackerIDForPlayer)>>,
        "type cl_enginefuncs_s.GetTrackerIDForPlayer"
    );
    static_assert(o309 == offsetof(::cl_enginefuncs_s, GetPlayerForTrackerID), "offsetof cl_enginefuncs_s.GetPlayerForTrackerID");
    static_assert(
        std::is_same_v<t309, map_t<decltype(::cl_enginefuncs_s::GetPlayerForTrackerID)>>,
        "type cl_enginefuncs_s.GetPlayerForTrackerID"
    );
    static_assert(o310 == offsetof(::cl_enginefuncs_s, pfnServerCmdUnreliable), "offsetof cl_enginefuncs_s.pfnServerCmdUnreliable");
    static_assert(
        std::is_same_v<t310, map_t<decltype(::cl_enginefuncs_s::pfnServerCmdUnreliable)>>,
        "type cl_enginefuncs_s.pfnServerCmdUnreliable"
    );
    static_assert(o311 == offsetof(::cl_enginefuncs_s, pfnGetMousePos), "offsetof cl_enginefuncs_s.pfnGetMousePos");
    static_assert(std::is_same_v<t311, map_t<decltype(::cl_enginefuncs_s::pfnGetMousePos)>>, "type cl_enginefuncs_s.pfnGetMousePos");
    static_assert(o312 == offsetof(::cl_enginefuncs_s, pfnSetMousePos), "offsetof cl_enginefuncs_s.pfnSetMousePos");
    static_assert(std::is_same_v<t312, map_t<decltype(::cl_enginefuncs_s::pfnSetMousePos)>>, "type cl_enginefuncs_s.pfnSetMousePos");
    static_assert(o313 == offsetof(::cl_enginefuncs_s, pfnSetMouseEnable), "offsetof cl_enginefuncs_s.pfnSetMouseEnable");
    static_assert(std::is_same_v<t313, map_t<decltype(::cl_enginefuncs_s::pfnSetMouseEnable)>>, "type cl_enginefuncs_s.pfnSetMouseEnable");
    static_assert(o314 == offsetof(::cl_enginefuncs_s, GetFirstCvarPtr), "offsetof cl_enginefuncs_s.GetFirstCvarPtr");
    static_assert(std::is_same_v<t314, map_t<decltype(::cl_enginefuncs_s::GetFirstCvarPtr)>>, "type cl_enginefuncs_s.GetFirstCvarPtr");
    static_assert(o315 == offsetof(::cl_enginefuncs_s, GetFirstCmdFunctionHandle), "offsetof cl_enginefuncs_s.GetFirstCmdFunctionHandle");
    static_assert(
        std::is_same_v<t315, map_t<decltype(::cl_enginefuncs_s::GetFirstCmdFunctionHandle)>>,
        "type cl_enginefuncs_s.GetFirstCmdFunctionHandle"
    );
    static_assert(o316 == offsetof(::cl_enginefuncs_s, GetNextCmdFunctionHandle), "offsetof cl_enginefuncs_s.GetNextCmdFunctionHandle");
    static_assert(
        std::is_same_v<t316, map_t<decltype(::cl_enginefuncs_s::GetNextCmdFunctionHandle)>>,
        "type cl_enginefuncs_s.GetNextCmdFunctionHandle"
    );
    static_assert(o317 == offsetof(::cl_enginefuncs_s, GetCmdFunctionName), "offsetof cl_enginefuncs_s.GetCmdFunctionName");
    static_assert(
        std::is_same_v<t317, map_t<decltype(::cl_enginefuncs_s::GetCmdFunctionName)>>,
        "type cl_enginefuncs_s.GetCmdFunctionName"
    );
    static_assert(o318 == offsetof(::cl_enginefuncs_s, hudGetClientOldTime), "offsetof cl_enginefuncs_s.hudGetClientOldTime");
    static_assert(
        std::is_same_v<t318, map_t<decltype(::cl_enginefuncs_s::hudGetClientOldTime)>>,
        "type cl_enginefuncs_s.hudGetClientOldTime"
    );
    static_assert(o319 == offsetof(::cl_enginefuncs_s, hudGetServerGravityValue), "offsetof cl_enginefuncs_s.hudGetServerGravityValue");
    static_assert(
        std::is_same_v<t319, map_t<decltype(::cl_enginefuncs_s::hudGetServerGravityValue)>>,
        "type cl_enginefuncs_s.hudGetServerGravityValue"
    );
    static_assert(o320 == offsetof(::cl_enginefuncs_s, hudGetModelByIndex), "offsetof cl_enginefuncs_s.hudGetModelByIndex");
    static_assert(
        std::is_same_v<t320, map_t<decltype(::cl_enginefuncs_s::hudGetModelByIndex)>>,
        "type cl_enginefuncs_s.hudGetModelByIndex"
    );
    static_assert(o321 == offsetof(::cl_enginefuncs_s, pfnSetFilterMode), "offsetof cl_enginefuncs_s.pfnSetFilterMode");
    static_assert(std::is_same_v<t321, map_t<decltype(::cl_enginefuncs_s::pfnSetFilterMode)>>, "type cl_enginefuncs_s.pfnSetFilterMode");
    static_assert(o322 == offsetof(::cl_enginefuncs_s, pfnSetFilterColor), "offsetof cl_enginefuncs_s.pfnSetFilterColor");
    static_assert(std::is_same_v<t322, map_t<decltype(::cl_enginefuncs_s::pfnSetFilterColor)>>, "type cl_enginefuncs_s.pfnSetFilterColor");
    static_assert(o323 == offsetof(::cl_enginefuncs_s, pfnSetFilterBrightness), "offsetof cl_enginefuncs_s.pfnSetFilterBrightness");
    static_assert(
        std::is_same_v<t323, map_t<decltype(::cl_enginefuncs_s::pfnSetFilterBrightness)>>,
        "type cl_enginefuncs_s.pfnSetFilterBrightness"
    );
    static_assert(o324 == offsetof(::cl_enginefuncs_s, pfnSequenceGet), "offsetof cl_enginefuncs_s.pfnSequenceGet");
    static_assert(std::is_same_v<t324, map_t<decltype(::cl_enginefuncs_s::pfnSequenceGet)>>, "type cl_enginefuncs_s.pfnSequenceGet");
    static_assert(o325 == offsetof(::cl_enginefuncs_s, pfnSPR_DrawGeneric), "offsetof cl_enginefuncs_s.pfnSPR_DrawGeneric");
    static_assert(
        std::is_same_v<t325, map_t<decltype(::cl_enginefuncs_s::pfnSPR_DrawGeneric)>>,
        "type cl_enginefuncs_s.pfnSPR_DrawGeneric"
    );
    static_assert(o326 == offsetof(::cl_enginefuncs_s, pfnSequencePickSentence), "offsetof cl_enginefuncs_s.pfnSequencePickSentence");
    static_assert(
        std::is_same_v<t326, map_t<decltype(::cl_enginefuncs_s::pfnSequencePickSentence)>>,
        "type cl_enginefuncs_s.pfnSequencePickSentence"
    );
    static_assert(o327 == offsetof(::cl_enginefuncs_s, pfnDrawString), "offsetof cl_enginefuncs_s.pfnDrawString");
    static_assert(std::is_same_v<t327, map_t<decltype(::cl_enginefuncs_s::pfnDrawString)>>, "type cl_enginefuncs_s.pfnDrawString");
    static_assert(o328 == offsetof(::cl_enginefuncs_s, pfnDrawStringReverse), "offsetof cl_enginefuncs_s.pfnDrawStringReverse");
    static_assert(
        std::is_same_v<t328, map_t<decltype(::cl_enginefuncs_s::pfnDrawStringReverse)>>,
        "type cl_enginefuncs_s.pfnDrawStringReverse"
    );
    static_assert(
        o329 == offsetof(::cl_enginefuncs_s, LocalPlayerInfo_ValueForKey),
        "offsetof cl_enginefuncs_s.LocalPlayerInfo_ValueForKey"
    );
    static_assert(
        std::is_same_v<t329, map_t<decltype(::cl_enginefuncs_s::LocalPlayerInfo_ValueForKey)>>,
        "type cl_enginefuncs_s.LocalPlayerInfo_ValueForKey"
    );
    static_assert(o330 == offsetof(::cl_enginefuncs_s, pfnVGUI2DrawCharacter), "offsetof cl_enginefuncs_s.pfnVGUI2DrawCharacter");
    static_assert(
        std::is_same_v<t330, map_t<decltype(::cl_enginefuncs_s::pfnVGUI2DrawCharacter)>>,
        "type cl_enginefuncs_s.pfnVGUI2DrawCharacter"
    );
    static_assert(o331 == offsetof(::cl_enginefuncs_s, pfnVGUI2DrawCharacterAdd), "offsetof cl_enginefuncs_s.pfnVGUI2DrawCharacterAdd");
    static_assert(
        std::is_same_v<t331, map_t<decltype(::cl_enginefuncs_s::pfnVGUI2DrawCharacterAdd)>>,
        "type cl_enginefuncs_s.pfnVGUI2DrawCharacterAdd"
    );
    static_assert(
        o332 == offsetof(::cl_enginefuncs_s, COM_GetApproxWavePlayLength),
        "offsetof cl_enginefuncs_s.COM_GetApproxWavePlayLength"
    );
    static_assert(
        std::is_same_v<t332, map_t<decltype(::cl_enginefuncs_s::COM_GetApproxWavePlayLength)>>,
        "type cl_enginefuncs_s.COM_GetApproxWavePlayLength"
    );
    static_assert(o333 == offsetof(::cl_enginefuncs_s, pfnGetCareerUI), "offsetof cl_enginefuncs_s.pfnGetCareerUI");
    static_assert(std::is_same_v<t333, map_t<decltype(::cl_enginefuncs_s::pfnGetCareerUI)>>, "type cl_enginefuncs_s.pfnGetCareerUI");
    static_assert(o334 == offsetof(::cl_enginefuncs_s, Cvar_Set), "offsetof cl_enginefuncs_s.Cvar_Set");
    static_assert(std::is_same_v<t334, map_t<decltype(::cl_enginefuncs_s::Cvar_Set)>>, "type cl_enginefuncs_s.Cvar_Set");
    static_assert(o335 == offsetof(::cl_enginefuncs_s, pfnIsCareerMatch), "offsetof cl_enginefuncs_s.pfnIsCareerMatch");
    static_assert(std::is_same_v<t335, map_t<decltype(::cl_enginefuncs_s::pfnIsCareerMatch)>>, "type cl_enginefuncs_s.pfnIsCareerMatch");
    static_assert(o336 == offsetof(::cl_enginefuncs_s, pfnPlaySoundVoiceByName), "offsetof cl_enginefuncs_s.pfnPlaySoundVoiceByName");
    static_assert(
        std::is_same_v<t336, map_t<decltype(::cl_enginefuncs_s::pfnPlaySoundVoiceByName)>>,
        "type cl_enginefuncs_s.pfnPlaySoundVoiceByName"
    );
    static_assert(o337 == offsetof(::cl_enginefuncs_s, pfnPrimeMusicStream), "offsetof cl_enginefuncs_s.pfnPrimeMusicStream");
    static_assert(
        std::is_same_v<t337, map_t<decltype(::cl_enginefuncs_s::pfnPrimeMusicStream)>>,
        "type cl_enginefuncs_s.pfnPrimeMusicStream"
    );
    static_assert(o338 == offsetof(::cl_enginefuncs_s, GetAbsoluteTime), "offsetof cl_enginefuncs_s.GetAbsoluteTime");
    static_assert(std::is_same_v<t338, map_t<decltype(::cl_enginefuncs_s::GetAbsoluteTime)>>, "type cl_enginefuncs_s.GetAbsoluteTime");
    static_assert(
        o339 == offsetof(::cl_enginefuncs_s, pfnProcessTutorMessageDecayBuffer),
        "offsetof cl_enginefuncs_s.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t339, map_t<decltype(::cl_enginefuncs_s::pfnProcessTutorMessageDecayBuffer)>>,
        "type cl_enginefuncs_s.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        o340 == offsetof(::cl_enginefuncs_s, pfnConstructTutorMessageDecayBuffer),
        "offsetof cl_enginefuncs_s.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t340, map_t<decltype(::cl_enginefuncs_s::pfnConstructTutorMessageDecayBuffer)>>,
        "type cl_enginefuncs_s.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        o341 == offsetof(::cl_enginefuncs_s, pfnResetTutorMessageDecayData),
        "offsetof cl_enginefuncs_s.pfnResetTutorMessageDecayData"
    );
    static_assert(
        std::is_same_v<t341, map_t<decltype(::cl_enginefuncs_s::pfnResetTutorMessageDecayData)>>,
        "type cl_enginefuncs_s.pfnResetTutorMessageDecayData"
    );
    static_assert(o342 == offsetof(::cl_enginefuncs_s, pfnPlaySoundByNameAtPitch), "offsetof cl_enginefuncs_s.pfnPlaySoundByNameAtPitch");
    static_assert(
        std::is_same_v<t342, map_t<decltype(::cl_enginefuncs_s::pfnPlaySoundByNameAtPitch)>>,
        "type cl_enginefuncs_s.pfnPlaySoundByNameAtPitch"
    );
    static_assert(o343 == offsetof(::cl_enginefuncs_s, pfnFillRGBABlend), "offsetof cl_enginefuncs_s.pfnFillRGBABlend");
    static_assert(std::is_same_v<t343, map_t<decltype(::cl_enginefuncs_s::pfnFillRGBABlend)>>, "type cl_enginefuncs_s.pfnFillRGBABlend");
    static_assert(o344 == offsetof(::cl_enginefuncs_s, pfnGetAppID), "offsetof cl_enginefuncs_s.pfnGetAppID");
    static_assert(std::is_same_v<t344, map_t<decltype(::cl_enginefuncs_s::pfnGetAppID)>>, "type cl_enginefuncs_s.pfnGetAppID");
    static_assert(o345 == offsetof(::cl_enginefuncs_s, pfnGetAliasList), "offsetof cl_enginefuncs_s.pfnGetAliasList");
    static_assert(std::is_same_v<t345, map_t<decltype(::cl_enginefuncs_s::pfnGetAliasList)>>, "type cl_enginefuncs_s.pfnGetAliasList");
    static_assert(o346 == offsetof(::cl_enginefuncs_s, pfnVguiWrap2_GetMouseDelta), "offsetof cl_enginefuncs_s.pfnVguiWrap2_GetMouseDelta");
    static_assert(
        std::is_same_v<t346, map_t<decltype(::cl_enginefuncs_s::pfnVguiWrap2_GetMouseDelta)>>,
        "type cl_enginefuncs_s.pfnVguiWrap2_GetMouseDelta"
    );
    static_assert(s7 == sizeof(::CHud), "sizeof CHud");
    static_assert(o347 == offsetof(::CHud, m_pHudList), "offsetof CHud.m_pHudList");
    static_assert(std::is_same_v<t347, map_t<decltype(::CHud::m_pHudList)>>, "type CHud.m_pHudList");
    static_assert(o348 == offsetof(::CHud, m_hsprLogo), "offsetof CHud.m_hsprLogo");
    static_assert(std::is_same_v<t348, map_t<decltype(::CHud::m_hsprLogo)>>, "type CHud.m_hsprLogo");
    static_assert(o349 == offsetof(::CHud, m_iLogo), "offsetof CHud.m_iLogo");
    static_assert(std::is_same_v<t349, map_t<decltype(::CHud::m_iLogo)>>, "type CHud.m_iLogo");
    static_assert(o350 == offsetof(::CHud, m_pSpriteList), "offsetof CHud.m_pSpriteList");
    static_assert(std::is_same_v<t350, map_t<decltype(::CHud::m_pSpriteList)>>, "type CHud.m_pSpriteList");
    static_assert(o351 == offsetof(::CHud, m_iSpriteCount), "offsetof CHud.m_iSpriteCount");
    static_assert(std::is_same_v<t351, map_t<decltype(::CHud::m_iSpriteCount)>>, "type CHud.m_iSpriteCount");
    static_assert(o352 == offsetof(::CHud, m_iSpriteCountAllRes), "offsetof CHud.m_iSpriteCountAllRes");
    static_assert(std::is_same_v<t352, map_t<decltype(::CHud::m_iSpriteCountAllRes)>>, "type CHud.m_iSpriteCountAllRes");
    static_assert(o353 == offsetof(::CHud, m_flMouseSensitivity), "offsetof CHud.m_flMouseSensitivity");
    static_assert(std::is_same_v<t353, map_t<decltype(::CHud::m_flMouseSensitivity)>>, "type CHud.m_flMouseSensitivity");
    static_assert(o354 == offsetof(::CHud, m_iConcussionEffect), "offsetof CHud.m_iConcussionEffect");
    static_assert(std::is_same_v<t354, map_t<decltype(::CHud::m_iConcussionEffect)>>, "type CHud.m_iConcussionEffect");
    static_assert(o355 == offsetof(::CHud, m_hsprCursor), "offsetof CHud.m_hsprCursor");
    static_assert(std::is_same_v<t355, map_t<decltype(::CHud::m_hsprCursor)>>, "type CHud.m_hsprCursor");
    static_assert(o356 == offsetof(::CHud, m_flTime), "offsetof CHud.m_flTime");
    static_assert(std::is_same_v<t356, map_t<decltype(::CHud::m_flTime)>>, "type CHud.m_flTime");
    static_assert(o357 == offsetof(::CHud, m_fOldTime), "offsetof CHud.m_fOldTime");
    static_assert(std::is_same_v<t357, map_t<decltype(::CHud::m_fOldTime)>>, "type CHud.m_fOldTime");
    static_assert(o358 == offsetof(::CHud, m_flTimeDelta), "offsetof CHud.m_flTimeDelta");
    static_assert(std::is_same_v<t358, map_t<decltype(::CHud::m_flTimeDelta)>>, "type CHud.m_flTimeDelta");
    static_assert(o359 == offsetof(::CHud, m_vecOrigin), "offsetof CHud.m_vecOrigin");
    // CHud.m_vecOrigin: type comparison skipped - the map substitutes vec3_t for Vector
    static_assert(o360 == offsetof(::CHud, m_vecAngles), "offsetof CHud.m_vecAngles");
    // CHud.m_vecAngles: type comparison skipped - the map substitutes vec3_t for Vector
    static_assert(o361 == offsetof(::CHud, m_iKeyBits), "offsetof CHud.m_iKeyBits");
    static_assert(std::is_same_v<t359, map_t<decltype(::CHud::m_iKeyBits)>>, "type CHud.m_iKeyBits");
    static_assert(o362 == offsetof(::CHud, m_iHideHUDDisplay), "offsetof CHud.m_iHideHUDDisplay");
    static_assert(std::is_same_v<t360, map_t<decltype(::CHud::m_iHideHUDDisplay)>>, "type CHud.m_iHideHUDDisplay");
    static_assert(o363 == offsetof(::CHud, m_iFOV), "offsetof CHud.m_iFOV");
    static_assert(std::is_same_v<t361, map_t<decltype(::CHud::m_iFOV)>>, "type CHud.m_iFOV");
    static_assert(o364 == offsetof(::CHud, m_Teamplay), "offsetof CHud.m_Teamplay");
    static_assert(std::is_same_v<t362, map_t<decltype(::CHud::m_Teamplay)>>, "type CHud.m_Teamplay");
    static_assert(o365 == offsetof(::CHud, m_iRes), "offsetof CHud.m_iRes");
    static_assert(std::is_same_v<t363, map_t<decltype(::CHud::m_iRes)>>, "type CHud.m_iRes");
    static_assert(o366 == offsetof(::CHud, m_pCvarStealMouse), "offsetof CHud.m_pCvarStealMouse");
    static_assert(std::is_same_v<t364, map_t<decltype(::CHud::m_pCvarStealMouse)>>, "type CHud.m_pCvarStealMouse");
    static_assert(o367 == offsetof(::CHud, m_pCvarDraw), "offsetof CHud.m_pCvarDraw");
    static_assert(std::is_same_v<t365, map_t<decltype(::CHud::m_pCvarDraw)>>, "type CHud.m_pCvarDraw");
    static_assert(o368 == offsetof(::CHud, m_PlayerFOV), "offsetof CHud.m_PlayerFOV");
    static_assert(std::is_same_v<t366, map_t<decltype(::CHud::m_PlayerFOV)>>, "type CHud.m_PlayerFOV");
    static_assert(o369 == offsetof(::CHud, m_iFontHeight), "offsetof CHud.m_iFontHeight");
    static_assert(std::is_same_v<t367, map_t<decltype(::CHud::m_iFontHeight)>>, "type CHud.m_iFontHeight");
    static_assert(o370 == offsetof(::CHud, m_iFontEngineHeight), "offsetof CHud.m_iFontEngineHeight");
    static_assert(std::is_same_v<t368, map_t<decltype(::CHud::m_iFontEngineHeight)>>, "type CHud.m_iFontEngineHeight");
    static_assert(o371 == offsetof(::CHud, default_fov), "offsetof CHud.default_fov");
    static_assert(std::is_same_v<t369, map_t<decltype(::CHud::default_fov)>>, "type CHud.default_fov");
    static_assert(o372 == offsetof(::CHud, m_rghSprites), "offsetof CHud.m_rghSprites");
    static_assert(std::is_same_v<t370, map_t<decltype(::CHud::m_rghSprites)>>, "type CHud.m_rghSprites");
    static_assert(o373 == offsetof(::CHud, m_rgrcRects), "offsetof CHud.m_rgrcRects");
    static_assert(std::is_same_v<t371, map_t<decltype(::CHud::m_rgrcRects)>>, "type CHud.m_rgrcRects");
    static_assert(s8 == sizeof(::CHudBase), "sizeof CHudBase");
    static_assert(o374 == offsetof(::CHudBase, m_pos), "offsetof CHudBase.m_pos");
    static_assert(std::is_same_v<t372, map_t<decltype(::CHudBase::m_pos)>>, "type CHudBase.m_pos");
    static_assert(o375 == offsetof(::CHudBase, m_type), "offsetof CHudBase.m_type");
    static_assert(std::is_same_v<t373, map_t<decltype(::CHudBase::m_type)>>, "type CHudBase.m_type");
    static_assert(o376 == offsetof(::CHudBase, m_iFlags), "offsetof CHudBase.m_iFlags");
    static_assert(std::is_same_v<t374, map_t<decltype(::CHudBase::m_iFlags)>>, "type CHudBase.m_iFlags");
    static_assert(s9 == sizeof(::CHudBase::POSITION), "sizeof CHudBase::POSITION");
    static_assert(o377 == offsetof(::CHudBase::POSITION, x), "offsetof CHudBase::POSITION.x");
    static_assert(std::is_same_v<t375, map_t<decltype(::CHudBase::POSITION::x)>>, "type CHudBase::POSITION.x");
    static_assert(o378 == offsetof(::CHudBase::POSITION, y), "offsetof CHudBase::POSITION.y");
    static_assert(std::is_same_v<t376, map_t<decltype(::CHudBase::POSITION::y)>>, "type CHudBase::POSITION.y");
    static_assert(s10 == sizeof(::extra_player_info_t), "sizeof extra_player_info_t");
    static_assert(o379 == offsetof(::extra_player_info_t, frags), "offsetof extra_player_info_t.frags");
    static_assert(std::is_same_v<t377, map_t<decltype(::extra_player_info_t::frags)>>, "type extra_player_info_t.frags");
    static_assert(o380 == offsetof(::extra_player_info_t, deaths), "offsetof extra_player_info_t.deaths");
    static_assert(std::is_same_v<t378, map_t<decltype(::extra_player_info_t::deaths)>>, "type extra_player_info_t.deaths");
    static_assert(o381 == offsetof(::extra_player_info_t, team_id), "offsetof extra_player_info_t.team_id");
    static_assert(std::is_same_v<t379, map_t<decltype(::extra_player_info_t::team_id)>>, "type extra_player_info_t.team_id");
    static_assert(o382 == offsetof(::extra_player_info_t, has_c4), "offsetof extra_player_info_t.has_c4");
    static_assert(std::is_same_v<t380, map_t<decltype(::extra_player_info_t::has_c4)>>, "type extra_player_info_t.has_c4");
    static_assert(o383 == offsetof(::extra_player_info_t, vip), "offsetof extra_player_info_t.vip");
    static_assert(std::is_same_v<t381, map_t<decltype(::extra_player_info_t::vip)>>, "type extra_player_info_t.vip");
    static_assert(o384 == offsetof(::extra_player_info_t, origin), "offsetof extra_player_info_t.origin");
    static_assert(std::is_same_v<t382, map_t<decltype(::extra_player_info_t::origin)>>, "type extra_player_info_t.origin");
    static_assert(o385 == offsetof(::extra_player_info_t, radarflash), "offsetof extra_player_info_t.radarflash");
    static_assert(std::is_same_v<t383, map_t<decltype(::extra_player_info_t::radarflash)>>, "type extra_player_info_t.radarflash");
    static_assert(o386 == offsetof(::extra_player_info_t, radarflashon), "offsetof extra_player_info_t.radarflashon");
    static_assert(std::is_same_v<t384, map_t<decltype(::extra_player_info_t::radarflashon)>>, "type extra_player_info_t.radarflashon");
    static_assert(o387 == offsetof(::extra_player_info_t, radarflashes), "offsetof extra_player_info_t.radarflashes");
    static_assert(std::is_same_v<t385, map_t<decltype(::extra_player_info_t::radarflashes)>>, "type extra_player_info_t.radarflashes");
    static_assert(o388 == offsetof(::extra_player_info_t, playerclass), "offsetof extra_player_info_t.playerclass");
    static_assert(std::is_same_v<t386, map_t<decltype(::extra_player_info_t::playerclass)>>, "type extra_player_info_t.playerclass");
    static_assert(o389 == offsetof(::extra_player_info_t, teamnumber), "offsetof extra_player_info_t.teamnumber");
    static_assert(std::is_same_v<t387, map_t<decltype(::extra_player_info_t::teamnumber)>>, "type extra_player_info_t.teamnumber");
    static_assert(o390 == offsetof(::extra_player_info_t, teamname), "offsetof extra_player_info_t.teamname");
    static_assert(std::is_same_v<t388, map_t<decltype(::extra_player_info_t::teamname)>>, "type extra_player_info_t.teamname");
    static_assert(o391 == offsetof(::extra_player_info_t, dead), "offsetof extra_player_info_t.dead");
    static_assert(std::is_same_v<t389, map_t<decltype(::extra_player_info_t::dead)>>, "type extra_player_info_t.dead");
    static_assert(o392 == offsetof(::extra_player_info_t, showhealth), "offsetof extra_player_info_t.showhealth");
    static_assert(std::is_same_v<t390, map_t<decltype(::extra_player_info_t::showhealth)>>, "type extra_player_info_t.showhealth");
    static_assert(o393 == offsetof(::extra_player_info_t, health), "offsetof extra_player_info_t.health");
    static_assert(std::is_same_v<t391, map_t<decltype(::extra_player_info_t::health)>>, "type extra_player_info_t.health");
    static_assert(o394 == offsetof(::extra_player_info_t, location), "offsetof extra_player_info_t.location");
    static_assert(std::is_same_v<t392, map_t<decltype(::extra_player_info_t::location)>>, "type extra_player_info_t.location");
    static_assert(o395 == offsetof(::extra_player_info_t, sb_health), "offsetof extra_player_info_t.sb_health");
    static_assert(std::is_same_v<t393, map_t<decltype(::extra_player_info_t::sb_health)>>, "type extra_player_info_t.sb_health");
    static_assert(s11 == sizeof(::CHudAmmo), "sizeof CHudAmmo");
    static_assert(s12 == sizeof(::CHudAmmo::RGBA), "sizeof CHudAmmo::RGBA");
    static_assert(o396 == offsetof(::CHudAmmo::RGBA, r), "offsetof CHudAmmo::RGBA.r");
    static_assert(std::is_same_v<t394, map_t<decltype(::CHudAmmo::RGBA::r)>>, "type CHudAmmo::RGBA.r");
    static_assert(o397 == offsetof(::CHudAmmo::RGBA, g), "offsetof CHudAmmo::RGBA.g");
    static_assert(std::is_same_v<t395, map_t<decltype(::CHudAmmo::RGBA::g)>>, "type CHudAmmo::RGBA.g");
    static_assert(o398 == offsetof(::CHudAmmo::RGBA, b), "offsetof CHudAmmo::RGBA.b");
    static_assert(std::is_same_v<t396, map_t<decltype(::CHudAmmo::RGBA::b)>>, "type CHudAmmo::RGBA.b");
    static_assert(o399 == offsetof(::CHudAmmo::RGBA, a), "offsetof CHudAmmo::RGBA.a");
    static_assert(std::is_same_v<t397, map_t<decltype(::CHudAmmo::RGBA::a)>>, "type CHudAmmo::RGBA.a");
    static_assert(s13 == sizeof(::CHudAmmoSecondary), "sizeof CHudAmmoSecondary");
    static_assert(s14 == sizeof(::CHudMessage), "sizeof CHudMessage");
    static_assert(s15 == sizeof(::CHudMessage::client_message_t), "sizeof CHudMessage::client_message_t");
    static_assert(o400 == offsetof(::CHudMessage::client_message_t, pMessage), "offsetof CHudMessage::client_message_t.pMessage");
    static_assert(
        std::is_same_v<t398, map_t<decltype(::CHudMessage::client_message_t::pMessage)>>,
        "type CHudMessage::client_message_t.pMessage"
    );
    static_assert(o401 == offsetof(::CHudMessage::client_message_t, font), "offsetof CHudMessage::client_message_t.font");
    // CHudMessage::client_message_t.font: type comparison skipped - the map substitutes unsigned long for vgui2::HFont
    static_assert(o402 == offsetof(::CHudMessage::client_message_t, args), "offsetof CHudMessage::client_message_t.args");
    static_assert(std::is_same_v<t399, map_t<decltype(::CHudMessage::client_message_t::args)>>, "type CHudMessage::client_message_t.args");
    static_assert(o403 == offsetof(::CHudMessage::client_message_t, numArgs), "offsetof CHudMessage::client_message_t.numArgs");
    static_assert(
        std::is_same_v<t400, map_t<decltype(::CHudMessage::client_message_t::numArgs)>>,
        "type CHudMessage::client_message_t.numArgs"
    );
    static_assert(s16 == sizeof(::CHudNightVision), "sizeof CHudNightVision");
    static_assert(o404 == offsetof(::CHudNightVision, m_fOn), "offsetof CHudNightVision.m_fOn");
    static_assert(std::is_same_v<t401, map_t<decltype(::CHudNightVision::m_fOn)>>, "type CHudNightVision.m_fOn");
    static_assert(s17 == sizeof(::CHudRoundTimer), "sizeof CHudRoundTimer");
    static_assert(o405 == offsetof(::CHudRoundTimer, m_flTimeEnd), "offsetof CHudRoundTimer.m_flTimeEnd");
    static_assert(std::is_same_v<t402, map_t<decltype(::CHudRoundTimer::m_flTimeEnd)>>, "type CHudRoundTimer.m_flTimeEnd");
    static_assert(o406 == offsetof(::CHudRoundTimer, m_HUD_stopwatch), "offsetof CHudRoundTimer.m_HUD_stopwatch");
    static_assert(std::is_same_v<t403, map_t<decltype(::CHudRoundTimer::m_HUD_stopwatch)>>, "type CHudRoundTimer.m_HUD_stopwatch");
    static_assert(o407 == offsetof(::CHudRoundTimer, m_fFade), "offsetof CHudRoundTimer.m_fFade");
    static_assert(std::is_same_v<t404, map_t<decltype(::CHudRoundTimer::m_fFade)>>, "type CHudRoundTimer.m_fFade");
    static_assert(o408 == offsetof(::CHudRoundTimer, m_flNewPeriod), "offsetof CHudRoundTimer.m_flNewPeriod");
    static_assert(std::is_same_v<t405, map_t<decltype(::CHudRoundTimer::m_flNewPeriod)>>, "type CHudRoundTimer.m_flNewPeriod");
    static_assert(o409 == offsetof(::CHudRoundTimer, m_flNextToggle), "offsetof CHudRoundTimer.m_flNextToggle");
    static_assert(std::is_same_v<t406, map_t<decltype(::CHudRoundTimer::m_flNextToggle)>>, "type CHudRoundTimer.m_flNextToggle");
    static_assert(o410 == offsetof(::CHudRoundTimer, m_bPanicColor), "offsetof CHudRoundTimer.m_bPanicColor");
    static_assert(std::is_same_v<t407, map_t<decltype(::CHudRoundTimer::m_bPanicColor)>>, "type CHudRoundTimer.m_bPanicColor");
    static_assert(o411 == offsetof(::CHudRoundTimer, m_closestRight), "offsetof CHudRoundTimer.m_closestRight");
    static_assert(std::is_same_v<t408, map_t<decltype(::CHudRoundTimer::m_closestRight)>>, "type CHudRoundTimer.m_closestRight");
    static_assert(s18 == sizeof(::CHudStatusBar), "sizeof CHudStatusBar");
    static_assert(s19 == sizeof(::CHudStatusIcons), "sizeof CHudStatusIcons");
    static_assert(s20 == sizeof(::CHudStatusIcons::icon_sprite_t), "sizeof CHudStatusIcons::icon_sprite_t");
    static_assert(o412 == offsetof(::CHudStatusIcons::icon_sprite_t, szSpriteName), "offsetof CHudStatusIcons::icon_sprite_t.szSpriteName");
    static_assert(
        std::is_same_v<t409, map_t<decltype(::CHudStatusIcons::icon_sprite_t::szSpriteName)>>,
        "type CHudStatusIcons::icon_sprite_t.szSpriteName"
    );
    static_assert(o413 == offsetof(::CHudStatusIcons::icon_sprite_t, spr), "offsetof CHudStatusIcons::icon_sprite_t.spr");
    static_assert(std::is_same_v<t410, map_t<decltype(::CHudStatusIcons::icon_sprite_t::spr)>>, "type CHudStatusIcons::icon_sprite_t.spr");
    static_assert(o414 == offsetof(::CHudStatusIcons::icon_sprite_t, rc), "offsetof CHudStatusIcons::icon_sprite_t.rc");
    static_assert(std::is_same_v<t411, map_t<decltype(::CHudStatusIcons::icon_sprite_t::rc)>>, "type CHudStatusIcons::icon_sprite_t.rc");
    static_assert(o415 == offsetof(::CHudStatusIcons::icon_sprite_t, r), "offsetof CHudStatusIcons::icon_sprite_t.r");
    static_assert(std::is_same_v<t412, map_t<decltype(::CHudStatusIcons::icon_sprite_t::r)>>, "type CHudStatusIcons::icon_sprite_t.r");
    static_assert(o416 == offsetof(::CHudStatusIcons::icon_sprite_t, g), "offsetof CHudStatusIcons::icon_sprite_t.g");
    static_assert(std::is_same_v<t413, map_t<decltype(::CHudStatusIcons::icon_sprite_t::g)>>, "type CHudStatusIcons::icon_sprite_t.g");
    static_assert(o417 == offsetof(::CHudStatusIcons::icon_sprite_t, b), "offsetof CHudStatusIcons::icon_sprite_t.b");
    static_assert(std::is_same_v<t414, map_t<decltype(::CHudStatusIcons::icon_sprite_t::b)>>, "type CHudStatusIcons::icon_sprite_t.b");
    static_assert(o418 == offsetof(::CHudStatusIcons::icon_sprite_t, bFlash), "offsetof CHudStatusIcons::icon_sprite_t.bFlash");
    static_assert(
        std::is_same_v<t415, map_t<decltype(::CHudStatusIcons::icon_sprite_t::bFlash)>>,
        "type CHudStatusIcons::icon_sprite_t.bFlash"
    );
    static_assert(s21 == sizeof(::DAMAGE_IMAGE), "sizeof DAMAGE_IMAGE");
    static_assert(o419 == offsetof(::DAMAGE_IMAGE, fExpire), "offsetof DAMAGE_IMAGE.fExpire");
    static_assert(std::is_same_v<t416, map_t<decltype(::DAMAGE_IMAGE::fExpire)>>, "type DAMAGE_IMAGE.fExpire");
    static_assert(o420 == offsetof(::DAMAGE_IMAGE, fBaseline), "offsetof DAMAGE_IMAGE.fBaseline");
    static_assert(std::is_same_v<t417, map_t<decltype(::DAMAGE_IMAGE::fBaseline)>>, "type DAMAGE_IMAGE.fBaseline");
    static_assert(o421 == offsetof(::DAMAGE_IMAGE, x), "offsetof DAMAGE_IMAGE.x");
    static_assert(std::is_same_v<t418, map_t<decltype(::DAMAGE_IMAGE::x)>>, "type DAMAGE_IMAGE.x");
    static_assert(o422 == offsetof(::DAMAGE_IMAGE, y), "offsetof DAMAGE_IMAGE.y");
    static_assert(std::is_same_v<t419, map_t<decltype(::DAMAGE_IMAGE::y)>>, "type DAMAGE_IMAGE.y");
    static_assert(s22 == sizeof(::CHudHealth), "sizeof CHudHealth");
    static_assert(o423 == offsetof(::CHudHealth, m_iHealth), "offsetof CHudHealth.m_iHealth");
    static_assert(std::is_same_v<t420, map_t<decltype(::CHudHealth::m_iHealth)>>, "type CHudHealth.m_iHealth");
    static_assert(o424 == offsetof(::CHudHealth, m_HUD_dmg_bio), "offsetof CHudHealth.m_HUD_dmg_bio");
    static_assert(std::is_same_v<t421, map_t<decltype(::CHudHealth::m_HUD_dmg_bio)>>, "type CHudHealth.m_HUD_dmg_bio");
    static_assert(o425 == offsetof(::CHudHealth, m_HUD_cross), "offsetof CHudHealth.m_HUD_cross");
    static_assert(std::is_same_v<t422, map_t<decltype(::CHudHealth::m_HUD_cross)>>, "type CHudHealth.m_HUD_cross");
    static_assert(o426 == offsetof(::CHudHealth, m_fFade), "offsetof CHudHealth.m_fFade");
    static_assert(std::is_same_v<t423, map_t<decltype(::CHudHealth::m_fFade)>>, "type CHudHealth.m_fFade");
    static_assert(o427 == offsetof(::CHudHealth, m_fAttackFront), "offsetof CHudHealth.m_fAttackFront");
    static_assert(std::is_same_v<t424, map_t<decltype(::CHudHealth::m_fAttackFront)>>, "type CHudHealth.m_fAttackFront");
    static_assert(o428 == offsetof(::CHudHealth, m_fAttackRear), "offsetof CHudHealth.m_fAttackRear");
    static_assert(std::is_same_v<t425, map_t<decltype(::CHudHealth::m_fAttackRear)>>, "type CHudHealth.m_fAttackRear");
    static_assert(o429 == offsetof(::CHudHealth, m_fAttackLeft), "offsetof CHudHealth.m_fAttackLeft");
    static_assert(std::is_same_v<t426, map_t<decltype(::CHudHealth::m_fAttackLeft)>>, "type CHudHealth.m_fAttackLeft");
    static_assert(o430 == offsetof(::CHudHealth, m_fAttackRight), "offsetof CHudHealth.m_fAttackRight");
    static_assert(std::is_same_v<t427, map_t<decltype(::CHudHealth::m_fAttackRight)>>, "type CHudHealth.m_fAttackRight");
    static_assert(o431 == offsetof(::CHudHealth, m_bTrackArray), "offsetof CHudHealth.m_bTrackArray");
    static_assert(std::is_same_v<t428, map_t<decltype(::CHudHealth::m_bTrackArray)>>, "type CHudHealth.m_bTrackArray");
    static_assert(o432 == offsetof(::CHudHealth, m_iPlayerLastPointedAt), "offsetof CHudHealth.m_iPlayerLastPointedAt");
    static_assert(std::is_same_v<t429, map_t<decltype(::CHudHealth::m_iPlayerLastPointedAt)>>, "type CHudHealth.m_iPlayerLastPointedAt");
    static_assert(o433 == offsetof(::CHudHealth, m_iPlayerNum), "offsetof CHudHealth.m_iPlayerNum");
    static_assert(std::is_same_v<t430, map_t<decltype(::CHudHealth::m_iPlayerNum)>>, "type CHudHealth.m_iPlayerNum");
    static_assert(o434 == offsetof(::CHudHealth, m_bDrawRadar), "offsetof CHudHealth.m_bDrawRadar");
    static_assert(std::is_same_v<t431, map_t<decltype(::CHudHealth::m_bDrawRadar)>>, "type CHudHealth.m_bDrawRadar");
    static_assert(o435 == offsetof(::CHudHealth, m_hSprite), "offsetof CHudHealth.m_hSprite");
    static_assert(std::is_same_v<t432, map_t<decltype(::CHudHealth::m_hSprite)>>, "type CHudHealth.m_hSprite");
    static_assert(o436 == offsetof(::CHudHealth, m_hDamage), "offsetof CHudHealth.m_hDamage");
    static_assert(std::is_same_v<t433, map_t<decltype(::CHudHealth::m_hDamage)>>, "type CHudHealth.m_hDamage");
    static_assert(o437 == offsetof(::CHudHealth, m_hrad), "offsetof CHudHealth.m_hrad");
    static_assert(std::is_same_v<t434, map_t<decltype(::CHudHealth::m_hrad)>>, "type CHudHealth.m_hrad");
    static_assert(o438 == offsetof(::CHudHealth, m_hradopaque), "offsetof CHudHealth.m_hradopaque");
    static_assert(std::is_same_v<t435, map_t<decltype(::CHudHealth::m_hradopaque)>>, "type CHudHealth.m_hradopaque");
    static_assert(o439 == offsetof(::CHudHealth, m_hRadar), "offsetof CHudHealth.m_hRadar");
    static_assert(std::is_same_v<t436, map_t<decltype(::CHudHealth::m_hRadar)>>, "type CHudHealth.m_hRadar");
    static_assert(o440 == offsetof(::CHudHealth, m_hRadaropaque), "offsetof CHudHealth.m_hRadaropaque");
    static_assert(std::is_same_v<t437, map_t<decltype(::CHudHealth::m_hRadaropaque)>>, "type CHudHealth.m_hRadaropaque");
    static_assert(o441 == offsetof(::CHudHealth, m_dmg), "offsetof CHudHealth.m_dmg");
    static_assert(std::is_same_v<t438, map_t<decltype(::CHudHealth::m_dmg)>>, "type CHudHealth.m_dmg");
    static_assert(o442 == offsetof(::CHudHealth, m_bitsDamage), "offsetof CHudHealth.m_bitsDamage");
    static_assert(std::is_same_v<t439, map_t<decltype(::CHudHealth::m_bitsDamage)>>, "type CHudHealth.m_bitsDamage");
    static_assert(s23 == sizeof(::cvar_s), "sizeof cvar_s");
    static_assert(o443 == offsetof(::cvar_s, name), "offsetof cvar_s.name");
    static_assert(std::is_same_v<t440, map_t<decltype(::cvar_s::name)>>, "type cvar_s.name");
    static_assert(o444 == offsetof(::cvar_s, string), "offsetof cvar_s.string");
    static_assert(std::is_same_v<t441, map_t<decltype(::cvar_s::string)>>, "type cvar_s.string");
    static_assert(o445 == offsetof(::cvar_s, flags), "offsetof cvar_s.flags");
    static_assert(std::is_same_v<t442, map_t<decltype(::cvar_s::flags)>>, "type cvar_s.flags");
    static_assert(o446 == offsetof(::cvar_s, value), "offsetof cvar_s.value");
    static_assert(std::is_same_v<t443, map_t<decltype(::cvar_s::value)>>, "type cvar_s.value");
    static_assert(o447 == offsetof(::cvar_s, next), "offsetof cvar_s.next");
    static_assert(std::is_same_v<t444, map_t<decltype(::cvar_s::next)>>, "type cvar_s.next");
    static_assert(s24 == sizeof(::cmd_s), "sizeof cmd_s");
    static_assert(o448 == offsetof(::cmd_s, cmd), "offsetof cmd_s.cmd");
    static_assert(std::is_same_v<t445, map_t<decltype(::cmd_s::cmd)>>, "type cmd_s.cmd");
    static_assert(o449 == offsetof(::cmd_s, senttime), "offsetof cmd_s.senttime");
    static_assert(std::is_same_v<t446, map_t<decltype(::cmd_s::senttime)>>, "type cmd_s.senttime");
    static_assert(o450 == offsetof(::cmd_s, receivedtime), "offsetof cmd_s.receivedtime");
    static_assert(std::is_same_v<t447, map_t<decltype(::cmd_s::receivedtime)>>, "type cmd_s.receivedtime");
    static_assert(o451 == offsetof(::cmd_s, frame_lerp), "offsetof cmd_s.frame_lerp");
    static_assert(std::is_same_v<t448, map_t<decltype(::cmd_s::frame_lerp)>>, "type cmd_s.frame_lerp");
    static_assert(o452 == offsetof(::cmd_s, processedfuncs), "offsetof cmd_s.processedfuncs");
    static_assert(std::is_same_v<t449, map_t<decltype(::cmd_s::processedfuncs)>>, "type cmd_s.processedfuncs");
    static_assert(o453 == offsetof(::cmd_s, heldback), "offsetof cmd_s.heldback");
    static_assert(std::is_same_v<t450, map_t<decltype(::cmd_s::heldback)>>, "type cmd_s.heldback");
    static_assert(o454 == offsetof(::cmd_s, sendsize), "offsetof cmd_s.sendsize");
    static_assert(std::is_same_v<t451, map_t<decltype(::cmd_s::sendsize)>>, "type cmd_s.sendsize");
    static_assert(s25 == sizeof(::consistency_s), "sizeof consistency_s");
    static_assert(o455 == offsetof(::consistency_s, filename), "offsetof consistency_s.filename");
    static_assert(std::is_same_v<t452, map_t<decltype(::consistency_s::filename)>>, "type consistency_s.filename");
    static_assert(o456 == offsetof(::consistency_s, issound), "offsetof consistency_s.issound");
    static_assert(std::is_same_v<t453, map_t<decltype(::consistency_s::issound)>>, "type consistency_s.issound");
    static_assert(o457 == offsetof(::consistency_s, orig_index), "offsetof consistency_s.orig_index");
    static_assert(std::is_same_v<t454, map_t<decltype(::consistency_s::orig_index)>>, "type consistency_s.orig_index");
    static_assert(o458 == offsetof(::consistency_s, value), "offsetof consistency_s.value");
    static_assert(std::is_same_v<t455, map_t<decltype(::consistency_s::value)>>, "type consistency_s.value");
    static_assert(o459 == offsetof(::consistency_s, check_type), "offsetof consistency_s.check_type");
    static_assert(std::is_same_v<t456, map_t<decltype(::consistency_s::check_type)>>, "type consistency_s.check_type");
    static_assert(o460 == offsetof(::consistency_s, mins), "offsetof consistency_s.mins");
    static_assert(std::is_same_v<t457, map_t<decltype(::consistency_s::mins)>>, "type consistency_s.mins");
    static_assert(o461 == offsetof(::consistency_s, maxs), "offsetof consistency_s.maxs");
    static_assert(std::is_same_v<t458, map_t<decltype(::consistency_s::maxs)>>, "type consistency_s.maxs");
    static_assert(s26 == sizeof(::packet_entities_s), "sizeof packet_entities_s");
    static_assert(o462 == offsetof(::packet_entities_s, num_entities), "offsetof packet_entities_s.num_entities");
    static_assert(std::is_same_v<t459, map_t<decltype(::packet_entities_s::num_entities)>>, "type packet_entities_s.num_entities");
    static_assert(o463 == offsetof(::packet_entities_s, flags), "offsetof packet_entities_s.flags");
    static_assert(std::is_same_v<t460, map_t<decltype(::packet_entities_s::flags)>>, "type packet_entities_s.flags");
    static_assert(o464 == offsetof(::packet_entities_s, entities), "offsetof packet_entities_s.entities");
    static_assert(std::is_same_v<t461, map_t<decltype(::packet_entities_s::entities)>>, "type packet_entities_s.entities");
    static_assert(s27 == sizeof(::player_info_s), "sizeof player_info_s");
    static_assert(o465 == offsetof(::player_info_s, userid), "offsetof player_info_s.userid");
    static_assert(std::is_same_v<t462, map_t<decltype(::player_info_s::userid)>>, "type player_info_s.userid");
    static_assert(o466 == offsetof(::player_info_s, userinfo), "offsetof player_info_s.userinfo");
    static_assert(std::is_same_v<t463, map_t<decltype(::player_info_s::userinfo)>>, "type player_info_s.userinfo");
    static_assert(o467 == offsetof(::player_info_s, name), "offsetof player_info_s.name");
    static_assert(std::is_same_v<t464, map_t<decltype(::player_info_s::name)>>, "type player_info_s.name");
    static_assert(o468 == offsetof(::player_info_s, spectator), "offsetof player_info_s.spectator");
    static_assert(std::is_same_v<t465, map_t<decltype(::player_info_s::spectator)>>, "type player_info_s.spectator");
    static_assert(o469 == offsetof(::player_info_s, ping), "offsetof player_info_s.ping");
    static_assert(std::is_same_v<t466, map_t<decltype(::player_info_s::ping)>>, "type player_info_s.ping");
    static_assert(o470 == offsetof(::player_info_s, packet_loss), "offsetof player_info_s.packet_loss");
    static_assert(std::is_same_v<t467, map_t<decltype(::player_info_s::packet_loss)>>, "type player_info_s.packet_loss");
    static_assert(o471 == offsetof(::player_info_s, model), "offsetof player_info_s.model");
    static_assert(std::is_same_v<t468, map_t<decltype(::player_info_s::model)>>, "type player_info_s.model");
    static_assert(o472 == offsetof(::player_info_s, topcolor), "offsetof player_info_s.topcolor");
    static_assert(std::is_same_v<t469, map_t<decltype(::player_info_s::topcolor)>>, "type player_info_s.topcolor");
    static_assert(o473 == offsetof(::player_info_s, bottomcolor), "offsetof player_info_s.bottomcolor");
    static_assert(std::is_same_v<t470, map_t<decltype(::player_info_s::bottomcolor)>>, "type player_info_s.bottomcolor");
    static_assert(o474 == offsetof(::player_info_s, renderframe), "offsetof player_info_s.renderframe");
    static_assert(std::is_same_v<t471, map_t<decltype(::player_info_s::renderframe)>>, "type player_info_s.renderframe");
    static_assert(o475 == offsetof(::player_info_s, gaitsequence), "offsetof player_info_s.gaitsequence");
    static_assert(std::is_same_v<t472, map_t<decltype(::player_info_s::gaitsequence)>>, "type player_info_s.gaitsequence");
    static_assert(o476 == offsetof(::player_info_s, gaitframe), "offsetof player_info_s.gaitframe");
    static_assert(std::is_same_v<t473, map_t<decltype(::player_info_s::gaitframe)>>, "type player_info_s.gaitframe");
    static_assert(o477 == offsetof(::player_info_s, gaityaw), "offsetof player_info_s.gaityaw");
    static_assert(std::is_same_v<t474, map_t<decltype(::player_info_s::gaityaw)>>, "type player_info_s.gaityaw");
    static_assert(o478 == offsetof(::player_info_s, prevgaitorigin), "offsetof player_info_s.prevgaitorigin");
    static_assert(std::is_same_v<t475, map_t<decltype(::player_info_s::prevgaitorigin)>>, "type player_info_s.prevgaitorigin");
    static_assert(o479 == offsetof(::player_info_s, customdata), "offsetof player_info_s.customdata");
    static_assert(std::is_same_v<t476, map_t<decltype(::player_info_s::customdata)>>, "type player_info_s.customdata");
    static_assert(o480 == offsetof(::player_info_s, hashedcdkey), "offsetof player_info_s.hashedcdkey");
    static_assert(std::is_same_v<t477, map_t<decltype(::player_info_s::hashedcdkey)>>, "type player_info_s.hashedcdkey");
    static_assert(o481 == offsetof(::player_info_s, m_nSteamID), "offsetof player_info_s.m_nSteamID");
    static_assert(std::is_same_v<t478, map_t<decltype(::player_info_s::m_nSteamID)>>, "type player_info_s.m_nSteamID");
    static_assert(s28 == sizeof(::soundfade_s), "sizeof soundfade_s");
    static_assert(o482 == offsetof(::soundfade_s, nStartPercent), "offsetof soundfade_s.nStartPercent");
    static_assert(std::is_same_v<t479, map_t<decltype(::soundfade_s::nStartPercent)>>, "type soundfade_s.nStartPercent");
    static_assert(o483 == offsetof(::soundfade_s, nClientSoundFadePercent), "offsetof soundfade_s.nClientSoundFadePercent");
    static_assert(
        std::is_same_v<t480, map_t<decltype(::soundfade_s::nClientSoundFadePercent)>>,
        "type soundfade_s.nClientSoundFadePercent"
    );
    static_assert(o484 == offsetof(::soundfade_s, soundFadeStartTime), "offsetof soundfade_s.soundFadeStartTime");
    static_assert(std::is_same_v<t481, map_t<decltype(::soundfade_s::soundFadeStartTime)>>, "type soundfade_s.soundFadeStartTime");
    static_assert(o485 == offsetof(::soundfade_s, soundFadeOutTime), "offsetof soundfade_s.soundFadeOutTime");
    static_assert(std::is_same_v<t482, map_t<decltype(::soundfade_s::soundFadeOutTime)>>, "type soundfade_s.soundFadeOutTime");
    static_assert(o486 == offsetof(::soundfade_s, soundFadeHoldTime), "offsetof soundfade_s.soundFadeHoldTime");
    static_assert(std::is_same_v<t483, map_t<decltype(::soundfade_s::soundFadeHoldTime)>>, "type soundfade_s.soundFadeHoldTime");
    static_assert(o487 == offsetof(::soundfade_s, soundFadeInTime), "offsetof soundfade_s.soundFadeInTime");
    static_assert(std::is_same_v<t484, map_t<decltype(::soundfade_s::soundFadeInTime)>>, "type soundfade_s.soundFadeInTime");
    static_assert(s29 == sizeof(::client_static_s), "sizeof client_static_s");
    static_assert(o488 == offsetof(::client_static_s, state), "offsetof client_static_s.state");
    static_assert(std::is_same_v<t485, map_t<decltype(::client_static_s::state)>>, "type client_static_s.state");
    static_assert(o489 == offsetof(::client_static_s, netchan), "offsetof client_static_s.netchan");
    static_assert(std::is_same_v<t486, map_t<decltype(::client_static_s::netchan)>>, "type client_static_s.netchan");
    static_assert(o490 == offsetof(::client_static_s, datagram), "offsetof client_static_s.datagram");
    static_assert(std::is_same_v<t487, map_t<decltype(::client_static_s::datagram)>>, "type client_static_s.datagram");
    static_assert(o491 == offsetof(::client_static_s, datagram_buf), "offsetof client_static_s.datagram_buf");
    static_assert(std::is_same_v<t488, map_t<decltype(::client_static_s::datagram_buf)>>, "type client_static_s.datagram_buf");
    static_assert(o492 == offsetof(::client_static_s, connect_time), "offsetof client_static_s.connect_time");
    static_assert(std::is_same_v<t489, map_t<decltype(::client_static_s::connect_time)>>, "type client_static_s.connect_time");
    static_assert(o493 == offsetof(::client_static_s, connect_retry), "offsetof client_static_s.connect_retry");
    static_assert(std::is_same_v<t490, map_t<decltype(::client_static_s::connect_retry)>>, "type client_static_s.connect_retry");
    static_assert(o494 == offsetof(::client_static_s, challenge), "offsetof client_static_s.challenge");
    static_assert(std::is_same_v<t491, map_t<decltype(::client_static_s::challenge)>>, "type client_static_s.challenge");
    static_assert(o495 == offsetof(::client_static_s, authprotocol), "offsetof client_static_s.authprotocol");
    static_assert(std::is_same_v<t492, map_t<decltype(::client_static_s::authprotocol)>>, "type client_static_s.authprotocol");
    static_assert(o496 == offsetof(::client_static_s, userid), "offsetof client_static_s.userid");
    static_assert(std::is_same_v<t493, map_t<decltype(::client_static_s::userid)>>, "type client_static_s.userid");
    static_assert(o497 == offsetof(::client_static_s, trueaddress), "offsetof client_static_s.trueaddress");
    static_assert(std::is_same_v<t494, map_t<decltype(::client_static_s::trueaddress)>>, "type client_static_s.trueaddress");
    static_assert(o498 == offsetof(::client_static_s, slist_time), "offsetof client_static_s.slist_time");
    static_assert(std::is_same_v<t495, map_t<decltype(::client_static_s::slist_time)>>, "type client_static_s.slist_time");
    static_assert(o499 == offsetof(::client_static_s, signon), "offsetof client_static_s.signon");
    static_assert(std::is_same_v<t496, map_t<decltype(::client_static_s::signon)>>, "type client_static_s.signon");
    static_assert(o500 == offsetof(::client_static_s, servername), "offsetof client_static_s.servername");
    static_assert(std::is_same_v<t497, map_t<decltype(::client_static_s::servername)>>, "type client_static_s.servername");
    static_assert(o501 == offsetof(::client_static_s, mapstring), "offsetof client_static_s.mapstring");
    static_assert(std::is_same_v<t498, map_t<decltype(::client_static_s::mapstring)>>, "type client_static_s.mapstring");
    static_assert(o502 == offsetof(::client_static_s, spawnparms), "offsetof client_static_s.spawnparms");
    static_assert(std::is_same_v<t499, map_t<decltype(::client_static_s::spawnparms)>>, "type client_static_s.spawnparms");
    static_assert(o503 == offsetof(::client_static_s, userinfo), "offsetof client_static_s.userinfo");
    static_assert(std::is_same_v<t500, map_t<decltype(::client_static_s::userinfo)>>, "type client_static_s.userinfo");
    static_assert(o504 == offsetof(::client_static_s, nextcmdtime), "offsetof client_static_s.nextcmdtime");
    static_assert(std::is_same_v<t501, map_t<decltype(::client_static_s::nextcmdtime)>>, "type client_static_s.nextcmdtime");
    static_assert(o505 == offsetof(::client_static_s, lastoutgoingcommand), "offsetof client_static_s.lastoutgoingcommand");
    static_assert(
        std::is_same_v<t502, map_t<decltype(::client_static_s::lastoutgoingcommand)>>,
        "type client_static_s.lastoutgoingcommand"
    );
    static_assert(o506 == offsetof(::client_static_s, demonum), "offsetof client_static_s.demonum");
    static_assert(std::is_same_v<t503, map_t<decltype(::client_static_s::demonum)>>, "type client_static_s.demonum");
    static_assert(o507 == offsetof(::client_static_s, demos), "offsetof client_static_s.demos");
    static_assert(std::is_same_v<t504, map_t<decltype(::client_static_s::demos)>>, "type client_static_s.demos");
    static_assert(o508 == offsetof(::client_static_s, demorecording), "offsetof client_static_s.demorecording");
    static_assert(std::is_same_v<t505, map_t<decltype(::client_static_s::demorecording)>>, "type client_static_s.demorecording");
    static_assert(o509 == offsetof(::client_static_s, demoplayback), "offsetof client_static_s.demoplayback");
    static_assert(std::is_same_v<t506, map_t<decltype(::client_static_s::demoplayback)>>, "type client_static_s.demoplayback");
    static_assert(o510 == offsetof(::client_static_s, timedemo), "offsetof client_static_s.timedemo");
    static_assert(std::is_same_v<t507, map_t<decltype(::client_static_s::timedemo)>>, "type client_static_s.timedemo");
    static_assert(o511 == offsetof(::client_static_s, demostarttime), "offsetof client_static_s.demostarttime");
    static_assert(std::is_same_v<t508, map_t<decltype(::client_static_s::demostarttime)>>, "type client_static_s.demostarttime");
    static_assert(o512 == offsetof(::client_static_s, demostartframe), "offsetof client_static_s.demostartframe");
    static_assert(std::is_same_v<t509, map_t<decltype(::client_static_s::demostartframe)>>, "type client_static_s.demostartframe");
    static_assert(o513 == offsetof(::client_static_s, forcetrack), "offsetof client_static_s.forcetrack");
    static_assert(std::is_same_v<t510, map_t<decltype(::client_static_s::forcetrack)>>, "type client_static_s.forcetrack");
    static_assert(o514 == offsetof(::client_static_s, demofile), "offsetof client_static_s.demofile");
    // client_static_s.demofile: type comparison skipped - the map substitutes void* for FileHandle_t
    static_assert(o515 == offsetof(::client_static_s, demoheader), "offsetof client_static_s.demoheader");
    // client_static_s.demoheader: type comparison skipped - the map substitutes void* for FileHandle_t
    static_assert(o516 == offsetof(::client_static_s, demowaiting), "offsetof client_static_s.demowaiting");
    static_assert(std::is_same_v<t511, map_t<decltype(::client_static_s::demowaiting)>>, "type client_static_s.demowaiting");
    static_assert(o517 == offsetof(::client_static_s, demoappending), "offsetof client_static_s.demoappending");
    static_assert(std::is_same_v<t512, map_t<decltype(::client_static_s::demoappending)>>, "type client_static_s.demoappending");
    static_assert(o518 == offsetof(::client_static_s, demofilename), "offsetof client_static_s.demofilename");
    static_assert(std::is_same_v<t513, map_t<decltype(::client_static_s::demofilename)>>, "type client_static_s.demofilename");
    static_assert(o519 == offsetof(::client_static_s, demoframecount), "offsetof client_static_s.demoframecount");
    static_assert(std::is_same_v<t514, map_t<decltype(::client_static_s::demoframecount)>>, "type client_static_s.demoframecount");
    static_assert(o520 == offsetof(::client_static_s, td_lastframe), "offsetof client_static_s.td_lastframe");
    static_assert(std::is_same_v<t515, map_t<decltype(::client_static_s::td_lastframe)>>, "type client_static_s.td_lastframe");
    static_assert(o521 == offsetof(::client_static_s, td_startframe), "offsetof client_static_s.td_startframe");
    static_assert(std::is_same_v<t516, map_t<decltype(::client_static_s::td_startframe)>>, "type client_static_s.td_startframe");
    static_assert(o522 == offsetof(::client_static_s, td_starttime), "offsetof client_static_s.td_starttime");
    static_assert(std::is_same_v<t517, map_t<decltype(::client_static_s::td_starttime)>>, "type client_static_s.td_starttime");
    static_assert(o523 == offsetof(::client_static_s, dl), "offsetof client_static_s.dl");
    static_assert(std::is_same_v<t518, map_t<decltype(::client_static_s::dl)>>, "type client_static_s.dl");
    static_assert(o524 == offsetof(::client_static_s, packet_loss), "offsetof client_static_s.packet_loss");
    static_assert(std::is_same_v<t519, map_t<decltype(::client_static_s::packet_loss)>>, "type client_static_s.packet_loss");
    static_assert(o525 == offsetof(::client_static_s, packet_loss_recalc_time), "offsetof client_static_s.packet_loss_recalc_time");
    static_assert(
        std::is_same_v<t520, map_t<decltype(::client_static_s::packet_loss_recalc_time)>>,
        "type client_static_s.packet_loss_recalc_time"
    );
    static_assert(o526 == offsetof(::client_static_s, playerbits), "offsetof client_static_s.playerbits");
    static_assert(std::is_same_v<t521, map_t<decltype(::client_static_s::playerbits)>>, "type client_static_s.playerbits");
    static_assert(o527 == offsetof(::client_static_s, soundfade), "offsetof client_static_s.soundfade");
    static_assert(std::is_same_v<t522, map_t<decltype(::client_static_s::soundfade)>>, "type client_static_s.soundfade");
    static_assert(o528 == offsetof(::client_static_s, physinfo), "offsetof client_static_s.physinfo");
    static_assert(std::is_same_v<t523, map_t<decltype(::client_static_s::physinfo)>>, "type client_static_s.physinfo");
    static_assert(o529 == offsetof(::client_static_s, md5_clientdll), "offsetof client_static_s.md5_clientdll");
    static_assert(std::is_same_v<t524, map_t<decltype(::client_static_s::md5_clientdll)>>, "type client_static_s.md5_clientdll");
    static_assert(o530 == offsetof(::client_static_s, game_stream), "offsetof client_static_s.game_stream");
    static_assert(std::is_same_v<t525, map_t<decltype(::client_static_s::game_stream)>>, "type client_static_s.game_stream");
    static_assert(o531 == offsetof(::client_static_s, connect_stream), "offsetof client_static_s.connect_stream");
    static_assert(std::is_same_v<t526, map_t<decltype(::client_static_s::connect_stream)>>, "type client_static_s.connect_stream");
    static_assert(o532 == offsetof(::client_static_s, passive), "offsetof client_static_s.passive");
    static_assert(std::is_same_v<t527, map_t<decltype(::client_static_s::passive)>>, "type client_static_s.passive");
    static_assert(o533 == offsetof(::client_static_s, spectator), "offsetof client_static_s.spectator");
    static_assert(std::is_same_v<t528, map_t<decltype(::client_static_s::spectator)>>, "type client_static_s.spectator");
    static_assert(o534 == offsetof(::client_static_s, director), "offsetof client_static_s.director");
    static_assert(std::is_same_v<t529, map_t<decltype(::client_static_s::director)>>, "type client_static_s.director");
    static_assert(o535 == offsetof(::client_static_s, fSecureClient), "offsetof client_static_s.fSecureClient");
    static_assert(std::is_same_v<t530, map_t<decltype(::client_static_s::fSecureClient)>>, "type client_static_s.fSecureClient");
    static_assert(o536 == offsetof(::client_static_s, isVAC2Secure), "offsetof client_static_s.isVAC2Secure");
    static_assert(std::is_same_v<t531, map_t<decltype(::client_static_s::isVAC2Secure)>>, "type client_static_s.isVAC2Secure");
    static_assert(o537 == offsetof(::client_static_s, GameServerSteamID), "offsetof client_static_s.GameServerSteamID");
    static_assert(std::is_same_v<t532, map_t<decltype(::client_static_s::GameServerSteamID)>>, "type client_static_s.GameServerSteamID");
    static_assert(o538 == offsetof(::client_static_s, build_num), "offsetof client_static_s.build_num");
    static_assert(std::is_same_v<t533, map_t<decltype(::client_static_s::build_num)>>, "type client_static_s.build_num");
    static_assert(s30 == sizeof(::frame_s), "sizeof frame_s");
    static_assert(o539 == offsetof(::frame_s, receivedtime), "offsetof frame_s.receivedtime");
    static_assert(std::is_same_v<t534, map_t<decltype(::frame_s::receivedtime)>>, "type frame_s.receivedtime");
    static_assert(o540 == offsetof(::frame_s, latency), "offsetof frame_s.latency");
    static_assert(std::is_same_v<t535, map_t<decltype(::frame_s::latency)>>, "type frame_s.latency");
    static_assert(o541 == offsetof(::frame_s, invalid), "offsetof frame_s.invalid");
    static_assert(std::is_same_v<t536, map_t<decltype(::frame_s::invalid)>>, "type frame_s.invalid");
    static_assert(o542 == offsetof(::frame_s, choked), "offsetof frame_s.choked");
    static_assert(std::is_same_v<t537, map_t<decltype(::frame_s::choked)>>, "type frame_s.choked");
    static_assert(o543 == offsetof(::frame_s, playerstate), "offsetof frame_s.playerstate");
    static_assert(std::is_same_v<t538, map_t<decltype(::frame_s::playerstate)>>, "type frame_s.playerstate");
    static_assert(o544 == offsetof(::frame_s, time), "offsetof frame_s.time");
    static_assert(std::is_same_v<t539, map_t<decltype(::frame_s::time)>>, "type frame_s.time");
    static_assert(o545 == offsetof(::frame_s, clientdata), "offsetof frame_s.clientdata");
    static_assert(std::is_same_v<t540, map_t<decltype(::frame_s::clientdata)>>, "type frame_s.clientdata");
    static_assert(o546 == offsetof(::frame_s, weapondata), "offsetof frame_s.weapondata");
    static_assert(std::is_same_v<t541, map_t<decltype(::frame_s::weapondata)>>, "type frame_s.weapondata");
    static_assert(o547 == offsetof(::frame_s, packet_entities), "offsetof frame_s.packet_entities");
    static_assert(std::is_same_v<t542, map_t<decltype(::frame_s::packet_entities)>>, "type frame_s.packet_entities");
    static_assert(o548 == offsetof(::frame_s, clientbytes), "offsetof frame_s.clientbytes");
    static_assert(std::is_same_v<t543, map_t<decltype(::frame_s::clientbytes)>>, "type frame_s.clientbytes");
    static_assert(o549 == offsetof(::frame_s, playerinfobytes), "offsetof frame_s.playerinfobytes");
    static_assert(std::is_same_v<t544, map_t<decltype(::frame_s::playerinfobytes)>>, "type frame_s.playerinfobytes");
    static_assert(o550 == offsetof(::frame_s, packetentitybytes), "offsetof frame_s.packetentitybytes");
    static_assert(std::is_same_v<t545, map_t<decltype(::frame_s::packetentitybytes)>>, "type frame_s.packetentitybytes");
    static_assert(o551 == offsetof(::frame_s, tentitybytes), "offsetof frame_s.tentitybytes");
    static_assert(std::is_same_v<t546, map_t<decltype(::frame_s::tentitybytes)>>, "type frame_s.tentitybytes");
    static_assert(o552 == offsetof(::frame_s, soundbytes), "offsetof frame_s.soundbytes");
    static_assert(std::is_same_v<t547, map_t<decltype(::frame_s::soundbytes)>>, "type frame_s.soundbytes");
    static_assert(o553 == offsetof(::frame_s, eventbytes), "offsetof frame_s.eventbytes");
    static_assert(std::is_same_v<t548, map_t<decltype(::frame_s::eventbytes)>>, "type frame_s.eventbytes");
    static_assert(o554 == offsetof(::frame_s, usrbytes), "offsetof frame_s.usrbytes");
    static_assert(std::is_same_v<t549, map_t<decltype(::frame_s::usrbytes)>>, "type frame_s.usrbytes");
    static_assert(o555 == offsetof(::frame_s, voicebytes), "offsetof frame_s.voicebytes");
    static_assert(std::is_same_v<t550, map_t<decltype(::frame_s::voicebytes)>>, "type frame_s.voicebytes");
    static_assert(o556 == offsetof(::frame_s, msgbytes), "offsetof frame_s.msgbytes");
    static_assert(std::is_same_v<t551, map_t<decltype(::frame_s::msgbytes)>>, "type frame_s.msgbytes");
    static_assert(s31 == sizeof(::client_state_s), "sizeof client_state_s");
    static_assert(o557 == offsetof(::client_state_s, max_edicts), "offsetof client_state_s.max_edicts");
    static_assert(std::is_same_v<t552, map_t<decltype(::client_state_s::max_edicts)>>, "type client_state_s.max_edicts");
    static_assert(o558 == offsetof(::client_state_s, resourcesonhand), "offsetof client_state_s.resourcesonhand");
    static_assert(std::is_same_v<t553, map_t<decltype(::client_state_s::resourcesonhand)>>, "type client_state_s.resourcesonhand");
    static_assert(o559 == offsetof(::client_state_s, resourcesneeded), "offsetof client_state_s.resourcesneeded");
    static_assert(std::is_same_v<t554, map_t<decltype(::client_state_s::resourcesneeded)>>, "type client_state_s.resourcesneeded");
    static_assert(o560 == offsetof(::client_state_s, resourcelist), "offsetof client_state_s.resourcelist");
    static_assert(std::is_same_v<t555, map_t<decltype(::client_state_s::resourcelist)>>, "type client_state_s.resourcelist");
    static_assert(o561 == offsetof(::client_state_s, num_resources), "offsetof client_state_s.num_resources");
    static_assert(std::is_same_v<t556, map_t<decltype(::client_state_s::num_resources)>>, "type client_state_s.num_resources");
    static_assert(
        o562 == offsetof(::client_state_s, need_force_consistency_response),
        "offsetof client_state_s.need_force_consistency_response"
    );
    static_assert(
        std::is_same_v<t557, map_t<decltype(::client_state_s::need_force_consistency_response)>>,
        "type client_state_s.need_force_consistency_response"
    );
    static_assert(o563 == offsetof(::client_state_s, serverinfo), "offsetof client_state_s.serverinfo");
    static_assert(std::is_same_v<t558, map_t<decltype(::client_state_s::serverinfo)>>, "type client_state_s.serverinfo");
    static_assert(o564 == offsetof(::client_state_s, servercount), "offsetof client_state_s.servercount");
    static_assert(std::is_same_v<t559, map_t<decltype(::client_state_s::servercount)>>, "type client_state_s.servercount");
    static_assert(o565 == offsetof(::client_state_s, validsequence), "offsetof client_state_s.validsequence");
    static_assert(std::is_same_v<t560, map_t<decltype(::client_state_s::validsequence)>>, "type client_state_s.validsequence");
    static_assert(o566 == offsetof(::client_state_s, parsecount), "offsetof client_state_s.parsecount");
    static_assert(std::is_same_v<t561, map_t<decltype(::client_state_s::parsecount)>>, "type client_state_s.parsecount");
    static_assert(o567 == offsetof(::client_state_s, parsecountmod), "offsetof client_state_s.parsecountmod");
    static_assert(std::is_same_v<t562, map_t<decltype(::client_state_s::parsecountmod)>>, "type client_state_s.parsecountmod");
    static_assert(o568 == offsetof(::client_state_s, stats), "offsetof client_state_s.stats");
    static_assert(std::is_same_v<t563, map_t<decltype(::client_state_s::stats)>>, "type client_state_s.stats");
    static_assert(o569 == offsetof(::client_state_s, weapons), "offsetof client_state_s.weapons");
    static_assert(std::is_same_v<t564, map_t<decltype(::client_state_s::weapons)>>, "type client_state_s.weapons");
    static_assert(o570 == offsetof(::client_state_s, cmd), "offsetof client_state_s.cmd");
    static_assert(std::is_same_v<t565, map_t<decltype(::client_state_s::cmd)>>, "type client_state_s.cmd");
    static_assert(o571 == offsetof(::client_state_s, viewangles), "offsetof client_state_s.viewangles");
    static_assert(std::is_same_v<t566, map_t<decltype(::client_state_s::viewangles)>>, "type client_state_s.viewangles");
    static_assert(o572 == offsetof(::client_state_s, punchangle), "offsetof client_state_s.punchangle");
    static_assert(std::is_same_v<t567, map_t<decltype(::client_state_s::punchangle)>>, "type client_state_s.punchangle");
    static_assert(o573 == offsetof(::client_state_s, crosshairangle), "offsetof client_state_s.crosshairangle");
    static_assert(std::is_same_v<t568, map_t<decltype(::client_state_s::crosshairangle)>>, "type client_state_s.crosshairangle");
    static_assert(o574 == offsetof(::client_state_s, simorg), "offsetof client_state_s.simorg");
    static_assert(std::is_same_v<t569, map_t<decltype(::client_state_s::simorg)>>, "type client_state_s.simorg");
    static_assert(o575 == offsetof(::client_state_s, simvel), "offsetof client_state_s.simvel");
    static_assert(std::is_same_v<t570, map_t<decltype(::client_state_s::simvel)>>, "type client_state_s.simvel");
    static_assert(o576 == offsetof(::client_state_s, simangles), "offsetof client_state_s.simangles");
    static_assert(std::is_same_v<t571, map_t<decltype(::client_state_s::simangles)>>, "type client_state_s.simangles");
    static_assert(o577 == offsetof(::client_state_s, predicted_origins), "offsetof client_state_s.predicted_origins");
    static_assert(std::is_same_v<t572, map_t<decltype(::client_state_s::predicted_origins)>>, "type client_state_s.predicted_origins");
    static_assert(o578 == offsetof(::client_state_s, prediction_error), "offsetof client_state_s.prediction_error");
    static_assert(std::is_same_v<t573, map_t<decltype(::client_state_s::prediction_error)>>, "type client_state_s.prediction_error");
    static_assert(o579 == offsetof(::client_state_s, idealpitch), "offsetof client_state_s.idealpitch");
    static_assert(std::is_same_v<t574, map_t<decltype(::client_state_s::idealpitch)>>, "type client_state_s.idealpitch");
    static_assert(o580 == offsetof(::client_state_s, viewheight), "offsetof client_state_s.viewheight");
    static_assert(std::is_same_v<t575, map_t<decltype(::client_state_s::viewheight)>>, "type client_state_s.viewheight");
    static_assert(o581 == offsetof(::client_state_s, sf), "offsetof client_state_s.sf");
    static_assert(std::is_same_v<t576, map_t<decltype(::client_state_s::sf)>>, "type client_state_s.sf");
    static_assert(o582 == offsetof(::client_state_s, paused), "offsetof client_state_s.paused");
    static_assert(std::is_same_v<t577, map_t<decltype(::client_state_s::paused)>>, "type client_state_s.paused");
    static_assert(o583 == offsetof(::client_state_s, onground), "offsetof client_state_s.onground");
    static_assert(std::is_same_v<t578, map_t<decltype(::client_state_s::onground)>>, "type client_state_s.onground");
    static_assert(o584 == offsetof(::client_state_s, moving), "offsetof client_state_s.moving");
    static_assert(std::is_same_v<t579, map_t<decltype(::client_state_s::moving)>>, "type client_state_s.moving");
    static_assert(o585 == offsetof(::client_state_s, waterlevel), "offsetof client_state_s.waterlevel");
    static_assert(std::is_same_v<t580, map_t<decltype(::client_state_s::waterlevel)>>, "type client_state_s.waterlevel");
    static_assert(o586 == offsetof(::client_state_s, usehull), "offsetof client_state_s.usehull");
    static_assert(std::is_same_v<t581, map_t<decltype(::client_state_s::usehull)>>, "type client_state_s.usehull");
    static_assert(o587 == offsetof(::client_state_s, maxspeed), "offsetof client_state_s.maxspeed");
    static_assert(std::is_same_v<t582, map_t<decltype(::client_state_s::maxspeed)>>, "type client_state_s.maxspeed");
    static_assert(o588 == offsetof(::client_state_s, pushmsec), "offsetof client_state_s.pushmsec");
    static_assert(std::is_same_v<t583, map_t<decltype(::client_state_s::pushmsec)>>, "type client_state_s.pushmsec");
    static_assert(o589 == offsetof(::client_state_s, light_level), "offsetof client_state_s.light_level");
    static_assert(std::is_same_v<t584, map_t<decltype(::client_state_s::light_level)>>, "type client_state_s.light_level");
    static_assert(o590 == offsetof(::client_state_s, intermission), "offsetof client_state_s.intermission");
    static_assert(std::is_same_v<t585, map_t<decltype(::client_state_s::intermission)>>, "type client_state_s.intermission");
    static_assert(o591 == offsetof(::client_state_s, mtime), "offsetof client_state_s.mtime");
    static_assert(std::is_same_v<t586, map_t<decltype(::client_state_s::mtime)>>, "type client_state_s.mtime");
    static_assert(o592 == offsetof(::client_state_s, time), "offsetof client_state_s.time");
    static_assert(std::is_same_v<t587, map_t<decltype(::client_state_s::time)>>, "type client_state_s.time");
    static_assert(o593 == offsetof(::client_state_s, oldtime), "offsetof client_state_s.oldtime");
    static_assert(std::is_same_v<t588, map_t<decltype(::client_state_s::oldtime)>>, "type client_state_s.oldtime");
    static_assert(o594 == offsetof(::client_state_s, frames), "offsetof client_state_s.frames");
    static_assert(std::is_same_v<t589, map_t<decltype(::client_state_s::frames)>>, "type client_state_s.frames");
    static_assert(o595 == offsetof(::client_state_s, commands), "offsetof client_state_s.commands");
    static_assert(std::is_same_v<t590, map_t<decltype(::client_state_s::commands)>>, "type client_state_s.commands");
    static_assert(o596 == offsetof(::client_state_s, predicted_frames), "offsetof client_state_s.predicted_frames");
    static_assert(std::is_same_v<t591, map_t<decltype(::client_state_s::predicted_frames)>>, "type client_state_s.predicted_frames");
    static_assert(o597 == offsetof(::client_state_s, delta_sequence), "offsetof client_state_s.delta_sequence");
    static_assert(std::is_same_v<t592, map_t<decltype(::client_state_s::delta_sequence)>>, "type client_state_s.delta_sequence");
    static_assert(o598 == offsetof(::client_state_s, playernum), "offsetof client_state_s.playernum");
    static_assert(std::is_same_v<t593, map_t<decltype(::client_state_s::playernum)>>, "type client_state_s.playernum");
    static_assert(o599 == offsetof(::client_state_s, event_precache), "offsetof client_state_s.event_precache");
    static_assert(std::is_same_v<t594, map_t<decltype(::client_state_s::event_precache)>>, "type client_state_s.event_precache");
    static_assert(o600 == offsetof(::client_state_s, model_precache), "offsetof client_state_s.model_precache");
    static_assert(std::is_same_v<t595, map_t<decltype(::client_state_s::model_precache)>>, "type client_state_s.model_precache");
    static_assert(o601 == offsetof(::client_state_s, model_precache_count), "offsetof client_state_s.model_precache_count");
    static_assert(
        std::is_same_v<t596, map_t<decltype(::client_state_s::model_precache_count)>>,
        "type client_state_s.model_precache_count"
    );
    static_assert(o602 == offsetof(::client_state_s, sound_precache), "offsetof client_state_s.sound_precache");
    static_assert(std::is_same_v<t597, map_t<decltype(::client_state_s::sound_precache)>>, "type client_state_s.sound_precache");
    static_assert(o603 == offsetof(::client_state_s, consistency_list), "offsetof client_state_s.consistency_list");
    static_assert(std::is_same_v<t598, map_t<decltype(::client_state_s::consistency_list)>>, "type client_state_s.consistency_list");
    static_assert(o604 == offsetof(::client_state_s, num_consistency), "offsetof client_state_s.num_consistency");
    static_assert(std::is_same_v<t599, map_t<decltype(::client_state_s::num_consistency)>>, "type client_state_s.num_consistency");
    static_assert(o605 == offsetof(::client_state_s, highentity), "offsetof client_state_s.highentity");
    static_assert(std::is_same_v<t600, map_t<decltype(::client_state_s::highentity)>>, "type client_state_s.highentity");
    static_assert(o606 == offsetof(::client_state_s, levelname), "offsetof client_state_s.levelname");
    static_assert(std::is_same_v<t601, map_t<decltype(::client_state_s::levelname)>>, "type client_state_s.levelname");
    static_assert(o607 == offsetof(::client_state_s, maxclients), "offsetof client_state_s.maxclients");
    static_assert(std::is_same_v<t602, map_t<decltype(::client_state_s::maxclients)>>, "type client_state_s.maxclients");
    static_assert(o608 == offsetof(::client_state_s, gametype), "offsetof client_state_s.gametype");
    static_assert(std::is_same_v<t603, map_t<decltype(::client_state_s::gametype)>>, "type client_state_s.gametype");
    static_assert(o609 == offsetof(::client_state_s, viewentity), "offsetof client_state_s.viewentity");
    static_assert(std::is_same_v<t604, map_t<decltype(::client_state_s::viewentity)>>, "type client_state_s.viewentity");
    static_assert(o610 == offsetof(::client_state_s, worldmodel), "offsetof client_state_s.worldmodel");
    static_assert(std::is_same_v<t605, map_t<decltype(::client_state_s::worldmodel)>>, "type client_state_s.worldmodel");
    static_assert(o611 == offsetof(::client_state_s, free_efrags), "offsetof client_state_s.free_efrags");
    static_assert(std::is_same_v<t606, map_t<decltype(::client_state_s::free_efrags)>>, "type client_state_s.free_efrags");
    static_assert(o612 == offsetof(::client_state_s, num_entities), "offsetof client_state_s.num_entities");
    static_assert(std::is_same_v<t607, map_t<decltype(::client_state_s::num_entities)>>, "type client_state_s.num_entities");
    static_assert(o613 == offsetof(::client_state_s, num_statics), "offsetof client_state_s.num_statics");
    static_assert(std::is_same_v<t608, map_t<decltype(::client_state_s::num_statics)>>, "type client_state_s.num_statics");
    static_assert(o614 == offsetof(::client_state_s, viewent), "offsetof client_state_s.viewent");
    static_assert(std::is_same_v<t609, map_t<decltype(::client_state_s::viewent)>>, "type client_state_s.viewent");
    static_assert(o615 == offsetof(::client_state_s, cdtrack), "offsetof client_state_s.cdtrack");
    static_assert(std::is_same_v<t610, map_t<decltype(::client_state_s::cdtrack)>>, "type client_state_s.cdtrack");
    static_assert(o616 == offsetof(::client_state_s, looptrack), "offsetof client_state_s.looptrack");
    static_assert(std::is_same_v<t611, map_t<decltype(::client_state_s::looptrack)>>, "type client_state_s.looptrack");
    static_assert(o617 == offsetof(::client_state_s, serverCRC), "offsetof client_state_s.serverCRC");
    static_assert(std::is_same_v<t612, map_t<decltype(::client_state_s::serverCRC)>>, "type client_state_s.serverCRC");
    static_assert(o618 == offsetof(::client_state_s, clientdllmd5), "offsetof client_state_s.clientdllmd5");
    static_assert(std::is_same_v<t613, map_t<decltype(::client_state_s::clientdllmd5)>>, "type client_state_s.clientdllmd5");
    static_assert(o619 == offsetof(::client_state_s, weaponstarttime), "offsetof client_state_s.weaponstarttime");
    static_assert(std::is_same_v<t614, map_t<decltype(::client_state_s::weaponstarttime)>>, "type client_state_s.weaponstarttime");
    static_assert(o620 == offsetof(::client_state_s, weaponsequence), "offsetof client_state_s.weaponsequence");
    static_assert(std::is_same_v<t615, map_t<decltype(::client_state_s::weaponsequence)>>, "type client_state_s.weaponsequence");
    static_assert(o621 == offsetof(::client_state_s, fPrecaching), "offsetof client_state_s.fPrecaching");
    static_assert(std::is_same_v<t616, map_t<decltype(::client_state_s::fPrecaching)>>, "type client_state_s.fPrecaching");
    static_assert(o622 == offsetof(::client_state_s, pLight), "offsetof client_state_s.pLight");
    static_assert(std::is_same_v<t617, map_t<decltype(::client_state_s::pLight)>>, "type client_state_s.pLight");
    static_assert(o623 == offsetof(::client_state_s, players), "offsetof client_state_s.players");
    static_assert(std::is_same_v<t618, map_t<decltype(::client_state_s::players)>>, "type client_state_s.players");
    static_assert(o624 == offsetof(::client_state_s, instanced_baseline), "offsetof client_state_s.instanced_baseline");
    static_assert(std::is_same_v<t619, map_t<decltype(::client_state_s::instanced_baseline)>>, "type client_state_s.instanced_baseline");
    static_assert(o625 == offsetof(::client_state_s, instanced_baseline_number), "offsetof client_state_s.instanced_baseline_number");
    static_assert(
        std::is_same_v<t620, map_t<decltype(::client_state_s::instanced_baseline_number)>>,
        "type client_state_s.instanced_baseline_number"
    );
    static_assert(o626 == offsetof(::client_state_s, mapCRC), "offsetof client_state_s.mapCRC");
    static_assert(std::is_same_v<t621, map_t<decltype(::client_state_s::mapCRC)>>, "type client_state_s.mapCRC");
    static_assert(o627 == offsetof(::client_state_s, events), "offsetof client_state_s.events");
    static_assert(std::is_same_v<t622, map_t<decltype(::client_state_s::events)>>, "type client_state_s.events");
    static_assert(o628 == offsetof(::client_state_s, downloadUrl), "offsetof client_state_s.downloadUrl");
    static_assert(std::is_same_v<t623, map_t<decltype(::client_state_s::downloadUrl)>>, "type client_state_s.downloadUrl");
    static_assert(s32 == sizeof(::event_info_s), "sizeof event_info_s");
    static_assert(o629 == offsetof(::event_info_s, index), "offsetof event_info_s.index");
    static_assert(std::is_same_v<t624, map_t<decltype(::event_info_s::index)>>, "type event_info_s.index");
    static_assert(o630 == offsetof(::event_info_s, packet_index), "offsetof event_info_s.packet_index");
    static_assert(std::is_same_v<t625, map_t<decltype(::event_info_s::packet_index)>>, "type event_info_s.packet_index");
    static_assert(o631 == offsetof(::event_info_s, entity_index), "offsetof event_info_s.entity_index");
    static_assert(std::is_same_v<t626, map_t<decltype(::event_info_s::entity_index)>>, "type event_info_s.entity_index");
    static_assert(o632 == offsetof(::event_info_s, fire_time), "offsetof event_info_s.fire_time");
    static_assert(std::is_same_v<t627, map_t<decltype(::event_info_s::fire_time)>>, "type event_info_s.fire_time");
    static_assert(o633 == offsetof(::event_info_s, args), "offsetof event_info_s.args");
    static_assert(std::is_same_v<t628, map_t<decltype(::event_info_s::args)>>, "type event_info_s.args");
    static_assert(o634 == offsetof(::event_info_s, flags), "offsetof event_info_s.flags");
    static_assert(std::is_same_v<t629, map_t<decltype(::event_info_s::flags)>>, "type event_info_s.flags");
    static_assert(s33 == sizeof(::event_s), "sizeof event_s");
    static_assert(o635 == offsetof(::event_s, index), "offsetof event_s.index");
    static_assert(std::is_same_v<t630, map_t<decltype(::event_s::index)>>, "type event_s.index");
    static_assert(o636 == offsetof(::event_s, filename), "offsetof event_s.filename");
    static_assert(std::is_same_v<t631, map_t<decltype(::event_s::filename)>>, "type event_s.filename");
    static_assert(o637 == offsetof(::event_s, filesize), "offsetof event_s.filesize");
    static_assert(std::is_same_v<t632, map_t<decltype(::event_s::filesize)>>, "type event_s.filesize");
    static_assert(o638 == offsetof(::event_s, pszScript), "offsetof event_s.pszScript");
    static_assert(std::is_same_v<t633, map_t<decltype(::event_s::pszScript)>>, "type event_s.pszScript");
    static_assert(s34 == sizeof(::event_state_s), "sizeof event_state_s");
    static_assert(o639 == offsetof(::event_state_s, ei), "offsetof event_state_s.ei");
    static_assert(std::is_same_v<t634, map_t<decltype(::event_state_s::ei)>>, "type event_state_s.ei");
    static_assert(s35 == sizeof(::cacheentry_s), "sizeof cacheentry_s");
    static_assert(o640 == offsetof(::cacheentry_s, name), "offsetof cacheentry_s.name");
    static_assert(std::is_same_v<t635, map_t<decltype(::cacheentry_s::name)>>, "type cacheentry_s.name");
    static_assert(o641 == offsetof(::cacheentry_s, cache), "offsetof cacheentry_s.cache");
    static_assert(std::is_same_v<t636, map_t<decltype(::cacheentry_s::cache)>>, "type cacheentry_s.cache");
    static_assert(s36 == sizeof(::glpoly_s), "sizeof glpoly_s");
    static_assert(o642 == offsetof(::glpoly_s, next), "offsetof glpoly_s.next");
    static_assert(std::is_same_v<t637, map_t<decltype(::glpoly_s::next)>>, "type glpoly_s.next");
    static_assert(o643 == offsetof(::glpoly_s, chain), "offsetof glpoly_s.chain");
    static_assert(std::is_same_v<t638, map_t<decltype(::glpoly_s::chain)>>, "type glpoly_s.chain");
    static_assert(o644 == offsetof(::glpoly_s, numverts), "offsetof glpoly_s.numverts");
    static_assert(std::is_same_v<t639, map_t<decltype(::glpoly_s::numverts)>>, "type glpoly_s.numverts");
    static_assert(o645 == offsetof(::glpoly_s, flags), "offsetof glpoly_s.flags");
    static_assert(std::is_same_v<t640, map_t<decltype(::glpoly_s::flags)>>, "type glpoly_s.flags");
    static_assert(o646 == offsetof(::glpoly_s, verts), "offsetof glpoly_s.verts");
    static_assert(std::is_same_v<t641, map_t<decltype(::glpoly_s::verts)>>, "type glpoly_s.verts");
    static_assert(s37 == sizeof(::hull_s), "sizeof hull_s");
    static_assert(o647 == offsetof(::hull_s, clipnodes), "offsetof hull_s.clipnodes");
    static_assert(std::is_same_v<t642, map_t<decltype(::hull_s::clipnodes)>>, "type hull_s.clipnodes");
    static_assert(o648 == offsetof(::hull_s, planes), "offsetof hull_s.planes");
    static_assert(std::is_same_v<t643, map_t<decltype(::hull_s::planes)>>, "type hull_s.planes");
    static_assert(o649 == offsetof(::hull_s, firstclipnode), "offsetof hull_s.firstclipnode");
    static_assert(std::is_same_v<t644, map_t<decltype(::hull_s::firstclipnode)>>, "type hull_s.firstclipnode");
    static_assert(o650 == offsetof(::hull_s, lastclipnode), "offsetof hull_s.lastclipnode");
    static_assert(std::is_same_v<t645, map_t<decltype(::hull_s::lastclipnode)>>, "type hull_s.lastclipnode");
    static_assert(o651 == offsetof(::hull_s, clip_mins), "offsetof hull_s.clip_mins");
    static_assert(std::is_same_v<t646, map_t<decltype(::hull_s::clip_mins)>>, "type hull_s.clip_mins");
    static_assert(o652 == offsetof(::hull_s, clip_maxs), "offsetof hull_s.clip_maxs");
    static_assert(std::is_same_v<t647, map_t<decltype(::hull_s::clip_maxs)>>, "type hull_s.clip_maxs");
    static_assert(s38 == sizeof(::medge_s), "sizeof medge_s");
    static_assert(o653 == offsetof(::medge_s, v), "offsetof medge_s.v");
    static_assert(std::is_same_v<t648, map_t<decltype(::medge_s::v)>>, "type medge_s.v");
    static_assert(s39 == sizeof(::mod_known_info_s), "sizeof mod_known_info_s");
    static_assert(o654 == offsetof(::mod_known_info_s, shouldCRC), "offsetof mod_known_info_s.shouldCRC");
    static_assert(std::is_same_v<t649, map_t<decltype(::mod_known_info_s::shouldCRC)>>, "type mod_known_info_s.shouldCRC");
    static_assert(o655 == offsetof(::mod_known_info_s, firstCRCDone), "offsetof mod_known_info_s.firstCRCDone");
    static_assert(std::is_same_v<t650, map_t<decltype(::mod_known_info_s::firstCRCDone)>>, "type mod_known_info_s.firstCRCDone");
    static_assert(o656 == offsetof(::mod_known_info_s, initialCRC), "offsetof mod_known_info_s.initialCRC");
    static_assert(std::is_same_v<t651, map_t<decltype(::mod_known_info_s::initialCRC)>>, "type mod_known_info_s.initialCRC");
    static_assert(s40 == sizeof(::mplane_s), "sizeof mplane_s");
    static_assert(o657 == offsetof(::mplane_s, normal), "offsetof mplane_s.normal");
    static_assert(std::is_same_v<t652, map_t<decltype(::mplane_s::normal)>>, "type mplane_s.normal");
    static_assert(s41 == sizeof(::mspriteframe_s), "sizeof mspriteframe_s");
    static_assert(o658 == offsetof(::mspriteframe_s, width), "offsetof mspriteframe_s.width");
    static_assert(std::is_same_v<t653, map_t<decltype(::mspriteframe_s::width)>>, "type mspriteframe_s.width");
    static_assert(o659 == offsetof(::mspriteframe_s, height), "offsetof mspriteframe_s.height");
    static_assert(std::is_same_v<t654, map_t<decltype(::mspriteframe_s::height)>>, "type mspriteframe_s.height");
    static_assert(o660 == offsetof(::mspriteframe_s, up), "offsetof mspriteframe_s.up");
    static_assert(std::is_same_v<t655, map_t<decltype(::mspriteframe_s::up)>>, "type mspriteframe_s.up");
    static_assert(o661 == offsetof(::mspriteframe_s, down), "offsetof mspriteframe_s.down");
    static_assert(std::is_same_v<t656, map_t<decltype(::mspriteframe_s::down)>>, "type mspriteframe_s.down");
    static_assert(o662 == offsetof(::mspriteframe_s, left), "offsetof mspriteframe_s.left");
    static_assert(std::is_same_v<t657, map_t<decltype(::mspriteframe_s::left)>>, "type mspriteframe_s.left");
    static_assert(o663 == offsetof(::mspriteframe_s, right), "offsetof mspriteframe_s.right");
    static_assert(std::is_same_v<t658, map_t<decltype(::mspriteframe_s::right)>>, "type mspriteframe_s.right");
    static_assert(o664 == offsetof(::mspriteframe_s, gl_texturenum), "offsetof mspriteframe_s.gl_texturenum");
    static_assert(std::is_same_v<t659, map_t<decltype(::mspriteframe_s::gl_texturenum)>>, "type mspriteframe_s.gl_texturenum");
    static_assert(s42 == sizeof(::mspriteframedesc_s), "sizeof mspriteframedesc_s");
    static_assert(o665 == offsetof(::mspriteframedesc_s, type), "offsetof mspriteframedesc_s.type");
    static_assert(std::is_same_v<t660, map_t<decltype(::mspriteframedesc_s::type)>>, "type mspriteframedesc_s.type");
    static_assert(o666 == offsetof(::mspriteframedesc_s, frameptr), "offsetof mspriteframedesc_s.frameptr");
    static_assert(std::is_same_v<t661, map_t<decltype(::mspriteframedesc_s::frameptr)>>, "type mspriteframedesc_s.frameptr");
    static_assert(s43 == sizeof(::mspritegroup_s), "sizeof mspritegroup_s");
    static_assert(o667 == offsetof(::mspritegroup_s, numframes), "offsetof mspritegroup_s.numframes");
    static_assert(std::is_same_v<t662, map_t<decltype(::mspritegroup_s::numframes)>>, "type mspritegroup_s.numframes");
    static_assert(o668 == offsetof(::mspritegroup_s, intervals), "offsetof mspritegroup_s.intervals");
    static_assert(std::is_same_v<t663, map_t<decltype(::mspritegroup_s::intervals)>>, "type mspritegroup_s.intervals");
    static_assert(o669 == offsetof(::mspritegroup_s, frames), "offsetof mspritegroup_s.frames");
    static_assert(std::is_same_v<t664, map_t<decltype(::mspritegroup_s::frames)>>, "type mspritegroup_s.frames");
    static_assert(s44 == sizeof(::msurface_s), "sizeof msurface_s");
    static_assert(o670 == offsetof(::msurface_s, visframe), "offsetof msurface_s.visframe");
    static_assert(std::is_same_v<t665, map_t<decltype(::msurface_s::visframe)>>, "type msurface_s.visframe");
    static_assert(o671 == offsetof(::msurface_s, plane), "offsetof msurface_s.plane");
    static_assert(std::is_same_v<t666, map_t<decltype(::msurface_s::plane)>>, "type msurface_s.plane");
    static_assert(o672 == offsetof(::msurface_s, flags), "offsetof msurface_s.flags");
    static_assert(std::is_same_v<t667, map_t<decltype(::msurface_s::flags)>>, "type msurface_s.flags");
    static_assert(o673 == offsetof(::msurface_s, firstedge), "offsetof msurface_s.firstedge");
    static_assert(std::is_same_v<t668, map_t<decltype(::msurface_s::firstedge)>>, "type msurface_s.firstedge");
    static_assert(o674 == offsetof(::msurface_s, numedges), "offsetof msurface_s.numedges");
    static_assert(std::is_same_v<t669, map_t<decltype(::msurface_s::numedges)>>, "type msurface_s.numedges");
    static_assert(o675 == offsetof(::msurface_s, texturemins), "offsetof msurface_s.texturemins");
    static_assert(std::is_same_v<t670, map_t<decltype(::msurface_s::texturemins)>>, "type msurface_s.texturemins");
    static_assert(o676 == offsetof(::msurface_s, extents), "offsetof msurface_s.extents");
    static_assert(std::is_same_v<t671, map_t<decltype(::msurface_s::extents)>>, "type msurface_s.extents");
    static_assert(o677 == offsetof(::msurface_s, light_s), "offsetof msurface_s.light_s");
    static_assert(std::is_same_v<t672, map_t<decltype(::msurface_s::light_s)>>, "type msurface_s.light_s");
    static_assert(o678 == offsetof(::msurface_s, light_t), "offsetof msurface_s.light_t");
    static_assert(std::is_same_v<t673, map_t<decltype(::msurface_s::light_t)>>, "type msurface_s.light_t");
    static_assert(o679 == offsetof(::msurface_s, polys), "offsetof msurface_s.polys");
    static_assert(std::is_same_v<t674, map_t<decltype(::msurface_s::polys)>>, "type msurface_s.polys");
    static_assert(o680 == offsetof(::msurface_s, texturechain), "offsetof msurface_s.texturechain");
    static_assert(std::is_same_v<t675, map_t<decltype(::msurface_s::texturechain)>>, "type msurface_s.texturechain");
    static_assert(o681 == offsetof(::msurface_s, texinfo), "offsetof msurface_s.texinfo");
    static_assert(std::is_same_v<t676, map_t<decltype(::msurface_s::texinfo)>>, "type msurface_s.texinfo");
    static_assert(o682 == offsetof(::msurface_s, dlightframe), "offsetof msurface_s.dlightframe");
    static_assert(std::is_same_v<t677, map_t<decltype(::msurface_s::dlightframe)>>, "type msurface_s.dlightframe");
    static_assert(o683 == offsetof(::msurface_s, dlightbits), "offsetof msurface_s.dlightbits");
    static_assert(std::is_same_v<t678, map_t<decltype(::msurface_s::dlightbits)>>, "type msurface_s.dlightbits");
    static_assert(o684 == offsetof(::msurface_s, lightmaptexturenum), "offsetof msurface_s.lightmaptexturenum");
    static_assert(std::is_same_v<t679, map_t<decltype(::msurface_s::lightmaptexturenum)>>, "type msurface_s.lightmaptexturenum");
    static_assert(s45 == sizeof(::mtexinfo_s), "sizeof mtexinfo_s");
    static_assert(o685 == offsetof(::mtexinfo_s, vecs), "offsetof mtexinfo_s.vecs");
    static_assert(std::is_same_v<t680, map_t<decltype(::mtexinfo_s::vecs)>>, "type mtexinfo_s.vecs");
    static_assert(o686 == offsetof(::mtexinfo_s, mipadjust), "offsetof mtexinfo_s.mipadjust");
    static_assert(std::is_same_v<t681, map_t<decltype(::mtexinfo_s::mipadjust)>>, "type mtexinfo_s.mipadjust");
    static_assert(o687 == offsetof(::mtexinfo_s, texture), "offsetof mtexinfo_s.texture");
    static_assert(std::is_same_v<t682, map_t<decltype(::mtexinfo_s::texture)>>, "type mtexinfo_s.texture");
    static_assert(s46 == sizeof(::mvertex_s), "sizeof mvertex_s");
    static_assert(o688 == offsetof(::mvertex_s, position), "offsetof mvertex_s.position");
    static_assert(std::is_same_v<t683, map_t<decltype(::mvertex_s::position)>>, "type mvertex_s.position");
    static_assert(s47 == sizeof(::skin_s), "sizeof skin_s");
    static_assert(o689 == offsetof(::skin_s, keynum), "offsetof skin_s.keynum");
    static_assert(std::is_same_v<t684, map_t<decltype(::skin_s::keynum)>>, "type skin_s.keynum");
    static_assert(o690 == offsetof(::skin_s, topcolor), "offsetof skin_s.topcolor");
    static_assert(std::is_same_v<t685, map_t<decltype(::skin_s::topcolor)>>, "type skin_s.topcolor");
    static_assert(o691 == offsetof(::skin_s, bottomcolor), "offsetof skin_s.bottomcolor");
    static_assert(std::is_same_v<t686, map_t<decltype(::skin_s::bottomcolor)>>, "type skin_s.bottomcolor");
    static_assert(o692 == offsetof(::skin_s, model), "offsetof skin_s.model");
    static_assert(std::is_same_v<t687, map_t<decltype(::skin_s::model)>>, "type skin_s.model");
    static_assert(o693 == offsetof(::skin_s, name), "offsetof skin_s.name");
    static_assert(std::is_same_v<t688, map_t<decltype(::skin_s::name)>>, "type skin_s.name");
    static_assert(o694 == offsetof(::skin_s, index), "offsetof skin_s.index");
    static_assert(std::is_same_v<t689, map_t<decltype(::skin_s::index)>>, "type skin_s.index");
    static_assert(o695 == offsetof(::skin_s, source), "offsetof skin_s.source");
    static_assert(std::is_same_v<t690, map_t<decltype(::skin_s::source)>>, "type skin_s.source");
    static_assert(o696 == offsetof(::skin_s, width), "offsetof skin_s.width");
    static_assert(std::is_same_v<t691, map_t<decltype(::skin_s::width)>>, "type skin_s.width");
    static_assert(o697 == offsetof(::skin_s, height), "offsetof skin_s.height");
    static_assert(std::is_same_v<t692, map_t<decltype(::skin_s::height)>>, "type skin_s.height");
    static_assert(s48 == sizeof(::texture_s), "sizeof texture_s");
    static_assert(o698 == offsetof(::texture_s, name), "offsetof texture_s.name");
    static_assert(std::is_same_v<t693, map_t<decltype(::texture_s::name)>>, "type texture_s.name");
    static_assert(o699 == offsetof(::texture_s, width), "offsetof texture_s.width");
    static_assert(std::is_same_v<t694, map_t<decltype(::texture_s::width)>>, "type texture_s.width");
    static_assert(o700 == offsetof(::texture_s, height), "offsetof texture_s.height");
    static_assert(std::is_same_v<t695, map_t<decltype(::texture_s::height)>>, "type texture_s.height");
    static_assert(o701 == offsetof(::texture_s, gl_texturenum), "offsetof texture_s.gl_texturenum");
    static_assert(std::is_same_v<t696, map_t<decltype(::texture_s::gl_texturenum)>>, "type texture_s.gl_texturenum");
    static_assert(s49 == sizeof(::cachewad_s), "sizeof cachewad_s");
    static_assert(o702 == offsetof(::cachewad_s, name), "offsetof cachewad_s.name");
    static_assert(std::is_same_v<t697, map_t<decltype(::cachewad_s::name)>>, "type cachewad_s.name");
    static_assert(o703 == offsetof(::cachewad_s, cache), "offsetof cachewad_s.cache");
    static_assert(std::is_same_v<t698, map_t<decltype(::cachewad_s::cache)>>, "type cachewad_s.cache");
    static_assert(o704 == offsetof(::cachewad_s, cacheCount), "offsetof cachewad_s.cacheCount");
    static_assert(std::is_same_v<t699, map_t<decltype(::cachewad_s::cacheCount)>>, "type cachewad_s.cacheCount");
    static_assert(o705 == offsetof(::cachewad_s, cacheMax), "offsetof cachewad_s.cacheMax");
    static_assert(std::is_same_v<t700, map_t<decltype(::cachewad_s::cacheMax)>>, "type cachewad_s.cacheMax");
    static_assert(o706 == offsetof(::cachewad_s, lumps), "offsetof cachewad_s.lumps");
    static_assert(std::is_same_v<t701, map_t<decltype(::cachewad_s::lumps)>>, "type cachewad_s.lumps");
    static_assert(o707 == offsetof(::cachewad_s, lumpCount), "offsetof cachewad_s.lumpCount");
    static_assert(std::is_same_v<t702, map_t<decltype(::cachewad_s::lumpCount)>>, "type cachewad_s.lumpCount");
    static_assert(o708 == offsetof(::cachewad_s, cacheExtra), "offsetof cachewad_s.cacheExtra");
    static_assert(std::is_same_v<t703, map_t<decltype(::cachewad_s::cacheExtra)>>, "type cachewad_s.cacheExtra");
    static_assert(o709 == offsetof(::cachewad_s, pfnCacheBuild), "offsetof cachewad_s.pfnCacheBuild");
    static_assert(std::is_same_v<t704, map_t<decltype(::cachewad_s::pfnCacheBuild)>>, "type cachewad_s.pfnCacheBuild");
    static_assert(o710 == offsetof(::cachewad_s, numpaths), "offsetof cachewad_s.numpaths");
    static_assert(std::is_same_v<t705, map_t<decltype(::cachewad_s::numpaths)>>, "type cachewad_s.numpaths");
    static_assert(o711 == offsetof(::cachewad_s, basedirs), "offsetof cachewad_s.basedirs");
    static_assert(std::is_same_v<t706, map_t<decltype(::cachewad_s::basedirs)>>, "type cachewad_s.basedirs");
    static_assert(o712 == offsetof(::cachewad_s, lumppathindices), "offsetof cachewad_s.lumppathindices");
    static_assert(std::is_same_v<t707, map_t<decltype(::cachewad_s::lumppathindices)>>, "type cachewad_s.lumppathindices");
    static_assert(o713 == offsetof(::cachewad_s, tempWad), "offsetof cachewad_s.tempWad");
    static_assert(std::is_same_v<t708, map_t<decltype(::cachewad_s::tempWad)>>, "type cachewad_s.tempWad");
    static_assert(s50 == sizeof(::model_s), "sizeof model_s");
    static_assert(o714 == offsetof(::model_s, name), "offsetof model_s.name");
    static_assert(std::is_same_v<t709, map_t<decltype(::model_s::name)>>, "type model_s.name");
    static_assert(o715 == offsetof(::model_s, needload), "offsetof model_s.needload");
    static_assert(std::is_same_v<t710, map_t<decltype(::model_s::needload)>>, "type model_s.needload");
    static_assert(o716 == offsetof(::model_s, type), "offsetof model_s.type");
    static_assert(std::is_same_v<t711, map_t<decltype(::model_s::type)>>, "type model_s.type");
    static_assert(o717 == offsetof(::model_s, numframes), "offsetof model_s.numframes");
    static_assert(std::is_same_v<t712, map_t<decltype(::model_s::numframes)>>, "type model_s.numframes");
    static_assert(o718 == offsetof(::model_s, synctype), "offsetof model_s.synctype");
    static_assert(std::is_same_v<t713, map_t<decltype(::model_s::synctype)>>, "type model_s.synctype");
    static_assert(o719 == offsetof(::model_s, flags), "offsetof model_s.flags");
    static_assert(std::is_same_v<t714, map_t<decltype(::model_s::flags)>>, "type model_s.flags");
    static_assert(o720 == offsetof(::model_s, mins), "offsetof model_s.mins");
    static_assert(std::is_same_v<t715, map_t<decltype(::model_s::mins)>>, "type model_s.mins");
    static_assert(o721 == offsetof(::model_s, maxs), "offsetof model_s.maxs");
    static_assert(std::is_same_v<t716, map_t<decltype(::model_s::maxs)>>, "type model_s.maxs");
    static_assert(o722 == offsetof(::model_s, radius), "offsetof model_s.radius");
    static_assert(std::is_same_v<t717, map_t<decltype(::model_s::radius)>>, "type model_s.radius");
    static_assert(o723 == offsetof(::model_s, firstmodelsurface), "offsetof model_s.firstmodelsurface");
    static_assert(std::is_same_v<t718, map_t<decltype(::model_s::firstmodelsurface)>>, "type model_s.firstmodelsurface");
    static_assert(o724 == offsetof(::model_s, nummodelsurfaces), "offsetof model_s.nummodelsurfaces");
    static_assert(std::is_same_v<t719, map_t<decltype(::model_s::nummodelsurfaces)>>, "type model_s.nummodelsurfaces");
    static_assert(o725 == offsetof(::model_s, numsubmodels), "offsetof model_s.numsubmodels");
    static_assert(std::is_same_v<t720, map_t<decltype(::model_s::numsubmodels)>>, "type model_s.numsubmodels");
    static_assert(o726 == offsetof(::model_s, submodels), "offsetof model_s.submodels");
    static_assert(std::is_same_v<t721, map_t<decltype(::model_s::submodels)>>, "type model_s.submodels");
    static_assert(o727 == offsetof(::model_s, numplanes), "offsetof model_s.numplanes");
    static_assert(std::is_same_v<t722, map_t<decltype(::model_s::numplanes)>>, "type model_s.numplanes");
    static_assert(o728 == offsetof(::model_s, planes), "offsetof model_s.planes");
    static_assert(std::is_same_v<t723, map_t<decltype(::model_s::planes)>>, "type model_s.planes");
    static_assert(o729 == offsetof(::model_s, numleafs), "offsetof model_s.numleafs");
    static_assert(std::is_same_v<t724, map_t<decltype(::model_s::numleafs)>>, "type model_s.numleafs");
    static_assert(o730 == offsetof(::model_s, leafs), "offsetof model_s.leafs");
    static_assert(std::is_same_v<t725, map_t<decltype(::model_s::leafs)>>, "type model_s.leafs");
    static_assert(o731 == offsetof(::model_s, numvertexes), "offsetof model_s.numvertexes");
    static_assert(std::is_same_v<t726, map_t<decltype(::model_s::numvertexes)>>, "type model_s.numvertexes");
    static_assert(o732 == offsetof(::model_s, vertexes), "offsetof model_s.vertexes");
    static_assert(std::is_same_v<t727, map_t<decltype(::model_s::vertexes)>>, "type model_s.vertexes");
    static_assert(o733 == offsetof(::model_s, numedges), "offsetof model_s.numedges");
    static_assert(std::is_same_v<t728, map_t<decltype(::model_s::numedges)>>, "type model_s.numedges");
    static_assert(o734 == offsetof(::model_s, edges), "offsetof model_s.edges");
    static_assert(std::is_same_v<t729, map_t<decltype(::model_s::edges)>>, "type model_s.edges");
    static_assert(o735 == offsetof(::model_s, numnodes), "offsetof model_s.numnodes");
    static_assert(std::is_same_v<t730, map_t<decltype(::model_s::numnodes)>>, "type model_s.numnodes");
    static_assert(o736 == offsetof(::model_s, nodes), "offsetof model_s.nodes");
    static_assert(std::is_same_v<t731, map_t<decltype(::model_s::nodes)>>, "type model_s.nodes");
    static_assert(o737 == offsetof(::model_s, numtexinfo), "offsetof model_s.numtexinfo");
    static_assert(std::is_same_v<t732, map_t<decltype(::model_s::numtexinfo)>>, "type model_s.numtexinfo");
    static_assert(o738 == offsetof(::model_s, texinfo), "offsetof model_s.texinfo");
    static_assert(std::is_same_v<t733, map_t<decltype(::model_s::texinfo)>>, "type model_s.texinfo");
    static_assert(o739 == offsetof(::model_s, numsurfaces), "offsetof model_s.numsurfaces");
    static_assert(std::is_same_v<t734, map_t<decltype(::model_s::numsurfaces)>>, "type model_s.numsurfaces");
    static_assert(o740 == offsetof(::model_s, surfaces), "offsetof model_s.surfaces");
    static_assert(std::is_same_v<t735, map_t<decltype(::model_s::surfaces)>>, "type model_s.surfaces");
    static_assert(o741 == offsetof(::model_s, numsurfedges), "offsetof model_s.numsurfedges");
    static_assert(std::is_same_v<t736, map_t<decltype(::model_s::numsurfedges)>>, "type model_s.numsurfedges");
    static_assert(o742 == offsetof(::model_s, surfedges), "offsetof model_s.surfedges");
    static_assert(std::is_same_v<t737, map_t<decltype(::model_s::surfedges)>>, "type model_s.surfedges");
    static_assert(o743 == offsetof(::model_s, numclipnodes), "offsetof model_s.numclipnodes");
    static_assert(std::is_same_v<t738, map_t<decltype(::model_s::numclipnodes)>>, "type model_s.numclipnodes");
    static_assert(o744 == offsetof(::model_s, clipnodes), "offsetof model_s.clipnodes");
    static_assert(std::is_same_v<t739, map_t<decltype(::model_s::clipnodes)>>, "type model_s.clipnodes");
    static_assert(o745 == offsetof(::model_s, nummarksurfaces), "offsetof model_s.nummarksurfaces");
    static_assert(std::is_same_v<t740, map_t<decltype(::model_s::nummarksurfaces)>>, "type model_s.nummarksurfaces");
    static_assert(o746 == offsetof(::model_s, marksurfaces), "offsetof model_s.marksurfaces");
    static_assert(std::is_same_v<t741, map_t<decltype(::model_s::marksurfaces)>>, "type model_s.marksurfaces");
    static_assert(o747 == offsetof(::model_s, hulls), "offsetof model_s.hulls");
    static_assert(std::is_same_v<t742, map_t<decltype(::model_s::hulls)>>, "type model_s.hulls");
    static_assert(o748 == offsetof(::model_s, numtextures), "offsetof model_s.numtextures");
    static_assert(std::is_same_v<t743, map_t<decltype(::model_s::numtextures)>>, "type model_s.numtextures");
    static_assert(o749 == offsetof(::model_s, textures), "offsetof model_s.textures");
    static_assert(std::is_same_v<t744, map_t<decltype(::model_s::textures)>>, "type model_s.textures");
    static_assert(o750 == offsetof(::model_s, visdata), "offsetof model_s.visdata");
    static_assert(std::is_same_v<t745, map_t<decltype(::model_s::visdata)>>, "type model_s.visdata");
    static_assert(o751 == offsetof(::model_s, lightdata), "offsetof model_s.lightdata");
    static_assert(std::is_same_v<t746, map_t<decltype(::model_s::lightdata)>>, "type model_s.lightdata");
    static_assert(o752 == offsetof(::model_s, entities), "offsetof model_s.entities");
    static_assert(std::is_same_v<t747, map_t<decltype(::model_s::entities)>>, "type model_s.entities");
    static_assert(o753 == offsetof(::model_s, cache), "offsetof model_s.cache");
    static_assert(std::is_same_v<t748, map_t<decltype(::model_s::cache)>>, "type model_s.cache");
    static_assert(s51 == sizeof(::msprite_s), "sizeof msprite_s");
    static_assert(o754 == offsetof(::msprite_s, type), "offsetof msprite_s.type");
    static_assert(std::is_same_v<t749, map_t<decltype(::msprite_s::type)>>, "type msprite_s.type");
    static_assert(o755 == offsetof(::msprite_s, texFormat), "offsetof msprite_s.texFormat");
    static_assert(std::is_same_v<t750, map_t<decltype(::msprite_s::texFormat)>>, "type msprite_s.texFormat");
    static_assert(o756 == offsetof(::msprite_s, maxwidth), "offsetof msprite_s.maxwidth");
    static_assert(std::is_same_v<t751, map_t<decltype(::msprite_s::maxwidth)>>, "type msprite_s.maxwidth");
    static_assert(o757 == offsetof(::msprite_s, maxheight), "offsetof msprite_s.maxheight");
    static_assert(std::is_same_v<t752, map_t<decltype(::msprite_s::maxheight)>>, "type msprite_s.maxheight");
    static_assert(o758 == offsetof(::msprite_s, numframes), "offsetof msprite_s.numframes");
    static_assert(std::is_same_v<t753, map_t<decltype(::msprite_s::numframes)>>, "type msprite_s.numframes");
    static_assert(o759 == offsetof(::msprite_s, paloffset), "offsetof msprite_s.paloffset");
    static_assert(std::is_same_v<t754, map_t<decltype(::msprite_s::paloffset)>>, "type msprite_s.paloffset");
    static_assert(o760 == offsetof(::msprite_s, beamlength), "offsetof msprite_s.beamlength");
    static_assert(std::is_same_v<t755, map_t<decltype(::msprite_s::beamlength)>>, "type msprite_s.beamlength");
    static_assert(o761 == offsetof(::msprite_s, cachespot), "offsetof msprite_s.cachespot");
    static_assert(std::is_same_v<t756, map_t<decltype(::msprite_s::cachespot)>>, "type msprite_s.cachespot");
    static_assert(o762 == offsetof(::msprite_s, frames), "offsetof msprite_s.frames");
    static_assert(std::is_same_v<t757, map_t<decltype(::msprite_s::frames)>>, "type msprite_s.frames");
    static_assert(s52 == sizeof(::downloadtime_s), "sizeof downloadtime_s");
    static_assert(o763 == offsetof(::downloadtime_s, bUsed), "offsetof downloadtime_s.bUsed");
    static_assert(std::is_same_v<t758, map_t<decltype(::downloadtime_s::bUsed)>>, "type downloadtime_s.bUsed");
    static_assert(o764 == offsetof(::downloadtime_s, fTime), "offsetof downloadtime_s.fTime");
    static_assert(std::is_same_v<t759, map_t<decltype(::downloadtime_s::fTime)>>, "type downloadtime_s.fTime");
    static_assert(o765 == offsetof(::downloadtime_s, nBytesRemaining), "offsetof downloadtime_s.nBytesRemaining");
    static_assert(std::is_same_v<t760, map_t<decltype(::downloadtime_s::nBytesRemaining)>>, "type downloadtime_s.nBytesRemaining");
    static_assert(s53 == sizeof(::flowstats_s), "sizeof flowstats_s");
    static_assert(o766 == offsetof(::flowstats_s, size), "offsetof flowstats_s.size");
    static_assert(std::is_same_v<t761, map_t<decltype(::flowstats_s::size)>>, "type flowstats_s.size");
    static_assert(o767 == offsetof(::flowstats_s, time), "offsetof flowstats_s.time");
    static_assert(std::is_same_v<t762, map_t<decltype(::flowstats_s::time)>>, "type flowstats_s.time");
    static_assert(s54 == sizeof(::sizebuf_s), "sizeof sizebuf_s");
    static_assert(o768 == offsetof(::sizebuf_s, buffername), "offsetof sizebuf_s.buffername");
    static_assert(std::is_same_v<t763, map_t<decltype(::sizebuf_s::buffername)>>, "type sizebuf_s.buffername");
    static_assert(o769 == offsetof(::sizebuf_s, flags), "offsetof sizebuf_s.flags");
    static_assert(std::is_same_v<t764, map_t<decltype(::sizebuf_s::flags)>>, "type sizebuf_s.flags");
    static_assert(o770 == offsetof(::sizebuf_s, data), "offsetof sizebuf_s.data");
    static_assert(std::is_same_v<t765, map_t<decltype(::sizebuf_s::data)>>, "type sizebuf_s.data");
    static_assert(o771 == offsetof(::sizebuf_s, maxsize), "offsetof sizebuf_s.maxsize");
    static_assert(std::is_same_v<t766, map_t<decltype(::sizebuf_s::maxsize)>>, "type sizebuf_s.maxsize");
    static_assert(o772 == offsetof(::sizebuf_s, cursize), "offsetof sizebuf_s.cursize");
    static_assert(std::is_same_v<t767, map_t<decltype(::sizebuf_s::cursize)>>, "type sizebuf_s.cursize");
    static_assert(s55 == sizeof(::flow_s), "sizeof flow_s");
    static_assert(o773 == offsetof(::flow_s, stats), "offsetof flow_s.stats");
    static_assert(std::is_same_v<t768, map_t<decltype(::flow_s::stats)>>, "type flow_s.stats");
    static_assert(o774 == offsetof(::flow_s, current), "offsetof flow_s.current");
    static_assert(std::is_same_v<t769, map_t<decltype(::flow_s::current)>>, "type flow_s.current");
    static_assert(o775 == offsetof(::flow_s, nextcompute), "offsetof flow_s.nextcompute");
    static_assert(std::is_same_v<t770, map_t<decltype(::flow_s::nextcompute)>>, "type flow_s.nextcompute");
    static_assert(o776 == offsetof(::flow_s, kbytespersec), "offsetof flow_s.kbytespersec");
    static_assert(std::is_same_v<t771, map_t<decltype(::flow_s::kbytespersec)>>, "type flow_s.kbytespersec");
    static_assert(o777 == offsetof(::flow_s, avgkbytespersec), "offsetof flow_s.avgkbytespersec");
    static_assert(std::is_same_v<t772, map_t<decltype(::flow_s::avgkbytespersec)>>, "type flow_s.avgkbytespersec");
    static_assert(s56 == sizeof(::fragbuf_s), "sizeof fragbuf_s");
    static_assert(o778 == offsetof(::fragbuf_s, next), "offsetof fragbuf_s.next");
    static_assert(std::is_same_v<t773, map_t<decltype(::fragbuf_s::next)>>, "type fragbuf_s.next");
    static_assert(o779 == offsetof(::fragbuf_s, bufferid), "offsetof fragbuf_s.bufferid");
    static_assert(std::is_same_v<t774, map_t<decltype(::fragbuf_s::bufferid)>>, "type fragbuf_s.bufferid");
    static_assert(o780 == offsetof(::fragbuf_s, frag_message), "offsetof fragbuf_s.frag_message");
    static_assert(std::is_same_v<t775, map_t<decltype(::fragbuf_s::frag_message)>>, "type fragbuf_s.frag_message");
    static_assert(s57 == sizeof(::incomingtransfer_s), "sizeof incomingtransfer_s");
    static_assert(o781 == offsetof(::incomingtransfer_s, doneregistering), "offsetof incomingtransfer_s.doneregistering");
    static_assert(std::is_same_v<t776, map_t<decltype(::incomingtransfer_s::doneregistering)>>, "type incomingtransfer_s.doneregistering");
    static_assert(o782 == offsetof(::incomingtransfer_s, percent), "offsetof incomingtransfer_s.percent");
    static_assert(std::is_same_v<t777, map_t<decltype(::incomingtransfer_s::percent)>>, "type incomingtransfer_s.percent");
    static_assert(o783 == offsetof(::incomingtransfer_s, downloadrequested), "offsetof incomingtransfer_s.downloadrequested");
    static_assert(
        std::is_same_v<t778, map_t<decltype(::incomingtransfer_s::downloadrequested)>>,
        "type incomingtransfer_s.downloadrequested"
    );
    static_assert(o784 == offsetof(::incomingtransfer_s, rgStats), "offsetof incomingtransfer_s.rgStats");
    static_assert(std::is_same_v<t779, map_t<decltype(::incomingtransfer_s::rgStats)>>, "type incomingtransfer_s.rgStats");
    static_assert(o785 == offsetof(::incomingtransfer_s, nCurStat), "offsetof incomingtransfer_s.nCurStat");
    static_assert(std::is_same_v<t780, map_t<decltype(::incomingtransfer_s::nCurStat)>>, "type incomingtransfer_s.nCurStat");
    static_assert(o786 == offsetof(::incomingtransfer_s, nTotalSize), "offsetof incomingtransfer_s.nTotalSize");
    static_assert(std::is_same_v<t781, map_t<decltype(::incomingtransfer_s::nTotalSize)>>, "type incomingtransfer_s.nTotalSize");
    static_assert(o787 == offsetof(::incomingtransfer_s, nTotalToTransfer), "offsetof incomingtransfer_s.nTotalToTransfer");
    static_assert(
        std::is_same_v<t782, map_t<decltype(::incomingtransfer_s::nTotalToTransfer)>>,
        "type incomingtransfer_s.nTotalToTransfer"
    );
    static_assert(o788 == offsetof(::incomingtransfer_s, nRemainingToTransfer), "offsetof incomingtransfer_s.nRemainingToTransfer");
    static_assert(
        std::is_same_v<t783, map_t<decltype(::incomingtransfer_s::nRemainingToTransfer)>>,
        "type incomingtransfer_s.nRemainingToTransfer"
    );
    static_assert(o789 == offsetof(::incomingtransfer_s, fLastStatusUpdate), "offsetof incomingtransfer_s.fLastStatusUpdate");
    static_assert(
        std::is_same_v<t784, map_t<decltype(::incomingtransfer_s::fLastStatusUpdate)>>,
        "type incomingtransfer_s.fLastStatusUpdate"
    );
    static_assert(o790 == offsetof(::incomingtransfer_s, custom), "offsetof incomingtransfer_s.custom");
    static_assert(std::is_same_v<t785, map_t<decltype(::incomingtransfer_s::custom)>>, "type incomingtransfer_s.custom");
    static_assert(s58 == sizeof(::netadr_s), "sizeof netadr_s");
    // netadr_s.type: comparisons skipped - declared private in the old header
    // netadr_s.ip: comparisons skipped - declared private in the old header
    // netadr_s.ipx: comparisons skipped - declared private in the old header
    // netadr_s.port: comparisons skipped - declared private in the old header
    static_assert(s59 == sizeof(::netchan_s), "sizeof netchan_s");
    static_assert(o791 == offsetof(::netchan_s, sock), "offsetof netchan_s.sock");
    static_assert(std::is_same_v<t786, map_t<decltype(::netchan_s::sock)>>, "type netchan_s.sock");
    static_assert(o792 == offsetof(::netchan_s, remote_address), "offsetof netchan_s.remote_address");
    static_assert(std::is_same_v<t787, map_t<decltype(::netchan_s::remote_address)>>, "type netchan_s.remote_address");
    static_assert(o793 == offsetof(::netchan_s, player_slot), "offsetof netchan_s.player_slot");
    static_assert(std::is_same_v<t788, map_t<decltype(::netchan_s::player_slot)>>, "type netchan_s.player_slot");
    static_assert(o794 == offsetof(::netchan_s, last_received), "offsetof netchan_s.last_received");
    static_assert(std::is_same_v<t789, map_t<decltype(::netchan_s::last_received)>>, "type netchan_s.last_received");
    static_assert(o795 == offsetof(::netchan_s, connect_time), "offsetof netchan_s.connect_time");
    static_assert(std::is_same_v<t790, map_t<decltype(::netchan_s::connect_time)>>, "type netchan_s.connect_time");
    static_assert(o796 == offsetof(::netchan_s, rate), "offsetof netchan_s.rate");
    static_assert(std::is_same_v<t791, map_t<decltype(::netchan_s::rate)>>, "type netchan_s.rate");
    static_assert(o797 == offsetof(::netchan_s, cleartime), "offsetof netchan_s.cleartime");
    static_assert(std::is_same_v<t792, map_t<decltype(::netchan_s::cleartime)>>, "type netchan_s.cleartime");
    static_assert(o798 == offsetof(::netchan_s, incoming_sequence), "offsetof netchan_s.incoming_sequence");
    static_assert(std::is_same_v<t793, map_t<decltype(::netchan_s::incoming_sequence)>>, "type netchan_s.incoming_sequence");
    static_assert(o799 == offsetof(::netchan_s, incoming_acknowledged), "offsetof netchan_s.incoming_acknowledged");
    static_assert(std::is_same_v<t794, map_t<decltype(::netchan_s::incoming_acknowledged)>>, "type netchan_s.incoming_acknowledged");
    static_assert(o800 == offsetof(::netchan_s, incoming_reliable_acknowledged), "offsetof netchan_s.incoming_reliable_acknowledged");
    static_assert(
        std::is_same_v<t795, map_t<decltype(::netchan_s::incoming_reliable_acknowledged)>>,
        "type netchan_s.incoming_reliable_acknowledged"
    );
    static_assert(o801 == offsetof(::netchan_s, incoming_reliable_sequence), "offsetof netchan_s.incoming_reliable_sequence");
    static_assert(
        std::is_same_v<t796, map_t<decltype(::netchan_s::incoming_reliable_sequence)>>,
        "type netchan_s.incoming_reliable_sequence"
    );
    static_assert(o802 == offsetof(::netchan_s, outgoing_sequence), "offsetof netchan_s.outgoing_sequence");
    static_assert(std::is_same_v<t797, map_t<decltype(::netchan_s::outgoing_sequence)>>, "type netchan_s.outgoing_sequence");
    static_assert(o803 == offsetof(::netchan_s, reliable_sequence), "offsetof netchan_s.reliable_sequence");
    static_assert(std::is_same_v<t798, map_t<decltype(::netchan_s::reliable_sequence)>>, "type netchan_s.reliable_sequence");
    static_assert(o804 == offsetof(::netchan_s, last_reliable_sequence), "offsetof netchan_s.last_reliable_sequence");
    static_assert(std::is_same_v<t799, map_t<decltype(::netchan_s::last_reliable_sequence)>>, "type netchan_s.last_reliable_sequence");
    static_assert(o805 == offsetof(::netchan_s, connection_status), "offsetof netchan_s.connection_status");
    static_assert(std::is_same_v<t800, map_t<decltype(::netchan_s::connection_status)>>, "type netchan_s.connection_status");
    static_assert(o806 == offsetof(::netchan_s, pfnNetchan_Blocksize), "offsetof netchan_s.pfnNetchan_Blocksize");
    static_assert(std::is_same_v<t801, map_t<decltype(::netchan_s::pfnNetchan_Blocksize)>>, "type netchan_s.pfnNetchan_Blocksize");
    static_assert(o807 == offsetof(::netchan_s, message), "offsetof netchan_s.message");
    static_assert(std::is_same_v<t802, map_t<decltype(::netchan_s::message)>>, "type netchan_s.message");
    static_assert(o808 == offsetof(::netchan_s, message_buf), "offsetof netchan_s.message_buf");
    static_assert(std::is_same_v<t803, map_t<decltype(::netchan_s::message_buf)>>, "type netchan_s.message_buf");
    static_assert(o809 == offsetof(::netchan_s, reliable_length), "offsetof netchan_s.reliable_length");
    static_assert(std::is_same_v<t804, map_t<decltype(::netchan_s::reliable_length)>>, "type netchan_s.reliable_length");
    static_assert(o810 == offsetof(::netchan_s, reliable_buf), "offsetof netchan_s.reliable_buf");
    static_assert(std::is_same_v<t805, map_t<decltype(::netchan_s::reliable_buf)>>, "type netchan_s.reliable_buf");
    static_assert(o811 == offsetof(::netchan_s, waitlist), "offsetof netchan_s.waitlist");
    static_assert(std::is_same_v<t806, map_t<decltype(::netchan_s::waitlist)>>, "type netchan_s.waitlist");
    static_assert(o812 == offsetof(::netchan_s, reliable_fragment), "offsetof netchan_s.reliable_fragment");
    static_assert(std::is_same_v<t807, map_t<decltype(::netchan_s::reliable_fragment)>>, "type netchan_s.reliable_fragment");
    static_assert(o813 == offsetof(::netchan_s, reliable_fragid), "offsetof netchan_s.reliable_fragid");
    static_assert(std::is_same_v<t808, map_t<decltype(::netchan_s::reliable_fragid)>>, "type netchan_s.reliable_fragid");
    static_assert(o814 == offsetof(::netchan_s, fragbufs), "offsetof netchan_s.fragbufs");
    static_assert(std::is_same_v<t809, map_t<decltype(::netchan_s::fragbufs)>>, "type netchan_s.fragbufs");
    static_assert(o815 == offsetof(::netchan_s, fragbufcount), "offsetof netchan_s.fragbufcount");
    static_assert(std::is_same_v<t810, map_t<decltype(::netchan_s::fragbufcount)>>, "type netchan_s.fragbufcount");
    static_assert(o816 == offsetof(::netchan_s, frag_startpos), "offsetof netchan_s.frag_startpos");
    static_assert(std::is_same_v<t811, map_t<decltype(::netchan_s::frag_startpos)>>, "type netchan_s.frag_startpos");
    static_assert(o817 == offsetof(::netchan_s, frag_length), "offsetof netchan_s.frag_length");
    static_assert(std::is_same_v<t812, map_t<decltype(::netchan_s::frag_length)>>, "type netchan_s.frag_length");
    static_assert(o818 == offsetof(::netchan_s, incomingbufs), "offsetof netchan_s.incomingbufs");
    static_assert(std::is_same_v<t813, map_t<decltype(::netchan_s::incomingbufs)>>, "type netchan_s.incomingbufs");
    static_assert(o819 == offsetof(::netchan_s, incomingready), "offsetof netchan_s.incomingready");
    static_assert(std::is_same_v<t814, map_t<decltype(::netchan_s::incomingready)>>, "type netchan_s.incomingready");
    static_assert(o820 == offsetof(::netchan_s, incomingfilename), "offsetof netchan_s.incomingfilename");
    static_assert(std::is_same_v<t815, map_t<decltype(::netchan_s::incomingfilename)>>, "type netchan_s.incomingfilename");
    static_assert(o821 == offsetof(::netchan_s, tempbuffer), "offsetof netchan_s.tempbuffer");
    static_assert(std::is_same_v<t816, map_t<decltype(::netchan_s::tempbuffer)>>, "type netchan_s.tempbuffer");
    static_assert(o822 == offsetof(::netchan_s, tempbuffersize), "offsetof netchan_s.tempbuffersize");
    static_assert(std::is_same_v<t817, map_t<decltype(::netchan_s::tempbuffersize)>>, "type netchan_s.tempbuffersize");
    static_assert(o823 == offsetof(::netchan_s, flow), "offsetof netchan_s.flow");
    static_assert(std::is_same_v<t818, map_t<decltype(::netchan_s::flow)>>, "type netchan_s.flow");
    static_assert(s60 == sizeof(::_resourceinfo_t), "sizeof _resourceinfo_t");
    static_assert(o824 == offsetof(::_resourceinfo_t, size), "offsetof _resourceinfo_t.size");
    static_assert(std::is_same_v<t819, map_t<decltype(::_resourceinfo_t::size)>>, "type _resourceinfo_t.size");
    static_assert(s61 == sizeof(::resource_s), "sizeof resource_s");
    static_assert(o825 == offsetof(::resource_s, szFileName), "offsetof resource_s.szFileName");
    static_assert(std::is_same_v<t820, map_t<decltype(::resource_s::szFileName)>>, "type resource_s.szFileName");
    static_assert(o826 == offsetof(::resource_s, type), "offsetof resource_s.type");
    static_assert(std::is_same_v<t821, map_t<decltype(::resource_s::type)>>, "type resource_s.type");
    static_assert(o827 == offsetof(::resource_s, nIndex), "offsetof resource_s.nIndex");
    static_assert(std::is_same_v<t822, map_t<decltype(::resource_s::nIndex)>>, "type resource_s.nIndex");
    static_assert(o828 == offsetof(::resource_s, nDownloadSize), "offsetof resource_s.nDownloadSize");
    static_assert(std::is_same_v<t823, map_t<decltype(::resource_s::nDownloadSize)>>, "type resource_s.nDownloadSize");
    static_assert(o829 == offsetof(::resource_s, ucFlags), "offsetof resource_s.ucFlags");
    static_assert(std::is_same_v<t824, map_t<decltype(::resource_s::ucFlags)>>, "type resource_s.ucFlags");
    static_assert(o830 == offsetof(::resource_s, rgucMD5_hash), "offsetof resource_s.rgucMD5_hash");
    static_assert(std::is_same_v<t825, map_t<decltype(::resource_s::rgucMD5_hash)>>, "type resource_s.rgucMD5_hash");
    static_assert(o831 == offsetof(::resource_s, playernum), "offsetof resource_s.playernum");
    static_assert(std::is_same_v<t826, map_t<decltype(::resource_s::playernum)>>, "type resource_s.playernum");
    static_assert(o832 == offsetof(::resource_s, rguc_reserved), "offsetof resource_s.rguc_reserved");
    static_assert(std::is_same_v<t827, map_t<decltype(::resource_s::rguc_reserved)>>, "type resource_s.rguc_reserved");
    static_assert(o833 == offsetof(::resource_s, pNext), "offsetof resource_s.pNext");
    static_assert(std::is_same_v<t828, map_t<decltype(::resource_s::pNext)>>, "type resource_s.pNext");
    static_assert(o834 == offsetof(::resource_s, pPrev), "offsetof resource_s.pPrev");
    static_assert(std::is_same_v<t829, map_t<decltype(::resource_s::pPrev)>>, "type resource_s.pPrev");
    static_assert(s62 == sizeof(::resourceinfo_s), "sizeof resourceinfo_s");
    static_assert(o835 == offsetof(::resourceinfo_s, info), "offsetof resourceinfo_s.info");
    static_assert(std::is_same_v<t830, map_t<decltype(::resourceinfo_s::info)>>, "type resourceinfo_s.info");
    static_assert(s63 == sizeof(::customization_s), "sizeof customization_s");
    static_assert(o836 == offsetof(::customization_s, bInUse), "offsetof customization_s.bInUse");
    static_assert(std::is_same_v<t831, map_t<decltype(::customization_s::bInUse)>>, "type customization_s.bInUse");
    static_assert(o837 == offsetof(::customization_s, resource), "offsetof customization_s.resource");
    static_assert(std::is_same_v<t832, map_t<decltype(::customization_s::resource)>>, "type customization_s.resource");
    static_assert(o838 == offsetof(::customization_s, bTranslated), "offsetof customization_s.bTranslated");
    static_assert(std::is_same_v<t833, map_t<decltype(::customization_s::bTranslated)>>, "type customization_s.bTranslated");
    static_assert(o839 == offsetof(::customization_s, nUserData1), "offsetof customization_s.nUserData1");
    static_assert(std::is_same_v<t834, map_t<decltype(::customization_s::nUserData1)>>, "type customization_s.nUserData1");
    static_assert(o840 == offsetof(::customization_s, nUserData2), "offsetof customization_s.nUserData2");
    static_assert(std::is_same_v<t835, map_t<decltype(::customization_s::nUserData2)>>, "type customization_s.nUserData2");
    static_assert(o841 == offsetof(::customization_s, pInfo), "offsetof customization_s.pInfo");
    static_assert(std::is_same_v<t836, map_t<decltype(::customization_s::pInfo)>>, "type customization_s.pInfo");
    static_assert(o842 == offsetof(::customization_s, pBuffer), "offsetof customization_s.pBuffer");
    static_assert(std::is_same_v<t837, map_t<decltype(::customization_s::pBuffer)>>, "type customization_s.pBuffer");
    static_assert(o843 == offsetof(::customization_s, pNext), "offsetof customization_s.pNext");
    static_assert(std::is_same_v<t838, map_t<decltype(::customization_s::pNext)>>, "type customization_s.pNext");
    static_assert(s64 == sizeof(::server_static_s), "sizeof server_static_s");
    static_assert(o844 == offsetof(::server_static_s, dll_initialized), "offsetof server_static_s.dll_initialized");
    static_assert(std::is_same_v<t839, map_t<decltype(::server_static_s::dll_initialized)>>, "type server_static_s.dll_initialized");
    static_assert(o845 == offsetof(::server_static_s, clients), "offsetof server_static_s.clients");
    static_assert(std::is_same_v<t840, map_t<decltype(::server_static_s::clients)>>, "type server_static_s.clients");
    static_assert(o846 == offsetof(::server_static_s, maxclients), "offsetof server_static_s.maxclients");
    static_assert(std::is_same_v<t841, map_t<decltype(::server_static_s::maxclients)>>, "type server_static_s.maxclients");
    static_assert(s65 == sizeof(::server_s), "sizeof server_s");
    static_assert(o847 == offsetof(::server_s, active), "offsetof server_s.active");
    static_assert(std::is_same_v<t842, map_t<decltype(::server_s::active)>>, "type server_s.active");
    static_assert(s66 == sizeof(::sfx_t), "sizeof sfx_t");
    static_assert(o848 == offsetof(::sfx_t, name), "offsetof sfx_t.name");
    static_assert(std::is_same_v<t843, map_t<decltype(::sfx_t::name)>>, "type sfx_t.name");
    static_assert(o849 == offsetof(::sfx_t, cache), "offsetof sfx_t.cache");
    static_assert(std::is_same_v<t844, map_t<decltype(::sfx_t::cache)>>, "type sfx_t.cache");
    static_assert(s67 == sizeof(::nitroapi::EngineMsg), "sizeof nitroapi::EngineMsg");
    static_assert(o850 == offsetof(::nitroapi::EngineMsg, number), "offsetof nitroapi::EngineMsg.number");
    static_assert(std::is_same_v<t845, map_t<decltype(::nitroapi::EngineMsg::number)>>, "type nitroapi::EngineMsg.number");
    static_assert(o851 == offsetof(::nitroapi::EngineMsg, name), "offsetof nitroapi::EngineMsg.name");
    static_assert(std::is_same_v<t846, map_t<decltype(::nitroapi::EngineMsg::name)>>, "type nitroapi::EngineMsg.name");
    static_assert(o852 == offsetof(::nitroapi::EngineMsg, pfn), "offsetof nitroapi::EngineMsg.pfn");
    static_assert(std::is_same_v<t847, map_t<decltype(::nitroapi::EngineMsg::pfn)>>, "type nitroapi::EngineMsg.pfn");
    static_assert(s68 == sizeof(::nitroapi::transObjRef), "sizeof nitroapi::transObjRef");
    static_assert(o853 == offsetof(::nitroapi::transObjRef, pEnt), "offsetof nitroapi::transObjRef.pEnt");
    static_assert(std::is_same_v<t848, map_t<decltype(::nitroapi::transObjRef::pEnt)>>, "type nitroapi::transObjRef.pEnt");
    static_assert(s69 == sizeof(::nitroapi::viddef_s), "sizeof nitroapi::viddef_s");
    static_assert(o854 == offsetof(::nitroapi::viddef_s, buffer), "offsetof nitroapi::viddef_s.buffer");
    static_assert(std::is_same_v<t849, map_t<decltype(::nitroapi::viddef_s::buffer)>>, "type nitroapi::viddef_s.buffer");
    static_assert(o855 == offsetof(::nitroapi::viddef_s, colormap), "offsetof nitroapi::viddef_s.colormap");
    static_assert(std::is_same_v<t850, map_t<decltype(::nitroapi::viddef_s::colormap)>>, "type nitroapi::viddef_s.colormap");
    static_assert(o856 == offsetof(::nitroapi::viddef_s, colormap16), "offsetof nitroapi::viddef_s.colormap16");
    static_assert(std::is_same_v<t851, map_t<decltype(::nitroapi::viddef_s::colormap16)>>, "type nitroapi::viddef_s.colormap16");
    static_assert(o857 == offsetof(::nitroapi::viddef_s, fullbright), "offsetof nitroapi::viddef_s.fullbright");
    static_assert(std::is_same_v<t852, map_t<decltype(::nitroapi::viddef_s::fullbright)>>, "type nitroapi::viddef_s.fullbright");
    static_assert(o858 == offsetof(::nitroapi::viddef_s, bits), "offsetof nitroapi::viddef_s.bits");
    static_assert(std::is_same_v<t853, map_t<decltype(::nitroapi::viddef_s::bits)>>, "type nitroapi::viddef_s.bits");
    static_assert(o859 == offsetof(::nitroapi::viddef_s, is15bit), "offsetof nitroapi::viddef_s.is15bit");
    static_assert(std::is_same_v<t854, map_t<decltype(::nitroapi::viddef_s::is15bit)>>, "type nitroapi::viddef_s.is15bit");
    static_assert(o860 == offsetof(::nitroapi::viddef_s, rowbytes), "offsetof nitroapi::viddef_s.rowbytes");
    static_assert(std::is_same_v<t855, map_t<decltype(::nitroapi::viddef_s::rowbytes)>>, "type nitroapi::viddef_s.rowbytes");
    static_assert(o861 == offsetof(::nitroapi::viddef_s, width), "offsetof nitroapi::viddef_s.width");
    static_assert(std::is_same_v<t856, map_t<decltype(::nitroapi::viddef_s::width)>>, "type nitroapi::viddef_s.width");
    static_assert(o862 == offsetof(::nitroapi::viddef_s, height), "offsetof nitroapi::viddef_s.height");
    static_assert(std::is_same_v<t857, map_t<decltype(::nitroapi::viddef_s::height)>>, "type nitroapi::viddef_s.height");
    static_assert(o863 == offsetof(::nitroapi::viddef_s, aspect), "offsetof nitroapi::viddef_s.aspect");
    static_assert(std::is_same_v<t858, map_t<decltype(::nitroapi::viddef_s::aspect)>>, "type nitroapi::viddef_s.aspect");
    static_assert(o864 == offsetof(::nitroapi::viddef_s, numpages), "offsetof nitroapi::viddef_s.numpages");
    static_assert(std::is_same_v<t859, map_t<decltype(::nitroapi::viddef_s::numpages)>>, "type nitroapi::viddef_s.numpages");
    static_assert(o865 == offsetof(::nitroapi::viddef_s, recalc_refdef), "offsetof nitroapi::viddef_s.recalc_refdef");
    static_assert(std::is_same_v<t860, map_t<decltype(::nitroapi::viddef_s::recalc_refdef)>>, "type nitroapi::viddef_s.recalc_refdef");
    static_assert(s70 == sizeof(::clientdata_s), "sizeof clientdata_s");
    static_assert(o866 == offsetof(::clientdata_s, origin), "offsetof clientdata_s.origin");
    static_assert(std::is_same_v<t861, map_t<decltype(::clientdata_s::origin)>>, "type clientdata_s.origin");
    static_assert(o867 == offsetof(::clientdata_s, velocity), "offsetof clientdata_s.velocity");
    static_assert(std::is_same_v<t862, map_t<decltype(::clientdata_s::velocity)>>, "type clientdata_s.velocity");
    static_assert(o868 == offsetof(::clientdata_s, viewmodel), "offsetof clientdata_s.viewmodel");
    static_assert(std::is_same_v<t863, map_t<decltype(::clientdata_s::viewmodel)>>, "type clientdata_s.viewmodel");
    static_assert(o869 == offsetof(::clientdata_s, punchangle), "offsetof clientdata_s.punchangle");
    static_assert(std::is_same_v<t864, map_t<decltype(::clientdata_s::punchangle)>>, "type clientdata_s.punchangle");
    static_assert(o870 == offsetof(::clientdata_s, flags), "offsetof clientdata_s.flags");
    static_assert(std::is_same_v<t865, map_t<decltype(::clientdata_s::flags)>>, "type clientdata_s.flags");
    static_assert(o871 == offsetof(::clientdata_s, waterlevel), "offsetof clientdata_s.waterlevel");
    static_assert(std::is_same_v<t866, map_t<decltype(::clientdata_s::waterlevel)>>, "type clientdata_s.waterlevel");
    static_assert(o872 == offsetof(::clientdata_s, watertype), "offsetof clientdata_s.watertype");
    static_assert(std::is_same_v<t867, map_t<decltype(::clientdata_s::watertype)>>, "type clientdata_s.watertype");
    static_assert(o873 == offsetof(::clientdata_s, view_ofs), "offsetof clientdata_s.view_ofs");
    static_assert(std::is_same_v<t868, map_t<decltype(::clientdata_s::view_ofs)>>, "type clientdata_s.view_ofs");
    static_assert(o874 == offsetof(::clientdata_s, health), "offsetof clientdata_s.health");
    static_assert(std::is_same_v<t869, map_t<decltype(::clientdata_s::health)>>, "type clientdata_s.health");
    static_assert(o875 == offsetof(::clientdata_s, bInDuck), "offsetof clientdata_s.bInDuck");
    static_assert(std::is_same_v<t870, map_t<decltype(::clientdata_s::bInDuck)>>, "type clientdata_s.bInDuck");
    static_assert(o876 == offsetof(::clientdata_s, weapons), "offsetof clientdata_s.weapons");
    static_assert(std::is_same_v<t871, map_t<decltype(::clientdata_s::weapons)>>, "type clientdata_s.weapons");
    static_assert(o877 == offsetof(::clientdata_s, flTimeStepSound), "offsetof clientdata_s.flTimeStepSound");
    static_assert(std::is_same_v<t872, map_t<decltype(::clientdata_s::flTimeStepSound)>>, "type clientdata_s.flTimeStepSound");
    static_assert(o878 == offsetof(::clientdata_s, flDuckTime), "offsetof clientdata_s.flDuckTime");
    static_assert(std::is_same_v<t873, map_t<decltype(::clientdata_s::flDuckTime)>>, "type clientdata_s.flDuckTime");
    static_assert(o879 == offsetof(::clientdata_s, flSwimTime), "offsetof clientdata_s.flSwimTime");
    static_assert(std::is_same_v<t874, map_t<decltype(::clientdata_s::flSwimTime)>>, "type clientdata_s.flSwimTime");
    static_assert(o880 == offsetof(::clientdata_s, waterjumptime), "offsetof clientdata_s.waterjumptime");
    static_assert(std::is_same_v<t875, map_t<decltype(::clientdata_s::waterjumptime)>>, "type clientdata_s.waterjumptime");
    static_assert(o881 == offsetof(::clientdata_s, maxspeed), "offsetof clientdata_s.maxspeed");
    static_assert(std::is_same_v<t876, map_t<decltype(::clientdata_s::maxspeed)>>, "type clientdata_s.maxspeed");
    static_assert(o882 == offsetof(::clientdata_s, fov), "offsetof clientdata_s.fov");
    static_assert(std::is_same_v<t877, map_t<decltype(::clientdata_s::fov)>>, "type clientdata_s.fov");
    static_assert(o883 == offsetof(::clientdata_s, weaponanim), "offsetof clientdata_s.weaponanim");
    static_assert(std::is_same_v<t878, map_t<decltype(::clientdata_s::weaponanim)>>, "type clientdata_s.weaponanim");
    static_assert(o884 == offsetof(::clientdata_s, m_iId), "offsetof clientdata_s.m_iId");
    static_assert(std::is_same_v<t879, map_t<decltype(::clientdata_s::m_iId)>>, "type clientdata_s.m_iId");
    static_assert(o885 == offsetof(::clientdata_s, ammo_shells), "offsetof clientdata_s.ammo_shells");
    static_assert(std::is_same_v<t880, map_t<decltype(::clientdata_s::ammo_shells)>>, "type clientdata_s.ammo_shells");
    static_assert(o886 == offsetof(::clientdata_s, ammo_nails), "offsetof clientdata_s.ammo_nails");
    static_assert(std::is_same_v<t881, map_t<decltype(::clientdata_s::ammo_nails)>>, "type clientdata_s.ammo_nails");
    static_assert(o887 == offsetof(::clientdata_s, ammo_cells), "offsetof clientdata_s.ammo_cells");
    static_assert(std::is_same_v<t882, map_t<decltype(::clientdata_s::ammo_cells)>>, "type clientdata_s.ammo_cells");
    static_assert(o888 == offsetof(::clientdata_s, ammo_rockets), "offsetof clientdata_s.ammo_rockets");
    static_assert(std::is_same_v<t883, map_t<decltype(::clientdata_s::ammo_rockets)>>, "type clientdata_s.ammo_rockets");
    static_assert(o889 == offsetof(::clientdata_s, m_flNextAttack), "offsetof clientdata_s.m_flNextAttack");
    static_assert(std::is_same_v<t884, map_t<decltype(::clientdata_s::m_flNextAttack)>>, "type clientdata_s.m_flNextAttack");
    static_assert(o890 == offsetof(::clientdata_s, tfstate), "offsetof clientdata_s.tfstate");
    static_assert(std::is_same_v<t885, map_t<decltype(::clientdata_s::tfstate)>>, "type clientdata_s.tfstate");
    static_assert(o891 == offsetof(::clientdata_s, pushmsec), "offsetof clientdata_s.pushmsec");
    static_assert(std::is_same_v<t886, map_t<decltype(::clientdata_s::pushmsec)>>, "type clientdata_s.pushmsec");
    static_assert(o892 == offsetof(::clientdata_s, deadflag), "offsetof clientdata_s.deadflag");
    static_assert(std::is_same_v<t887, map_t<decltype(::clientdata_s::deadflag)>>, "type clientdata_s.deadflag");
    static_assert(o893 == offsetof(::clientdata_s, physinfo), "offsetof clientdata_s.physinfo");
    static_assert(std::is_same_v<t888, map_t<decltype(::clientdata_s::physinfo)>>, "type clientdata_s.physinfo");
    static_assert(o894 == offsetof(::clientdata_s, iuser1), "offsetof clientdata_s.iuser1");
    static_assert(std::is_same_v<t889, map_t<decltype(::clientdata_s::iuser1)>>, "type clientdata_s.iuser1");
    static_assert(o895 == offsetof(::clientdata_s, iuser2), "offsetof clientdata_s.iuser2");
    static_assert(std::is_same_v<t890, map_t<decltype(::clientdata_s::iuser2)>>, "type clientdata_s.iuser2");
    static_assert(o896 == offsetof(::clientdata_s, iuser3), "offsetof clientdata_s.iuser3");
    static_assert(std::is_same_v<t891, map_t<decltype(::clientdata_s::iuser3)>>, "type clientdata_s.iuser3");
    static_assert(o897 == offsetof(::clientdata_s, iuser4), "offsetof clientdata_s.iuser4");
    static_assert(std::is_same_v<t892, map_t<decltype(::clientdata_s::iuser4)>>, "type clientdata_s.iuser4");
    static_assert(o898 == offsetof(::clientdata_s, fuser1), "offsetof clientdata_s.fuser1");
    static_assert(std::is_same_v<t893, map_t<decltype(::clientdata_s::fuser1)>>, "type clientdata_s.fuser1");
    static_assert(o899 == offsetof(::clientdata_s, fuser2), "offsetof clientdata_s.fuser2");
    static_assert(std::is_same_v<t894, map_t<decltype(::clientdata_s::fuser2)>>, "type clientdata_s.fuser2");
    static_assert(o900 == offsetof(::clientdata_s, fuser3), "offsetof clientdata_s.fuser3");
    static_assert(std::is_same_v<t895, map_t<decltype(::clientdata_s::fuser3)>>, "type clientdata_s.fuser3");
    static_assert(o901 == offsetof(::clientdata_s, fuser4), "offsetof clientdata_s.fuser4");
    static_assert(std::is_same_v<t896, map_t<decltype(::clientdata_s::fuser4)>>, "type clientdata_s.fuser4");
    static_assert(o902 == offsetof(::clientdata_s, vuser1), "offsetof clientdata_s.vuser1");
    static_assert(std::is_same_v<t897, map_t<decltype(::clientdata_s::vuser1)>>, "type clientdata_s.vuser1");
    static_assert(o903 == offsetof(::clientdata_s, vuser2), "offsetof clientdata_s.vuser2");
    static_assert(std::is_same_v<t898, map_t<decltype(::clientdata_s::vuser2)>>, "type clientdata_s.vuser2");
    static_assert(o904 == offsetof(::clientdata_s, vuser3), "offsetof clientdata_s.vuser3");
    static_assert(std::is_same_v<t899, map_t<decltype(::clientdata_s::vuser3)>>, "type clientdata_s.vuser3");
    static_assert(o905 == offsetof(::clientdata_s, vuser4), "offsetof clientdata_s.vuser4");
    static_assert(std::is_same_v<t900, map_t<decltype(::clientdata_s::vuser4)>>, "type clientdata_s.vuser4");
    static_assert(s71 == sizeof(::entity_state_s), "sizeof entity_state_s");
    static_assert(o906 == offsetof(::entity_state_s, entityType), "offsetof entity_state_s.entityType");
    static_assert(std::is_same_v<t901, map_t<decltype(::entity_state_s::entityType)>>, "type entity_state_s.entityType");
    static_assert(o907 == offsetof(::entity_state_s, number), "offsetof entity_state_s.number");
    static_assert(std::is_same_v<t902, map_t<decltype(::entity_state_s::number)>>, "type entity_state_s.number");
    static_assert(o908 == offsetof(::entity_state_s, msg_time), "offsetof entity_state_s.msg_time");
    static_assert(std::is_same_v<t903, map_t<decltype(::entity_state_s::msg_time)>>, "type entity_state_s.msg_time");
    static_assert(o909 == offsetof(::entity_state_s, messagenum), "offsetof entity_state_s.messagenum");
    static_assert(std::is_same_v<t904, map_t<decltype(::entity_state_s::messagenum)>>, "type entity_state_s.messagenum");
    static_assert(o910 == offsetof(::entity_state_s, origin), "offsetof entity_state_s.origin");
    static_assert(std::is_same_v<t905, map_t<decltype(::entity_state_s::origin)>>, "type entity_state_s.origin");
    static_assert(o911 == offsetof(::entity_state_s, angles), "offsetof entity_state_s.angles");
    static_assert(std::is_same_v<t906, map_t<decltype(::entity_state_s::angles)>>, "type entity_state_s.angles");
    static_assert(o912 == offsetof(::entity_state_s, modelindex), "offsetof entity_state_s.modelindex");
    static_assert(std::is_same_v<t907, map_t<decltype(::entity_state_s::modelindex)>>, "type entity_state_s.modelindex");
    static_assert(o913 == offsetof(::entity_state_s, sequence), "offsetof entity_state_s.sequence");
    static_assert(std::is_same_v<t908, map_t<decltype(::entity_state_s::sequence)>>, "type entity_state_s.sequence");
    static_assert(o914 == offsetof(::entity_state_s, frame), "offsetof entity_state_s.frame");
    static_assert(std::is_same_v<t909, map_t<decltype(::entity_state_s::frame)>>, "type entity_state_s.frame");
    static_assert(o915 == offsetof(::entity_state_s, colormap), "offsetof entity_state_s.colormap");
    static_assert(std::is_same_v<t910, map_t<decltype(::entity_state_s::colormap)>>, "type entity_state_s.colormap");
    static_assert(o916 == offsetof(::entity_state_s, skin), "offsetof entity_state_s.skin");
    static_assert(std::is_same_v<t911, map_t<decltype(::entity_state_s::skin)>>, "type entity_state_s.skin");
    static_assert(o917 == offsetof(::entity_state_s, solid), "offsetof entity_state_s.solid");
    static_assert(std::is_same_v<t912, map_t<decltype(::entity_state_s::solid)>>, "type entity_state_s.solid");
    static_assert(o918 == offsetof(::entity_state_s, effects), "offsetof entity_state_s.effects");
    static_assert(std::is_same_v<t913, map_t<decltype(::entity_state_s::effects)>>, "type entity_state_s.effects");
    static_assert(o919 == offsetof(::entity_state_s, scale), "offsetof entity_state_s.scale");
    static_assert(std::is_same_v<t914, map_t<decltype(::entity_state_s::scale)>>, "type entity_state_s.scale");
    static_assert(o920 == offsetof(::entity_state_s, eflags), "offsetof entity_state_s.eflags");
    static_assert(std::is_same_v<t915, map_t<decltype(::entity_state_s::eflags)>>, "type entity_state_s.eflags");
    static_assert(o921 == offsetof(::entity_state_s, rendermode), "offsetof entity_state_s.rendermode");
    static_assert(std::is_same_v<t916, map_t<decltype(::entity_state_s::rendermode)>>, "type entity_state_s.rendermode");
    static_assert(o922 == offsetof(::entity_state_s, renderamt), "offsetof entity_state_s.renderamt");
    static_assert(std::is_same_v<t917, map_t<decltype(::entity_state_s::renderamt)>>, "type entity_state_s.renderamt");
    static_assert(o923 == offsetof(::entity_state_s, rendercolor), "offsetof entity_state_s.rendercolor");
    static_assert(std::is_same_v<t918, map_t<decltype(::entity_state_s::rendercolor)>>, "type entity_state_s.rendercolor");
    static_assert(o924 == offsetof(::entity_state_s, renderfx), "offsetof entity_state_s.renderfx");
    static_assert(std::is_same_v<t919, map_t<decltype(::entity_state_s::renderfx)>>, "type entity_state_s.renderfx");
    static_assert(o925 == offsetof(::entity_state_s, movetype), "offsetof entity_state_s.movetype");
    static_assert(std::is_same_v<t920, map_t<decltype(::entity_state_s::movetype)>>, "type entity_state_s.movetype");
    static_assert(o926 == offsetof(::entity_state_s, animtime), "offsetof entity_state_s.animtime");
    static_assert(std::is_same_v<t921, map_t<decltype(::entity_state_s::animtime)>>, "type entity_state_s.animtime");
    static_assert(o927 == offsetof(::entity_state_s, framerate), "offsetof entity_state_s.framerate");
    static_assert(std::is_same_v<t922, map_t<decltype(::entity_state_s::framerate)>>, "type entity_state_s.framerate");
    static_assert(o928 == offsetof(::entity_state_s, body), "offsetof entity_state_s.body");
    static_assert(std::is_same_v<t923, map_t<decltype(::entity_state_s::body)>>, "type entity_state_s.body");
    static_assert(o929 == offsetof(::entity_state_s, controller), "offsetof entity_state_s.controller");
    static_assert(std::is_same_v<t924, map_t<decltype(::entity_state_s::controller)>>, "type entity_state_s.controller");
    static_assert(o930 == offsetof(::entity_state_s, blending), "offsetof entity_state_s.blending");
    static_assert(std::is_same_v<t925, map_t<decltype(::entity_state_s::blending)>>, "type entity_state_s.blending");
    static_assert(o931 == offsetof(::entity_state_s, velocity), "offsetof entity_state_s.velocity");
    static_assert(std::is_same_v<t926, map_t<decltype(::entity_state_s::velocity)>>, "type entity_state_s.velocity");
    static_assert(o932 == offsetof(::entity_state_s, mins), "offsetof entity_state_s.mins");
    static_assert(std::is_same_v<t927, map_t<decltype(::entity_state_s::mins)>>, "type entity_state_s.mins");
    static_assert(o933 == offsetof(::entity_state_s, maxs), "offsetof entity_state_s.maxs");
    static_assert(std::is_same_v<t928, map_t<decltype(::entity_state_s::maxs)>>, "type entity_state_s.maxs");
    static_assert(o934 == offsetof(::entity_state_s, aiment), "offsetof entity_state_s.aiment");
    static_assert(std::is_same_v<t929, map_t<decltype(::entity_state_s::aiment)>>, "type entity_state_s.aiment");
    static_assert(o935 == offsetof(::entity_state_s, owner), "offsetof entity_state_s.owner");
    static_assert(std::is_same_v<t930, map_t<decltype(::entity_state_s::owner)>>, "type entity_state_s.owner");
    static_assert(o936 == offsetof(::entity_state_s, friction), "offsetof entity_state_s.friction");
    static_assert(std::is_same_v<t931, map_t<decltype(::entity_state_s::friction)>>, "type entity_state_s.friction");
    static_assert(o937 == offsetof(::entity_state_s, gravity), "offsetof entity_state_s.gravity");
    static_assert(std::is_same_v<t932, map_t<decltype(::entity_state_s::gravity)>>, "type entity_state_s.gravity");
    static_assert(o938 == offsetof(::entity_state_s, team), "offsetof entity_state_s.team");
    static_assert(std::is_same_v<t933, map_t<decltype(::entity_state_s::team)>>, "type entity_state_s.team");
    static_assert(o939 == offsetof(::entity_state_s, playerclass), "offsetof entity_state_s.playerclass");
    static_assert(std::is_same_v<t934, map_t<decltype(::entity_state_s::playerclass)>>, "type entity_state_s.playerclass");
    static_assert(o940 == offsetof(::entity_state_s, health), "offsetof entity_state_s.health");
    static_assert(std::is_same_v<t935, map_t<decltype(::entity_state_s::health)>>, "type entity_state_s.health");
    static_assert(o941 == offsetof(::entity_state_s, spectator), "offsetof entity_state_s.spectator");
    static_assert(std::is_same_v<t936, map_t<decltype(::entity_state_s::spectator)>>, "type entity_state_s.spectator");
    static_assert(o942 == offsetof(::entity_state_s, weaponmodel), "offsetof entity_state_s.weaponmodel");
    static_assert(std::is_same_v<t937, map_t<decltype(::entity_state_s::weaponmodel)>>, "type entity_state_s.weaponmodel");
    static_assert(o943 == offsetof(::entity_state_s, gaitsequence), "offsetof entity_state_s.gaitsequence");
    static_assert(std::is_same_v<t938, map_t<decltype(::entity_state_s::gaitsequence)>>, "type entity_state_s.gaitsequence");
    static_assert(o944 == offsetof(::entity_state_s, basevelocity), "offsetof entity_state_s.basevelocity");
    static_assert(std::is_same_v<t939, map_t<decltype(::entity_state_s::basevelocity)>>, "type entity_state_s.basevelocity");
    static_assert(o945 == offsetof(::entity_state_s, usehull), "offsetof entity_state_s.usehull");
    static_assert(std::is_same_v<t940, map_t<decltype(::entity_state_s::usehull)>>, "type entity_state_s.usehull");
    static_assert(o946 == offsetof(::entity_state_s, oldbuttons), "offsetof entity_state_s.oldbuttons");
    static_assert(std::is_same_v<t941, map_t<decltype(::entity_state_s::oldbuttons)>>, "type entity_state_s.oldbuttons");
    static_assert(o947 == offsetof(::entity_state_s, onground), "offsetof entity_state_s.onground");
    static_assert(std::is_same_v<t942, map_t<decltype(::entity_state_s::onground)>>, "type entity_state_s.onground");
    static_assert(o948 == offsetof(::entity_state_s, iStepLeft), "offsetof entity_state_s.iStepLeft");
    static_assert(std::is_same_v<t943, map_t<decltype(::entity_state_s::iStepLeft)>>, "type entity_state_s.iStepLeft");
    static_assert(o949 == offsetof(::entity_state_s, flFallVelocity), "offsetof entity_state_s.flFallVelocity");
    static_assert(std::is_same_v<t944, map_t<decltype(::entity_state_s::flFallVelocity)>>, "type entity_state_s.flFallVelocity");
    static_assert(o950 == offsetof(::entity_state_s, fov), "offsetof entity_state_s.fov");
    static_assert(std::is_same_v<t945, map_t<decltype(::entity_state_s::fov)>>, "type entity_state_s.fov");
    static_assert(o951 == offsetof(::entity_state_s, weaponanim), "offsetof entity_state_s.weaponanim");
    static_assert(std::is_same_v<t946, map_t<decltype(::entity_state_s::weaponanim)>>, "type entity_state_s.weaponanim");
    static_assert(o952 == offsetof(::entity_state_s, startpos), "offsetof entity_state_s.startpos");
    static_assert(std::is_same_v<t947, map_t<decltype(::entity_state_s::startpos)>>, "type entity_state_s.startpos");
    static_assert(o953 == offsetof(::entity_state_s, endpos), "offsetof entity_state_s.endpos");
    static_assert(std::is_same_v<t948, map_t<decltype(::entity_state_s::endpos)>>, "type entity_state_s.endpos");
    static_assert(o954 == offsetof(::entity_state_s, impacttime), "offsetof entity_state_s.impacttime");
    static_assert(std::is_same_v<t949, map_t<decltype(::entity_state_s::impacttime)>>, "type entity_state_s.impacttime");
    static_assert(o955 == offsetof(::entity_state_s, starttime), "offsetof entity_state_s.starttime");
    static_assert(std::is_same_v<t950, map_t<decltype(::entity_state_s::starttime)>>, "type entity_state_s.starttime");
    static_assert(o956 == offsetof(::entity_state_s, iuser1), "offsetof entity_state_s.iuser1");
    static_assert(std::is_same_v<t951, map_t<decltype(::entity_state_s::iuser1)>>, "type entity_state_s.iuser1");
    static_assert(o957 == offsetof(::entity_state_s, iuser2), "offsetof entity_state_s.iuser2");
    static_assert(std::is_same_v<t952, map_t<decltype(::entity_state_s::iuser2)>>, "type entity_state_s.iuser2");
    static_assert(o958 == offsetof(::entity_state_s, iuser3), "offsetof entity_state_s.iuser3");
    static_assert(std::is_same_v<t953, map_t<decltype(::entity_state_s::iuser3)>>, "type entity_state_s.iuser3");
    static_assert(o959 == offsetof(::entity_state_s, iuser4), "offsetof entity_state_s.iuser4");
    static_assert(std::is_same_v<t954, map_t<decltype(::entity_state_s::iuser4)>>, "type entity_state_s.iuser4");
    static_assert(o960 == offsetof(::entity_state_s, fuser1), "offsetof entity_state_s.fuser1");
    static_assert(std::is_same_v<t955, map_t<decltype(::entity_state_s::fuser1)>>, "type entity_state_s.fuser1");
    static_assert(o961 == offsetof(::entity_state_s, fuser2), "offsetof entity_state_s.fuser2");
    static_assert(std::is_same_v<t956, map_t<decltype(::entity_state_s::fuser2)>>, "type entity_state_s.fuser2");
    static_assert(o962 == offsetof(::entity_state_s, fuser3), "offsetof entity_state_s.fuser3");
    static_assert(std::is_same_v<t957, map_t<decltype(::entity_state_s::fuser3)>>, "type entity_state_s.fuser3");
    static_assert(o963 == offsetof(::entity_state_s, fuser4), "offsetof entity_state_s.fuser4");
    static_assert(std::is_same_v<t958, map_t<decltype(::entity_state_s::fuser4)>>, "type entity_state_s.fuser4");
    static_assert(o964 == offsetof(::entity_state_s, vuser1), "offsetof entity_state_s.vuser1");
    static_assert(std::is_same_v<t959, map_t<decltype(::entity_state_s::vuser1)>>, "type entity_state_s.vuser1");
    static_assert(o965 == offsetof(::entity_state_s, vuser2), "offsetof entity_state_s.vuser2");
    static_assert(std::is_same_v<t960, map_t<decltype(::entity_state_s::vuser2)>>, "type entity_state_s.vuser2");
    static_assert(o966 == offsetof(::entity_state_s, vuser3), "offsetof entity_state_s.vuser3");
    static_assert(std::is_same_v<t961, map_t<decltype(::entity_state_s::vuser3)>>, "type entity_state_s.vuser3");
    static_assert(o967 == offsetof(::entity_state_s, vuser4), "offsetof entity_state_s.vuser4");
    static_assert(std::is_same_v<t962, map_t<decltype(::entity_state_s::vuser4)>>, "type entity_state_s.vuser4");
    static_assert(s72 == sizeof(::latchedvars_t), "sizeof latchedvars_t");
    static_assert(o968 == offsetof(::latchedvars_t, prevanimtime), "offsetof latchedvars_t.prevanimtime");
    static_assert(std::is_same_v<t963, map_t<decltype(::latchedvars_t::prevanimtime)>>, "type latchedvars_t.prevanimtime");
    static_assert(o969 == offsetof(::latchedvars_t, sequencetime), "offsetof latchedvars_t.sequencetime");
    static_assert(std::is_same_v<t964, map_t<decltype(::latchedvars_t::sequencetime)>>, "type latchedvars_t.sequencetime");
    static_assert(o970 == offsetof(::latchedvars_t, prevseqblending), "offsetof latchedvars_t.prevseqblending");
    static_assert(std::is_same_v<t965, map_t<decltype(::latchedvars_t::prevseqblending)>>, "type latchedvars_t.prevseqblending");
    static_assert(o971 == offsetof(::latchedvars_t, prevorigin), "offsetof latchedvars_t.prevorigin");
    static_assert(std::is_same_v<t966, map_t<decltype(::latchedvars_t::prevorigin)>>, "type latchedvars_t.prevorigin");
    static_assert(o972 == offsetof(::latchedvars_t, prevangles), "offsetof latchedvars_t.prevangles");
    static_assert(std::is_same_v<t967, map_t<decltype(::latchedvars_t::prevangles)>>, "type latchedvars_t.prevangles");
    static_assert(o973 == offsetof(::latchedvars_t, prevsequence), "offsetof latchedvars_t.prevsequence");
    static_assert(std::is_same_v<t968, map_t<decltype(::latchedvars_t::prevsequence)>>, "type latchedvars_t.prevsequence");
    static_assert(o974 == offsetof(::latchedvars_t, prevframe), "offsetof latchedvars_t.prevframe");
    static_assert(std::is_same_v<t969, map_t<decltype(::latchedvars_t::prevframe)>>, "type latchedvars_t.prevframe");
    static_assert(o975 == offsetof(::latchedvars_t, prevcontroller), "offsetof latchedvars_t.prevcontroller");
    static_assert(std::is_same_v<t970, map_t<decltype(::latchedvars_t::prevcontroller)>>, "type latchedvars_t.prevcontroller");
    static_assert(o976 == offsetof(::latchedvars_t, prevblending), "offsetof latchedvars_t.prevblending");
    static_assert(std::is_same_v<t971, map_t<decltype(::latchedvars_t::prevblending)>>, "type latchedvars_t.prevblending");
    static_assert(s73 == sizeof(::mouth_t), "sizeof mouth_t");
    static_assert(o977 == offsetof(::mouth_t, mouthopen), "offsetof mouth_t.mouthopen");
    static_assert(std::is_same_v<t972, map_t<decltype(::mouth_t::mouthopen)>>, "type mouth_t.mouthopen");
    static_assert(o978 == offsetof(::mouth_t, sndcount), "offsetof mouth_t.sndcount");
    static_assert(std::is_same_v<t973, map_t<decltype(::mouth_t::sndcount)>>, "type mouth_t.sndcount");
    static_assert(o979 == offsetof(::mouth_t, sndavg), "offsetof mouth_t.sndavg");
    static_assert(std::is_same_v<t974, map_t<decltype(::mouth_t::sndavg)>>, "type mouth_t.sndavg");
    static_assert(s74 == sizeof(::position_history_t), "sizeof position_history_t");
    static_assert(o980 == offsetof(::position_history_t, animtime), "offsetof position_history_t.animtime");
    static_assert(std::is_same_v<t975, map_t<decltype(::position_history_t::animtime)>>, "type position_history_t.animtime");
    static_assert(o981 == offsetof(::position_history_t, origin), "offsetof position_history_t.origin");
    static_assert(std::is_same_v<t976, map_t<decltype(::position_history_t::origin)>>, "type position_history_t.origin");
    static_assert(o982 == offsetof(::position_history_t, angles), "offsetof position_history_t.angles");
    static_assert(std::is_same_v<t977, map_t<decltype(::position_history_t::angles)>>, "type position_history_t.angles");
    static_assert(s75 == sizeof(::weapon_data_s), "sizeof weapon_data_s");
    static_assert(o983 == offsetof(::weapon_data_s, m_iId), "offsetof weapon_data_s.m_iId");
    static_assert(std::is_same_v<t978, map_t<decltype(::weapon_data_s::m_iId)>>, "type weapon_data_s.m_iId");
    static_assert(o984 == offsetof(::weapon_data_s, m_iClip), "offsetof weapon_data_s.m_iClip");
    static_assert(std::is_same_v<t979, map_t<decltype(::weapon_data_s::m_iClip)>>, "type weapon_data_s.m_iClip");
    static_assert(o985 == offsetof(::weapon_data_s, m_flNextPrimaryAttack), "offsetof weapon_data_s.m_flNextPrimaryAttack");
    static_assert(
        std::is_same_v<t980, map_t<decltype(::weapon_data_s::m_flNextPrimaryAttack)>>,
        "type weapon_data_s.m_flNextPrimaryAttack"
    );
    static_assert(o986 == offsetof(::weapon_data_s, m_flNextSecondaryAttack), "offsetof weapon_data_s.m_flNextSecondaryAttack");
    static_assert(
        std::is_same_v<t981, map_t<decltype(::weapon_data_s::m_flNextSecondaryAttack)>>,
        "type weapon_data_s.m_flNextSecondaryAttack"
    );
    static_assert(o987 == offsetof(::weapon_data_s, m_flTimeWeaponIdle), "offsetof weapon_data_s.m_flTimeWeaponIdle");
    static_assert(std::is_same_v<t982, map_t<decltype(::weapon_data_s::m_flTimeWeaponIdle)>>, "type weapon_data_s.m_flTimeWeaponIdle");
    static_assert(o988 == offsetof(::weapon_data_s, m_fInReload), "offsetof weapon_data_s.m_fInReload");
    static_assert(std::is_same_v<t983, map_t<decltype(::weapon_data_s::m_fInReload)>>, "type weapon_data_s.m_fInReload");
    static_assert(o989 == offsetof(::weapon_data_s, m_fInSpecialReload), "offsetof weapon_data_s.m_fInSpecialReload");
    static_assert(std::is_same_v<t984, map_t<decltype(::weapon_data_s::m_fInSpecialReload)>>, "type weapon_data_s.m_fInSpecialReload");
    static_assert(o990 == offsetof(::weapon_data_s, m_flNextReload), "offsetof weapon_data_s.m_flNextReload");
    static_assert(std::is_same_v<t985, map_t<decltype(::weapon_data_s::m_flNextReload)>>, "type weapon_data_s.m_flNextReload");
    static_assert(o991 == offsetof(::weapon_data_s, m_flPumpTime), "offsetof weapon_data_s.m_flPumpTime");
    static_assert(std::is_same_v<t986, map_t<decltype(::weapon_data_s::m_flPumpTime)>>, "type weapon_data_s.m_flPumpTime");
    static_assert(o992 == offsetof(::weapon_data_s, m_fReloadTime), "offsetof weapon_data_s.m_fReloadTime");
    static_assert(std::is_same_v<t987, map_t<decltype(::weapon_data_s::m_fReloadTime)>>, "type weapon_data_s.m_fReloadTime");
    static_assert(o993 == offsetof(::weapon_data_s, m_fAimedDamage), "offsetof weapon_data_s.m_fAimedDamage");
    static_assert(std::is_same_v<t988, map_t<decltype(::weapon_data_s::m_fAimedDamage)>>, "type weapon_data_s.m_fAimedDamage");
    static_assert(o994 == offsetof(::weapon_data_s, m_fNextAimBonus), "offsetof weapon_data_s.m_fNextAimBonus");
    static_assert(std::is_same_v<t989, map_t<decltype(::weapon_data_s::m_fNextAimBonus)>>, "type weapon_data_s.m_fNextAimBonus");
    static_assert(o995 == offsetof(::weapon_data_s, m_fInZoom), "offsetof weapon_data_s.m_fInZoom");
    static_assert(std::is_same_v<t990, map_t<decltype(::weapon_data_s::m_fInZoom)>>, "type weapon_data_s.m_fInZoom");
    static_assert(o996 == offsetof(::weapon_data_s, m_iWeaponState), "offsetof weapon_data_s.m_iWeaponState");
    static_assert(std::is_same_v<t991, map_t<decltype(::weapon_data_s::m_iWeaponState)>>, "type weapon_data_s.m_iWeaponState");
    static_assert(o997 == offsetof(::weapon_data_s, iuser1), "offsetof weapon_data_s.iuser1");
    static_assert(std::is_same_v<t992, map_t<decltype(::weapon_data_s::iuser1)>>, "type weapon_data_s.iuser1");
    static_assert(o998 == offsetof(::weapon_data_s, iuser2), "offsetof weapon_data_s.iuser2");
    static_assert(std::is_same_v<t993, map_t<decltype(::weapon_data_s::iuser2)>>, "type weapon_data_s.iuser2");
    static_assert(o999 == offsetof(::weapon_data_s, iuser3), "offsetof weapon_data_s.iuser3");
    static_assert(std::is_same_v<t994, map_t<decltype(::weapon_data_s::iuser3)>>, "type weapon_data_s.iuser3");
    static_assert(o1000 == offsetof(::weapon_data_s, iuser4), "offsetof weapon_data_s.iuser4");
    static_assert(std::is_same_v<t995, map_t<decltype(::weapon_data_s::iuser4)>>, "type weapon_data_s.iuser4");
    static_assert(o1001 == offsetof(::weapon_data_s, fuser1), "offsetof weapon_data_s.fuser1");
    static_assert(std::is_same_v<t996, map_t<decltype(::weapon_data_s::fuser1)>>, "type weapon_data_s.fuser1");
    static_assert(o1002 == offsetof(::weapon_data_s, fuser2), "offsetof weapon_data_s.fuser2");
    static_assert(std::is_same_v<t997, map_t<decltype(::weapon_data_s::fuser2)>>, "type weapon_data_s.fuser2");
    static_assert(o1003 == offsetof(::weapon_data_s, fuser3), "offsetof weapon_data_s.fuser3");
    static_assert(std::is_same_v<t998, map_t<decltype(::weapon_data_s::fuser3)>>, "type weapon_data_s.fuser3");
    static_assert(o1004 == offsetof(::weapon_data_s, fuser4), "offsetof weapon_data_s.fuser4");
    static_assert(std::is_same_v<t999, map_t<decltype(::weapon_data_s::fuser4)>>, "type weapon_data_s.fuser4");
    static_assert(s76 == sizeof(::cl_entity_s), "sizeof cl_entity_s");
    static_assert(o1005 == offsetof(::cl_entity_s, index), "offsetof cl_entity_s.index");
    static_assert(std::is_same_v<t1000, map_t<decltype(::cl_entity_s::index)>>, "type cl_entity_s.index");
    static_assert(o1006 == offsetof(::cl_entity_s, player), "offsetof cl_entity_s.player");
    static_assert(std::is_same_v<t1001, map_t<decltype(::cl_entity_s::player)>>, "type cl_entity_s.player");
    static_assert(o1007 == offsetof(::cl_entity_s, baseline), "offsetof cl_entity_s.baseline");
    static_assert(std::is_same_v<t1002, map_t<decltype(::cl_entity_s::baseline)>>, "type cl_entity_s.baseline");
    static_assert(o1008 == offsetof(::cl_entity_s, prevstate), "offsetof cl_entity_s.prevstate");
    static_assert(std::is_same_v<t1003, map_t<decltype(::cl_entity_s::prevstate)>>, "type cl_entity_s.prevstate");
    static_assert(o1009 == offsetof(::cl_entity_s, curstate), "offsetof cl_entity_s.curstate");
    static_assert(std::is_same_v<t1004, map_t<decltype(::cl_entity_s::curstate)>>, "type cl_entity_s.curstate");
    static_assert(o1010 == offsetof(::cl_entity_s, current_position), "offsetof cl_entity_s.current_position");
    static_assert(std::is_same_v<t1005, map_t<decltype(::cl_entity_s::current_position)>>, "type cl_entity_s.current_position");
    static_assert(o1011 == offsetof(::cl_entity_s, ph), "offsetof cl_entity_s.ph");
    static_assert(std::is_same_v<t1006, map_t<decltype(::cl_entity_s::ph)>>, "type cl_entity_s.ph");
    static_assert(o1012 == offsetof(::cl_entity_s, mouth), "offsetof cl_entity_s.mouth");
    static_assert(std::is_same_v<t1007, map_t<decltype(::cl_entity_s::mouth)>>, "type cl_entity_s.mouth");
    static_assert(o1013 == offsetof(::cl_entity_s, latched), "offsetof cl_entity_s.latched");
    static_assert(std::is_same_v<t1008, map_t<decltype(::cl_entity_s::latched)>>, "type cl_entity_s.latched");
    static_assert(o1014 == offsetof(::cl_entity_s, lastmove), "offsetof cl_entity_s.lastmove");
    static_assert(std::is_same_v<t1009, map_t<decltype(::cl_entity_s::lastmove)>>, "type cl_entity_s.lastmove");
    static_assert(o1015 == offsetof(::cl_entity_s, origin), "offsetof cl_entity_s.origin");
    static_assert(std::is_same_v<t1010, map_t<decltype(::cl_entity_s::origin)>>, "type cl_entity_s.origin");
    static_assert(o1016 == offsetof(::cl_entity_s, angles), "offsetof cl_entity_s.angles");
    static_assert(std::is_same_v<t1011, map_t<decltype(::cl_entity_s::angles)>>, "type cl_entity_s.angles");
    static_assert(o1017 == offsetof(::cl_entity_s, attachment), "offsetof cl_entity_s.attachment");
    static_assert(std::is_same_v<t1012, map_t<decltype(::cl_entity_s::attachment)>>, "type cl_entity_s.attachment");
    static_assert(o1018 == offsetof(::cl_entity_s, trivial_accept), "offsetof cl_entity_s.trivial_accept");
    static_assert(std::is_same_v<t1013, map_t<decltype(::cl_entity_s::trivial_accept)>>, "type cl_entity_s.trivial_accept");
    static_assert(o1019 == offsetof(::cl_entity_s, model), "offsetof cl_entity_s.model");
    static_assert(std::is_same_v<t1014, map_t<decltype(::cl_entity_s::model)>>, "type cl_entity_s.model");
    static_assert(o1020 == offsetof(::cl_entity_s, efrag), "offsetof cl_entity_s.efrag");
    static_assert(std::is_same_v<t1015, map_t<decltype(::cl_entity_s::efrag)>>, "type cl_entity_s.efrag");
    static_assert(o1021 == offsetof(::cl_entity_s, topnode), "offsetof cl_entity_s.topnode");
    static_assert(std::is_same_v<t1016, map_t<decltype(::cl_entity_s::topnode)>>, "type cl_entity_s.topnode");
    static_assert(o1022 == offsetof(::cl_entity_s, syncbase), "offsetof cl_entity_s.syncbase");
    static_assert(std::is_same_v<t1017, map_t<decltype(::cl_entity_s::syncbase)>>, "type cl_entity_s.syncbase");
    static_assert(o1023 == offsetof(::cl_entity_s, visframe), "offsetof cl_entity_s.visframe");
    static_assert(std::is_same_v<t1018, map_t<decltype(::cl_entity_s::visframe)>>, "type cl_entity_s.visframe");
    static_assert(o1024 == offsetof(::cl_entity_s, cvFloorColor), "offsetof cl_entity_s.cvFloorColor");
    static_assert(std::is_same_v<t1019, map_t<decltype(::cl_entity_s::cvFloorColor)>>, "type cl_entity_s.cvFloorColor");
    static_assert(s77 == sizeof(::local_state_s), "sizeof local_state_s");
    static_assert(o1025 == offsetof(::local_state_s, playerstate), "offsetof local_state_s.playerstate");
    static_assert(std::is_same_v<t1020, map_t<decltype(::local_state_s::playerstate)>>, "type local_state_s.playerstate");
    static_assert(o1026 == offsetof(::local_state_s, client), "offsetof local_state_s.client");
    static_assert(std::is_same_v<t1021, map_t<decltype(::local_state_s::client)>>, "type local_state_s.client");
    static_assert(o1027 == offsetof(::local_state_s, weapondata), "offsetof local_state_s.weapondata");
    static_assert(std::is_same_v<t1022, map_t<decltype(::local_state_s::weapondata)>>, "type local_state_s.weapondata");
    static_assert(s78 == sizeof(::event_api_s), "sizeof event_api_s");
    static_assert(o1028 == offsetof(::event_api_s, version), "offsetof event_api_s.version");
    static_assert(std::is_same_v<t1023, map_t<decltype(::event_api_s::version)>>, "type event_api_s.version");
    static_assert(o1029 == offsetof(::event_api_s, EV_PlaySound), "offsetof event_api_s.EV_PlaySound");
    static_assert(std::is_same_v<t1024, map_t<decltype(::event_api_s::EV_PlaySound)>>, "type event_api_s.EV_PlaySound");
    static_assert(o1030 == offsetof(::event_api_s, EV_StopSound), "offsetof event_api_s.EV_StopSound");
    static_assert(std::is_same_v<t1025, map_t<decltype(::event_api_s::EV_StopSound)>>, "type event_api_s.EV_StopSound");
    static_assert(o1031 == offsetof(::event_api_s, EV_FindModelIndex), "offsetof event_api_s.EV_FindModelIndex");
    static_assert(std::is_same_v<t1026, map_t<decltype(::event_api_s::EV_FindModelIndex)>>, "type event_api_s.EV_FindModelIndex");
    static_assert(o1032 == offsetof(::event_api_s, EV_IsLocal), "offsetof event_api_s.EV_IsLocal");
    static_assert(std::is_same_v<t1027, map_t<decltype(::event_api_s::EV_IsLocal)>>, "type event_api_s.EV_IsLocal");
    static_assert(o1033 == offsetof(::event_api_s, EV_LocalPlayerDucking), "offsetof event_api_s.EV_LocalPlayerDucking");
    static_assert(std::is_same_v<t1028, map_t<decltype(::event_api_s::EV_LocalPlayerDucking)>>, "type event_api_s.EV_LocalPlayerDucking");
    static_assert(o1034 == offsetof(::event_api_s, EV_LocalPlayerViewheight), "offsetof event_api_s.EV_LocalPlayerViewheight");
    static_assert(
        std::is_same_v<t1029, map_t<decltype(::event_api_s::EV_LocalPlayerViewheight)>>,
        "type event_api_s.EV_LocalPlayerViewheight"
    );
    static_assert(o1035 == offsetof(::event_api_s, EV_LocalPlayerBounds), "offsetof event_api_s.EV_LocalPlayerBounds");
    static_assert(std::is_same_v<t1030, map_t<decltype(::event_api_s::EV_LocalPlayerBounds)>>, "type event_api_s.EV_LocalPlayerBounds");
    static_assert(o1036 == offsetof(::event_api_s, EV_IndexFromTrace), "offsetof event_api_s.EV_IndexFromTrace");
    static_assert(std::is_same_v<t1031, map_t<decltype(::event_api_s::EV_IndexFromTrace)>>, "type event_api_s.EV_IndexFromTrace");
    static_assert(o1037 == offsetof(::event_api_s, EV_GetPhysent), "offsetof event_api_s.EV_GetPhysent");
    static_assert(std::is_same_v<t1032, map_t<decltype(::event_api_s::EV_GetPhysent)>>, "type event_api_s.EV_GetPhysent");
    static_assert(o1038 == offsetof(::event_api_s, EV_SetUpPlayerPrediction), "offsetof event_api_s.EV_SetUpPlayerPrediction");
    static_assert(
        std::is_same_v<t1033, map_t<decltype(::event_api_s::EV_SetUpPlayerPrediction)>>,
        "type event_api_s.EV_SetUpPlayerPrediction"
    );
    static_assert(o1039 == offsetof(::event_api_s, EV_PushPMStates), "offsetof event_api_s.EV_PushPMStates");
    static_assert(std::is_same_v<t1034, map_t<decltype(::event_api_s::EV_PushPMStates)>>, "type event_api_s.EV_PushPMStates");
    static_assert(o1040 == offsetof(::event_api_s, EV_PopPMStates), "offsetof event_api_s.EV_PopPMStates");
    static_assert(std::is_same_v<t1035, map_t<decltype(::event_api_s::EV_PopPMStates)>>, "type event_api_s.EV_PopPMStates");
    static_assert(o1041 == offsetof(::event_api_s, EV_SetSolidPlayers), "offsetof event_api_s.EV_SetSolidPlayers");
    static_assert(std::is_same_v<t1036, map_t<decltype(::event_api_s::EV_SetSolidPlayers)>>, "type event_api_s.EV_SetSolidPlayers");
    static_assert(o1042 == offsetof(::event_api_s, EV_SetTraceHull), "offsetof event_api_s.EV_SetTraceHull");
    static_assert(std::is_same_v<t1037, map_t<decltype(::event_api_s::EV_SetTraceHull)>>, "type event_api_s.EV_SetTraceHull");
    static_assert(o1043 == offsetof(::event_api_s, EV_PlayerTrace), "offsetof event_api_s.EV_PlayerTrace");
    static_assert(std::is_same_v<t1038, map_t<decltype(::event_api_s::EV_PlayerTrace)>>, "type event_api_s.EV_PlayerTrace");
    static_assert(o1044 == offsetof(::event_api_s, EV_WeaponAnimation), "offsetof event_api_s.EV_WeaponAnimation");
    static_assert(std::is_same_v<t1039, map_t<decltype(::event_api_s::EV_WeaponAnimation)>>, "type event_api_s.EV_WeaponAnimation");
    static_assert(o1045 == offsetof(::event_api_s, EV_PrecacheEvent), "offsetof event_api_s.EV_PrecacheEvent");
    static_assert(std::is_same_v<t1040, map_t<decltype(::event_api_s::EV_PrecacheEvent)>>, "type event_api_s.EV_PrecacheEvent");
    static_assert(o1046 == offsetof(::event_api_s, EV_PlaybackEvent), "offsetof event_api_s.EV_PlaybackEvent");
    static_assert(std::is_same_v<t1041, map_t<decltype(::event_api_s::EV_PlaybackEvent)>>, "type event_api_s.EV_PlaybackEvent");
    static_assert(o1047 == offsetof(::event_api_s, EV_TraceTexture), "offsetof event_api_s.EV_TraceTexture");
    static_assert(std::is_same_v<t1042, map_t<decltype(::event_api_s::EV_TraceTexture)>>, "type event_api_s.EV_TraceTexture");
    static_assert(o1048 == offsetof(::event_api_s, EV_StopAllSounds), "offsetof event_api_s.EV_StopAllSounds");
    static_assert(std::is_same_v<t1043, map_t<decltype(::event_api_s::EV_StopAllSounds)>>, "type event_api_s.EV_StopAllSounds");
    static_assert(o1049 == offsetof(::event_api_s, EV_KillEvents), "offsetof event_api_s.EV_KillEvents");
    static_assert(std::is_same_v<t1044, map_t<decltype(::event_api_s::EV_KillEvents)>>, "type event_api_s.EV_KillEvents");
    static_assert(s79 == sizeof(::event_args_s), "sizeof event_args_s");
    static_assert(o1050 == offsetof(::event_args_s, flags), "offsetof event_args_s.flags");
    static_assert(std::is_same_v<t1045, map_t<decltype(::event_args_s::flags)>>, "type event_args_s.flags");
    static_assert(o1051 == offsetof(::event_args_s, entindex), "offsetof event_args_s.entindex");
    static_assert(std::is_same_v<t1046, map_t<decltype(::event_args_s::entindex)>>, "type event_args_s.entindex");
    static_assert(o1052 == offsetof(::event_args_s, origin), "offsetof event_args_s.origin");
    static_assert(std::is_same_v<t1047, map_t<decltype(::event_args_s::origin)>>, "type event_args_s.origin");
    static_assert(o1053 == offsetof(::event_args_s, angles), "offsetof event_args_s.angles");
    static_assert(std::is_same_v<t1048, map_t<decltype(::event_args_s::angles)>>, "type event_args_s.angles");
    static_assert(o1054 == offsetof(::event_args_s, velocity), "offsetof event_args_s.velocity");
    static_assert(std::is_same_v<t1049, map_t<decltype(::event_args_s::velocity)>>, "type event_args_s.velocity");
    static_assert(o1055 == offsetof(::event_args_s, ducking), "offsetof event_args_s.ducking");
    static_assert(std::is_same_v<t1050, map_t<decltype(::event_args_s::ducking)>>, "type event_args_s.ducking");
    static_assert(o1056 == offsetof(::event_args_s, fparam1), "offsetof event_args_s.fparam1");
    static_assert(std::is_same_v<t1051, map_t<decltype(::event_args_s::fparam1)>>, "type event_args_s.fparam1");
    static_assert(o1057 == offsetof(::event_args_s, fparam2), "offsetof event_args_s.fparam2");
    static_assert(std::is_same_v<t1052, map_t<decltype(::event_args_s::fparam2)>>, "type event_args_s.fparam2");
    static_assert(o1058 == offsetof(::event_args_s, iparam1), "offsetof event_args_s.iparam1");
    static_assert(std::is_same_v<t1053, map_t<decltype(::event_args_s::iparam1)>>, "type event_args_s.iparam1");
    static_assert(o1059 == offsetof(::event_args_s, iparam2), "offsetof event_args_s.iparam2");
    static_assert(std::is_same_v<t1054, map_t<decltype(::event_args_s::iparam2)>>, "type event_args_s.iparam2");
    static_assert(o1060 == offsetof(::event_args_s, bparam1), "offsetof event_args_s.bparam1");
    static_assert(std::is_same_v<t1055, map_t<decltype(::event_args_s::bparam1)>>, "type event_args_s.bparam1");
    static_assert(o1061 == offsetof(::event_args_s, bparam2), "offsetof event_args_s.bparam2");
    static_assert(std::is_same_v<t1056, map_t<decltype(::event_args_s::bparam2)>>, "type event_args_s.bparam2");
    static_assert(s80 == sizeof(::dmodel_s), "sizeof dmodel_s");
    static_assert(o1062 == offsetof(::dmodel_s, mins), "offsetof dmodel_s.mins");
    static_assert(std::is_same_v<t1057, map_t<decltype(::dmodel_s::mins)>>, "type dmodel_s.mins");
    static_assert(o1063 == offsetof(::dmodel_s, maxs), "offsetof dmodel_s.maxs");
    static_assert(std::is_same_v<t1058, map_t<decltype(::dmodel_s::maxs)>>, "type dmodel_s.maxs");
    static_assert(o1064 == offsetof(::dmodel_s, origin), "offsetof dmodel_s.origin");
    static_assert(std::is_same_v<t1059, map_t<decltype(::dmodel_s::origin)>>, "type dmodel_s.origin");
    static_assert(o1065 == offsetof(::dmodel_s, headnode), "offsetof dmodel_s.headnode");
    static_assert(std::is_same_v<t1060, map_t<decltype(::dmodel_s::headnode)>>, "type dmodel_s.headnode");
    static_assert(o1066 == offsetof(::dmodel_s, visleafs), "offsetof dmodel_s.visleafs");
    static_assert(std::is_same_v<t1061, map_t<decltype(::dmodel_s::visleafs)>>, "type dmodel_s.visleafs");
    static_assert(o1067 == offsetof(::dmodel_s, firstface), "offsetof dmodel_s.firstface");
    static_assert(std::is_same_v<t1062, map_t<decltype(::dmodel_s::firstface)>>, "type dmodel_s.firstface");
    static_assert(o1068 == offsetof(::dmodel_s, numfaces), "offsetof dmodel_s.numfaces");
    static_assert(std::is_same_v<t1063, map_t<decltype(::dmodel_s::numfaces)>>, "type dmodel_s.numfaces");
    static_assert(s81 == sizeof(::dspriteframe_s), "sizeof dspriteframe_s");
    static_assert(o1069 == offsetof(::dspriteframe_s, origin), "offsetof dspriteframe_s.origin");
    static_assert(std::is_same_v<t1064, map_t<decltype(::dspriteframe_s::origin)>>, "type dspriteframe_s.origin");
    static_assert(o1070 == offsetof(::dspriteframe_s, width), "offsetof dspriteframe_s.width");
    static_assert(std::is_same_v<t1065, map_t<decltype(::dspriteframe_s::width)>>, "type dspriteframe_s.width");
    static_assert(o1071 == offsetof(::dspriteframe_s, height), "offsetof dspriteframe_s.height");
    static_assert(std::is_same_v<t1066, map_t<decltype(::dspriteframe_s::height)>>, "type dspriteframe_s.height");
    static_assert(s82 == sizeof(::dspritegroup_s), "sizeof dspritegroup_s");
    static_assert(o1072 == offsetof(::dspritegroup_s, numframes), "offsetof dspritegroup_s.numframes");
    static_assert(std::is_same_v<t1067, map_t<decltype(::dspritegroup_s::numframes)>>, "type dspritegroup_s.numframes");
    static_assert(s83 == sizeof(::dspriteinterval_s), "sizeof dspriteinterval_s");
    static_assert(o1073 == offsetof(::dspriteinterval_s, interval), "offsetof dspriteinterval_s.interval");
    static_assert(std::is_same_v<t1068, map_t<decltype(::dspriteinterval_s::interval)>>, "type dspriteinterval_s.interval");
    static_assert(s84 == sizeof(::dsprite_s), "sizeof dsprite_s");
    static_assert(o1074 == offsetof(::dsprite_s, ident), "offsetof dsprite_s.ident");
    static_assert(std::is_same_v<t1069, map_t<decltype(::dsprite_s::ident)>>, "type dsprite_s.ident");
    static_assert(o1075 == offsetof(::dsprite_s, version), "offsetof dsprite_s.version");
    static_assert(std::is_same_v<t1070, map_t<decltype(::dsprite_s::version)>>, "type dsprite_s.version");
    static_assert(o1076 == offsetof(::dsprite_s, type), "offsetof dsprite_s.type");
    static_assert(std::is_same_v<t1071, map_t<decltype(::dsprite_s::type)>>, "type dsprite_s.type");
    static_assert(o1077 == offsetof(::dsprite_s, texFormat), "offsetof dsprite_s.texFormat");
    static_assert(std::is_same_v<t1072, map_t<decltype(::dsprite_s::texFormat)>>, "type dsprite_s.texFormat");
    static_assert(o1078 == offsetof(::dsprite_s, boundingradius), "offsetof dsprite_s.boundingradius");
    static_assert(std::is_same_v<t1073, map_t<decltype(::dsprite_s::boundingradius)>>, "type dsprite_s.boundingradius");
    static_assert(o1079 == offsetof(::dsprite_s, width), "offsetof dsprite_s.width");
    static_assert(std::is_same_v<t1074, map_t<decltype(::dsprite_s::width)>>, "type dsprite_s.width");
    static_assert(o1080 == offsetof(::dsprite_s, height), "offsetof dsprite_s.height");
    static_assert(std::is_same_v<t1075, map_t<decltype(::dsprite_s::height)>>, "type dsprite_s.height");
    static_assert(o1081 == offsetof(::dsprite_s, numframes), "offsetof dsprite_s.numframes");
    static_assert(std::is_same_v<t1076, map_t<decltype(::dsprite_s::numframes)>>, "type dsprite_s.numframes");
    static_assert(o1082 == offsetof(::dsprite_s, beamlength), "offsetof dsprite_s.beamlength");
    static_assert(std::is_same_v<t1077, map_t<decltype(::dsprite_s::beamlength)>>, "type dsprite_s.beamlength");
    static_assert(o1083 == offsetof(::dsprite_s, synctype), "offsetof dsprite_s.synctype");
    static_assert(std::is_same_v<t1078, map_t<decltype(::dsprite_s::synctype)>>, "type dsprite_s.synctype");
    static_assert(s85 == sizeof(::dspriteframetype_s), "sizeof dspriteframetype_s");
    static_assert(o1084 == offsetof(::dspriteframetype_s, type), "offsetof dspriteframetype_s.type");
    static_assert(std::is_same_v<t1079, map_t<decltype(::dspriteframetype_s::type)>>, "type dspriteframetype_s.type");
    static_assert(s86 == sizeof(::mstudioanim_t), "sizeof mstudioanim_t");
    static_assert(o1085 == offsetof(::mstudioanim_t, offset), "offsetof mstudioanim_t.offset");
    static_assert(std::is_same_v<t1080, map_t<decltype(::mstudioanim_t::offset)>>, "type mstudioanim_t.offset");
    static_assert(s87 == sizeof(::mstudioanimvalue_t), "sizeof mstudioanimvalue_t");
    static_assert(o1086 == offsetof(::mstudioanimvalue_t, num), "offsetof mstudioanimvalue_t.num");
    // mstudioanimvalue_t.num: type comparison skipped - an unnamed record has no name to compare by
    static_assert(o1087 == offsetof(::mstudioanimvalue_t, value), "offsetof mstudioanimvalue_t.value");
    static_assert(std::is_same_v<t1081, map_t<decltype(::mstudioanimvalue_t::value)>>, "type mstudioanimvalue_t.value");
    static_assert(s88 == sizeof(::mstudioattachment_t), "sizeof mstudioattachment_t");
    static_assert(o1088 == offsetof(::mstudioattachment_t, name), "offsetof mstudioattachment_t.name");
    static_assert(std::is_same_v<t1082, map_t<decltype(::mstudioattachment_t::name)>>, "type mstudioattachment_t.name");
    static_assert(o1089 == offsetof(::mstudioattachment_t, type), "offsetof mstudioattachment_t.type");
    static_assert(std::is_same_v<t1083, map_t<decltype(::mstudioattachment_t::type)>>, "type mstudioattachment_t.type");
    static_assert(o1090 == offsetof(::mstudioattachment_t, bone), "offsetof mstudioattachment_t.bone");
    static_assert(std::is_same_v<t1084, map_t<decltype(::mstudioattachment_t::bone)>>, "type mstudioattachment_t.bone");
    static_assert(o1091 == offsetof(::mstudioattachment_t, org), "offsetof mstudioattachment_t.org");
    static_assert(std::is_same_v<t1085, map_t<decltype(::mstudioattachment_t::org)>>, "type mstudioattachment_t.org");
    static_assert(o1092 == offsetof(::mstudioattachment_t, vectors), "offsetof mstudioattachment_t.vectors");
    static_assert(std::is_same_v<t1086, map_t<decltype(::mstudioattachment_t::vectors)>>, "type mstudioattachment_t.vectors");
    static_assert(s89 == sizeof(::mstudiobodyparts_t), "sizeof mstudiobodyparts_t");
    static_assert(o1093 == offsetof(::mstudiobodyparts_t, name), "offsetof mstudiobodyparts_t.name");
    static_assert(std::is_same_v<t1087, map_t<decltype(::mstudiobodyparts_t::name)>>, "type mstudiobodyparts_t.name");
    static_assert(o1094 == offsetof(::mstudiobodyparts_t, nummodels), "offsetof mstudiobodyparts_t.nummodels");
    static_assert(std::is_same_v<t1088, map_t<decltype(::mstudiobodyparts_t::nummodels)>>, "type mstudiobodyparts_t.nummodels");
    static_assert(o1095 == offsetof(::mstudiobodyparts_t, base), "offsetof mstudiobodyparts_t.base");
    static_assert(std::is_same_v<t1089, map_t<decltype(::mstudiobodyparts_t::base)>>, "type mstudiobodyparts_t.base");
    static_assert(o1096 == offsetof(::mstudiobodyparts_t, modelindex), "offsetof mstudiobodyparts_t.modelindex");
    static_assert(std::is_same_v<t1090, map_t<decltype(::mstudiobodyparts_t::modelindex)>>, "type mstudiobodyparts_t.modelindex");
    static_assert(s90 == sizeof(::mstudiobone_t), "sizeof mstudiobone_t");
    static_assert(o1097 == offsetof(::mstudiobone_t, name), "offsetof mstudiobone_t.name");
    static_assert(std::is_same_v<t1091, map_t<decltype(::mstudiobone_t::name)>>, "type mstudiobone_t.name");
    static_assert(o1098 == offsetof(::mstudiobone_t, parent), "offsetof mstudiobone_t.parent");
    static_assert(std::is_same_v<t1092, map_t<decltype(::mstudiobone_t::parent)>>, "type mstudiobone_t.parent");
    static_assert(o1099 == offsetof(::mstudiobone_t, flags), "offsetof mstudiobone_t.flags");
    static_assert(std::is_same_v<t1093, map_t<decltype(::mstudiobone_t::flags)>>, "type mstudiobone_t.flags");
    static_assert(o1100 == offsetof(::mstudiobone_t, bonecontroller), "offsetof mstudiobone_t.bonecontroller");
    static_assert(std::is_same_v<t1094, map_t<decltype(::mstudiobone_t::bonecontroller)>>, "type mstudiobone_t.bonecontroller");
    static_assert(o1101 == offsetof(::mstudiobone_t, value), "offsetof mstudiobone_t.value");
    static_assert(std::is_same_v<t1095, map_t<decltype(::mstudiobone_t::value)>>, "type mstudiobone_t.value");
    static_assert(o1102 == offsetof(::mstudiobone_t, scale), "offsetof mstudiobone_t.scale");
    static_assert(std::is_same_v<t1096, map_t<decltype(::mstudiobone_t::scale)>>, "type mstudiobone_t.scale");
    static_assert(s91 == sizeof(::mstudioevent_s), "sizeof mstudioevent_s");
    static_assert(o1103 == offsetof(::mstudioevent_s, frame), "offsetof mstudioevent_s.frame");
    static_assert(std::is_same_v<t1097, map_t<decltype(::mstudioevent_s::frame)>>, "type mstudioevent_s.frame");
    static_assert(o1104 == offsetof(::mstudioevent_s, event), "offsetof mstudioevent_s.event");
    static_assert(std::is_same_v<t1098, map_t<decltype(::mstudioevent_s::event)>>, "type mstudioevent_s.event");
    static_assert(o1105 == offsetof(::mstudioevent_s, type), "offsetof mstudioevent_s.type");
    static_assert(std::is_same_v<t1099, map_t<decltype(::mstudioevent_s::type)>>, "type mstudioevent_s.type");
    static_assert(o1106 == offsetof(::mstudioevent_s, options), "offsetof mstudioevent_s.options");
    static_assert(std::is_same_v<t1100, map_t<decltype(::mstudioevent_s::options)>>, "type mstudioevent_s.options");
    static_assert(s92 == sizeof(::mstudioseqdesc_t), "sizeof mstudioseqdesc_t");
    static_assert(o1107 == offsetof(::mstudioseqdesc_t, label), "offsetof mstudioseqdesc_t.label");
    static_assert(std::is_same_v<t1101, map_t<decltype(::mstudioseqdesc_t::label)>>, "type mstudioseqdesc_t.label");
    static_assert(o1108 == offsetof(::mstudioseqdesc_t, fps), "offsetof mstudioseqdesc_t.fps");
    static_assert(std::is_same_v<t1102, map_t<decltype(::mstudioseqdesc_t::fps)>>, "type mstudioseqdesc_t.fps");
    static_assert(o1109 == offsetof(::mstudioseqdesc_t, flags), "offsetof mstudioseqdesc_t.flags");
    static_assert(std::is_same_v<t1103, map_t<decltype(::mstudioseqdesc_t::flags)>>, "type mstudioseqdesc_t.flags");
    static_assert(o1110 == offsetof(::mstudioseqdesc_t, activity), "offsetof mstudioseqdesc_t.activity");
    static_assert(std::is_same_v<t1104, map_t<decltype(::mstudioseqdesc_t::activity)>>, "type mstudioseqdesc_t.activity");
    static_assert(o1111 == offsetof(::mstudioseqdesc_t, actweight), "offsetof mstudioseqdesc_t.actweight");
    static_assert(std::is_same_v<t1105, map_t<decltype(::mstudioseqdesc_t::actweight)>>, "type mstudioseqdesc_t.actweight");
    static_assert(o1112 == offsetof(::mstudioseqdesc_t, numevents), "offsetof mstudioseqdesc_t.numevents");
    static_assert(std::is_same_v<t1106, map_t<decltype(::mstudioseqdesc_t::numevents)>>, "type mstudioseqdesc_t.numevents");
    static_assert(o1113 == offsetof(::mstudioseqdesc_t, eventindex), "offsetof mstudioseqdesc_t.eventindex");
    static_assert(std::is_same_v<t1107, map_t<decltype(::mstudioseqdesc_t::eventindex)>>, "type mstudioseqdesc_t.eventindex");
    static_assert(o1114 == offsetof(::mstudioseqdesc_t, numframes), "offsetof mstudioseqdesc_t.numframes");
    static_assert(std::is_same_v<t1108, map_t<decltype(::mstudioseqdesc_t::numframes)>>, "type mstudioseqdesc_t.numframes");
    static_assert(o1115 == offsetof(::mstudioseqdesc_t, numpivots), "offsetof mstudioseqdesc_t.numpivots");
    static_assert(std::is_same_v<t1109, map_t<decltype(::mstudioseqdesc_t::numpivots)>>, "type mstudioseqdesc_t.numpivots");
    static_assert(o1116 == offsetof(::mstudioseqdesc_t, pivotindex), "offsetof mstudioseqdesc_t.pivotindex");
    static_assert(std::is_same_v<t1110, map_t<decltype(::mstudioseqdesc_t::pivotindex)>>, "type mstudioseqdesc_t.pivotindex");
    static_assert(o1117 == offsetof(::mstudioseqdesc_t, motiontype), "offsetof mstudioseqdesc_t.motiontype");
    static_assert(std::is_same_v<t1111, map_t<decltype(::mstudioseqdesc_t::motiontype)>>, "type mstudioseqdesc_t.motiontype");
    static_assert(o1118 == offsetof(::mstudioseqdesc_t, motionbone), "offsetof mstudioseqdesc_t.motionbone");
    static_assert(std::is_same_v<t1112, map_t<decltype(::mstudioseqdesc_t::motionbone)>>, "type mstudioseqdesc_t.motionbone");
    static_assert(o1119 == offsetof(::mstudioseqdesc_t, linearmovement), "offsetof mstudioseqdesc_t.linearmovement");
    static_assert(std::is_same_v<t1113, map_t<decltype(::mstudioseqdesc_t::linearmovement)>>, "type mstudioseqdesc_t.linearmovement");
    static_assert(o1120 == offsetof(::mstudioseqdesc_t, automoveposindex), "offsetof mstudioseqdesc_t.automoveposindex");
    static_assert(std::is_same_v<t1114, map_t<decltype(::mstudioseqdesc_t::automoveposindex)>>, "type mstudioseqdesc_t.automoveposindex");
    static_assert(o1121 == offsetof(::mstudioseqdesc_t, automoveangleindex), "offsetof mstudioseqdesc_t.automoveangleindex");
    static_assert(
        std::is_same_v<t1115, map_t<decltype(::mstudioseqdesc_t::automoveangleindex)>>,
        "type mstudioseqdesc_t.automoveangleindex"
    );
    static_assert(o1122 == offsetof(::mstudioseqdesc_t, bbmin), "offsetof mstudioseqdesc_t.bbmin");
    static_assert(std::is_same_v<t1116, map_t<decltype(::mstudioseqdesc_t::bbmin)>>, "type mstudioseqdesc_t.bbmin");
    static_assert(o1123 == offsetof(::mstudioseqdesc_t, bbmax), "offsetof mstudioseqdesc_t.bbmax");
    static_assert(std::is_same_v<t1117, map_t<decltype(::mstudioseqdesc_t::bbmax)>>, "type mstudioseqdesc_t.bbmax");
    static_assert(o1124 == offsetof(::mstudioseqdesc_t, numblends), "offsetof mstudioseqdesc_t.numblends");
    static_assert(std::is_same_v<t1118, map_t<decltype(::mstudioseqdesc_t::numblends)>>, "type mstudioseqdesc_t.numblends");
    static_assert(o1125 == offsetof(::mstudioseqdesc_t, animindex), "offsetof mstudioseqdesc_t.animindex");
    static_assert(std::is_same_v<t1119, map_t<decltype(::mstudioseqdesc_t::animindex)>>, "type mstudioseqdesc_t.animindex");
    static_assert(o1126 == offsetof(::mstudioseqdesc_t, blendtype), "offsetof mstudioseqdesc_t.blendtype");
    static_assert(std::is_same_v<t1120, map_t<decltype(::mstudioseqdesc_t::blendtype)>>, "type mstudioseqdesc_t.blendtype");
    static_assert(o1127 == offsetof(::mstudioseqdesc_t, blendstart), "offsetof mstudioseqdesc_t.blendstart");
    static_assert(std::is_same_v<t1121, map_t<decltype(::mstudioseqdesc_t::blendstart)>>, "type mstudioseqdesc_t.blendstart");
    static_assert(o1128 == offsetof(::mstudioseqdesc_t, blendend), "offsetof mstudioseqdesc_t.blendend");
    static_assert(std::is_same_v<t1122, map_t<decltype(::mstudioseqdesc_t::blendend)>>, "type mstudioseqdesc_t.blendend");
    static_assert(o1129 == offsetof(::mstudioseqdesc_t, blendparent), "offsetof mstudioseqdesc_t.blendparent");
    static_assert(std::is_same_v<t1123, map_t<decltype(::mstudioseqdesc_t::blendparent)>>, "type mstudioseqdesc_t.blendparent");
    static_assert(o1130 == offsetof(::mstudioseqdesc_t, seqgroup), "offsetof mstudioseqdesc_t.seqgroup");
    static_assert(std::is_same_v<t1124, map_t<decltype(::mstudioseqdesc_t::seqgroup)>>, "type mstudioseqdesc_t.seqgroup");
    static_assert(o1131 == offsetof(::mstudioseqdesc_t, entrynode), "offsetof mstudioseqdesc_t.entrynode");
    static_assert(std::is_same_v<t1125, map_t<decltype(::mstudioseqdesc_t::entrynode)>>, "type mstudioseqdesc_t.entrynode");
    static_assert(o1132 == offsetof(::mstudioseqdesc_t, exitnode), "offsetof mstudioseqdesc_t.exitnode");
    static_assert(std::is_same_v<t1126, map_t<decltype(::mstudioseqdesc_t::exitnode)>>, "type mstudioseqdesc_t.exitnode");
    static_assert(o1133 == offsetof(::mstudioseqdesc_t, nodeflags), "offsetof mstudioseqdesc_t.nodeflags");
    static_assert(std::is_same_v<t1127, map_t<decltype(::mstudioseqdesc_t::nodeflags)>>, "type mstudioseqdesc_t.nodeflags");
    static_assert(o1134 == offsetof(::mstudioseqdesc_t, nextseq), "offsetof mstudioseqdesc_t.nextseq");
    static_assert(std::is_same_v<t1128, map_t<decltype(::mstudioseqdesc_t::nextseq)>>, "type mstudioseqdesc_t.nextseq");
    static_assert(s93 == sizeof(::mstudiotexture_t), "sizeof mstudiotexture_t");
    static_assert(o1135 == offsetof(::mstudiotexture_t, name), "offsetof mstudiotexture_t.name");
    static_assert(std::is_same_v<t1129, map_t<decltype(::mstudiotexture_t::name)>>, "type mstudiotexture_t.name");
    static_assert(o1136 == offsetof(::mstudiotexture_t, flags), "offsetof mstudiotexture_t.flags");
    static_assert(std::is_same_v<t1130, map_t<decltype(::mstudiotexture_t::flags)>>, "type mstudiotexture_t.flags");
    static_assert(o1137 == offsetof(::mstudiotexture_t, width), "offsetof mstudiotexture_t.width");
    static_assert(std::is_same_v<t1131, map_t<decltype(::mstudiotexture_t::width)>>, "type mstudiotexture_t.width");
    static_assert(o1138 == offsetof(::mstudiotexture_t, height), "offsetof mstudiotexture_t.height");
    static_assert(std::is_same_v<t1132, map_t<decltype(::mstudiotexture_t::height)>>, "type mstudiotexture_t.height");
    static_assert(o1139 == offsetof(::mstudiotexture_t, index), "offsetof mstudiotexture_t.index");
    static_assert(std::is_same_v<t1133, map_t<decltype(::mstudiotexture_t::index)>>, "type mstudiotexture_t.index");
    static_assert(s94 == sizeof(::studiohdr_t), "sizeof studiohdr_t");
    static_assert(o1140 == offsetof(::studiohdr_t, id), "offsetof studiohdr_t.id");
    static_assert(std::is_same_v<t1134, map_t<decltype(::studiohdr_t::id)>>, "type studiohdr_t.id");
    static_assert(o1141 == offsetof(::studiohdr_t, version), "offsetof studiohdr_t.version");
    static_assert(std::is_same_v<t1135, map_t<decltype(::studiohdr_t::version)>>, "type studiohdr_t.version");
    static_assert(o1142 == offsetof(::studiohdr_t, name), "offsetof studiohdr_t.name");
    static_assert(std::is_same_v<t1136, map_t<decltype(::studiohdr_t::name)>>, "type studiohdr_t.name");
    static_assert(o1143 == offsetof(::studiohdr_t, length), "offsetof studiohdr_t.length");
    static_assert(std::is_same_v<t1137, map_t<decltype(::studiohdr_t::length)>>, "type studiohdr_t.length");
    static_assert(o1144 == offsetof(::studiohdr_t, eyeposition), "offsetof studiohdr_t.eyeposition");
    static_assert(std::is_same_v<t1138, map_t<decltype(::studiohdr_t::eyeposition)>>, "type studiohdr_t.eyeposition");
    static_assert(o1145 == offsetof(::studiohdr_t, min), "offsetof studiohdr_t.min");
    static_assert(std::is_same_v<t1139, map_t<decltype(::studiohdr_t::min)>>, "type studiohdr_t.min");
    static_assert(o1146 == offsetof(::studiohdr_t, max), "offsetof studiohdr_t.max");
    static_assert(std::is_same_v<t1140, map_t<decltype(::studiohdr_t::max)>>, "type studiohdr_t.max");
    static_assert(o1147 == offsetof(::studiohdr_t, bbmin), "offsetof studiohdr_t.bbmin");
    static_assert(std::is_same_v<t1141, map_t<decltype(::studiohdr_t::bbmin)>>, "type studiohdr_t.bbmin");
    static_assert(o1148 == offsetof(::studiohdr_t, bbmax), "offsetof studiohdr_t.bbmax");
    static_assert(std::is_same_v<t1142, map_t<decltype(::studiohdr_t::bbmax)>>, "type studiohdr_t.bbmax");
    static_assert(o1149 == offsetof(::studiohdr_t, flags), "offsetof studiohdr_t.flags");
    static_assert(std::is_same_v<t1143, map_t<decltype(::studiohdr_t::flags)>>, "type studiohdr_t.flags");
    static_assert(o1150 == offsetof(::studiohdr_t, numbones), "offsetof studiohdr_t.numbones");
    static_assert(std::is_same_v<t1144, map_t<decltype(::studiohdr_t::numbones)>>, "type studiohdr_t.numbones");
    static_assert(o1151 == offsetof(::studiohdr_t, boneindex), "offsetof studiohdr_t.boneindex");
    static_assert(std::is_same_v<t1145, map_t<decltype(::studiohdr_t::boneindex)>>, "type studiohdr_t.boneindex");
    static_assert(o1152 == offsetof(::studiohdr_t, numbonecontrollers), "offsetof studiohdr_t.numbonecontrollers");
    static_assert(std::is_same_v<t1146, map_t<decltype(::studiohdr_t::numbonecontrollers)>>, "type studiohdr_t.numbonecontrollers");
    static_assert(o1153 == offsetof(::studiohdr_t, bonecontrollerindex), "offsetof studiohdr_t.bonecontrollerindex");
    static_assert(std::is_same_v<t1147, map_t<decltype(::studiohdr_t::bonecontrollerindex)>>, "type studiohdr_t.bonecontrollerindex");
    static_assert(o1154 == offsetof(::studiohdr_t, numhitboxes), "offsetof studiohdr_t.numhitboxes");
    static_assert(std::is_same_v<t1148, map_t<decltype(::studiohdr_t::numhitboxes)>>, "type studiohdr_t.numhitboxes");
    static_assert(o1155 == offsetof(::studiohdr_t, hitboxindex), "offsetof studiohdr_t.hitboxindex");
    static_assert(std::is_same_v<t1149, map_t<decltype(::studiohdr_t::hitboxindex)>>, "type studiohdr_t.hitboxindex");
    static_assert(o1156 == offsetof(::studiohdr_t, numseq), "offsetof studiohdr_t.numseq");
    static_assert(std::is_same_v<t1150, map_t<decltype(::studiohdr_t::numseq)>>, "type studiohdr_t.numseq");
    static_assert(o1157 == offsetof(::studiohdr_t, seqindex), "offsetof studiohdr_t.seqindex");
    static_assert(std::is_same_v<t1151, map_t<decltype(::studiohdr_t::seqindex)>>, "type studiohdr_t.seqindex");
    static_assert(o1158 == offsetof(::studiohdr_t, numseqgroups), "offsetof studiohdr_t.numseqgroups");
    static_assert(std::is_same_v<t1152, map_t<decltype(::studiohdr_t::numseqgroups)>>, "type studiohdr_t.numseqgroups");
    static_assert(o1159 == offsetof(::studiohdr_t, seqgroupindex), "offsetof studiohdr_t.seqgroupindex");
    static_assert(std::is_same_v<t1153, map_t<decltype(::studiohdr_t::seqgroupindex)>>, "type studiohdr_t.seqgroupindex");
    static_assert(o1160 == offsetof(::studiohdr_t, numtextures), "offsetof studiohdr_t.numtextures");
    static_assert(std::is_same_v<t1154, map_t<decltype(::studiohdr_t::numtextures)>>, "type studiohdr_t.numtextures");
    static_assert(o1161 == offsetof(::studiohdr_t, textureindex), "offsetof studiohdr_t.textureindex");
    static_assert(std::is_same_v<t1155, map_t<decltype(::studiohdr_t::textureindex)>>, "type studiohdr_t.textureindex");
    static_assert(o1162 == offsetof(::studiohdr_t, texturedataindex), "offsetof studiohdr_t.texturedataindex");
    static_assert(std::is_same_v<t1156, map_t<decltype(::studiohdr_t::texturedataindex)>>, "type studiohdr_t.texturedataindex");
    static_assert(o1163 == offsetof(::studiohdr_t, numskinref), "offsetof studiohdr_t.numskinref");
    static_assert(std::is_same_v<t1157, map_t<decltype(::studiohdr_t::numskinref)>>, "type studiohdr_t.numskinref");
    static_assert(o1164 == offsetof(::studiohdr_t, numskinfamilies), "offsetof studiohdr_t.numskinfamilies");
    static_assert(std::is_same_v<t1158, map_t<decltype(::studiohdr_t::numskinfamilies)>>, "type studiohdr_t.numskinfamilies");
    static_assert(o1165 == offsetof(::studiohdr_t, skinindex), "offsetof studiohdr_t.skinindex");
    static_assert(std::is_same_v<t1159, map_t<decltype(::studiohdr_t::skinindex)>>, "type studiohdr_t.skinindex");
    static_assert(o1166 == offsetof(::studiohdr_t, numbodyparts), "offsetof studiohdr_t.numbodyparts");
    static_assert(std::is_same_v<t1160, map_t<decltype(::studiohdr_t::numbodyparts)>>, "type studiohdr_t.numbodyparts");
    static_assert(o1167 == offsetof(::studiohdr_t, bodypartindex), "offsetof studiohdr_t.bodypartindex");
    static_assert(std::is_same_v<t1161, map_t<decltype(::studiohdr_t::bodypartindex)>>, "type studiohdr_t.bodypartindex");
    static_assert(o1168 == offsetof(::studiohdr_t, numattachments), "offsetof studiohdr_t.numattachments");
    static_assert(std::is_same_v<t1162, map_t<decltype(::studiohdr_t::numattachments)>>, "type studiohdr_t.numattachments");
    static_assert(o1169 == offsetof(::studiohdr_t, attachmentindex), "offsetof studiohdr_t.attachmentindex");
    static_assert(std::is_same_v<t1163, map_t<decltype(::studiohdr_t::attachmentindex)>>, "type studiohdr_t.attachmentindex");
    static_assert(o1170 == offsetof(::studiohdr_t, soundtable), "offsetof studiohdr_t.soundtable");
    static_assert(std::is_same_v<t1164, map_t<decltype(::studiohdr_t::soundtable)>>, "type studiohdr_t.soundtable");
    static_assert(o1171 == offsetof(::studiohdr_t, soundindex), "offsetof studiohdr_t.soundindex");
    static_assert(std::is_same_v<t1165, map_t<decltype(::studiohdr_t::soundindex)>>, "type studiohdr_t.soundindex");
    static_assert(o1172 == offsetof(::studiohdr_t, soundgroups), "offsetof studiohdr_t.soundgroups");
    static_assert(std::is_same_v<t1166, map_t<decltype(::studiohdr_t::soundgroups)>>, "type studiohdr_t.soundgroups");
    static_assert(o1173 == offsetof(::studiohdr_t, soundgroupindex), "offsetof studiohdr_t.soundgroupindex");
    static_assert(std::is_same_v<t1167, map_t<decltype(::studiohdr_t::soundgroupindex)>>, "type studiohdr_t.soundgroupindex");
    static_assert(o1174 == offsetof(::studiohdr_t, numtransitions), "offsetof studiohdr_t.numtransitions");
    static_assert(std::is_same_v<t1168, map_t<decltype(::studiohdr_t::numtransitions)>>, "type studiohdr_t.numtransitions");
    static_assert(o1175 == offsetof(::studiohdr_t, transitionindex), "offsetof studiohdr_t.transitionindex");
    static_assert(std::is_same_v<t1169, map_t<decltype(::studiohdr_t::transitionindex)>>, "type studiohdr_t.transitionindex");
    // lumpinfo_s: comparisons skipped - declared in our NextClient sources, which this test does not include; the golden test checks its
    // layout against the facts

    // qpic_t: comparisons skipped - declared in our NextClient sources, which this test does not include; the golden test checks its layout
    // against the facts

    // wadinfo_t: comparisons skipped - declared in our NextClient sources, which this test does not include; the golden test checks its
    // layout against the facts

    static_assert(s95 == sizeof(::movevars_s), "sizeof movevars_s");
    static_assert(o1176 == offsetof(::movevars_s, gravity), "offsetof movevars_s.gravity");
    static_assert(std::is_same_v<t1170, map_t<decltype(::movevars_s::gravity)>>, "type movevars_s.gravity");
    static_assert(o1177 == offsetof(::movevars_s, stopspeed), "offsetof movevars_s.stopspeed");
    static_assert(std::is_same_v<t1171, map_t<decltype(::movevars_s::stopspeed)>>, "type movevars_s.stopspeed");
    static_assert(o1178 == offsetof(::movevars_s, maxspeed), "offsetof movevars_s.maxspeed");
    static_assert(std::is_same_v<t1172, map_t<decltype(::movevars_s::maxspeed)>>, "type movevars_s.maxspeed");
    static_assert(o1179 == offsetof(::movevars_s, spectatormaxspeed), "offsetof movevars_s.spectatormaxspeed");
    static_assert(std::is_same_v<t1173, map_t<decltype(::movevars_s::spectatormaxspeed)>>, "type movevars_s.spectatormaxspeed");
    static_assert(o1180 == offsetof(::movevars_s, accelerate), "offsetof movevars_s.accelerate");
    static_assert(std::is_same_v<t1174, map_t<decltype(::movevars_s::accelerate)>>, "type movevars_s.accelerate");
    static_assert(o1181 == offsetof(::movevars_s, airaccelerate), "offsetof movevars_s.airaccelerate");
    static_assert(std::is_same_v<t1175, map_t<decltype(::movevars_s::airaccelerate)>>, "type movevars_s.airaccelerate");
    static_assert(o1182 == offsetof(::movevars_s, wateraccelerate), "offsetof movevars_s.wateraccelerate");
    static_assert(std::is_same_v<t1176, map_t<decltype(::movevars_s::wateraccelerate)>>, "type movevars_s.wateraccelerate");
    static_assert(o1183 == offsetof(::movevars_s, friction), "offsetof movevars_s.friction");
    static_assert(std::is_same_v<t1177, map_t<decltype(::movevars_s::friction)>>, "type movevars_s.friction");
    static_assert(o1184 == offsetof(::movevars_s, edgefriction), "offsetof movevars_s.edgefriction");
    static_assert(std::is_same_v<t1178, map_t<decltype(::movevars_s::edgefriction)>>, "type movevars_s.edgefriction");
    static_assert(o1185 == offsetof(::movevars_s, waterfriction), "offsetof movevars_s.waterfriction");
    static_assert(std::is_same_v<t1179, map_t<decltype(::movevars_s::waterfriction)>>, "type movevars_s.waterfriction");
    static_assert(o1186 == offsetof(::movevars_s, entgravity), "offsetof movevars_s.entgravity");
    static_assert(std::is_same_v<t1180, map_t<decltype(::movevars_s::entgravity)>>, "type movevars_s.entgravity");
    static_assert(o1187 == offsetof(::movevars_s, bounce), "offsetof movevars_s.bounce");
    static_assert(std::is_same_v<t1181, map_t<decltype(::movevars_s::bounce)>>, "type movevars_s.bounce");
    static_assert(o1188 == offsetof(::movevars_s, stepsize), "offsetof movevars_s.stepsize");
    static_assert(std::is_same_v<t1182, map_t<decltype(::movevars_s::stepsize)>>, "type movevars_s.stepsize");
    static_assert(o1189 == offsetof(::movevars_s, maxvelocity), "offsetof movevars_s.maxvelocity");
    static_assert(std::is_same_v<t1183, map_t<decltype(::movevars_s::maxvelocity)>>, "type movevars_s.maxvelocity");
    static_assert(o1190 == offsetof(::movevars_s, zmax), "offsetof movevars_s.zmax");
    static_assert(std::is_same_v<t1184, map_t<decltype(::movevars_s::zmax)>>, "type movevars_s.zmax");
    static_assert(o1191 == offsetof(::movevars_s, waveHeight), "offsetof movevars_s.waveHeight");
    static_assert(std::is_same_v<t1185, map_t<decltype(::movevars_s::waveHeight)>>, "type movevars_s.waveHeight");
    static_assert(o1192 == offsetof(::movevars_s, footsteps), "offsetof movevars_s.footsteps");
    static_assert(std::is_same_v<t1186, map_t<decltype(::movevars_s::footsteps)>>, "type movevars_s.footsteps");
    static_assert(o1193 == offsetof(::movevars_s, skyName), "offsetof movevars_s.skyName");
    static_assert(std::is_same_v<t1187, map_t<decltype(::movevars_s::skyName)>>, "type movevars_s.skyName");
    static_assert(o1194 == offsetof(::movevars_s, rollangle), "offsetof movevars_s.rollangle");
    static_assert(std::is_same_v<t1188, map_t<decltype(::movevars_s::rollangle)>>, "type movevars_s.rollangle");
    static_assert(o1195 == offsetof(::movevars_s, rollspeed), "offsetof movevars_s.rollspeed");
    static_assert(std::is_same_v<t1189, map_t<decltype(::movevars_s::rollspeed)>>, "type movevars_s.rollspeed");
    static_assert(o1196 == offsetof(::movevars_s, skycolor_r), "offsetof movevars_s.skycolor_r");
    static_assert(std::is_same_v<t1190, map_t<decltype(::movevars_s::skycolor_r)>>, "type movevars_s.skycolor_r");
    static_assert(o1197 == offsetof(::movevars_s, skycolor_g), "offsetof movevars_s.skycolor_g");
    static_assert(std::is_same_v<t1191, map_t<decltype(::movevars_s::skycolor_g)>>, "type movevars_s.skycolor_g");
    static_assert(o1198 == offsetof(::movevars_s, skycolor_b), "offsetof movevars_s.skycolor_b");
    static_assert(std::is_same_v<t1192, map_t<decltype(::movevars_s::skycolor_b)>>, "type movevars_s.skycolor_b");
    static_assert(o1199 == offsetof(::movevars_s, skyvec_x), "offsetof movevars_s.skyvec_x");
    static_assert(std::is_same_v<t1193, map_t<decltype(::movevars_s::skyvec_x)>>, "type movevars_s.skyvec_x");
    static_assert(o1200 == offsetof(::movevars_s, skyvec_y), "offsetof movevars_s.skyvec_y");
    static_assert(std::is_same_v<t1194, map_t<decltype(::movevars_s::skyvec_y)>>, "type movevars_s.skyvec_y");
    static_assert(o1201 == offsetof(::movevars_s, skyvec_z), "offsetof movevars_s.skyvec_z");
    static_assert(std::is_same_v<t1195, map_t<decltype(::movevars_s::skyvec_z)>>, "type movevars_s.skyvec_z");
    static_assert(s96 == sizeof(::physent_s), "sizeof physent_s");
    static_assert(o1202 == offsetof(::physent_s, name), "offsetof physent_s.name");
    static_assert(std::is_same_v<t1196, map_t<decltype(::physent_s::name)>>, "type physent_s.name");
    static_assert(o1203 == offsetof(::physent_s, player), "offsetof physent_s.player");
    static_assert(std::is_same_v<t1197, map_t<decltype(::physent_s::player)>>, "type physent_s.player");
    static_assert(o1204 == offsetof(::physent_s, origin), "offsetof physent_s.origin");
    static_assert(std::is_same_v<t1198, map_t<decltype(::physent_s::origin)>>, "type physent_s.origin");
    static_assert(o1205 == offsetof(::physent_s, model), "offsetof physent_s.model");
    static_assert(std::is_same_v<t1199, map_t<decltype(::physent_s::model)>>, "type physent_s.model");
    static_assert(o1206 == offsetof(::physent_s, studiomodel), "offsetof physent_s.studiomodel");
    static_assert(std::is_same_v<t1200, map_t<decltype(::physent_s::studiomodel)>>, "type physent_s.studiomodel");
    static_assert(o1207 == offsetof(::physent_s, mins), "offsetof physent_s.mins");
    static_assert(std::is_same_v<t1201, map_t<decltype(::physent_s::mins)>>, "type physent_s.mins");
    static_assert(o1208 == offsetof(::physent_s, maxs), "offsetof physent_s.maxs");
    static_assert(std::is_same_v<t1202, map_t<decltype(::physent_s::maxs)>>, "type physent_s.maxs");
    static_assert(o1209 == offsetof(::physent_s, info), "offsetof physent_s.info");
    static_assert(std::is_same_v<t1203, map_t<decltype(::physent_s::info)>>, "type physent_s.info");
    static_assert(o1210 == offsetof(::physent_s, angles), "offsetof physent_s.angles");
    static_assert(std::is_same_v<t1204, map_t<decltype(::physent_s::angles)>>, "type physent_s.angles");
    static_assert(o1211 == offsetof(::physent_s, solid), "offsetof physent_s.solid");
    static_assert(std::is_same_v<t1205, map_t<decltype(::physent_s::solid)>>, "type physent_s.solid");
    static_assert(o1212 == offsetof(::physent_s, skin), "offsetof physent_s.skin");
    static_assert(std::is_same_v<t1206, map_t<decltype(::physent_s::skin)>>, "type physent_s.skin");
    static_assert(o1213 == offsetof(::physent_s, rendermode), "offsetof physent_s.rendermode");
    static_assert(std::is_same_v<t1207, map_t<decltype(::physent_s::rendermode)>>, "type physent_s.rendermode");
    static_assert(o1214 == offsetof(::physent_s, frame), "offsetof physent_s.frame");
    static_assert(std::is_same_v<t1208, map_t<decltype(::physent_s::frame)>>, "type physent_s.frame");
    static_assert(o1215 == offsetof(::physent_s, sequence), "offsetof physent_s.sequence");
    static_assert(std::is_same_v<t1209, map_t<decltype(::physent_s::sequence)>>, "type physent_s.sequence");
    static_assert(o1216 == offsetof(::physent_s, controller), "offsetof physent_s.controller");
    static_assert(std::is_same_v<t1210, map_t<decltype(::physent_s::controller)>>, "type physent_s.controller");
    static_assert(o1217 == offsetof(::physent_s, blending), "offsetof physent_s.blending");
    static_assert(std::is_same_v<t1211, map_t<decltype(::physent_s::blending)>>, "type physent_s.blending");
    static_assert(o1218 == offsetof(::physent_s, movetype), "offsetof physent_s.movetype");
    static_assert(std::is_same_v<t1212, map_t<decltype(::physent_s::movetype)>>, "type physent_s.movetype");
    static_assert(o1219 == offsetof(::physent_s, takedamage), "offsetof physent_s.takedamage");
    static_assert(std::is_same_v<t1213, map_t<decltype(::physent_s::takedamage)>>, "type physent_s.takedamage");
    static_assert(o1220 == offsetof(::physent_s, blooddecal), "offsetof physent_s.blooddecal");
    static_assert(std::is_same_v<t1214, map_t<decltype(::physent_s::blooddecal)>>, "type physent_s.blooddecal");
    static_assert(o1221 == offsetof(::physent_s, team), "offsetof physent_s.team");
    static_assert(std::is_same_v<t1215, map_t<decltype(::physent_s::team)>>, "type physent_s.team");
    static_assert(o1222 == offsetof(::physent_s, classnumber), "offsetof physent_s.classnumber");
    static_assert(std::is_same_v<t1216, map_t<decltype(::physent_s::classnumber)>>, "type physent_s.classnumber");
    static_assert(o1223 == offsetof(::physent_s, iuser1), "offsetof physent_s.iuser1");
    static_assert(std::is_same_v<t1217, map_t<decltype(::physent_s::iuser1)>>, "type physent_s.iuser1");
    static_assert(o1224 == offsetof(::physent_s, iuser2), "offsetof physent_s.iuser2");
    static_assert(std::is_same_v<t1218, map_t<decltype(::physent_s::iuser2)>>, "type physent_s.iuser2");
    static_assert(o1225 == offsetof(::physent_s, iuser3), "offsetof physent_s.iuser3");
    static_assert(std::is_same_v<t1219, map_t<decltype(::physent_s::iuser3)>>, "type physent_s.iuser3");
    static_assert(o1226 == offsetof(::physent_s, iuser4), "offsetof physent_s.iuser4");
    static_assert(std::is_same_v<t1220, map_t<decltype(::physent_s::iuser4)>>, "type physent_s.iuser4");
    static_assert(o1227 == offsetof(::physent_s, fuser1), "offsetof physent_s.fuser1");
    static_assert(std::is_same_v<t1221, map_t<decltype(::physent_s::fuser1)>>, "type physent_s.fuser1");
    static_assert(o1228 == offsetof(::physent_s, fuser2), "offsetof physent_s.fuser2");
    static_assert(std::is_same_v<t1222, map_t<decltype(::physent_s::fuser2)>>, "type physent_s.fuser2");
    static_assert(o1229 == offsetof(::physent_s, fuser3), "offsetof physent_s.fuser3");
    static_assert(std::is_same_v<t1223, map_t<decltype(::physent_s::fuser3)>>, "type physent_s.fuser3");
    static_assert(o1230 == offsetof(::physent_s, fuser4), "offsetof physent_s.fuser4");
    static_assert(std::is_same_v<t1224, map_t<decltype(::physent_s::fuser4)>>, "type physent_s.fuser4");
    static_assert(o1231 == offsetof(::physent_s, vuser1), "offsetof physent_s.vuser1");
    static_assert(std::is_same_v<t1225, map_t<decltype(::physent_s::vuser1)>>, "type physent_s.vuser1");
    static_assert(o1232 == offsetof(::physent_s, vuser2), "offsetof physent_s.vuser2");
    static_assert(std::is_same_v<t1226, map_t<decltype(::physent_s::vuser2)>>, "type physent_s.vuser2");
    static_assert(o1233 == offsetof(::physent_s, vuser3), "offsetof physent_s.vuser3");
    static_assert(std::is_same_v<t1227, map_t<decltype(::physent_s::vuser3)>>, "type physent_s.vuser3");
    static_assert(o1234 == offsetof(::physent_s, vuser4), "offsetof physent_s.vuser4");
    static_assert(std::is_same_v<t1228, map_t<decltype(::physent_s::vuser4)>>, "type physent_s.vuser4");
    static_assert(s97 == sizeof(::pmplane_t), "sizeof pmplane_t");
    static_assert(o1235 == offsetof(::pmplane_t, normal), "offsetof pmplane_t.normal");
    static_assert(std::is_same_v<t1229, map_t<decltype(::pmplane_t::normal)>>, "type pmplane_t.normal");
    static_assert(o1236 == offsetof(::pmplane_t, dist), "offsetof pmplane_t.dist");
    static_assert(std::is_same_v<t1230, map_t<decltype(::pmplane_t::dist)>>, "type pmplane_t.dist");
    static_assert(s98 == sizeof(::pmtrace_s), "sizeof pmtrace_s");
    static_assert(o1237 == offsetof(::pmtrace_s, allsolid), "offsetof pmtrace_s.allsolid");
    static_assert(std::is_same_v<t1231, map_t<decltype(::pmtrace_s::allsolid)>>, "type pmtrace_s.allsolid");
    static_assert(o1238 == offsetof(::pmtrace_s, startsolid), "offsetof pmtrace_s.startsolid");
    static_assert(std::is_same_v<t1232, map_t<decltype(::pmtrace_s::startsolid)>>, "type pmtrace_s.startsolid");
    static_assert(o1239 == offsetof(::pmtrace_s, inopen), "offsetof pmtrace_s.inopen");
    static_assert(std::is_same_v<t1233, map_t<decltype(::pmtrace_s::inopen)>>, "type pmtrace_s.inopen");
    static_assert(o1240 == offsetof(::pmtrace_s, inwater), "offsetof pmtrace_s.inwater");
    static_assert(std::is_same_v<t1234, map_t<decltype(::pmtrace_s::inwater)>>, "type pmtrace_s.inwater");
    static_assert(o1241 == offsetof(::pmtrace_s, fraction), "offsetof pmtrace_s.fraction");
    static_assert(std::is_same_v<t1235, map_t<decltype(::pmtrace_s::fraction)>>, "type pmtrace_s.fraction");
    static_assert(o1242 == offsetof(::pmtrace_s, endpos), "offsetof pmtrace_s.endpos");
    static_assert(std::is_same_v<t1236, map_t<decltype(::pmtrace_s::endpos)>>, "type pmtrace_s.endpos");
    static_assert(o1243 == offsetof(::pmtrace_s, plane), "offsetof pmtrace_s.plane");
    static_assert(std::is_same_v<t1237, map_t<decltype(::pmtrace_s::plane)>>, "type pmtrace_s.plane");
    static_assert(o1244 == offsetof(::pmtrace_s, ent), "offsetof pmtrace_s.ent");
    static_assert(std::is_same_v<t1238, map_t<decltype(::pmtrace_s::ent)>>, "type pmtrace_s.ent");
    static_assert(o1245 == offsetof(::pmtrace_s, deltavelocity), "offsetof pmtrace_s.deltavelocity");
    static_assert(std::is_same_v<t1239, map_t<decltype(::pmtrace_s::deltavelocity)>>, "type pmtrace_s.deltavelocity");
    static_assert(o1246 == offsetof(::pmtrace_s, hitgroup), "offsetof pmtrace_s.hitgroup");
    static_assert(std::is_same_v<t1240, map_t<decltype(::pmtrace_s::hitgroup)>>, "type pmtrace_s.hitgroup");
    static_assert(s99 == sizeof(::playermove_s), "sizeof playermove_s");
    static_assert(o1247 == offsetof(::playermove_s, player_index), "offsetof playermove_s.player_index");
    static_assert(std::is_same_v<t1241, map_t<decltype(::playermove_s::player_index)>>, "type playermove_s.player_index");
    static_assert(o1248 == offsetof(::playermove_s, server), "offsetof playermove_s.server");
    static_assert(std::is_same_v<t1242, map_t<decltype(::playermove_s::server)>>, "type playermove_s.server");
    static_assert(o1249 == offsetof(::playermove_s, multiplayer), "offsetof playermove_s.multiplayer");
    static_assert(std::is_same_v<t1243, map_t<decltype(::playermove_s::multiplayer)>>, "type playermove_s.multiplayer");
    static_assert(o1250 == offsetof(::playermove_s, time), "offsetof playermove_s.time");
    static_assert(std::is_same_v<t1244, map_t<decltype(::playermove_s::time)>>, "type playermove_s.time");
    static_assert(o1251 == offsetof(::playermove_s, frametime), "offsetof playermove_s.frametime");
    static_assert(std::is_same_v<t1245, map_t<decltype(::playermove_s::frametime)>>, "type playermove_s.frametime");
    static_assert(o1252 == offsetof(::playermove_s, forward), "offsetof playermove_s.forward");
    static_assert(std::is_same_v<t1246, map_t<decltype(::playermove_s::forward)>>, "type playermove_s.forward");
    static_assert(o1253 == offsetof(::playermove_s, right), "offsetof playermove_s.right");
    static_assert(std::is_same_v<t1247, map_t<decltype(::playermove_s::right)>>, "type playermove_s.right");
    static_assert(o1254 == offsetof(::playermove_s, up), "offsetof playermove_s.up");
    static_assert(std::is_same_v<t1248, map_t<decltype(::playermove_s::up)>>, "type playermove_s.up");
    static_assert(o1255 == offsetof(::playermove_s, origin), "offsetof playermove_s.origin");
    static_assert(std::is_same_v<t1249, map_t<decltype(::playermove_s::origin)>>, "type playermove_s.origin");
    static_assert(o1256 == offsetof(::playermove_s, angles), "offsetof playermove_s.angles");
    static_assert(std::is_same_v<t1250, map_t<decltype(::playermove_s::angles)>>, "type playermove_s.angles");
    static_assert(o1257 == offsetof(::playermove_s, oldangles), "offsetof playermove_s.oldangles");
    static_assert(std::is_same_v<t1251, map_t<decltype(::playermove_s::oldangles)>>, "type playermove_s.oldangles");
    static_assert(o1258 == offsetof(::playermove_s, velocity), "offsetof playermove_s.velocity");
    static_assert(std::is_same_v<t1252, map_t<decltype(::playermove_s::velocity)>>, "type playermove_s.velocity");
    static_assert(o1259 == offsetof(::playermove_s, movedir), "offsetof playermove_s.movedir");
    static_assert(std::is_same_v<t1253, map_t<decltype(::playermove_s::movedir)>>, "type playermove_s.movedir");
    static_assert(o1260 == offsetof(::playermove_s, basevelocity), "offsetof playermove_s.basevelocity");
    static_assert(std::is_same_v<t1254, map_t<decltype(::playermove_s::basevelocity)>>, "type playermove_s.basevelocity");
    static_assert(o1261 == offsetof(::playermove_s, view_ofs), "offsetof playermove_s.view_ofs");
    static_assert(std::is_same_v<t1255, map_t<decltype(::playermove_s::view_ofs)>>, "type playermove_s.view_ofs");
    static_assert(o1262 == offsetof(::playermove_s, flDuckTime), "offsetof playermove_s.flDuckTime");
    static_assert(std::is_same_v<t1256, map_t<decltype(::playermove_s::flDuckTime)>>, "type playermove_s.flDuckTime");
    static_assert(o1263 == offsetof(::playermove_s, bInDuck), "offsetof playermove_s.bInDuck");
    static_assert(std::is_same_v<t1257, map_t<decltype(::playermove_s::bInDuck)>>, "type playermove_s.bInDuck");
    static_assert(o1264 == offsetof(::playermove_s, flTimeStepSound), "offsetof playermove_s.flTimeStepSound");
    static_assert(std::is_same_v<t1258, map_t<decltype(::playermove_s::flTimeStepSound)>>, "type playermove_s.flTimeStepSound");
    static_assert(o1265 == offsetof(::playermove_s, iStepLeft), "offsetof playermove_s.iStepLeft");
    static_assert(std::is_same_v<t1259, map_t<decltype(::playermove_s::iStepLeft)>>, "type playermove_s.iStepLeft");
    static_assert(o1266 == offsetof(::playermove_s, flFallVelocity), "offsetof playermove_s.flFallVelocity");
    static_assert(std::is_same_v<t1260, map_t<decltype(::playermove_s::flFallVelocity)>>, "type playermove_s.flFallVelocity");
    static_assert(o1267 == offsetof(::playermove_s, punchangle), "offsetof playermove_s.punchangle");
    static_assert(std::is_same_v<t1261, map_t<decltype(::playermove_s::punchangle)>>, "type playermove_s.punchangle");
    static_assert(o1268 == offsetof(::playermove_s, flSwimTime), "offsetof playermove_s.flSwimTime");
    static_assert(std::is_same_v<t1262, map_t<decltype(::playermove_s::flSwimTime)>>, "type playermove_s.flSwimTime");
    static_assert(o1269 == offsetof(::playermove_s, flNextPrimaryAttack), "offsetof playermove_s.flNextPrimaryAttack");
    static_assert(std::is_same_v<t1263, map_t<decltype(::playermove_s::flNextPrimaryAttack)>>, "type playermove_s.flNextPrimaryAttack");
    static_assert(o1270 == offsetof(::playermove_s, effects), "offsetof playermove_s.effects");
    static_assert(std::is_same_v<t1264, map_t<decltype(::playermove_s::effects)>>, "type playermove_s.effects");
    static_assert(o1271 == offsetof(::playermove_s, flags), "offsetof playermove_s.flags");
    static_assert(std::is_same_v<t1265, map_t<decltype(::playermove_s::flags)>>, "type playermove_s.flags");
    static_assert(o1272 == offsetof(::playermove_s, usehull), "offsetof playermove_s.usehull");
    static_assert(std::is_same_v<t1266, map_t<decltype(::playermove_s::usehull)>>, "type playermove_s.usehull");
    static_assert(o1273 == offsetof(::playermove_s, gravity), "offsetof playermove_s.gravity");
    static_assert(std::is_same_v<t1267, map_t<decltype(::playermove_s::gravity)>>, "type playermove_s.gravity");
    static_assert(o1274 == offsetof(::playermove_s, friction), "offsetof playermove_s.friction");
    static_assert(std::is_same_v<t1268, map_t<decltype(::playermove_s::friction)>>, "type playermove_s.friction");
    static_assert(o1275 == offsetof(::playermove_s, oldbuttons), "offsetof playermove_s.oldbuttons");
    static_assert(std::is_same_v<t1269, map_t<decltype(::playermove_s::oldbuttons)>>, "type playermove_s.oldbuttons");
    static_assert(o1276 == offsetof(::playermove_s, waterjumptime), "offsetof playermove_s.waterjumptime");
    static_assert(std::is_same_v<t1270, map_t<decltype(::playermove_s::waterjumptime)>>, "type playermove_s.waterjumptime");
    static_assert(o1277 == offsetof(::playermove_s, dead), "offsetof playermove_s.dead");
    static_assert(std::is_same_v<t1271, map_t<decltype(::playermove_s::dead)>>, "type playermove_s.dead");
    static_assert(o1278 == offsetof(::playermove_s, deadflag), "offsetof playermove_s.deadflag");
    static_assert(std::is_same_v<t1272, map_t<decltype(::playermove_s::deadflag)>>, "type playermove_s.deadflag");
    static_assert(o1279 == offsetof(::playermove_s, spectator), "offsetof playermove_s.spectator");
    static_assert(std::is_same_v<t1273, map_t<decltype(::playermove_s::spectator)>>, "type playermove_s.spectator");
    static_assert(o1280 == offsetof(::playermove_s, movetype), "offsetof playermove_s.movetype");
    static_assert(std::is_same_v<t1274, map_t<decltype(::playermove_s::movetype)>>, "type playermove_s.movetype");
    static_assert(o1281 == offsetof(::playermove_s, onground), "offsetof playermove_s.onground");
    static_assert(std::is_same_v<t1275, map_t<decltype(::playermove_s::onground)>>, "type playermove_s.onground");
    static_assert(o1282 == offsetof(::playermove_s, waterlevel), "offsetof playermove_s.waterlevel");
    static_assert(std::is_same_v<t1276, map_t<decltype(::playermove_s::waterlevel)>>, "type playermove_s.waterlevel");
    static_assert(o1283 == offsetof(::playermove_s, watertype), "offsetof playermove_s.watertype");
    static_assert(std::is_same_v<t1277, map_t<decltype(::playermove_s::watertype)>>, "type playermove_s.watertype");
    static_assert(o1284 == offsetof(::playermove_s, oldwaterlevel), "offsetof playermove_s.oldwaterlevel");
    static_assert(std::is_same_v<t1278, map_t<decltype(::playermove_s::oldwaterlevel)>>, "type playermove_s.oldwaterlevel");
    static_assert(o1285 == offsetof(::playermove_s, sztexturename), "offsetof playermove_s.sztexturename");
    static_assert(std::is_same_v<t1279, map_t<decltype(::playermove_s::sztexturename)>>, "type playermove_s.sztexturename");
    static_assert(o1286 == offsetof(::playermove_s, chtexturetype), "offsetof playermove_s.chtexturetype");
    static_assert(std::is_same_v<t1280, map_t<decltype(::playermove_s::chtexturetype)>>, "type playermove_s.chtexturetype");
    static_assert(o1287 == offsetof(::playermove_s, maxspeed), "offsetof playermove_s.maxspeed");
    static_assert(std::is_same_v<t1281, map_t<decltype(::playermove_s::maxspeed)>>, "type playermove_s.maxspeed");
    static_assert(o1288 == offsetof(::playermove_s, clientmaxspeed), "offsetof playermove_s.clientmaxspeed");
    static_assert(std::is_same_v<t1282, map_t<decltype(::playermove_s::clientmaxspeed)>>, "type playermove_s.clientmaxspeed");
    static_assert(o1289 == offsetof(::playermove_s, iuser1), "offsetof playermove_s.iuser1");
    static_assert(std::is_same_v<t1283, map_t<decltype(::playermove_s::iuser1)>>, "type playermove_s.iuser1");
    static_assert(o1290 == offsetof(::playermove_s, iuser2), "offsetof playermove_s.iuser2");
    static_assert(std::is_same_v<t1284, map_t<decltype(::playermove_s::iuser2)>>, "type playermove_s.iuser2");
    static_assert(o1291 == offsetof(::playermove_s, iuser3), "offsetof playermove_s.iuser3");
    static_assert(std::is_same_v<t1285, map_t<decltype(::playermove_s::iuser3)>>, "type playermove_s.iuser3");
    static_assert(o1292 == offsetof(::playermove_s, iuser4), "offsetof playermove_s.iuser4");
    static_assert(std::is_same_v<t1286, map_t<decltype(::playermove_s::iuser4)>>, "type playermove_s.iuser4");
    static_assert(o1293 == offsetof(::playermove_s, fuser1), "offsetof playermove_s.fuser1");
    static_assert(std::is_same_v<t1287, map_t<decltype(::playermove_s::fuser1)>>, "type playermove_s.fuser1");
    static_assert(o1294 == offsetof(::playermove_s, fuser2), "offsetof playermove_s.fuser2");
    static_assert(std::is_same_v<t1288, map_t<decltype(::playermove_s::fuser2)>>, "type playermove_s.fuser2");
    static_assert(o1295 == offsetof(::playermove_s, fuser3), "offsetof playermove_s.fuser3");
    static_assert(std::is_same_v<t1289, map_t<decltype(::playermove_s::fuser3)>>, "type playermove_s.fuser3");
    static_assert(o1296 == offsetof(::playermove_s, fuser4), "offsetof playermove_s.fuser4");
    static_assert(std::is_same_v<t1290, map_t<decltype(::playermove_s::fuser4)>>, "type playermove_s.fuser4");
    static_assert(o1297 == offsetof(::playermove_s, vuser1), "offsetof playermove_s.vuser1");
    static_assert(std::is_same_v<t1291, map_t<decltype(::playermove_s::vuser1)>>, "type playermove_s.vuser1");
    static_assert(o1298 == offsetof(::playermove_s, vuser2), "offsetof playermove_s.vuser2");
    static_assert(std::is_same_v<t1292, map_t<decltype(::playermove_s::vuser2)>>, "type playermove_s.vuser2");
    static_assert(o1299 == offsetof(::playermove_s, vuser3), "offsetof playermove_s.vuser3");
    static_assert(std::is_same_v<t1293, map_t<decltype(::playermove_s::vuser3)>>, "type playermove_s.vuser3");
    static_assert(o1300 == offsetof(::playermove_s, vuser4), "offsetof playermove_s.vuser4");
    static_assert(std::is_same_v<t1294, map_t<decltype(::playermove_s::vuser4)>>, "type playermove_s.vuser4");
    static_assert(o1301 == offsetof(::playermove_s, numphysent), "offsetof playermove_s.numphysent");
    static_assert(std::is_same_v<t1295, map_t<decltype(::playermove_s::numphysent)>>, "type playermove_s.numphysent");
    static_assert(o1302 == offsetof(::playermove_s, physents), "offsetof playermove_s.physents");
    static_assert(std::is_same_v<t1296, map_t<decltype(::playermove_s::physents)>>, "type playermove_s.physents");
    static_assert(o1303 == offsetof(::playermove_s, nummoveent), "offsetof playermove_s.nummoveent");
    static_assert(std::is_same_v<t1297, map_t<decltype(::playermove_s::nummoveent)>>, "type playermove_s.nummoveent");
    static_assert(o1304 == offsetof(::playermove_s, moveents), "offsetof playermove_s.moveents");
    static_assert(std::is_same_v<t1298, map_t<decltype(::playermove_s::moveents)>>, "type playermove_s.moveents");
    static_assert(o1305 == offsetof(::playermove_s, numvisent), "offsetof playermove_s.numvisent");
    static_assert(std::is_same_v<t1299, map_t<decltype(::playermove_s::numvisent)>>, "type playermove_s.numvisent");
    static_assert(o1306 == offsetof(::playermove_s, visents), "offsetof playermove_s.visents");
    static_assert(std::is_same_v<t1300, map_t<decltype(::playermove_s::visents)>>, "type playermove_s.visents");
    static_assert(o1307 == offsetof(::playermove_s, cmd), "offsetof playermove_s.cmd");
    static_assert(std::is_same_v<t1301, map_t<decltype(::playermove_s::cmd)>>, "type playermove_s.cmd");
    static_assert(o1308 == offsetof(::playermove_s, numtouch), "offsetof playermove_s.numtouch");
    static_assert(std::is_same_v<t1302, map_t<decltype(::playermove_s::numtouch)>>, "type playermove_s.numtouch");
    static_assert(o1309 == offsetof(::playermove_s, touchindex), "offsetof playermove_s.touchindex");
    static_assert(std::is_same_v<t1303, map_t<decltype(::playermove_s::touchindex)>>, "type playermove_s.touchindex");
    static_assert(o1310 == offsetof(::playermove_s, physinfo), "offsetof playermove_s.physinfo");
    static_assert(std::is_same_v<t1304, map_t<decltype(::playermove_s::physinfo)>>, "type playermove_s.physinfo");
    static_assert(o1311 == offsetof(::playermove_s, movevars), "offsetof playermove_s.movevars");
    static_assert(std::is_same_v<t1305, map_t<decltype(::playermove_s::movevars)>>, "type playermove_s.movevars");
    static_assert(o1312 == offsetof(::playermove_s, _player_mins), "offsetof playermove_s._player_mins");
    static_assert(std::is_same_v<t1306, map_t<decltype(::playermove_s::_player_mins)>>, "type playermove_s._player_mins");
    static_assert(o1313 == offsetof(::playermove_s, _player_maxs), "offsetof playermove_s._player_maxs");
    static_assert(std::is_same_v<t1307, map_t<decltype(::playermove_s::_player_maxs)>>, "type playermove_s._player_maxs");
    static_assert(o1314 == offsetof(::playermove_s, PM_Info_ValueForKey), "offsetof playermove_s.PM_Info_ValueForKey");
    static_assert(std::is_same_v<t1308, map_t<decltype(::playermove_s::PM_Info_ValueForKey)>>, "type playermove_s.PM_Info_ValueForKey");
    static_assert(o1315 == offsetof(::playermove_s, PM_Particle), "offsetof playermove_s.PM_Particle");
    static_assert(std::is_same_v<t1309, map_t<decltype(::playermove_s::PM_Particle)>>, "type playermove_s.PM_Particle");
    static_assert(o1316 == offsetof(::playermove_s, PM_TestPlayerPosition), "offsetof playermove_s.PM_TestPlayerPosition");
    static_assert(std::is_same_v<t1310, map_t<decltype(::playermove_s::PM_TestPlayerPosition)>>, "type playermove_s.PM_TestPlayerPosition");
    static_assert(o1317 == offsetof(::playermove_s, Con_NPrintf), "offsetof playermove_s.Con_NPrintf");
    static_assert(std::is_same_v<t1311, map_t<decltype(::playermove_s::Con_NPrintf)>>, "type playermove_s.Con_NPrintf");
    static_assert(o1318 == offsetof(::playermove_s, Con_DPrintf), "offsetof playermove_s.Con_DPrintf");
    static_assert(std::is_same_v<t1312, map_t<decltype(::playermove_s::Con_DPrintf)>>, "type playermove_s.Con_DPrintf");
    static_assert(o1319 == offsetof(::playermove_s, Con_Printf), "offsetof playermove_s.Con_Printf");
    static_assert(std::is_same_v<t1313, map_t<decltype(::playermove_s::Con_Printf)>>, "type playermove_s.Con_Printf");
    static_assert(o1320 == offsetof(::playermove_s, Sys_FloatTime), "offsetof playermove_s.Sys_FloatTime");
    static_assert(std::is_same_v<t1314, map_t<decltype(::playermove_s::Sys_FloatTime)>>, "type playermove_s.Sys_FloatTime");
    static_assert(o1321 == offsetof(::playermove_s, PM_StuckTouch), "offsetof playermove_s.PM_StuckTouch");
    static_assert(std::is_same_v<t1315, map_t<decltype(::playermove_s::PM_StuckTouch)>>, "type playermove_s.PM_StuckTouch");
    static_assert(o1322 == offsetof(::playermove_s, PM_PointContents), "offsetof playermove_s.PM_PointContents");
    static_assert(std::is_same_v<t1316, map_t<decltype(::playermove_s::PM_PointContents)>>, "type playermove_s.PM_PointContents");
    static_assert(o1323 == offsetof(::playermove_s, PM_TruePointContents), "offsetof playermove_s.PM_TruePointContents");
    static_assert(std::is_same_v<t1317, map_t<decltype(::playermove_s::PM_TruePointContents)>>, "type playermove_s.PM_TruePointContents");
    static_assert(o1324 == offsetof(::playermove_s, PM_HullPointContents), "offsetof playermove_s.PM_HullPointContents");
    static_assert(std::is_same_v<t1318, map_t<decltype(::playermove_s::PM_HullPointContents)>>, "type playermove_s.PM_HullPointContents");
    static_assert(o1325 == offsetof(::playermove_s, PM_PlayerTrace), "offsetof playermove_s.PM_PlayerTrace");
    static_assert(std::is_same_v<t1319, map_t<decltype(::playermove_s::PM_PlayerTrace)>>, "type playermove_s.PM_PlayerTrace");
    static_assert(o1326 == offsetof(::playermove_s, PM_TraceLine), "offsetof playermove_s.PM_TraceLine");
    static_assert(std::is_same_v<t1320, map_t<decltype(::playermove_s::PM_TraceLine)>>, "type playermove_s.PM_TraceLine");
    static_assert(o1327 == offsetof(::playermove_s, RandomLong), "offsetof playermove_s.RandomLong");
    static_assert(std::is_same_v<t1321, map_t<decltype(::playermove_s::RandomLong)>>, "type playermove_s.RandomLong");
    static_assert(o1328 == offsetof(::playermove_s, RandomFloat), "offsetof playermove_s.RandomFloat");
    static_assert(std::is_same_v<t1322, map_t<decltype(::playermove_s::RandomFloat)>>, "type playermove_s.RandomFloat");
    static_assert(o1329 == offsetof(::playermove_s, PM_GetModelType), "offsetof playermove_s.PM_GetModelType");
    static_assert(std::is_same_v<t1323, map_t<decltype(::playermove_s::PM_GetModelType)>>, "type playermove_s.PM_GetModelType");
    static_assert(o1330 == offsetof(::playermove_s, PM_GetModelBounds), "offsetof playermove_s.PM_GetModelBounds");
    static_assert(std::is_same_v<t1324, map_t<decltype(::playermove_s::PM_GetModelBounds)>>, "type playermove_s.PM_GetModelBounds");
    static_assert(o1331 == offsetof(::playermove_s, PM_HullForBsp), "offsetof playermove_s.PM_HullForBsp");
    static_assert(std::is_same_v<t1325, map_t<decltype(::playermove_s::PM_HullForBsp)>>, "type playermove_s.PM_HullForBsp");
    static_assert(o1332 == offsetof(::playermove_s, PM_TraceModel), "offsetof playermove_s.PM_TraceModel");
    static_assert(std::is_same_v<t1326, map_t<decltype(::playermove_s::PM_TraceModel)>>, "type playermove_s.PM_TraceModel");
    static_assert(o1333 == offsetof(::playermove_s, COM_FileSize), "offsetof playermove_s.COM_FileSize");
    static_assert(std::is_same_v<t1327, map_t<decltype(::playermove_s::COM_FileSize)>>, "type playermove_s.COM_FileSize");
    static_assert(o1334 == offsetof(::playermove_s, COM_LoadFile), "offsetof playermove_s.COM_LoadFile");
    static_assert(std::is_same_v<t1328, map_t<decltype(::playermove_s::COM_LoadFile)>>, "type playermove_s.COM_LoadFile");
    static_assert(o1335 == offsetof(::playermove_s, COM_FreeFile), "offsetof playermove_s.COM_FreeFile");
    static_assert(std::is_same_v<t1329, map_t<decltype(::playermove_s::COM_FreeFile)>>, "type playermove_s.COM_FreeFile");
    static_assert(o1336 == offsetof(::playermove_s, memfgets), "offsetof playermove_s.memfgets");
    static_assert(std::is_same_v<t1330, map_t<decltype(::playermove_s::memfgets)>>, "type playermove_s.memfgets");
    static_assert(o1337 == offsetof(::playermove_s, runfuncs), "offsetof playermove_s.runfuncs");
    static_assert(std::is_same_v<t1331, map_t<decltype(::playermove_s::runfuncs)>>, "type playermove_s.runfuncs");
    static_assert(o1338 == offsetof(::playermove_s, PM_PlaySound), "offsetof playermove_s.PM_PlaySound");
    static_assert(std::is_same_v<t1332, map_t<decltype(::playermove_s::PM_PlaySound)>>, "type playermove_s.PM_PlaySound");
    static_assert(o1339 == offsetof(::playermove_s, PM_TraceTexture), "offsetof playermove_s.PM_TraceTexture");
    static_assert(std::is_same_v<t1333, map_t<decltype(::playermove_s::PM_TraceTexture)>>, "type playermove_s.PM_TraceTexture");
    static_assert(o1340 == offsetof(::playermove_s, PM_PlaybackEventFull), "offsetof playermove_s.PM_PlaybackEventFull");
    static_assert(std::is_same_v<t1334, map_t<decltype(::playermove_s::PM_PlaybackEventFull)>>, "type playermove_s.PM_PlaybackEventFull");
    static_assert(o1341 == offsetof(::playermove_s, PM_PlayerTraceEx), "offsetof playermove_s.PM_PlayerTraceEx");
    static_assert(std::is_same_v<t1335, map_t<decltype(::playermove_s::PM_PlayerTraceEx)>>, "type playermove_s.PM_PlayerTraceEx");
    static_assert(o1342 == offsetof(::playermove_s, PM_TestPlayerPositionEx), "offsetof playermove_s.PM_TestPlayerPositionEx");
    static_assert(
        std::is_same_v<t1336, map_t<decltype(::playermove_s::PM_TestPlayerPositionEx)>>,
        "type playermove_s.PM_TestPlayerPositionEx"
    );
    static_assert(o1343 == offsetof(::playermove_s, PM_TraceLineEx), "offsetof playermove_s.PM_TraceLineEx");
    static_assert(std::is_same_v<t1337, map_t<decltype(::playermove_s::PM_TraceLineEx)>>, "type playermove_s.PM_TraceLineEx");
    static_assert(s100 == sizeof(::enginefuncs_s), "sizeof enginefuncs_s");
    static_assert(o1344 == offsetof(::enginefuncs_s, pfnPrecacheModel), "offsetof enginefuncs_s.pfnPrecacheModel");
    static_assert(std::is_same_v<t1338, map_t<decltype(::enginefuncs_s::pfnPrecacheModel)>>, "type enginefuncs_s.pfnPrecacheModel");
    static_assert(o1345 == offsetof(::enginefuncs_s, pfnPrecacheSound), "offsetof enginefuncs_s.pfnPrecacheSound");
    static_assert(std::is_same_v<t1339, map_t<decltype(::enginefuncs_s::pfnPrecacheSound)>>, "type enginefuncs_s.pfnPrecacheSound");
    static_assert(o1346 == offsetof(::enginefuncs_s, pfnSetModel), "offsetof enginefuncs_s.pfnSetModel");
    static_assert(std::is_same_v<t1340, map_t<decltype(::enginefuncs_s::pfnSetModel)>>, "type enginefuncs_s.pfnSetModel");
    static_assert(o1347 == offsetof(::enginefuncs_s, pfnModelIndex), "offsetof enginefuncs_s.pfnModelIndex");
    static_assert(std::is_same_v<t1341, map_t<decltype(::enginefuncs_s::pfnModelIndex)>>, "type enginefuncs_s.pfnModelIndex");
    static_assert(o1348 == offsetof(::enginefuncs_s, pfnModelFrames), "offsetof enginefuncs_s.pfnModelFrames");
    static_assert(std::is_same_v<t1342, map_t<decltype(::enginefuncs_s::pfnModelFrames)>>, "type enginefuncs_s.pfnModelFrames");
    static_assert(o1349 == offsetof(::enginefuncs_s, pfnSetSize), "offsetof enginefuncs_s.pfnSetSize");
    static_assert(std::is_same_v<t1343, map_t<decltype(::enginefuncs_s::pfnSetSize)>>, "type enginefuncs_s.pfnSetSize");
    static_assert(o1350 == offsetof(::enginefuncs_s, pfnChangeLevel), "offsetof enginefuncs_s.pfnChangeLevel");
    static_assert(std::is_same_v<t1344, map_t<decltype(::enginefuncs_s::pfnChangeLevel)>>, "type enginefuncs_s.pfnChangeLevel");
    static_assert(o1351 == offsetof(::enginefuncs_s, pfnGetSpawnParms), "offsetof enginefuncs_s.pfnGetSpawnParms");
    static_assert(std::is_same_v<t1345, map_t<decltype(::enginefuncs_s::pfnGetSpawnParms)>>, "type enginefuncs_s.pfnGetSpawnParms");
    static_assert(o1352 == offsetof(::enginefuncs_s, pfnSaveSpawnParms), "offsetof enginefuncs_s.pfnSaveSpawnParms");
    static_assert(std::is_same_v<t1346, map_t<decltype(::enginefuncs_s::pfnSaveSpawnParms)>>, "type enginefuncs_s.pfnSaveSpawnParms");
    static_assert(o1353 == offsetof(::enginefuncs_s, pfnVecToYaw), "offsetof enginefuncs_s.pfnVecToYaw");
    static_assert(std::is_same_v<t1347, map_t<decltype(::enginefuncs_s::pfnVecToYaw)>>, "type enginefuncs_s.pfnVecToYaw");
    static_assert(o1354 == offsetof(::enginefuncs_s, pfnVecToAngles), "offsetof enginefuncs_s.pfnVecToAngles");
    static_assert(std::is_same_v<t1348, map_t<decltype(::enginefuncs_s::pfnVecToAngles)>>, "type enginefuncs_s.pfnVecToAngles");
    static_assert(o1355 == offsetof(::enginefuncs_s, pfnMoveToOrigin), "offsetof enginefuncs_s.pfnMoveToOrigin");
    static_assert(std::is_same_v<t1349, map_t<decltype(::enginefuncs_s::pfnMoveToOrigin)>>, "type enginefuncs_s.pfnMoveToOrigin");
    static_assert(o1356 == offsetof(::enginefuncs_s, pfnChangeYaw), "offsetof enginefuncs_s.pfnChangeYaw");
    static_assert(std::is_same_v<t1350, map_t<decltype(::enginefuncs_s::pfnChangeYaw)>>, "type enginefuncs_s.pfnChangeYaw");
    static_assert(o1357 == offsetof(::enginefuncs_s, pfnChangePitch), "offsetof enginefuncs_s.pfnChangePitch");
    static_assert(std::is_same_v<t1351, map_t<decltype(::enginefuncs_s::pfnChangePitch)>>, "type enginefuncs_s.pfnChangePitch");
    static_assert(o1358 == offsetof(::enginefuncs_s, pfnFindEntityByString), "offsetof enginefuncs_s.pfnFindEntityByString");
    static_assert(
        std::is_same_v<t1352, map_t<decltype(::enginefuncs_s::pfnFindEntityByString)>>,
        "type enginefuncs_s.pfnFindEntityByString"
    );
    static_assert(o1359 == offsetof(::enginefuncs_s, pfnGetEntityIllum), "offsetof enginefuncs_s.pfnGetEntityIllum");
    static_assert(std::is_same_v<t1353, map_t<decltype(::enginefuncs_s::pfnGetEntityIllum)>>, "type enginefuncs_s.pfnGetEntityIllum");
    static_assert(o1360 == offsetof(::enginefuncs_s, pfnFindEntityInSphere), "offsetof enginefuncs_s.pfnFindEntityInSphere");
    static_assert(
        std::is_same_v<t1354, map_t<decltype(::enginefuncs_s::pfnFindEntityInSphere)>>,
        "type enginefuncs_s.pfnFindEntityInSphere"
    );
    static_assert(o1361 == offsetof(::enginefuncs_s, pfnFindClientInPVS), "offsetof enginefuncs_s.pfnFindClientInPVS");
    static_assert(std::is_same_v<t1355, map_t<decltype(::enginefuncs_s::pfnFindClientInPVS)>>, "type enginefuncs_s.pfnFindClientInPVS");
    static_assert(o1362 == offsetof(::enginefuncs_s, pfnEntitiesInPVS), "offsetof enginefuncs_s.pfnEntitiesInPVS");
    static_assert(std::is_same_v<t1356, map_t<decltype(::enginefuncs_s::pfnEntitiesInPVS)>>, "type enginefuncs_s.pfnEntitiesInPVS");
    static_assert(o1363 == offsetof(::enginefuncs_s, pfnMakeVectors), "offsetof enginefuncs_s.pfnMakeVectors");
    static_assert(std::is_same_v<t1357, map_t<decltype(::enginefuncs_s::pfnMakeVectors)>>, "type enginefuncs_s.pfnMakeVectors");
    static_assert(o1364 == offsetof(::enginefuncs_s, pfnAngleVectors), "offsetof enginefuncs_s.pfnAngleVectors");
    static_assert(std::is_same_v<t1358, map_t<decltype(::enginefuncs_s::pfnAngleVectors)>>, "type enginefuncs_s.pfnAngleVectors");
    static_assert(o1365 == offsetof(::enginefuncs_s, pfnCreateEntity), "offsetof enginefuncs_s.pfnCreateEntity");
    static_assert(std::is_same_v<t1359, map_t<decltype(::enginefuncs_s::pfnCreateEntity)>>, "type enginefuncs_s.pfnCreateEntity");
    static_assert(o1366 == offsetof(::enginefuncs_s, pfnRemoveEntity), "offsetof enginefuncs_s.pfnRemoveEntity");
    static_assert(std::is_same_v<t1360, map_t<decltype(::enginefuncs_s::pfnRemoveEntity)>>, "type enginefuncs_s.pfnRemoveEntity");
    static_assert(o1367 == offsetof(::enginefuncs_s, pfnCreateNamedEntity), "offsetof enginefuncs_s.pfnCreateNamedEntity");
    static_assert(std::is_same_v<t1361, map_t<decltype(::enginefuncs_s::pfnCreateNamedEntity)>>, "type enginefuncs_s.pfnCreateNamedEntity");
    static_assert(o1368 == offsetof(::enginefuncs_s, pfnMakeStatic), "offsetof enginefuncs_s.pfnMakeStatic");
    static_assert(std::is_same_v<t1362, map_t<decltype(::enginefuncs_s::pfnMakeStatic)>>, "type enginefuncs_s.pfnMakeStatic");
    static_assert(o1369 == offsetof(::enginefuncs_s, pfnEntIsOnFloor), "offsetof enginefuncs_s.pfnEntIsOnFloor");
    static_assert(std::is_same_v<t1363, map_t<decltype(::enginefuncs_s::pfnEntIsOnFloor)>>, "type enginefuncs_s.pfnEntIsOnFloor");
    static_assert(o1370 == offsetof(::enginefuncs_s, pfnDropToFloor), "offsetof enginefuncs_s.pfnDropToFloor");
    static_assert(std::is_same_v<t1364, map_t<decltype(::enginefuncs_s::pfnDropToFloor)>>, "type enginefuncs_s.pfnDropToFloor");
    static_assert(o1371 == offsetof(::enginefuncs_s, pfnWalkMove), "offsetof enginefuncs_s.pfnWalkMove");
    static_assert(std::is_same_v<t1365, map_t<decltype(::enginefuncs_s::pfnWalkMove)>>, "type enginefuncs_s.pfnWalkMove");
    static_assert(o1372 == offsetof(::enginefuncs_s, pfnSetOrigin), "offsetof enginefuncs_s.pfnSetOrigin");
    static_assert(std::is_same_v<t1366, map_t<decltype(::enginefuncs_s::pfnSetOrigin)>>, "type enginefuncs_s.pfnSetOrigin");
    static_assert(o1373 == offsetof(::enginefuncs_s, pfnEmitSound), "offsetof enginefuncs_s.pfnEmitSound");
    static_assert(std::is_same_v<t1367, map_t<decltype(::enginefuncs_s::pfnEmitSound)>>, "type enginefuncs_s.pfnEmitSound");
    static_assert(o1374 == offsetof(::enginefuncs_s, pfnEmitAmbientSound), "offsetof enginefuncs_s.pfnEmitAmbientSound");
    static_assert(std::is_same_v<t1368, map_t<decltype(::enginefuncs_s::pfnEmitAmbientSound)>>, "type enginefuncs_s.pfnEmitAmbientSound");
    static_assert(o1375 == offsetof(::enginefuncs_s, pfnTraceLine), "offsetof enginefuncs_s.pfnTraceLine");
    static_assert(std::is_same_v<t1369, map_t<decltype(::enginefuncs_s::pfnTraceLine)>>, "type enginefuncs_s.pfnTraceLine");
    static_assert(o1376 == offsetof(::enginefuncs_s, pfnTraceToss), "offsetof enginefuncs_s.pfnTraceToss");
    static_assert(std::is_same_v<t1370, map_t<decltype(::enginefuncs_s::pfnTraceToss)>>, "type enginefuncs_s.pfnTraceToss");
    static_assert(o1377 == offsetof(::enginefuncs_s, pfnTraceMonsterHull), "offsetof enginefuncs_s.pfnTraceMonsterHull");
    static_assert(std::is_same_v<t1371, map_t<decltype(::enginefuncs_s::pfnTraceMonsterHull)>>, "type enginefuncs_s.pfnTraceMonsterHull");
    static_assert(o1378 == offsetof(::enginefuncs_s, pfnTraceHull), "offsetof enginefuncs_s.pfnTraceHull");
    static_assert(std::is_same_v<t1372, map_t<decltype(::enginefuncs_s::pfnTraceHull)>>, "type enginefuncs_s.pfnTraceHull");
    static_assert(o1379 == offsetof(::enginefuncs_s, pfnTraceModel), "offsetof enginefuncs_s.pfnTraceModel");
    static_assert(std::is_same_v<t1373, map_t<decltype(::enginefuncs_s::pfnTraceModel)>>, "type enginefuncs_s.pfnTraceModel");
    static_assert(o1380 == offsetof(::enginefuncs_s, pfnTraceTexture), "offsetof enginefuncs_s.pfnTraceTexture");
    static_assert(std::is_same_v<t1374, map_t<decltype(::enginefuncs_s::pfnTraceTexture)>>, "type enginefuncs_s.pfnTraceTexture");
    static_assert(o1381 == offsetof(::enginefuncs_s, pfnTraceSphere), "offsetof enginefuncs_s.pfnTraceSphere");
    static_assert(std::is_same_v<t1375, map_t<decltype(::enginefuncs_s::pfnTraceSphere)>>, "type enginefuncs_s.pfnTraceSphere");
    static_assert(o1382 == offsetof(::enginefuncs_s, pfnGetAimVector), "offsetof enginefuncs_s.pfnGetAimVector");
    static_assert(std::is_same_v<t1376, map_t<decltype(::enginefuncs_s::pfnGetAimVector)>>, "type enginefuncs_s.pfnGetAimVector");
    static_assert(o1383 == offsetof(::enginefuncs_s, pfnServerCommand), "offsetof enginefuncs_s.pfnServerCommand");
    static_assert(std::is_same_v<t1377, map_t<decltype(::enginefuncs_s::pfnServerCommand)>>, "type enginefuncs_s.pfnServerCommand");
    static_assert(o1384 == offsetof(::enginefuncs_s, pfnServerExecute), "offsetof enginefuncs_s.pfnServerExecute");
    static_assert(std::is_same_v<t1378, map_t<decltype(::enginefuncs_s::pfnServerExecute)>>, "type enginefuncs_s.pfnServerExecute");
    static_assert(o1385 == offsetof(::enginefuncs_s, pfnClientCommand), "offsetof enginefuncs_s.pfnClientCommand");
    static_assert(std::is_same_v<t1379, map_t<decltype(::enginefuncs_s::pfnClientCommand)>>, "type enginefuncs_s.pfnClientCommand");
    static_assert(o1386 == offsetof(::enginefuncs_s, pfnParticleEffect), "offsetof enginefuncs_s.pfnParticleEffect");
    static_assert(std::is_same_v<t1380, map_t<decltype(::enginefuncs_s::pfnParticleEffect)>>, "type enginefuncs_s.pfnParticleEffect");
    static_assert(o1387 == offsetof(::enginefuncs_s, pfnLightStyle), "offsetof enginefuncs_s.pfnLightStyle");
    static_assert(std::is_same_v<t1381, map_t<decltype(::enginefuncs_s::pfnLightStyle)>>, "type enginefuncs_s.pfnLightStyle");
    static_assert(o1388 == offsetof(::enginefuncs_s, pfnDecalIndex), "offsetof enginefuncs_s.pfnDecalIndex");
    static_assert(std::is_same_v<t1382, map_t<decltype(::enginefuncs_s::pfnDecalIndex)>>, "type enginefuncs_s.pfnDecalIndex");
    static_assert(o1389 == offsetof(::enginefuncs_s, pfnPointContents), "offsetof enginefuncs_s.pfnPointContents");
    static_assert(std::is_same_v<t1383, map_t<decltype(::enginefuncs_s::pfnPointContents)>>, "type enginefuncs_s.pfnPointContents");
    static_assert(o1390 == offsetof(::enginefuncs_s, pfnMessageBegin), "offsetof enginefuncs_s.pfnMessageBegin");
    static_assert(std::is_same_v<t1384, map_t<decltype(::enginefuncs_s::pfnMessageBegin)>>, "type enginefuncs_s.pfnMessageBegin");
    static_assert(o1391 == offsetof(::enginefuncs_s, pfnMessageEnd), "offsetof enginefuncs_s.pfnMessageEnd");
    static_assert(std::is_same_v<t1385, map_t<decltype(::enginefuncs_s::pfnMessageEnd)>>, "type enginefuncs_s.pfnMessageEnd");
    static_assert(o1392 == offsetof(::enginefuncs_s, pfnWriteByte), "offsetof enginefuncs_s.pfnWriteByte");
    static_assert(std::is_same_v<t1386, map_t<decltype(::enginefuncs_s::pfnWriteByte)>>, "type enginefuncs_s.pfnWriteByte");
    static_assert(o1393 == offsetof(::enginefuncs_s, pfnWriteChar), "offsetof enginefuncs_s.pfnWriteChar");
    static_assert(std::is_same_v<t1387, map_t<decltype(::enginefuncs_s::pfnWriteChar)>>, "type enginefuncs_s.pfnWriteChar");
    static_assert(o1394 == offsetof(::enginefuncs_s, pfnWriteShort), "offsetof enginefuncs_s.pfnWriteShort");
    static_assert(std::is_same_v<t1388, map_t<decltype(::enginefuncs_s::pfnWriteShort)>>, "type enginefuncs_s.pfnWriteShort");
    static_assert(o1395 == offsetof(::enginefuncs_s, pfnWriteLong), "offsetof enginefuncs_s.pfnWriteLong");
    static_assert(std::is_same_v<t1389, map_t<decltype(::enginefuncs_s::pfnWriteLong)>>, "type enginefuncs_s.pfnWriteLong");
    static_assert(o1396 == offsetof(::enginefuncs_s, pfnWriteAngle), "offsetof enginefuncs_s.pfnWriteAngle");
    static_assert(std::is_same_v<t1390, map_t<decltype(::enginefuncs_s::pfnWriteAngle)>>, "type enginefuncs_s.pfnWriteAngle");
    static_assert(o1397 == offsetof(::enginefuncs_s, pfnWriteCoord), "offsetof enginefuncs_s.pfnWriteCoord");
    static_assert(std::is_same_v<t1391, map_t<decltype(::enginefuncs_s::pfnWriteCoord)>>, "type enginefuncs_s.pfnWriteCoord");
    static_assert(o1398 == offsetof(::enginefuncs_s, pfnWriteString), "offsetof enginefuncs_s.pfnWriteString");
    static_assert(std::is_same_v<t1392, map_t<decltype(::enginefuncs_s::pfnWriteString)>>, "type enginefuncs_s.pfnWriteString");
    static_assert(o1399 == offsetof(::enginefuncs_s, pfnWriteEntity), "offsetof enginefuncs_s.pfnWriteEntity");
    static_assert(std::is_same_v<t1393, map_t<decltype(::enginefuncs_s::pfnWriteEntity)>>, "type enginefuncs_s.pfnWriteEntity");
    static_assert(o1400 == offsetof(::enginefuncs_s, pfnCVarRegister), "offsetof enginefuncs_s.pfnCVarRegister");
    static_assert(std::is_same_v<t1394, map_t<decltype(::enginefuncs_s::pfnCVarRegister)>>, "type enginefuncs_s.pfnCVarRegister");
    static_assert(o1401 == offsetof(::enginefuncs_s, pfnCVarGetFloat), "offsetof enginefuncs_s.pfnCVarGetFloat");
    static_assert(std::is_same_v<t1395, map_t<decltype(::enginefuncs_s::pfnCVarGetFloat)>>, "type enginefuncs_s.pfnCVarGetFloat");
    static_assert(o1402 == offsetof(::enginefuncs_s, pfnCVarGetString), "offsetof enginefuncs_s.pfnCVarGetString");
    static_assert(std::is_same_v<t1396, map_t<decltype(::enginefuncs_s::pfnCVarGetString)>>, "type enginefuncs_s.pfnCVarGetString");
    static_assert(o1403 == offsetof(::enginefuncs_s, pfnCVarSetFloat), "offsetof enginefuncs_s.pfnCVarSetFloat");
    static_assert(std::is_same_v<t1397, map_t<decltype(::enginefuncs_s::pfnCVarSetFloat)>>, "type enginefuncs_s.pfnCVarSetFloat");
    static_assert(o1404 == offsetof(::enginefuncs_s, pfnCVarSetString), "offsetof enginefuncs_s.pfnCVarSetString");
    static_assert(std::is_same_v<t1398, map_t<decltype(::enginefuncs_s::pfnCVarSetString)>>, "type enginefuncs_s.pfnCVarSetString");
    static_assert(o1405 == offsetof(::enginefuncs_s, pfnAlertMessage), "offsetof enginefuncs_s.pfnAlertMessage");
    static_assert(std::is_same_v<t1399, map_t<decltype(::enginefuncs_s::pfnAlertMessage)>>, "type enginefuncs_s.pfnAlertMessage");
    static_assert(o1406 == offsetof(::enginefuncs_s, pfnEngineFprintf), "offsetof enginefuncs_s.pfnEngineFprintf");
    static_assert(std::is_same_v<t1400, map_t<decltype(::enginefuncs_s::pfnEngineFprintf)>>, "type enginefuncs_s.pfnEngineFprintf");
    static_assert(o1407 == offsetof(::enginefuncs_s, pfnPvAllocEntPrivateData), "offsetof enginefuncs_s.pfnPvAllocEntPrivateData");
    static_assert(
        std::is_same_v<t1401, map_t<decltype(::enginefuncs_s::pfnPvAllocEntPrivateData)>>,
        "type enginefuncs_s.pfnPvAllocEntPrivateData"
    );
    static_assert(o1408 == offsetof(::enginefuncs_s, pfnPvEntPrivateData), "offsetof enginefuncs_s.pfnPvEntPrivateData");
    static_assert(std::is_same_v<t1402, map_t<decltype(::enginefuncs_s::pfnPvEntPrivateData)>>, "type enginefuncs_s.pfnPvEntPrivateData");
    static_assert(o1409 == offsetof(::enginefuncs_s, pfnFreeEntPrivateData), "offsetof enginefuncs_s.pfnFreeEntPrivateData");
    static_assert(
        std::is_same_v<t1403, map_t<decltype(::enginefuncs_s::pfnFreeEntPrivateData)>>,
        "type enginefuncs_s.pfnFreeEntPrivateData"
    );
    static_assert(o1410 == offsetof(::enginefuncs_s, pfnSzFromIndex), "offsetof enginefuncs_s.pfnSzFromIndex");
    static_assert(std::is_same_v<t1404, map_t<decltype(::enginefuncs_s::pfnSzFromIndex)>>, "type enginefuncs_s.pfnSzFromIndex");
    static_assert(o1411 == offsetof(::enginefuncs_s, pfnAllocString), "offsetof enginefuncs_s.pfnAllocString");
    static_assert(std::is_same_v<t1405, map_t<decltype(::enginefuncs_s::pfnAllocString)>>, "type enginefuncs_s.pfnAllocString");
    static_assert(o1412 == offsetof(::enginefuncs_s, pfnGetVarsOfEnt), "offsetof enginefuncs_s.pfnGetVarsOfEnt");
    static_assert(std::is_same_v<t1406, map_t<decltype(::enginefuncs_s::pfnGetVarsOfEnt)>>, "type enginefuncs_s.pfnGetVarsOfEnt");
    static_assert(o1413 == offsetof(::enginefuncs_s, pfnPEntityOfEntOffset), "offsetof enginefuncs_s.pfnPEntityOfEntOffset");
    static_assert(
        std::is_same_v<t1407, map_t<decltype(::enginefuncs_s::pfnPEntityOfEntOffset)>>,
        "type enginefuncs_s.pfnPEntityOfEntOffset"
    );
    static_assert(o1414 == offsetof(::enginefuncs_s, pfnEntOffsetOfPEntity), "offsetof enginefuncs_s.pfnEntOffsetOfPEntity");
    static_assert(
        std::is_same_v<t1408, map_t<decltype(::enginefuncs_s::pfnEntOffsetOfPEntity)>>,
        "type enginefuncs_s.pfnEntOffsetOfPEntity"
    );
    static_assert(o1415 == offsetof(::enginefuncs_s, pfnIndexOfEdict), "offsetof enginefuncs_s.pfnIndexOfEdict");
    static_assert(std::is_same_v<t1409, map_t<decltype(::enginefuncs_s::pfnIndexOfEdict)>>, "type enginefuncs_s.pfnIndexOfEdict");
    static_assert(o1416 == offsetof(::enginefuncs_s, pfnPEntityOfEntIndex), "offsetof enginefuncs_s.pfnPEntityOfEntIndex");
    static_assert(std::is_same_v<t1410, map_t<decltype(::enginefuncs_s::pfnPEntityOfEntIndex)>>, "type enginefuncs_s.pfnPEntityOfEntIndex");
    static_assert(o1417 == offsetof(::enginefuncs_s, pfnFindEntityByVars), "offsetof enginefuncs_s.pfnFindEntityByVars");
    static_assert(std::is_same_v<t1411, map_t<decltype(::enginefuncs_s::pfnFindEntityByVars)>>, "type enginefuncs_s.pfnFindEntityByVars");
    static_assert(o1418 == offsetof(::enginefuncs_s, pfnGetModelPtr), "offsetof enginefuncs_s.pfnGetModelPtr");
    static_assert(std::is_same_v<t1412, map_t<decltype(::enginefuncs_s::pfnGetModelPtr)>>, "type enginefuncs_s.pfnGetModelPtr");
    static_assert(o1419 == offsetof(::enginefuncs_s, pfnRegUserMsg), "offsetof enginefuncs_s.pfnRegUserMsg");
    static_assert(std::is_same_v<t1413, map_t<decltype(::enginefuncs_s::pfnRegUserMsg)>>, "type enginefuncs_s.pfnRegUserMsg");
    static_assert(o1420 == offsetof(::enginefuncs_s, pfnAnimationAutomove), "offsetof enginefuncs_s.pfnAnimationAutomove");
    static_assert(std::is_same_v<t1414, map_t<decltype(::enginefuncs_s::pfnAnimationAutomove)>>, "type enginefuncs_s.pfnAnimationAutomove");
    static_assert(o1421 == offsetof(::enginefuncs_s, pfnGetBonePosition), "offsetof enginefuncs_s.pfnGetBonePosition");
    static_assert(std::is_same_v<t1415, map_t<decltype(::enginefuncs_s::pfnGetBonePosition)>>, "type enginefuncs_s.pfnGetBonePosition");
    static_assert(o1422 == offsetof(::enginefuncs_s, pfnFunctionFromName), "offsetof enginefuncs_s.pfnFunctionFromName");
    static_assert(std::is_same_v<t1416, map_t<decltype(::enginefuncs_s::pfnFunctionFromName)>>, "type enginefuncs_s.pfnFunctionFromName");
    static_assert(o1423 == offsetof(::enginefuncs_s, pfnNameForFunction), "offsetof enginefuncs_s.pfnNameForFunction");
    static_assert(std::is_same_v<t1417, map_t<decltype(::enginefuncs_s::pfnNameForFunction)>>, "type enginefuncs_s.pfnNameForFunction");
    static_assert(o1424 == offsetof(::enginefuncs_s, pfnClientPrintf), "offsetof enginefuncs_s.pfnClientPrintf");
    static_assert(std::is_same_v<t1418, map_t<decltype(::enginefuncs_s::pfnClientPrintf)>>, "type enginefuncs_s.pfnClientPrintf");
    static_assert(o1425 == offsetof(::enginefuncs_s, pfnServerPrint), "offsetof enginefuncs_s.pfnServerPrint");
    static_assert(std::is_same_v<t1419, map_t<decltype(::enginefuncs_s::pfnServerPrint)>>, "type enginefuncs_s.pfnServerPrint");
    static_assert(o1426 == offsetof(::enginefuncs_s, pfnCmd_Args), "offsetof enginefuncs_s.pfnCmd_Args");
    static_assert(std::is_same_v<t1420, map_t<decltype(::enginefuncs_s::pfnCmd_Args)>>, "type enginefuncs_s.pfnCmd_Args");
    static_assert(o1427 == offsetof(::enginefuncs_s, pfnCmd_Argv), "offsetof enginefuncs_s.pfnCmd_Argv");
    static_assert(std::is_same_v<t1421, map_t<decltype(::enginefuncs_s::pfnCmd_Argv)>>, "type enginefuncs_s.pfnCmd_Argv");
    static_assert(o1428 == offsetof(::enginefuncs_s, pfnCmd_Argc), "offsetof enginefuncs_s.pfnCmd_Argc");
    static_assert(std::is_same_v<t1422, map_t<decltype(::enginefuncs_s::pfnCmd_Argc)>>, "type enginefuncs_s.pfnCmd_Argc");
    static_assert(o1429 == offsetof(::enginefuncs_s, pfnGetAttachment), "offsetof enginefuncs_s.pfnGetAttachment");
    static_assert(std::is_same_v<t1423, map_t<decltype(::enginefuncs_s::pfnGetAttachment)>>, "type enginefuncs_s.pfnGetAttachment");
    static_assert(o1430 == offsetof(::enginefuncs_s, pfnCRC32_Init), "offsetof enginefuncs_s.pfnCRC32_Init");
    static_assert(std::is_same_v<t1424, map_t<decltype(::enginefuncs_s::pfnCRC32_Init)>>, "type enginefuncs_s.pfnCRC32_Init");
    static_assert(o1431 == offsetof(::enginefuncs_s, pfnCRC32_ProcessBuffer), "offsetof enginefuncs_s.pfnCRC32_ProcessBuffer");
    static_assert(
        std::is_same_v<t1425, map_t<decltype(::enginefuncs_s::pfnCRC32_ProcessBuffer)>>,
        "type enginefuncs_s.pfnCRC32_ProcessBuffer"
    );
    static_assert(o1432 == offsetof(::enginefuncs_s, pfnCRC32_ProcessByte), "offsetof enginefuncs_s.pfnCRC32_ProcessByte");
    static_assert(std::is_same_v<t1426, map_t<decltype(::enginefuncs_s::pfnCRC32_ProcessByte)>>, "type enginefuncs_s.pfnCRC32_ProcessByte");
    static_assert(o1433 == offsetof(::enginefuncs_s, pfnCRC32_Final), "offsetof enginefuncs_s.pfnCRC32_Final");
    static_assert(std::is_same_v<t1427, map_t<decltype(::enginefuncs_s::pfnCRC32_Final)>>, "type enginefuncs_s.pfnCRC32_Final");
    static_assert(o1434 == offsetof(::enginefuncs_s, pfnRandomLong), "offsetof enginefuncs_s.pfnRandomLong");
    static_assert(std::is_same_v<t1428, map_t<decltype(::enginefuncs_s::pfnRandomLong)>>, "type enginefuncs_s.pfnRandomLong");
    static_assert(o1435 == offsetof(::enginefuncs_s, pfnRandomFloat), "offsetof enginefuncs_s.pfnRandomFloat");
    static_assert(std::is_same_v<t1429, map_t<decltype(::enginefuncs_s::pfnRandomFloat)>>, "type enginefuncs_s.pfnRandomFloat");
    static_assert(o1436 == offsetof(::enginefuncs_s, pfnSetView), "offsetof enginefuncs_s.pfnSetView");
    static_assert(std::is_same_v<t1430, map_t<decltype(::enginefuncs_s::pfnSetView)>>, "type enginefuncs_s.pfnSetView");
    static_assert(o1437 == offsetof(::enginefuncs_s, pfnTime), "offsetof enginefuncs_s.pfnTime");
    static_assert(std::is_same_v<t1431, map_t<decltype(::enginefuncs_s::pfnTime)>>, "type enginefuncs_s.pfnTime");
    static_assert(o1438 == offsetof(::enginefuncs_s, pfnCrosshairAngle), "offsetof enginefuncs_s.pfnCrosshairAngle");
    static_assert(std::is_same_v<t1432, map_t<decltype(::enginefuncs_s::pfnCrosshairAngle)>>, "type enginefuncs_s.pfnCrosshairAngle");
    static_assert(o1439 == offsetof(::enginefuncs_s, pfnLoadFileForMe), "offsetof enginefuncs_s.pfnLoadFileForMe");
    static_assert(std::is_same_v<t1433, map_t<decltype(::enginefuncs_s::pfnLoadFileForMe)>>, "type enginefuncs_s.pfnLoadFileForMe");
    static_assert(o1440 == offsetof(::enginefuncs_s, pfnFreeFile), "offsetof enginefuncs_s.pfnFreeFile");
    static_assert(std::is_same_v<t1434, map_t<decltype(::enginefuncs_s::pfnFreeFile)>>, "type enginefuncs_s.pfnFreeFile");
    static_assert(o1441 == offsetof(::enginefuncs_s, pfnEndSection), "offsetof enginefuncs_s.pfnEndSection");
    static_assert(std::is_same_v<t1435, map_t<decltype(::enginefuncs_s::pfnEndSection)>>, "type enginefuncs_s.pfnEndSection");
    static_assert(o1442 == offsetof(::enginefuncs_s, pfnCompareFileTime), "offsetof enginefuncs_s.pfnCompareFileTime");
    static_assert(std::is_same_v<t1436, map_t<decltype(::enginefuncs_s::pfnCompareFileTime)>>, "type enginefuncs_s.pfnCompareFileTime");
    static_assert(o1443 == offsetof(::enginefuncs_s, pfnGetGameDir), "offsetof enginefuncs_s.pfnGetGameDir");
    static_assert(std::is_same_v<t1437, map_t<decltype(::enginefuncs_s::pfnGetGameDir)>>, "type enginefuncs_s.pfnGetGameDir");
    static_assert(o1444 == offsetof(::enginefuncs_s, pfnCvar_RegisterVariable), "offsetof enginefuncs_s.pfnCvar_RegisterVariable");
    static_assert(
        std::is_same_v<t1438, map_t<decltype(::enginefuncs_s::pfnCvar_RegisterVariable)>>,
        "type enginefuncs_s.pfnCvar_RegisterVariable"
    );
    static_assert(o1445 == offsetof(::enginefuncs_s, pfnFadeClientVolume), "offsetof enginefuncs_s.pfnFadeClientVolume");
    static_assert(std::is_same_v<t1439, map_t<decltype(::enginefuncs_s::pfnFadeClientVolume)>>, "type enginefuncs_s.pfnFadeClientVolume");
    static_assert(o1446 == offsetof(::enginefuncs_s, pfnSetClientMaxspeed), "offsetof enginefuncs_s.pfnSetClientMaxspeed");
    static_assert(std::is_same_v<t1440, map_t<decltype(::enginefuncs_s::pfnSetClientMaxspeed)>>, "type enginefuncs_s.pfnSetClientMaxspeed");
    static_assert(o1447 == offsetof(::enginefuncs_s, pfnCreateFakeClient), "offsetof enginefuncs_s.pfnCreateFakeClient");
    static_assert(std::is_same_v<t1441, map_t<decltype(::enginefuncs_s::pfnCreateFakeClient)>>, "type enginefuncs_s.pfnCreateFakeClient");
    static_assert(o1448 == offsetof(::enginefuncs_s, pfnRunPlayerMove), "offsetof enginefuncs_s.pfnRunPlayerMove");
    static_assert(std::is_same_v<t1442, map_t<decltype(::enginefuncs_s::pfnRunPlayerMove)>>, "type enginefuncs_s.pfnRunPlayerMove");
    static_assert(o1449 == offsetof(::enginefuncs_s, pfnNumberOfEntities), "offsetof enginefuncs_s.pfnNumberOfEntities");
    static_assert(std::is_same_v<t1443, map_t<decltype(::enginefuncs_s::pfnNumberOfEntities)>>, "type enginefuncs_s.pfnNumberOfEntities");
    static_assert(o1450 == offsetof(::enginefuncs_s, pfnGetInfoKeyBuffer), "offsetof enginefuncs_s.pfnGetInfoKeyBuffer");
    static_assert(std::is_same_v<t1444, map_t<decltype(::enginefuncs_s::pfnGetInfoKeyBuffer)>>, "type enginefuncs_s.pfnGetInfoKeyBuffer");
    static_assert(o1451 == offsetof(::enginefuncs_s, pfnInfoKeyValue), "offsetof enginefuncs_s.pfnInfoKeyValue");
    static_assert(std::is_same_v<t1445, map_t<decltype(::enginefuncs_s::pfnInfoKeyValue)>>, "type enginefuncs_s.pfnInfoKeyValue");
    static_assert(o1452 == offsetof(::enginefuncs_s, pfnSetKeyValue), "offsetof enginefuncs_s.pfnSetKeyValue");
    static_assert(std::is_same_v<t1446, map_t<decltype(::enginefuncs_s::pfnSetKeyValue)>>, "type enginefuncs_s.pfnSetKeyValue");
    static_assert(o1453 == offsetof(::enginefuncs_s, pfnSetClientKeyValue), "offsetof enginefuncs_s.pfnSetClientKeyValue");
    static_assert(std::is_same_v<t1447, map_t<decltype(::enginefuncs_s::pfnSetClientKeyValue)>>, "type enginefuncs_s.pfnSetClientKeyValue");
    static_assert(o1454 == offsetof(::enginefuncs_s, pfnIsMapValid), "offsetof enginefuncs_s.pfnIsMapValid");
    static_assert(std::is_same_v<t1448, map_t<decltype(::enginefuncs_s::pfnIsMapValid)>>, "type enginefuncs_s.pfnIsMapValid");
    static_assert(o1455 == offsetof(::enginefuncs_s, pfnStaticDecal), "offsetof enginefuncs_s.pfnStaticDecal");
    static_assert(std::is_same_v<t1449, map_t<decltype(::enginefuncs_s::pfnStaticDecal)>>, "type enginefuncs_s.pfnStaticDecal");
    static_assert(o1456 == offsetof(::enginefuncs_s, pfnPrecacheGeneric), "offsetof enginefuncs_s.pfnPrecacheGeneric");
    static_assert(std::is_same_v<t1450, map_t<decltype(::enginefuncs_s::pfnPrecacheGeneric)>>, "type enginefuncs_s.pfnPrecacheGeneric");
    static_assert(o1457 == offsetof(::enginefuncs_s, pfnGetPlayerUserId), "offsetof enginefuncs_s.pfnGetPlayerUserId");
    static_assert(std::is_same_v<t1451, map_t<decltype(::enginefuncs_s::pfnGetPlayerUserId)>>, "type enginefuncs_s.pfnGetPlayerUserId");
    static_assert(o1458 == offsetof(::enginefuncs_s, pfnBuildSoundMsg), "offsetof enginefuncs_s.pfnBuildSoundMsg");
    static_assert(std::is_same_v<t1452, map_t<decltype(::enginefuncs_s::pfnBuildSoundMsg)>>, "type enginefuncs_s.pfnBuildSoundMsg");
    static_assert(o1459 == offsetof(::enginefuncs_s, pfnIsDedicatedServer), "offsetof enginefuncs_s.pfnIsDedicatedServer");
    static_assert(std::is_same_v<t1453, map_t<decltype(::enginefuncs_s::pfnIsDedicatedServer)>>, "type enginefuncs_s.pfnIsDedicatedServer");
    static_assert(o1460 == offsetof(::enginefuncs_s, pfnCVarGetPointer), "offsetof enginefuncs_s.pfnCVarGetPointer");
    static_assert(std::is_same_v<t1454, map_t<decltype(::enginefuncs_s::pfnCVarGetPointer)>>, "type enginefuncs_s.pfnCVarGetPointer");
    static_assert(o1461 == offsetof(::enginefuncs_s, pfnGetPlayerWONId), "offsetof enginefuncs_s.pfnGetPlayerWONId");
    static_assert(std::is_same_v<t1455, map_t<decltype(::enginefuncs_s::pfnGetPlayerWONId)>>, "type enginefuncs_s.pfnGetPlayerWONId");
    static_assert(o1462 == offsetof(::enginefuncs_s, pfnInfo_RemoveKey), "offsetof enginefuncs_s.pfnInfo_RemoveKey");
    static_assert(std::is_same_v<t1456, map_t<decltype(::enginefuncs_s::pfnInfo_RemoveKey)>>, "type enginefuncs_s.pfnInfo_RemoveKey");
    static_assert(o1463 == offsetof(::enginefuncs_s, pfnGetPhysicsKeyValue), "offsetof enginefuncs_s.pfnGetPhysicsKeyValue");
    static_assert(
        std::is_same_v<t1457, map_t<decltype(::enginefuncs_s::pfnGetPhysicsKeyValue)>>,
        "type enginefuncs_s.pfnGetPhysicsKeyValue"
    );
    static_assert(o1464 == offsetof(::enginefuncs_s, pfnSetPhysicsKeyValue), "offsetof enginefuncs_s.pfnSetPhysicsKeyValue");
    static_assert(
        std::is_same_v<t1458, map_t<decltype(::enginefuncs_s::pfnSetPhysicsKeyValue)>>,
        "type enginefuncs_s.pfnSetPhysicsKeyValue"
    );
    static_assert(o1465 == offsetof(::enginefuncs_s, pfnGetPhysicsInfoString), "offsetof enginefuncs_s.pfnGetPhysicsInfoString");
    static_assert(
        std::is_same_v<t1459, map_t<decltype(::enginefuncs_s::pfnGetPhysicsInfoString)>>,
        "type enginefuncs_s.pfnGetPhysicsInfoString"
    );
    static_assert(o1466 == offsetof(::enginefuncs_s, pfnPrecacheEvent), "offsetof enginefuncs_s.pfnPrecacheEvent");
    static_assert(std::is_same_v<t1460, map_t<decltype(::enginefuncs_s::pfnPrecacheEvent)>>, "type enginefuncs_s.pfnPrecacheEvent");
    static_assert(o1467 == offsetof(::enginefuncs_s, pfnPlaybackEvent), "offsetof enginefuncs_s.pfnPlaybackEvent");
    static_assert(std::is_same_v<t1461, map_t<decltype(::enginefuncs_s::pfnPlaybackEvent)>>, "type enginefuncs_s.pfnPlaybackEvent");
    static_assert(o1468 == offsetof(::enginefuncs_s, pfnSetFatPVS), "offsetof enginefuncs_s.pfnSetFatPVS");
    static_assert(std::is_same_v<t1462, map_t<decltype(::enginefuncs_s::pfnSetFatPVS)>>, "type enginefuncs_s.pfnSetFatPVS");
    static_assert(o1469 == offsetof(::enginefuncs_s, pfnSetFatPAS), "offsetof enginefuncs_s.pfnSetFatPAS");
    static_assert(std::is_same_v<t1463, map_t<decltype(::enginefuncs_s::pfnSetFatPAS)>>, "type enginefuncs_s.pfnSetFatPAS");
    static_assert(o1470 == offsetof(::enginefuncs_s, pfnCheckVisibility), "offsetof enginefuncs_s.pfnCheckVisibility");
    static_assert(std::is_same_v<t1464, map_t<decltype(::enginefuncs_s::pfnCheckVisibility)>>, "type enginefuncs_s.pfnCheckVisibility");
    static_assert(o1471 == offsetof(::enginefuncs_s, pfnDeltaSetField), "offsetof enginefuncs_s.pfnDeltaSetField");
    static_assert(std::is_same_v<t1465, map_t<decltype(::enginefuncs_s::pfnDeltaSetField)>>, "type enginefuncs_s.pfnDeltaSetField");
    static_assert(o1472 == offsetof(::enginefuncs_s, pfnDeltaUnsetField), "offsetof enginefuncs_s.pfnDeltaUnsetField");
    static_assert(std::is_same_v<t1466, map_t<decltype(::enginefuncs_s::pfnDeltaUnsetField)>>, "type enginefuncs_s.pfnDeltaUnsetField");
    static_assert(o1473 == offsetof(::enginefuncs_s, pfnDeltaAddEncoder), "offsetof enginefuncs_s.pfnDeltaAddEncoder");
    static_assert(std::is_same_v<t1467, map_t<decltype(::enginefuncs_s::pfnDeltaAddEncoder)>>, "type enginefuncs_s.pfnDeltaAddEncoder");
    static_assert(o1474 == offsetof(::enginefuncs_s, pfnGetCurrentPlayer), "offsetof enginefuncs_s.pfnGetCurrentPlayer");
    static_assert(std::is_same_v<t1468, map_t<decltype(::enginefuncs_s::pfnGetCurrentPlayer)>>, "type enginefuncs_s.pfnGetCurrentPlayer");
    static_assert(o1475 == offsetof(::enginefuncs_s, pfnCanSkipPlayer), "offsetof enginefuncs_s.pfnCanSkipPlayer");
    static_assert(std::is_same_v<t1469, map_t<decltype(::enginefuncs_s::pfnCanSkipPlayer)>>, "type enginefuncs_s.pfnCanSkipPlayer");
    static_assert(o1476 == offsetof(::enginefuncs_s, pfnDeltaFindField), "offsetof enginefuncs_s.pfnDeltaFindField");
    static_assert(std::is_same_v<t1470, map_t<decltype(::enginefuncs_s::pfnDeltaFindField)>>, "type enginefuncs_s.pfnDeltaFindField");
    static_assert(o1477 == offsetof(::enginefuncs_s, pfnDeltaSetFieldByIndex), "offsetof enginefuncs_s.pfnDeltaSetFieldByIndex");
    static_assert(
        std::is_same_v<t1471, map_t<decltype(::enginefuncs_s::pfnDeltaSetFieldByIndex)>>,
        "type enginefuncs_s.pfnDeltaSetFieldByIndex"
    );
    static_assert(o1478 == offsetof(::enginefuncs_s, pfnDeltaUnsetFieldByIndex), "offsetof enginefuncs_s.pfnDeltaUnsetFieldByIndex");
    static_assert(
        std::is_same_v<t1472, map_t<decltype(::enginefuncs_s::pfnDeltaUnsetFieldByIndex)>>,
        "type enginefuncs_s.pfnDeltaUnsetFieldByIndex"
    );
    static_assert(o1479 == offsetof(::enginefuncs_s, pfnSetGroupMask), "offsetof enginefuncs_s.pfnSetGroupMask");
    static_assert(std::is_same_v<t1473, map_t<decltype(::enginefuncs_s::pfnSetGroupMask)>>, "type enginefuncs_s.pfnSetGroupMask");
    static_assert(o1480 == offsetof(::enginefuncs_s, pfnCreateInstancedBaseline), "offsetof enginefuncs_s.pfnCreateInstancedBaseline");
    static_assert(
        std::is_same_v<t1474, map_t<decltype(::enginefuncs_s::pfnCreateInstancedBaseline)>>,
        "type enginefuncs_s.pfnCreateInstancedBaseline"
    );
    static_assert(o1481 == offsetof(::enginefuncs_s, pfnCvar_DirectSet), "offsetof enginefuncs_s.pfnCvar_DirectSet");
    static_assert(std::is_same_v<t1475, map_t<decltype(::enginefuncs_s::pfnCvar_DirectSet)>>, "type enginefuncs_s.pfnCvar_DirectSet");
    static_assert(o1482 == offsetof(::enginefuncs_s, pfnForceUnmodified), "offsetof enginefuncs_s.pfnForceUnmodified");
    static_assert(std::is_same_v<t1476, map_t<decltype(::enginefuncs_s::pfnForceUnmodified)>>, "type enginefuncs_s.pfnForceUnmodified");
    static_assert(o1483 == offsetof(::enginefuncs_s, pfnGetPlayerStats), "offsetof enginefuncs_s.pfnGetPlayerStats");
    static_assert(std::is_same_v<t1477, map_t<decltype(::enginefuncs_s::pfnGetPlayerStats)>>, "type enginefuncs_s.pfnGetPlayerStats");
    static_assert(o1484 == offsetof(::enginefuncs_s, pfnAddServerCommand), "offsetof enginefuncs_s.pfnAddServerCommand");
    static_assert(std::is_same_v<t1478, map_t<decltype(::enginefuncs_s::pfnAddServerCommand)>>, "type enginefuncs_s.pfnAddServerCommand");
    static_assert(o1485 == offsetof(::enginefuncs_s, pfnVoice_GetClientListening), "offsetof enginefuncs_s.pfnVoice_GetClientListening");
    static_assert(
        std::is_same_v<t1479, map_t<decltype(::enginefuncs_s::pfnVoice_GetClientListening)>>,
        "type enginefuncs_s.pfnVoice_GetClientListening"
    );
    static_assert(o1486 == offsetof(::enginefuncs_s, pfnVoice_SetClientListening), "offsetof enginefuncs_s.pfnVoice_SetClientListening");
    static_assert(
        std::is_same_v<t1480, map_t<decltype(::enginefuncs_s::pfnVoice_SetClientListening)>>,
        "type enginefuncs_s.pfnVoice_SetClientListening"
    );
    static_assert(o1487 == offsetof(::enginefuncs_s, pfnGetPlayerAuthId), "offsetof enginefuncs_s.pfnGetPlayerAuthId");
    static_assert(std::is_same_v<t1481, map_t<decltype(::enginefuncs_s::pfnGetPlayerAuthId)>>, "type enginefuncs_s.pfnGetPlayerAuthId");
    static_assert(o1488 == offsetof(::enginefuncs_s, pfnSequenceGet), "offsetof enginefuncs_s.pfnSequenceGet");
    static_assert(std::is_same_v<t1482, map_t<decltype(::enginefuncs_s::pfnSequenceGet)>>, "type enginefuncs_s.pfnSequenceGet");
    static_assert(o1489 == offsetof(::enginefuncs_s, pfnSequencePickSentence), "offsetof enginefuncs_s.pfnSequencePickSentence");
    static_assert(
        std::is_same_v<t1483, map_t<decltype(::enginefuncs_s::pfnSequencePickSentence)>>,
        "type enginefuncs_s.pfnSequencePickSentence"
    );
    static_assert(o1490 == offsetof(::enginefuncs_s, pfnGetFileSize), "offsetof enginefuncs_s.pfnGetFileSize");
    static_assert(std::is_same_v<t1484, map_t<decltype(::enginefuncs_s::pfnGetFileSize)>>, "type enginefuncs_s.pfnGetFileSize");
    static_assert(o1491 == offsetof(::enginefuncs_s, pfnGetApproxWavePlayLen), "offsetof enginefuncs_s.pfnGetApproxWavePlayLen");
    static_assert(
        std::is_same_v<t1485, map_t<decltype(::enginefuncs_s::pfnGetApproxWavePlayLen)>>,
        "type enginefuncs_s.pfnGetApproxWavePlayLen"
    );
    static_assert(o1492 == offsetof(::enginefuncs_s, pfnIsCareerMatch), "offsetof enginefuncs_s.pfnIsCareerMatch");
    static_assert(std::is_same_v<t1486, map_t<decltype(::enginefuncs_s::pfnIsCareerMatch)>>, "type enginefuncs_s.pfnIsCareerMatch");
    static_assert(o1493 == offsetof(::enginefuncs_s, pfnGetLocalizedStringLength), "offsetof enginefuncs_s.pfnGetLocalizedStringLength");
    static_assert(
        std::is_same_v<t1487, map_t<decltype(::enginefuncs_s::pfnGetLocalizedStringLength)>>,
        "type enginefuncs_s.pfnGetLocalizedStringLength"
    );
    static_assert(o1494 == offsetof(::enginefuncs_s, pfnRegisterTutorMessageShown), "offsetof enginefuncs_s.pfnRegisterTutorMessageShown");
    static_assert(
        std::is_same_v<t1488, map_t<decltype(::enginefuncs_s::pfnRegisterTutorMessageShown)>>,
        "type enginefuncs_s.pfnRegisterTutorMessageShown"
    );
    static_assert(o1495 == offsetof(::enginefuncs_s, pfnGetTimesTutorMessageShown), "offsetof enginefuncs_s.pfnGetTimesTutorMessageShown");
    static_assert(
        std::is_same_v<t1489, map_t<decltype(::enginefuncs_s::pfnGetTimesTutorMessageShown)>>,
        "type enginefuncs_s.pfnGetTimesTutorMessageShown"
    );
    static_assert(
        o1496 == offsetof(::enginefuncs_s, pfnProcessTutorMessageDecayBuffer),
        "offsetof enginefuncs_s.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t1490, map_t<decltype(::enginefuncs_s::pfnProcessTutorMessageDecayBuffer)>>,
        "type enginefuncs_s.pfnProcessTutorMessageDecayBuffer"
    );
    static_assert(
        o1497 == offsetof(::enginefuncs_s, pfnConstructTutorMessageDecayBuffer),
        "offsetof enginefuncs_s.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        std::is_same_v<t1491, map_t<decltype(::enginefuncs_s::pfnConstructTutorMessageDecayBuffer)>>,
        "type enginefuncs_s.pfnConstructTutorMessageDecayBuffer"
    );
    static_assert(
        o1498 == offsetof(::enginefuncs_s, pfnResetTutorMessageDecayData),
        "offsetof enginefuncs_s.pfnResetTutorMessageDecayData"
    );
    static_assert(
        std::is_same_v<t1492, map_t<decltype(::enginefuncs_s::pfnResetTutorMessageDecayData)>>,
        "type enginefuncs_s.pfnResetTutorMessageDecayData"
    );
    static_assert(o1499 == offsetof(::enginefuncs_s, pfnQueryClientCvarValue), "offsetof enginefuncs_s.pfnQueryClientCvarValue");
    static_assert(
        std::is_same_v<t1493, map_t<decltype(::enginefuncs_s::pfnQueryClientCvarValue)>>,
        "type enginefuncs_s.pfnQueryClientCvarValue"
    );
    static_assert(o1500 == offsetof(::enginefuncs_s, pfnQueryClientCvarValue2), "offsetof enginefuncs_s.pfnQueryClientCvarValue2");
    static_assert(
        std::is_same_v<t1494, map_t<decltype(::enginefuncs_s::pfnQueryClientCvarValue2)>>,
        "type enginefuncs_s.pfnQueryClientCvarValue2"
    );
    static_assert(o1501 == offsetof(::enginefuncs_s, pfnEngCheckParm), "offsetof enginefuncs_s.pfnEngCheckParm");
    static_assert(std::is_same_v<t1495, map_t<decltype(::enginefuncs_s::pfnEngCheckParm)>>, "type enginefuncs_s.pfnEngCheckParm");
    static_assert(s101 == sizeof(::engine_studio_api_s), "sizeof engine_studio_api_s");
    static_assert(o1502 == offsetof(::engine_studio_api_s, Mem_Calloc), "offsetof engine_studio_api_s.Mem_Calloc");
    static_assert(std::is_same_v<t1496, map_t<decltype(::engine_studio_api_s::Mem_Calloc)>>, "type engine_studio_api_s.Mem_Calloc");
    static_assert(o1503 == offsetof(::engine_studio_api_s, Cache_Check), "offsetof engine_studio_api_s.Cache_Check");
    static_assert(std::is_same_v<t1497, map_t<decltype(::engine_studio_api_s::Cache_Check)>>, "type engine_studio_api_s.Cache_Check");
    static_assert(o1504 == offsetof(::engine_studio_api_s, LoadCacheFile), "offsetof engine_studio_api_s.LoadCacheFile");
    static_assert(std::is_same_v<t1498, map_t<decltype(::engine_studio_api_s::LoadCacheFile)>>, "type engine_studio_api_s.LoadCacheFile");
    static_assert(o1505 == offsetof(::engine_studio_api_s, Mod_ForName), "offsetof engine_studio_api_s.Mod_ForName");
    static_assert(std::is_same_v<t1499, map_t<decltype(::engine_studio_api_s::Mod_ForName)>>, "type engine_studio_api_s.Mod_ForName");
    static_assert(o1506 == offsetof(::engine_studio_api_s, Mod_Extradata), "offsetof engine_studio_api_s.Mod_Extradata");
    static_assert(std::is_same_v<t1500, map_t<decltype(::engine_studio_api_s::Mod_Extradata)>>, "type engine_studio_api_s.Mod_Extradata");
    static_assert(o1507 == offsetof(::engine_studio_api_s, GetModelByIndex), "offsetof engine_studio_api_s.GetModelByIndex");
    static_assert(
        std::is_same_v<t1501, map_t<decltype(::engine_studio_api_s::GetModelByIndex)>>,
        "type engine_studio_api_s.GetModelByIndex"
    );
    static_assert(o1508 == offsetof(::engine_studio_api_s, GetCurrentEntity), "offsetof engine_studio_api_s.GetCurrentEntity");
    static_assert(
        std::is_same_v<t1502, map_t<decltype(::engine_studio_api_s::GetCurrentEntity)>>,
        "type engine_studio_api_s.GetCurrentEntity"
    );
    static_assert(o1509 == offsetof(::engine_studio_api_s, PlayerInfo), "offsetof engine_studio_api_s.PlayerInfo");
    static_assert(std::is_same_v<t1503, map_t<decltype(::engine_studio_api_s::PlayerInfo)>>, "type engine_studio_api_s.PlayerInfo");
    static_assert(o1510 == offsetof(::engine_studio_api_s, GetPlayerState), "offsetof engine_studio_api_s.GetPlayerState");
    static_assert(std::is_same_v<t1504, map_t<decltype(::engine_studio_api_s::GetPlayerState)>>, "type engine_studio_api_s.GetPlayerState");
    static_assert(o1511 == offsetof(::engine_studio_api_s, GetViewEntity), "offsetof engine_studio_api_s.GetViewEntity");
    static_assert(std::is_same_v<t1505, map_t<decltype(::engine_studio_api_s::GetViewEntity)>>, "type engine_studio_api_s.GetViewEntity");
    static_assert(o1512 == offsetof(::engine_studio_api_s, GetTimes), "offsetof engine_studio_api_s.GetTimes");
    static_assert(std::is_same_v<t1506, map_t<decltype(::engine_studio_api_s::GetTimes)>>, "type engine_studio_api_s.GetTimes");
    static_assert(o1513 == offsetof(::engine_studio_api_s, GetCvar), "offsetof engine_studio_api_s.GetCvar");
    static_assert(std::is_same_v<t1507, map_t<decltype(::engine_studio_api_s::GetCvar)>>, "type engine_studio_api_s.GetCvar");
    static_assert(o1514 == offsetof(::engine_studio_api_s, GetViewInfo), "offsetof engine_studio_api_s.GetViewInfo");
    static_assert(std::is_same_v<t1508, map_t<decltype(::engine_studio_api_s::GetViewInfo)>>, "type engine_studio_api_s.GetViewInfo");
    static_assert(o1515 == offsetof(::engine_studio_api_s, GetChromeSprite), "offsetof engine_studio_api_s.GetChromeSprite");
    static_assert(
        std::is_same_v<t1509, map_t<decltype(::engine_studio_api_s::GetChromeSprite)>>,
        "type engine_studio_api_s.GetChromeSprite"
    );
    static_assert(o1516 == offsetof(::engine_studio_api_s, GetModelCounters), "offsetof engine_studio_api_s.GetModelCounters");
    static_assert(
        std::is_same_v<t1510, map_t<decltype(::engine_studio_api_s::GetModelCounters)>>,
        "type engine_studio_api_s.GetModelCounters"
    );
    static_assert(o1517 == offsetof(::engine_studio_api_s, GetAliasScale), "offsetof engine_studio_api_s.GetAliasScale");
    static_assert(std::is_same_v<t1511, map_t<decltype(::engine_studio_api_s::GetAliasScale)>>, "type engine_studio_api_s.GetAliasScale");
    static_assert(o1518 == offsetof(::engine_studio_api_s, StudioGetBoneTransform), "offsetof engine_studio_api_s.StudioGetBoneTransform");
    static_assert(
        std::is_same_v<t1512, map_t<decltype(::engine_studio_api_s::StudioGetBoneTransform)>>,
        "type engine_studio_api_s.StudioGetBoneTransform"
    );
    static_assert(
        o1519 == offsetof(::engine_studio_api_s, StudioGetLightTransform),
        "offsetof engine_studio_api_s.StudioGetLightTransform"
    );
    static_assert(
        std::is_same_v<t1513, map_t<decltype(::engine_studio_api_s::StudioGetLightTransform)>>,
        "type engine_studio_api_s.StudioGetLightTransform"
    );
    static_assert(
        o1520 == offsetof(::engine_studio_api_s, StudioGetAliasTransform),
        "offsetof engine_studio_api_s.StudioGetAliasTransform"
    );
    static_assert(
        std::is_same_v<t1514, map_t<decltype(::engine_studio_api_s::StudioGetAliasTransform)>>,
        "type engine_studio_api_s.StudioGetAliasTransform"
    );
    static_assert(
        o1521 == offsetof(::engine_studio_api_s, StudioGetRotationMatrix),
        "offsetof engine_studio_api_s.StudioGetRotationMatrix"
    );
    static_assert(
        std::is_same_v<t1515, map_t<decltype(::engine_studio_api_s::StudioGetRotationMatrix)>>,
        "type engine_studio_api_s.StudioGetRotationMatrix"
    );
    static_assert(o1522 == offsetof(::engine_studio_api_s, StudioSetupModel), "offsetof engine_studio_api_s.StudioSetupModel");
    static_assert(
        std::is_same_v<t1516, map_t<decltype(::engine_studio_api_s::StudioSetupModel)>>,
        "type engine_studio_api_s.StudioSetupModel"
    );
    static_assert(o1523 == offsetof(::engine_studio_api_s, StudioCheckBBox), "offsetof engine_studio_api_s.StudioCheckBBox");
    static_assert(
        std::is_same_v<t1517, map_t<decltype(::engine_studio_api_s::StudioCheckBBox)>>,
        "type engine_studio_api_s.StudioCheckBBox"
    );
    static_assert(o1524 == offsetof(::engine_studio_api_s, StudioDynamicLight), "offsetof engine_studio_api_s.StudioDynamicLight");
    static_assert(
        std::is_same_v<t1518, map_t<decltype(::engine_studio_api_s::StudioDynamicLight)>>,
        "type engine_studio_api_s.StudioDynamicLight"
    );
    static_assert(o1525 == offsetof(::engine_studio_api_s, StudioEntityLight), "offsetof engine_studio_api_s.StudioEntityLight");
    static_assert(
        std::is_same_v<t1519, map_t<decltype(::engine_studio_api_s::StudioEntityLight)>>,
        "type engine_studio_api_s.StudioEntityLight"
    );
    static_assert(o1526 == offsetof(::engine_studio_api_s, StudioSetupLighting), "offsetof engine_studio_api_s.StudioSetupLighting");
    static_assert(
        std::is_same_v<t1520, map_t<decltype(::engine_studio_api_s::StudioSetupLighting)>>,
        "type engine_studio_api_s.StudioSetupLighting"
    );
    static_assert(o1527 == offsetof(::engine_studio_api_s, StudioDrawPoints), "offsetof engine_studio_api_s.StudioDrawPoints");
    static_assert(
        std::is_same_v<t1521, map_t<decltype(::engine_studio_api_s::StudioDrawPoints)>>,
        "type engine_studio_api_s.StudioDrawPoints"
    );
    static_assert(o1528 == offsetof(::engine_studio_api_s, StudioDrawHulls), "offsetof engine_studio_api_s.StudioDrawHulls");
    static_assert(
        std::is_same_v<t1522, map_t<decltype(::engine_studio_api_s::StudioDrawHulls)>>,
        "type engine_studio_api_s.StudioDrawHulls"
    );
    static_assert(o1529 == offsetof(::engine_studio_api_s, StudioDrawAbsBBox), "offsetof engine_studio_api_s.StudioDrawAbsBBox");
    static_assert(
        std::is_same_v<t1523, map_t<decltype(::engine_studio_api_s::StudioDrawAbsBBox)>>,
        "type engine_studio_api_s.StudioDrawAbsBBox"
    );
    static_assert(o1530 == offsetof(::engine_studio_api_s, StudioDrawBones), "offsetof engine_studio_api_s.StudioDrawBones");
    static_assert(
        std::is_same_v<t1524, map_t<decltype(::engine_studio_api_s::StudioDrawBones)>>,
        "type engine_studio_api_s.StudioDrawBones"
    );
    static_assert(o1531 == offsetof(::engine_studio_api_s, StudioSetupSkin), "offsetof engine_studio_api_s.StudioSetupSkin");
    static_assert(
        std::is_same_v<t1525, map_t<decltype(::engine_studio_api_s::StudioSetupSkin)>>,
        "type engine_studio_api_s.StudioSetupSkin"
    );
    static_assert(o1532 == offsetof(::engine_studio_api_s, StudioSetRemapColors), "offsetof engine_studio_api_s.StudioSetRemapColors");
    static_assert(
        std::is_same_v<t1526, map_t<decltype(::engine_studio_api_s::StudioSetRemapColors)>>,
        "type engine_studio_api_s.StudioSetRemapColors"
    );
    static_assert(o1533 == offsetof(::engine_studio_api_s, SetupPlayerModel), "offsetof engine_studio_api_s.SetupPlayerModel");
    static_assert(
        std::is_same_v<t1527, map_t<decltype(::engine_studio_api_s::SetupPlayerModel)>>,
        "type engine_studio_api_s.SetupPlayerModel"
    );
    static_assert(o1534 == offsetof(::engine_studio_api_s, StudioClientEvents), "offsetof engine_studio_api_s.StudioClientEvents");
    static_assert(
        std::is_same_v<t1528, map_t<decltype(::engine_studio_api_s::StudioClientEvents)>>,
        "type engine_studio_api_s.StudioClientEvents"
    );
    static_assert(o1535 == offsetof(::engine_studio_api_s, GetForceFaceFlags), "offsetof engine_studio_api_s.GetForceFaceFlags");
    static_assert(
        std::is_same_v<t1529, map_t<decltype(::engine_studio_api_s::GetForceFaceFlags)>>,
        "type engine_studio_api_s.GetForceFaceFlags"
    );
    static_assert(o1536 == offsetof(::engine_studio_api_s, SetForceFaceFlags), "offsetof engine_studio_api_s.SetForceFaceFlags");
    static_assert(
        std::is_same_v<t1530, map_t<decltype(::engine_studio_api_s::SetForceFaceFlags)>>,
        "type engine_studio_api_s.SetForceFaceFlags"
    );
    static_assert(o1537 == offsetof(::engine_studio_api_s, StudioSetHeader), "offsetof engine_studio_api_s.StudioSetHeader");
    static_assert(
        std::is_same_v<t1531, map_t<decltype(::engine_studio_api_s::StudioSetHeader)>>,
        "type engine_studio_api_s.StudioSetHeader"
    );
    static_assert(o1538 == offsetof(::engine_studio_api_s, SetRenderModel), "offsetof engine_studio_api_s.SetRenderModel");
    static_assert(std::is_same_v<t1532, map_t<decltype(::engine_studio_api_s::SetRenderModel)>>, "type engine_studio_api_s.SetRenderModel");
    static_assert(o1539 == offsetof(::engine_studio_api_s, SetupRenderer), "offsetof engine_studio_api_s.SetupRenderer");
    static_assert(std::is_same_v<t1533, map_t<decltype(::engine_studio_api_s::SetupRenderer)>>, "type engine_studio_api_s.SetupRenderer");
    static_assert(o1540 == offsetof(::engine_studio_api_s, RestoreRenderer), "offsetof engine_studio_api_s.RestoreRenderer");
    static_assert(
        std::is_same_v<t1534, map_t<decltype(::engine_studio_api_s::RestoreRenderer)>>,
        "type engine_studio_api_s.RestoreRenderer"
    );
    static_assert(o1541 == offsetof(::engine_studio_api_s, SetChromeOrigin), "offsetof engine_studio_api_s.SetChromeOrigin");
    static_assert(
        std::is_same_v<t1535, map_t<decltype(::engine_studio_api_s::SetChromeOrigin)>>,
        "type engine_studio_api_s.SetChromeOrigin"
    );
    static_assert(o1542 == offsetof(::engine_studio_api_s, IsHardware), "offsetof engine_studio_api_s.IsHardware");
    static_assert(std::is_same_v<t1536, map_t<decltype(::engine_studio_api_s::IsHardware)>>, "type engine_studio_api_s.IsHardware");
    static_assert(o1543 == offsetof(::engine_studio_api_s, GL_StudioDrawShadow), "offsetof engine_studio_api_s.GL_StudioDrawShadow");
    static_assert(
        std::is_same_v<t1537, map_t<decltype(::engine_studio_api_s::GL_StudioDrawShadow)>>,
        "type engine_studio_api_s.GL_StudioDrawShadow"
    );
    static_assert(o1544 == offsetof(::engine_studio_api_s, GL_SetRenderMode), "offsetof engine_studio_api_s.GL_SetRenderMode");
    static_assert(
        std::is_same_v<t1538, map_t<decltype(::engine_studio_api_s::GL_SetRenderMode)>>,
        "type engine_studio_api_s.GL_SetRenderMode"
    );
    static_assert(o1545 == offsetof(::engine_studio_api_s, StudioSetRenderamt), "offsetof engine_studio_api_s.StudioSetRenderamt");
    static_assert(
        std::is_same_v<t1539, map_t<decltype(::engine_studio_api_s::StudioSetRenderamt)>>,
        "type engine_studio_api_s.StudioSetRenderamt"
    );
    static_assert(o1546 == offsetof(::engine_studio_api_s, StudioSetCullState), "offsetof engine_studio_api_s.StudioSetCullState");
    static_assert(
        std::is_same_v<t1540, map_t<decltype(::engine_studio_api_s::StudioSetCullState)>>,
        "type engine_studio_api_s.StudioSetCullState"
    );
    static_assert(o1547 == offsetof(::engine_studio_api_s, StudioRenderShadow), "offsetof engine_studio_api_s.StudioRenderShadow");
    static_assert(
        std::is_same_v<t1541, map_t<decltype(::engine_studio_api_s::StudioRenderShadow)>>,
        "type engine_studio_api_s.StudioRenderShadow"
    );
    static_assert(s102 == sizeof(::r_studio_interface_s), "sizeof r_studio_interface_s");
    static_assert(o1548 == offsetof(::r_studio_interface_s, version), "offsetof r_studio_interface_s.version");
    static_assert(std::is_same_v<t1542, map_t<decltype(::r_studio_interface_s::version)>>, "type r_studio_interface_s.version");
    static_assert(o1549 == offsetof(::r_studio_interface_s, StudioDrawModel), "offsetof r_studio_interface_s.StudioDrawModel");
    static_assert(
        std::is_same_v<t1543, map_t<decltype(::r_studio_interface_s::StudioDrawModel)>>,
        "type r_studio_interface_s.StudioDrawModel"
    );
    static_assert(o1550 == offsetof(::r_studio_interface_s, StudioDrawPlayer), "offsetof r_studio_interface_s.StudioDrawPlayer");
    static_assert(
        std::is_same_v<t1544, map_t<decltype(::r_studio_interface_s::StudioDrawPlayer)>>,
        "type r_studio_interface_s.StudioDrawPlayer"
    );
    static_assert(s103 == sizeof(::triangleapi_s), "sizeof triangleapi_s");
    static_assert(o1551 == offsetof(::triangleapi_s, version), "offsetof triangleapi_s.version");
    static_assert(std::is_same_v<t1545, map_t<decltype(::triangleapi_s::version)>>, "type triangleapi_s.version");
    static_assert(o1552 == offsetof(::triangleapi_s, RenderMode), "offsetof triangleapi_s.RenderMode");
    static_assert(std::is_same_v<t1546, map_t<decltype(::triangleapi_s::RenderMode)>>, "type triangleapi_s.RenderMode");
    static_assert(o1553 == offsetof(::triangleapi_s, Begin), "offsetof triangleapi_s.Begin");
    static_assert(std::is_same_v<t1547, map_t<decltype(::triangleapi_s::Begin)>>, "type triangleapi_s.Begin");
    static_assert(o1554 == offsetof(::triangleapi_s, End), "offsetof triangleapi_s.End");
    static_assert(std::is_same_v<t1548, map_t<decltype(::triangleapi_s::End)>>, "type triangleapi_s.End");
    static_assert(o1555 == offsetof(::triangleapi_s, Color4f), "offsetof triangleapi_s.Color4f");
    static_assert(std::is_same_v<t1549, map_t<decltype(::triangleapi_s::Color4f)>>, "type triangleapi_s.Color4f");
    static_assert(o1556 == offsetof(::triangleapi_s, Color4ub), "offsetof triangleapi_s.Color4ub");
    static_assert(std::is_same_v<t1550, map_t<decltype(::triangleapi_s::Color4ub)>>, "type triangleapi_s.Color4ub");
    static_assert(o1557 == offsetof(::triangleapi_s, TexCoord2f), "offsetof triangleapi_s.TexCoord2f");
    static_assert(std::is_same_v<t1551, map_t<decltype(::triangleapi_s::TexCoord2f)>>, "type triangleapi_s.TexCoord2f");
    static_assert(o1558 == offsetof(::triangleapi_s, Vertex3fv), "offsetof triangleapi_s.Vertex3fv");
    static_assert(std::is_same_v<t1552, map_t<decltype(::triangleapi_s::Vertex3fv)>>, "type triangleapi_s.Vertex3fv");
    static_assert(o1559 == offsetof(::triangleapi_s, Vertex3f), "offsetof triangleapi_s.Vertex3f");
    static_assert(std::is_same_v<t1553, map_t<decltype(::triangleapi_s::Vertex3f)>>, "type triangleapi_s.Vertex3f");
    static_assert(o1560 == offsetof(::triangleapi_s, Brightness), "offsetof triangleapi_s.Brightness");
    static_assert(std::is_same_v<t1554, map_t<decltype(::triangleapi_s::Brightness)>>, "type triangleapi_s.Brightness");
    static_assert(o1561 == offsetof(::triangleapi_s, CullFace), "offsetof triangleapi_s.CullFace");
    static_assert(std::is_same_v<t1555, map_t<decltype(::triangleapi_s::CullFace)>>, "type triangleapi_s.CullFace");
    static_assert(o1562 == offsetof(::triangleapi_s, SpriteTexture), "offsetof triangleapi_s.SpriteTexture");
    static_assert(std::is_same_v<t1556, map_t<decltype(::triangleapi_s::SpriteTexture)>>, "type triangleapi_s.SpriteTexture");
    static_assert(o1563 == offsetof(::triangleapi_s, WorldToScreen), "offsetof triangleapi_s.WorldToScreen");
    static_assert(std::is_same_v<t1557, map_t<decltype(::triangleapi_s::WorldToScreen)>>, "type triangleapi_s.WorldToScreen");
    static_assert(o1564 == offsetof(::triangleapi_s, Fog), "offsetof triangleapi_s.Fog");
    static_assert(std::is_same_v<t1558, map_t<decltype(::triangleapi_s::Fog)>>, "type triangleapi_s.Fog");
    static_assert(o1565 == offsetof(::triangleapi_s, ScreenToWorld), "offsetof triangleapi_s.ScreenToWorld");
    static_assert(std::is_same_v<t1559, map_t<decltype(::triangleapi_s::ScreenToWorld)>>, "type triangleapi_s.ScreenToWorld");
    static_assert(o1566 == offsetof(::triangleapi_s, GetMatrix), "offsetof triangleapi_s.GetMatrix");
    static_assert(std::is_same_v<t1560, map_t<decltype(::triangleapi_s::GetMatrix)>>, "type triangleapi_s.GetMatrix");
    static_assert(o1567 == offsetof(::triangleapi_s, BoxInPVS), "offsetof triangleapi_s.BoxInPVS");
    static_assert(std::is_same_v<t1561, map_t<decltype(::triangleapi_s::BoxInPVS)>>, "type triangleapi_s.BoxInPVS");
    static_assert(o1568 == offsetof(::triangleapi_s, LightAtPoint), "offsetof triangleapi_s.LightAtPoint");
    static_assert(std::is_same_v<t1562, map_t<decltype(::triangleapi_s::LightAtPoint)>>, "type triangleapi_s.LightAtPoint");
    static_assert(o1569 == offsetof(::triangleapi_s, Color4fRendermode), "offsetof triangleapi_s.Color4fRendermode");
    static_assert(std::is_same_v<t1563, map_t<decltype(::triangleapi_s::Color4fRendermode)>>, "type triangleapi_s.Color4fRendermode");
    static_assert(o1570 == offsetof(::triangleapi_s, FogParams), "offsetof triangleapi_s.FogParams");
    static_assert(std::is_same_v<t1564, map_t<decltype(::triangleapi_s::FogParams)>>, "type triangleapi_s.FogParams");
    static_assert(s104 == sizeof(::cache_user_s), "sizeof cache_user_s");
    static_assert(o1571 == offsetof(::cache_user_s, data), "offsetof cache_user_s.data");
    static_assert(std::is_same_v<t1565, map_t<decltype(::cache_user_s::data)>>, "type cache_user_s.data");
    static_assert(s105 == sizeof(::color24), "sizeof color24");
    static_assert(o1572 == offsetof(::color24, r), "offsetof color24.r");
    static_assert(std::is_same_v<t1566, map_t<decltype(::color24::r)>>, "type color24.r");
    static_assert(o1573 == offsetof(::color24, g), "offsetof color24.g");
    static_assert(std::is_same_v<t1567, map_t<decltype(::color24::g)>>, "type color24.g");
    static_assert(o1574 == offsetof(::color24, b), "offsetof color24.b");
    static_assert(std::is_same_v<t1568, map_t<decltype(::color24::b)>>, "type color24.b");
    static_assert(s106 == sizeof(::colorVec), "sizeof colorVec");
    static_assert(o1575 == offsetof(::colorVec, r), "offsetof colorVec.r");
    static_assert(std::is_same_v<t1569, map_t<decltype(::colorVec::r)>>, "type colorVec.r");
    static_assert(o1576 == offsetof(::colorVec, g), "offsetof colorVec.g");
    static_assert(std::is_same_v<t1570, map_t<decltype(::colorVec::g)>>, "type colorVec.g");
    static_assert(o1577 == offsetof(::colorVec, b), "offsetof colorVec.b");
    static_assert(std::is_same_v<t1571, map_t<decltype(::colorVec::b)>>, "type colorVec.b");
    static_assert(o1578 == offsetof(::colorVec, a), "offsetof colorVec.a");
    static_assert(std::is_same_v<t1572, map_t<decltype(::colorVec::a)>>, "type colorVec.a");
    static_assert(s107 == sizeof(::vrect_t), "sizeof vrect_t");
    static_assert(o1579 == offsetof(::vrect_t, x), "offsetof vrect_t.x");
    static_assert(std::is_same_v<t1573, map_t<decltype(::vrect_t::x)>>, "type vrect_t.x");
    static_assert(o1580 == offsetof(::vrect_t, y), "offsetof vrect_t.y");
    static_assert(std::is_same_v<t1574, map_t<decltype(::vrect_t::y)>>, "type vrect_t.y");
    static_assert(o1581 == offsetof(::vrect_t, width), "offsetof vrect_t.width");
    static_assert(std::is_same_v<t1575, map_t<decltype(::vrect_t::width)>>, "type vrect_t.width");
    static_assert(o1582 == offsetof(::vrect_t, height), "offsetof vrect_t.height");
    static_assert(std::is_same_v<t1576, map_t<decltype(::vrect_t::height)>>, "type vrect_t.height");
    static_assert(o1583 == offsetof(::vrect_t, pnext), "offsetof vrect_t.pnext");
    static_assert(std::is_same_v<t1577, map_t<decltype(::vrect_t::pnext)>>, "type vrect_t.pnext");
    static_assert(s108 == sizeof(::rect_s), "sizeof rect_s");
    static_assert(o1584 == offsetof(::rect_s, left), "offsetof rect_s.left");
    static_assert(std::is_same_v<t1578, map_t<decltype(::rect_s::left)>>, "type rect_s.left");
    static_assert(o1585 == offsetof(::rect_s, right), "offsetof rect_s.right");
    static_assert(std::is_same_v<t1579, map_t<decltype(::rect_s::right)>>, "type rect_s.right");
    static_assert(o1586 == offsetof(::rect_s, top), "offsetof rect_s.top");
    static_assert(std::is_same_v<t1580, map_t<decltype(::rect_s::top)>>, "type rect_s.top");
    static_assert(o1587 == offsetof(::rect_s, bottom), "offsetof rect_s.bottom");
    static_assert(std::is_same_v<t1581, map_t<decltype(::rect_s::bottom)>>, "type rect_s.bottom");
    static_assert(s109 == sizeof(::kbutton_s), "sizeof kbutton_s");
    static_assert(o1588 == offsetof(::kbutton_s, down), "offsetof kbutton_s.down");
    static_assert(std::is_same_v<t1582, map_t<decltype(::kbutton_s::down)>>, "type kbutton_s.down");
    static_assert(o1589 == offsetof(::kbutton_s, state), "offsetof kbutton_s.state");
    static_assert(std::is_same_v<t1583, map_t<decltype(::kbutton_s::state)>>, "type kbutton_s.state");
    static_assert(s110 == sizeof(::usercmd_s), "sizeof usercmd_s");
    static_assert(o1590 == offsetof(::usercmd_s, lerp_msec), "offsetof usercmd_s.lerp_msec");
    static_assert(std::is_same_v<t1584, map_t<decltype(::usercmd_s::lerp_msec)>>, "type usercmd_s.lerp_msec");
    static_assert(o1591 == offsetof(::usercmd_s, msec), "offsetof usercmd_s.msec");
    static_assert(std::is_same_v<t1585, map_t<decltype(::usercmd_s::msec)>>, "type usercmd_s.msec");
    static_assert(o1592 == offsetof(::usercmd_s, viewangles), "offsetof usercmd_s.viewangles");
    static_assert(std::is_same_v<t1586, map_t<decltype(::usercmd_s::viewangles)>>, "type usercmd_s.viewangles");
    static_assert(o1593 == offsetof(::usercmd_s, forwardmove), "offsetof usercmd_s.forwardmove");
    static_assert(std::is_same_v<t1587, map_t<decltype(::usercmd_s::forwardmove)>>, "type usercmd_s.forwardmove");
    static_assert(o1594 == offsetof(::usercmd_s, sidemove), "offsetof usercmd_s.sidemove");
    static_assert(std::is_same_v<t1588, map_t<decltype(::usercmd_s::sidemove)>>, "type usercmd_s.sidemove");
    static_assert(o1595 == offsetof(::usercmd_s, upmove), "offsetof usercmd_s.upmove");
    static_assert(std::is_same_v<t1589, map_t<decltype(::usercmd_s::upmove)>>, "type usercmd_s.upmove");
    static_assert(o1596 == offsetof(::usercmd_s, lightlevel), "offsetof usercmd_s.lightlevel");
    static_assert(std::is_same_v<t1590, map_t<decltype(::usercmd_s::lightlevel)>>, "type usercmd_s.lightlevel");
    static_assert(o1597 == offsetof(::usercmd_s, buttons), "offsetof usercmd_s.buttons");
    static_assert(std::is_same_v<t1591, map_t<decltype(::usercmd_s::buttons)>>, "type usercmd_s.buttons");
    static_assert(o1598 == offsetof(::usercmd_s, impulse), "offsetof usercmd_s.impulse");
    static_assert(std::is_same_v<t1592, map_t<decltype(::usercmd_s::impulse)>>, "type usercmd_s.impulse");
    static_assert(o1599 == offsetof(::usercmd_s, weaponselect), "offsetof usercmd_s.weaponselect");
    static_assert(std::is_same_v<t1593, map_t<decltype(::usercmd_s::weaponselect)>>, "type usercmd_s.weaponselect");
    static_assert(o1600 == offsetof(::usercmd_s, impact_index), "offsetof usercmd_s.impact_index");
    static_assert(std::is_same_v<t1594, map_t<decltype(::usercmd_s::impact_index)>>, "type usercmd_s.impact_index");
    static_assert(o1601 == offsetof(::usercmd_s, impact_position), "offsetof usercmd_s.impact_position");
    static_assert(std::is_same_v<t1595, map_t<decltype(::usercmd_s::impact_position)>>, "type usercmd_s.impact_position");
    static_assert(s111 == sizeof(::ref_params_s), "sizeof ref_params_s");
    static_assert(o1602 == offsetof(::ref_params_s, vieworg), "offsetof ref_params_s.vieworg");
    static_assert(std::is_same_v<t1596, map_t<decltype(::ref_params_s::vieworg)>>, "type ref_params_s.vieworg");
    static_assert(o1603 == offsetof(::ref_params_s, viewangles), "offsetof ref_params_s.viewangles");
    static_assert(std::is_same_v<t1597, map_t<decltype(::ref_params_s::viewangles)>>, "type ref_params_s.viewangles");
    static_assert(o1604 == offsetof(::ref_params_s, forward), "offsetof ref_params_s.forward");
    static_assert(std::is_same_v<t1598, map_t<decltype(::ref_params_s::forward)>>, "type ref_params_s.forward");
    static_assert(o1605 == offsetof(::ref_params_s, right), "offsetof ref_params_s.right");
    static_assert(std::is_same_v<t1599, map_t<decltype(::ref_params_s::right)>>, "type ref_params_s.right");
    static_assert(o1606 == offsetof(::ref_params_s, up), "offsetof ref_params_s.up");
    static_assert(std::is_same_v<t1600, map_t<decltype(::ref_params_s::up)>>, "type ref_params_s.up");
    static_assert(o1607 == offsetof(::ref_params_s, frametime), "offsetof ref_params_s.frametime");
    static_assert(std::is_same_v<t1601, map_t<decltype(::ref_params_s::frametime)>>, "type ref_params_s.frametime");
    static_assert(o1608 == offsetof(::ref_params_s, time), "offsetof ref_params_s.time");
    static_assert(std::is_same_v<t1602, map_t<decltype(::ref_params_s::time)>>, "type ref_params_s.time");
    static_assert(o1609 == offsetof(::ref_params_s, intermission), "offsetof ref_params_s.intermission");
    static_assert(std::is_same_v<t1603, map_t<decltype(::ref_params_s::intermission)>>, "type ref_params_s.intermission");
    static_assert(o1610 == offsetof(::ref_params_s, paused), "offsetof ref_params_s.paused");
    static_assert(std::is_same_v<t1604, map_t<decltype(::ref_params_s::paused)>>, "type ref_params_s.paused");
    static_assert(o1611 == offsetof(::ref_params_s, spectator), "offsetof ref_params_s.spectator");
    static_assert(std::is_same_v<t1605, map_t<decltype(::ref_params_s::spectator)>>, "type ref_params_s.spectator");
    static_assert(o1612 == offsetof(::ref_params_s, onground), "offsetof ref_params_s.onground");
    static_assert(std::is_same_v<t1606, map_t<decltype(::ref_params_s::onground)>>, "type ref_params_s.onground");
    static_assert(o1613 == offsetof(::ref_params_s, waterlevel), "offsetof ref_params_s.waterlevel");
    static_assert(std::is_same_v<t1607, map_t<decltype(::ref_params_s::waterlevel)>>, "type ref_params_s.waterlevel");
    static_assert(o1614 == offsetof(::ref_params_s, simvel), "offsetof ref_params_s.simvel");
    static_assert(std::is_same_v<t1608, map_t<decltype(::ref_params_s::simvel)>>, "type ref_params_s.simvel");
    static_assert(o1615 == offsetof(::ref_params_s, simorg), "offsetof ref_params_s.simorg");
    static_assert(std::is_same_v<t1609, map_t<decltype(::ref_params_s::simorg)>>, "type ref_params_s.simorg");
    static_assert(o1616 == offsetof(::ref_params_s, viewheight), "offsetof ref_params_s.viewheight");
    static_assert(std::is_same_v<t1610, map_t<decltype(::ref_params_s::viewheight)>>, "type ref_params_s.viewheight");
    static_assert(o1617 == offsetof(::ref_params_s, idealpitch), "offsetof ref_params_s.idealpitch");
    static_assert(std::is_same_v<t1611, map_t<decltype(::ref_params_s::idealpitch)>>, "type ref_params_s.idealpitch");
    static_assert(o1618 == offsetof(::ref_params_s, cl_viewangles), "offsetof ref_params_s.cl_viewangles");
    static_assert(std::is_same_v<t1612, map_t<decltype(::ref_params_s::cl_viewangles)>>, "type ref_params_s.cl_viewangles");
    static_assert(o1619 == offsetof(::ref_params_s, health), "offsetof ref_params_s.health");
    static_assert(std::is_same_v<t1613, map_t<decltype(::ref_params_s::health)>>, "type ref_params_s.health");
    static_assert(o1620 == offsetof(::ref_params_s, crosshairangle), "offsetof ref_params_s.crosshairangle");
    static_assert(std::is_same_v<t1614, map_t<decltype(::ref_params_s::crosshairangle)>>, "type ref_params_s.crosshairangle");
    static_assert(o1621 == offsetof(::ref_params_s, viewsize), "offsetof ref_params_s.viewsize");
    static_assert(std::is_same_v<t1615, map_t<decltype(::ref_params_s::viewsize)>>, "type ref_params_s.viewsize");
    static_assert(o1622 == offsetof(::ref_params_s, punchangle), "offsetof ref_params_s.punchangle");
    static_assert(std::is_same_v<t1616, map_t<decltype(::ref_params_s::punchangle)>>, "type ref_params_s.punchangle");
    static_assert(o1623 == offsetof(::ref_params_s, maxclients), "offsetof ref_params_s.maxclients");
    static_assert(std::is_same_v<t1617, map_t<decltype(::ref_params_s::maxclients)>>, "type ref_params_s.maxclients");
    static_assert(o1624 == offsetof(::ref_params_s, viewentity), "offsetof ref_params_s.viewentity");
    static_assert(std::is_same_v<t1618, map_t<decltype(::ref_params_s::viewentity)>>, "type ref_params_s.viewentity");
    static_assert(o1625 == offsetof(::ref_params_s, playernum), "offsetof ref_params_s.playernum");
    static_assert(std::is_same_v<t1619, map_t<decltype(::ref_params_s::playernum)>>, "type ref_params_s.playernum");
    static_assert(o1626 == offsetof(::ref_params_s, max_entities), "offsetof ref_params_s.max_entities");
    static_assert(std::is_same_v<t1620, map_t<decltype(::ref_params_s::max_entities)>>, "type ref_params_s.max_entities");
    static_assert(o1627 == offsetof(::ref_params_s, demoplayback), "offsetof ref_params_s.demoplayback");
    static_assert(std::is_same_v<t1621, map_t<decltype(::ref_params_s::demoplayback)>>, "type ref_params_s.demoplayback");
    static_assert(o1628 == offsetof(::ref_params_s, hardware), "offsetof ref_params_s.hardware");
    static_assert(std::is_same_v<t1622, map_t<decltype(::ref_params_s::hardware)>>, "type ref_params_s.hardware");
    static_assert(o1629 == offsetof(::ref_params_s, smoothing), "offsetof ref_params_s.smoothing");
    static_assert(std::is_same_v<t1623, map_t<decltype(::ref_params_s::smoothing)>>, "type ref_params_s.smoothing");
    static_assert(o1630 == offsetof(::ref_params_s, cmd), "offsetof ref_params_s.cmd");
    static_assert(std::is_same_v<t1624, map_t<decltype(::ref_params_s::cmd)>>, "type ref_params_s.cmd");
    static_assert(o1631 == offsetof(::ref_params_s, movevars), "offsetof ref_params_s.movevars");
    static_assert(std::is_same_v<t1625, map_t<decltype(::ref_params_s::movevars)>>, "type ref_params_s.movevars");
    static_assert(o1632 == offsetof(::ref_params_s, viewport), "offsetof ref_params_s.viewport");
    static_assert(std::is_same_v<t1626, map_t<decltype(::ref_params_s::viewport)>>, "type ref_params_s.viewport");
    static_assert(o1633 == offsetof(::ref_params_s, nextView), "offsetof ref_params_s.nextView");
    static_assert(std::is_same_v<t1627, map_t<decltype(::ref_params_s::nextView)>>, "type ref_params_s.nextView");
    static_assert(o1634 == offsetof(::ref_params_s, onlyClientDraw), "offsetof ref_params_s.onlyClientDraw");
    static_assert(std::is_same_v<t1628, map_t<decltype(::ref_params_s::onlyClientDraw)>>, "type ref_params_s.onlyClientDraw");
    static_assert(s112 == sizeof(::refdef_s), "sizeof refdef_s");
    static_assert(o1635 == offsetof(::refdef_s, vrect), "offsetof refdef_s.vrect");
    static_assert(std::is_same_v<t1629, map_t<decltype(::refdef_s::vrect)>>, "type refdef_s.vrect");
    static_assert(o1636 == offsetof(::refdef_s, aliasvrect), "offsetof refdef_s.aliasvrect");
    static_assert(std::is_same_v<t1630, map_t<decltype(::refdef_s::aliasvrect)>>, "type refdef_s.aliasvrect");
    static_assert(o1637 == offsetof(::refdef_s, vrectright), "offsetof refdef_s.vrectright");
    static_assert(std::is_same_v<t1631, map_t<decltype(::refdef_s::vrectright)>>, "type refdef_s.vrectright");
    static_assert(o1638 == offsetof(::refdef_s, vrectbottom), "offsetof refdef_s.vrectbottom");
    static_assert(std::is_same_v<t1632, map_t<decltype(::refdef_s::vrectbottom)>>, "type refdef_s.vrectbottom");
    static_assert(o1639 == offsetof(::refdef_s, aliasvrectright), "offsetof refdef_s.aliasvrectright");
    static_assert(std::is_same_v<t1633, map_t<decltype(::refdef_s::aliasvrectright)>>, "type refdef_s.aliasvrectright");
    static_assert(o1640 == offsetof(::refdef_s, aliasvrectbottom), "offsetof refdef_s.aliasvrectbottom");
    static_assert(std::is_same_v<t1634, map_t<decltype(::refdef_s::aliasvrectbottom)>>, "type refdef_s.aliasvrectbottom");
    static_assert(o1641 == offsetof(::refdef_s, vrectrightedge), "offsetof refdef_s.vrectrightedge");
    static_assert(std::is_same_v<t1635, map_t<decltype(::refdef_s::vrectrightedge)>>, "type refdef_s.vrectrightedge");
    static_assert(o1642 == offsetof(::refdef_s, fvrectx), "offsetof refdef_s.fvrectx");
    static_assert(std::is_same_v<t1636, map_t<decltype(::refdef_s::fvrectx)>>, "type refdef_s.fvrectx");
    static_assert(o1643 == offsetof(::refdef_s, fvrecty), "offsetof refdef_s.fvrecty");
    static_assert(std::is_same_v<t1637, map_t<decltype(::refdef_s::fvrecty)>>, "type refdef_s.fvrecty");
    static_assert(o1644 == offsetof(::refdef_s, fvrectx_adj), "offsetof refdef_s.fvrectx_adj");
    static_assert(std::is_same_v<t1638, map_t<decltype(::refdef_s::fvrectx_adj)>>, "type refdef_s.fvrectx_adj");
    static_assert(o1645 == offsetof(::refdef_s, fvrecty_adj), "offsetof refdef_s.fvrecty_adj");
    static_assert(std::is_same_v<t1639, map_t<decltype(::refdef_s::fvrecty_adj)>>, "type refdef_s.fvrecty_adj");
    static_assert(o1646 == offsetof(::refdef_s, vrect_x_adj_shift20), "offsetof refdef_s.vrect_x_adj_shift20");
    static_assert(std::is_same_v<t1640, map_t<decltype(::refdef_s::vrect_x_adj_shift20)>>, "type refdef_s.vrect_x_adj_shift20");
    static_assert(o1647 == offsetof(::refdef_s, vrectright_adj_shift20), "offsetof refdef_s.vrectright_adj_shift20");
    static_assert(std::is_same_v<t1641, map_t<decltype(::refdef_s::vrectright_adj_shift20)>>, "type refdef_s.vrectright_adj_shift20");
    static_assert(o1648 == offsetof(::refdef_s, fvrectright_adj), "offsetof refdef_s.fvrectright_adj");
    static_assert(std::is_same_v<t1642, map_t<decltype(::refdef_s::fvrectright_adj)>>, "type refdef_s.fvrectright_adj");
    static_assert(o1649 == offsetof(::refdef_s, fvrectbottom_adj), "offsetof refdef_s.fvrectbottom_adj");
    static_assert(std::is_same_v<t1643, map_t<decltype(::refdef_s::fvrectbottom_adj)>>, "type refdef_s.fvrectbottom_adj");
    static_assert(o1650 == offsetof(::refdef_s, fvrectright), "offsetof refdef_s.fvrectright");
    static_assert(std::is_same_v<t1644, map_t<decltype(::refdef_s::fvrectright)>>, "type refdef_s.fvrectright");
    static_assert(o1651 == offsetof(::refdef_s, fvrectbottom), "offsetof refdef_s.fvrectbottom");
    static_assert(std::is_same_v<t1645, map_t<decltype(::refdef_s::fvrectbottom)>>, "type refdef_s.fvrectbottom");
    static_assert(o1652 == offsetof(::refdef_s, horizontalFieldOfView), "offsetof refdef_s.horizontalFieldOfView");
    static_assert(std::is_same_v<t1646, map_t<decltype(::refdef_s::horizontalFieldOfView)>>, "type refdef_s.horizontalFieldOfView");
    static_assert(o1653 == offsetof(::refdef_s, xOrigin), "offsetof refdef_s.xOrigin");
    static_assert(std::is_same_v<t1647, map_t<decltype(::refdef_s::xOrigin)>>, "type refdef_s.xOrigin");
    static_assert(o1654 == offsetof(::refdef_s, yOrigin), "offsetof refdef_s.yOrigin");
    static_assert(std::is_same_v<t1648, map_t<decltype(::refdef_s::yOrigin)>>, "type refdef_s.yOrigin");
    static_assert(o1655 == offsetof(::refdef_s, vieworg), "offsetof refdef_s.vieworg");
    static_assert(std::is_same_v<t1649, map_t<decltype(::refdef_s::vieworg)>>, "type refdef_s.vieworg");
    static_assert(o1656 == offsetof(::refdef_s, viewangles), "offsetof refdef_s.viewangles");
    static_assert(std::is_same_v<t1650, map_t<decltype(::refdef_s::viewangles)>>, "type refdef_s.viewangles");
    static_assert(o1657 == offsetof(::refdef_s, ambientlight), "offsetof refdef_s.ambientlight");
    static_assert(std::is_same_v<t1651, map_t<decltype(::refdef_s::ambientlight)>>, "type refdef_s.ambientlight");
    static_assert(o1658 == offsetof(::refdef_s, onlyClientDraws), "offsetof refdef_s.onlyClientDraws");
    static_assert(std::is_same_v<t1652, map_t<decltype(::refdef_s::onlyClientDraws)>>, "type refdef_s.onlyClientDraws");
    static_assert(s113 == sizeof(::screenfade_s), "sizeof screenfade_s");
    static_assert(o1659 == offsetof(::screenfade_s, fadeSpeed), "offsetof screenfade_s.fadeSpeed");
    static_assert(std::is_same_v<t1653, map_t<decltype(::screenfade_s::fadeSpeed)>>, "type screenfade_s.fadeSpeed");
    static_assert(o1660 == offsetof(::screenfade_s, fadeEnd), "offsetof screenfade_s.fadeEnd");
    static_assert(std::is_same_v<t1654, map_t<decltype(::screenfade_s::fadeEnd)>>, "type screenfade_s.fadeEnd");
    static_assert(o1661 == offsetof(::screenfade_s, fadeTotalEnd), "offsetof screenfade_s.fadeTotalEnd");
    static_assert(std::is_same_v<t1655, map_t<decltype(::screenfade_s::fadeTotalEnd)>>, "type screenfade_s.fadeTotalEnd");
    static_assert(o1662 == offsetof(::screenfade_s, fadeReset), "offsetof screenfade_s.fadeReset");
    static_assert(std::is_same_v<t1656, map_t<decltype(::screenfade_s::fadeReset)>>, "type screenfade_s.fadeReset");
    static_assert(o1663 == offsetof(::screenfade_s, fader), "offsetof screenfade_s.fader");
    static_assert(std::is_same_v<t1657, map_t<decltype(::screenfade_s::fader)>>, "type screenfade_s.fader");
    static_assert(o1664 == offsetof(::screenfade_s, fadeg), "offsetof screenfade_s.fadeg");
    static_assert(std::is_same_v<t1658, map_t<decltype(::screenfade_s::fadeg)>>, "type screenfade_s.fadeg");
    static_assert(o1665 == offsetof(::screenfade_s, fadeb), "offsetof screenfade_s.fadeb");
    static_assert(std::is_same_v<t1659, map_t<decltype(::screenfade_s::fadeb)>>, "type screenfade_s.fadeb");
    static_assert(o1666 == offsetof(::screenfade_s, fadealpha), "offsetof screenfade_s.fadealpha");
    static_assert(std::is_same_v<t1660, map_t<decltype(::screenfade_s::fadealpha)>>, "type screenfade_s.fadealpha");
    static_assert(o1667 == offsetof(::screenfade_s, fadeFlags), "offsetof screenfade_s.fadeFlags");
    static_assert(std::is_same_v<t1661, map_t<decltype(::screenfade_s::fadeFlags)>>, "type screenfade_s.fadeFlags");
    static_assert(s114 == sizeof(::IVoiceTweak_s), "sizeof IVoiceTweak_s");
    static_assert(o1668 == offsetof(::IVoiceTweak_s, StartVoiceTweakMode), "offsetof IVoiceTweak_s.StartVoiceTweakMode");
    static_assert(std::is_same_v<t1662, map_t<decltype(::IVoiceTweak_s::StartVoiceTweakMode)>>, "type IVoiceTweak_s.StartVoiceTweakMode");
    static_assert(o1669 == offsetof(::IVoiceTweak_s, EndVoiceTweakMode), "offsetof IVoiceTweak_s.EndVoiceTweakMode");
    static_assert(std::is_same_v<t1663, map_t<decltype(::IVoiceTweak_s::EndVoiceTweakMode)>>, "type IVoiceTweak_s.EndVoiceTweakMode");
    static_assert(o1670 == offsetof(::IVoiceTweak_s, SetControlFloat), "offsetof IVoiceTweak_s.SetControlFloat");
    static_assert(std::is_same_v<t1664, map_t<decltype(::IVoiceTweak_s::SetControlFloat)>>, "type IVoiceTweak_s.SetControlFloat");
    static_assert(o1671 == offsetof(::IVoiceTweak_s, GetControlFloat), "offsetof IVoiceTweak_s.GetControlFloat");
    static_assert(std::is_same_v<t1665, map_t<decltype(::IVoiceTweak_s::GetControlFloat)>>, "type IVoiceTweak_s.GetControlFloat");
    static_assert(o1672 == offsetof(::IVoiceTweak_s, GetSpeakingVolume), "offsetof IVoiceTweak_s.GetSpeakingVolume");
    static_assert(std::is_same_v<t1666, map_t<decltype(::IVoiceTweak_s::GetSpeakingVolume)>>, "type IVoiceTweak_s.GetSpeakingVolume");
    static_assert(c0 == DMG_HALLUC, "value DMG_HALLUC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c0)>, decltype(DMG_HALLUC)>, "type DMG_HALLUC");
    static_assert(c1 == DMG_GENERIC, "value DMG_GENERIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c1)>, decltype(DMG_GENERIC)>, "type DMG_GENERIC");
    static_assert(c2 == DMG_CRUSH, "value DMG_CRUSH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c2)>, decltype(DMG_CRUSH)>, "type DMG_CRUSH");
    static_assert(c3 == DMG_BULLET, "value DMG_BULLET");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c3)>, decltype(DMG_BULLET)>, "type DMG_BULLET");
    static_assert(c4 == DMG_NAIL, "value DMG_NAIL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c4)>, decltype(DMG_NAIL)>, "type DMG_NAIL");
    static_assert(c5 == DMG_SLASH, "value DMG_SLASH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c5)>, decltype(DMG_SLASH)>, "type DMG_SLASH");
    static_assert(c6 == DMG_BURN, "value DMG_BURN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c6)>, decltype(DMG_BURN)>, "type DMG_BURN");
    static_assert(c7 == DMG_FREEZE, "value DMG_FREEZE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c7)>, decltype(DMG_FREEZE)>, "type DMG_FREEZE");
    static_assert(c8 == DMG_NOT_SELF, "value DMG_NOT_SELF");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c8)>, decltype(DMG_NOT_SELF)>, "type DMG_NOT_SELF");
    static_assert(c9 == DMG_FALL, "value DMG_FALL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c9)>, decltype(DMG_FALL)>, "type DMG_FALL");
    static_assert(c10 == DMG_BLAST, "value DMG_BLAST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c10)>, decltype(DMG_BLAST)>, "type DMG_BLAST");
    static_assert(c11 == DMG_CLUB, "value DMG_CLUB");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c11)>, decltype(DMG_CLUB)>, "type DMG_CLUB");
    static_assert(c12 == DMG_SHOCK, "value DMG_SHOCK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c12)>, decltype(DMG_SHOCK)>, "type DMG_SHOCK");
    static_assert(c13 == DMG_CONCUSS, "value DMG_CONCUSS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c13)>, decltype(DMG_CONCUSS)>, "type DMG_CONCUSS");
    static_assert(c14 == DMG_SONIC, "value DMG_SONIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c14)>, decltype(DMG_SONIC)>, "type DMG_SONIC");
    static_assert(c15 == DMG_GIB_CORPSE, "value DMG_GIB_CORPSE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c15)>, decltype(DMG_GIB_CORPSE)>, "type DMG_GIB_CORPSE");
    static_assert(c16 == DMG_ENERGYBEAM, "value DMG_ENERGYBEAM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c16)>, decltype(DMG_ENERGYBEAM)>, "type DMG_ENERGYBEAM");
    static_assert(c17 == DMG_NEVERGIB, "value DMG_NEVERGIB");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c17)>, decltype(DMG_NEVERGIB)>, "type DMG_NEVERGIB");
    static_assert(c18 == DMG_ALWAYSGIB, "value DMG_ALWAYSGIB");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c18)>, decltype(DMG_ALWAYSGIB)>, "type DMG_ALWAYSGIB");
    static_assert(c19 == DMG_DROWN, "value DMG_DROWN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c19)>, decltype(DMG_DROWN)>, "type DMG_DROWN");
    static_assert(c20 == DMG_FIRSTTIMEBASED, "value DMG_FIRSTTIMEBASED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c20)>, decltype(DMG_FIRSTTIMEBASED)>, "type DMG_FIRSTTIMEBASED");
    static_assert(c21 == DMG_PARALYZE, "value DMG_PARALYZE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c21)>, decltype(DMG_PARALYZE)>, "type DMG_PARALYZE");
    static_assert(c22 == DMG_NERVEGAS, "value DMG_NERVEGAS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c22)>, decltype(DMG_NERVEGAS)>, "type DMG_NERVEGAS");
    static_assert(c23 == DMG_POISON, "value DMG_POISON");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c23)>, decltype(DMG_POISON)>, "type DMG_POISON");
    static_assert(c24 == DMG_RADIATION, "value DMG_RADIATION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c24)>, decltype(DMG_RADIATION)>, "type DMG_RADIATION");
    static_assert(c25 == DMG_DROWNRECOVER, "value DMG_DROWNRECOVER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c25)>, decltype(DMG_DROWNRECOVER)>, "type DMG_DROWNRECOVER");
    static_assert(c26 == DMG_ACID, "value DMG_ACID");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c26)>, decltype(DMG_ACID)>, "type DMG_ACID");
    static_assert(c27 == DMG_SLOWBURN, "value DMG_SLOWBURN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c27)>, decltype(DMG_SLOWBURN)>, "type DMG_SLOWBURN");
    static_assert(c28 == DMG_SLOWFREEZE, "value DMG_SLOWFREEZE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c28)>, decltype(DMG_SLOWFREEZE)>, "type DMG_SLOWFREEZE");
    static_assert(c29 == DMG_SHOWNHUD, "value DMG_SHOWNHUD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c29)>, decltype(DMG_SHOWNHUD)>, "type DMG_SHOWNHUD");
    static_assert(c30 == DMG_MORTAR, "value DMG_MORTAR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c30)>, decltype(DMG_MORTAR)>, "type DMG_MORTAR");
    static_assert(c31 == DMG_TRANQ, "value DMG_TRANQ");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c31)>, decltype(DMG_TRANQ)>, "type DMG_TRANQ");
    static_assert(c32 == DMG_TIMEBASED, "value DMG_TIMEBASED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c32)>, decltype(DMG_TIMEBASED)>, "type DMG_TIMEBASED");
    static_assert(c33 == DMG_EXPLOSION, "value DMG_EXPLOSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c33)>, decltype(DMG_EXPLOSION)>, "type DMG_EXPLOSION");
    static_assert(c34 == DMG_IGNITE, "value DMG_IGNITE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c34)>, decltype(DMG_IGNITE)>, "type DMG_IGNITE");
    static_assert(c35 == DMG_IGNORE_MAXHEALTH, "value DMG_IGNORE_MAXHEALTH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c35)>, decltype(DMG_IGNORE_MAXHEALTH)>, "type DMG_IGNORE_MAXHEALTH");
    static_assert(c36 == DMG_RADIUS_MAX, "value DMG_RADIUS_MAX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c36)>, decltype(DMG_RADIUS_MAX)>, "type DMG_RADIUS_MAX");
    static_assert(c37 == DMG_RADIUS_QUAKE, "value DMG_RADIUS_QUAKE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c37)>, decltype(DMG_RADIUS_QUAKE)>, "type DMG_RADIUS_QUAKE");
    static_assert(c38 == DMG_IGNOREARMOR, "value DMG_IGNOREARMOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c38)>, decltype(DMG_IGNOREARMOR)>, "type DMG_IGNOREARMOR");
    static_assert(c39 == DMG_AIMED, "value DMG_AIMED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c39)>, decltype(DMG_AIMED)>, "type DMG_AIMED");
    static_assert(c40 == DMG_WALLPIERCING, "value DMG_WALLPIERCING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c40)>, decltype(DMG_WALLPIERCING)>, "type DMG_WALLPIERCING");
    static_assert(c41 == DMG_CALTROP, "value DMG_CALTROP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c41)>, decltype(DMG_CALTROP)>, "type DMG_CALTROP");
    static_assert(c42 == HIDEHUD_WEAPONS, "value HIDEHUD_WEAPONS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c42)>, decltype(HIDEHUD_WEAPONS)>, "type HIDEHUD_WEAPONS");
    static_assert(c43 == HIDEHUD_FLASHLIGHT, "value HIDEHUD_FLASHLIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c43)>, decltype(HIDEHUD_FLASHLIGHT)>, "type HIDEHUD_FLASHLIGHT");
    static_assert(c44 == HIDEHUD_ALL, "value HIDEHUD_ALL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c44)>, decltype(HIDEHUD_ALL)>, "type HIDEHUD_ALL");
    static_assert(c45 == HIDEHUD_HEALTH, "value HIDEHUD_HEALTH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c45)>, decltype(HIDEHUD_HEALTH)>, "type HIDEHUD_HEALTH");
    static_assert(c46 == HIDEHUD_TIMER, "value HIDEHUD_TIMER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c46)>, decltype(HIDEHUD_TIMER)>, "type HIDEHUD_TIMER");
    static_assert(c47 == HIDEHUD_MONEY, "value HIDEHUD_MONEY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c47)>, decltype(HIDEHUD_MONEY)>, "type HIDEHUD_MONEY");
    static_assert(c48 == HIDEHUD_CROSSHAIR, "value HIDEHUD_CROSSHAIR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c48)>, decltype(HIDEHUD_CROSSHAIR)>, "type HIDEHUD_CROSSHAIR");
    static_assert(c49 == HIDEHUD_OBSERVER_CROSSHAIR, "value HIDEHUD_OBSERVER_CROSSHAIR");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c49)>, decltype(HIDEHUD_OBSERVER_CROSSHAIR)>,
        "type HIDEHUD_OBSERVER_CROSSHAIR"
    );
    static_assert(c50 == TEAM_UNASSIGNED, "value TEAM_UNASSIGNED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c50)>, decltype(TEAM_UNASSIGNED)>, "type TEAM_UNASSIGNED");
    static_assert(c51 == TEAM_TERRORIST, "value TEAM_TERRORIST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c51)>, decltype(TEAM_TERRORIST)>, "type TEAM_TERRORIST");
    static_assert(c52 == TEAM_CT, "value TEAM_CT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c52)>, decltype(TEAM_CT)>, "type TEAM_CT");
    static_assert(c53 == TEAM_SPECTATOR, "value TEAM_SPECTATOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c53)>, decltype(TEAM_SPECTATOR)>, "type TEAM_SPECTATOR");
    static_assert(c54 == WEAPON_SUIT, "value WEAPON_SUIT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c54)>, decltype(WEAPON_SUIT)>, "type WEAPON_SUIT");
    static_assert(c55 == DHN_DRAWZERO, "value DHN_DRAWZERO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c55)>, decltype(DHN_DRAWZERO)>, "type DHN_DRAWZERO");
    static_assert(c56 == DHN_2DIGITS, "value DHN_2DIGITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c56)>, decltype(DHN_2DIGITS)>, "type DHN_2DIGITS");
    static_assert(c57 == DHN_3DIGITS, "value DHN_3DIGITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c57)>, decltype(DHN_3DIGITS)>, "type DHN_3DIGITS");
    static_assert(c58 == DHN_PADZERO, "value DHN_PADZERO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c58)>, decltype(DHN_PADZERO)>, "type DHN_PADZERO");
    static_assert(c59 == DHN_4DIGITS, "value DHN_4DIGITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c59)>, decltype(DHN_4DIGITS)>, "type DHN_4DIGITS");
    static_assert(c60 == DHN_5DIGITS, "value DHN_5DIGITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c60)>, decltype(DHN_5DIGITS)>, "type DHN_5DIGITS");
    static_assert(c61 == DMG_IMAGE_POISON, "value DMG_IMAGE_POISON");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c61)>, decltype(DMG_IMAGE_POISON)>, "type DMG_IMAGE_POISON");
    static_assert(c62 == DMG_IMAGE_ACID, "value DMG_IMAGE_ACID");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c62)>, decltype(DMG_IMAGE_ACID)>, "type DMG_IMAGE_ACID");
    static_assert(c63 == DMG_IMAGE_COLD, "value DMG_IMAGE_COLD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c63)>, decltype(DMG_IMAGE_COLD)>, "type DMG_IMAGE_COLD");
    static_assert(c64 == DMG_IMAGE_LIFE, "value DMG_IMAGE_LIFE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c64)>, decltype(DMG_IMAGE_LIFE)>, "type DMG_IMAGE_LIFE");
    static_assert(c65 == DMG_IMAGE_DROWN, "value DMG_IMAGE_DROWN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c65)>, decltype(DMG_IMAGE_DROWN)>, "type DMG_IMAGE_DROWN");
    static_assert(c66 == DMG_IMAGE_BURN, "value DMG_IMAGE_BURN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c66)>, decltype(DMG_IMAGE_BURN)>, "type DMG_IMAGE_BURN");
    static_assert(c67 == DMG_IMAGE_NERVE, "value DMG_IMAGE_NERVE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c67)>, decltype(DMG_IMAGE_NERVE)>, "type DMG_IMAGE_NERVE");
    static_assert(c68 == DMG_IMAGE_RAD, "value DMG_IMAGE_RAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c68)>, decltype(DMG_IMAGE_RAD)>, "type DMG_IMAGE_RAD");
    static_assert(c69 == DMG_IMAGE_SHOCK, "value DMG_IMAGE_SHOCK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c69)>, decltype(DMG_IMAGE_SHOCK)>, "type DMG_IMAGE_SHOCK");
    static_assert(c70 == DMG_IMAGE_CALTROP, "value DMG_IMAGE_CALTROP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c70)>, decltype(DMG_IMAGE_CALTROP)>, "type DMG_IMAGE_CALTROP");
    static_assert(c71 == DMG_IMAGE_TRANQ, "value DMG_IMAGE_TRANQ");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c71)>, decltype(DMG_IMAGE_TRANQ)>, "type DMG_IMAGE_TRANQ");
    static_assert(c72 == DMG_IMAGE_CONCUSS, "value DMG_IMAGE_CONCUSS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c72)>, decltype(DMG_IMAGE_CONCUSS)>, "type DMG_IMAGE_CONCUSS");
    static_assert(c73 == DMG_IMAGE_HALLUC, "value DMG_IMAGE_HALLUC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c73)>, decltype(DMG_IMAGE_HALLUC)>, "type DMG_IMAGE_HALLUC");
    static_assert(c74 == FADE_TIME, "value FADE_TIME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c74)>, decltype(FADE_TIME)>, "type FADE_TIME");
    static_assert(c75 == MIN_ALPHA, "value MIN_ALPHA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c75)>, decltype(MIN_ALPHA)>, "type MIN_ALPHA");
    static_assert(c76 == NUM_DMG_TYPES, "value NUM_DMG_TYPES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c76)>, decltype(NUM_DMG_TYPES)>, "type NUM_DMG_TYPES");
    static_assert(c77 == RGB_GREENISH, "value RGB_GREENISH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c77)>, decltype(RGB_GREENISH)>, "type RGB_GREENISH");
    static_assert(c78 == RGB_REDISH, "value RGB_REDISH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c78)>, decltype(RGB_REDISH)>, "type RGB_REDISH");
    static_assert(c79 == RGB_YELLOWISH, "value RGB_YELLOWISH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c79)>, decltype(RGB_YELLOWISH)>, "type RGB_YELLOWISH");
    static_assert(c80 == ::MAX_SPRITE_NAME_LENGTH, "value MAX_SPRITE_NAME_LENGTH");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c80)>, std::remove_const_t<decltype(::MAX_SPRITE_NAME_LENGTH)>>,
        "type MAX_SPRITE_NAME_LENGTH"
    );
    static_assert(c81 == INSET_OFF, "value INSET_OFF");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c81)>, decltype(INSET_OFF)>, "type INSET_OFF");
    static_assert(c82 == INSET_CHASE_FREE, "value INSET_CHASE_FREE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c82)>, decltype(INSET_CHASE_FREE)>, "type INSET_CHASE_FREE");
    static_assert(c83 == INSET_IN_EYE, "value INSET_IN_EYE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c83)>, decltype(INSET_IN_EYE)>, "type INSET_IN_EYE");
    static_assert(c84 == INSET_MAP_FREE, "value INSET_MAP_FREE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c84)>, decltype(INSET_MAP_FREE)>, "type INSET_MAP_FREE");
    static_assert(c85 == INSET_MAP_CHASE, "value INSET_MAP_CHASE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c85)>, decltype(INSET_MAP_CHASE)>, "type INSET_MAP_CHASE");
    static_assert(c86 == WEAPON_IS_ONTARGET, "value WEAPON_IS_ONTARGET");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c86)>, decltype(WEAPON_IS_ONTARGET)>, "type WEAPON_IS_ONTARGET");
    static_assert(c87 == FCVAR_ARCHIVE, "value FCVAR_ARCHIVE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c87)>, decltype(FCVAR_ARCHIVE)>, "type FCVAR_ARCHIVE");
    static_assert(c88 == FCVAR_USERINFO, "value FCVAR_USERINFO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c88)>, decltype(FCVAR_USERINFO)>, "type FCVAR_USERINFO");
    static_assert(c89 == FCVAR_SERVER, "value FCVAR_SERVER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c89)>, decltype(FCVAR_SERVER)>, "type FCVAR_SERVER");
    static_assert(c90 == FCVAR_EXTDLL, "value FCVAR_EXTDLL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c90)>, decltype(FCVAR_EXTDLL)>, "type FCVAR_EXTDLL");
    static_assert(c91 == FCVAR_CLIENTDLL, "value FCVAR_CLIENTDLL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c91)>, decltype(FCVAR_CLIENTDLL)>, "type FCVAR_CLIENTDLL");
    static_assert(c92 == FCVAR_PROTECTED, "value FCVAR_PROTECTED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c92)>, decltype(FCVAR_PROTECTED)>, "type FCVAR_PROTECTED");
    static_assert(c93 == FCVAR_SPONLY, "value FCVAR_SPONLY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c93)>, decltype(FCVAR_SPONLY)>, "type FCVAR_SPONLY");
    static_assert(c94 == FCVAR_PRINTABLEONLY, "value FCVAR_PRINTABLEONLY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c94)>, decltype(FCVAR_PRINTABLEONLY)>, "type FCVAR_PRINTABLEONLY");
    static_assert(c95 == FCVAR_UNLOGGED, "value FCVAR_UNLOGGED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c95)>, decltype(FCVAR_UNLOGGED)>, "type FCVAR_UNLOGGED");
    static_assert(c96 == FCVAR_NOEXTRAWHITEPACE, "value FCVAR_NOEXTRAWHITEPACE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c96)>, decltype(FCVAR_NOEXTRAWHITEPACE)>, "type FCVAR_NOEXTRAWHITEPACE");
    static_assert(c97 == FCVAR_PRIVILEGED, "value FCVAR_PRIVILEGED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c97)>, decltype(FCVAR_PRIVILEGED)>, "type FCVAR_PRIVILEGED");
    static_assert(c98 == FCVAR_FILTERABLE, "value FCVAR_FILTERABLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c98)>, decltype(FCVAR_FILTERABLE)>, "type FCVAR_FILTERABLE");
    static_assert(c99 == FCVAR_ISEXECUTED, "value FCVAR_ISEXECUTED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c99)>, decltype(FCVAR_ISEXECUTED)>, "type FCVAR_ISEXECUTED");
    static_assert(c100 == FCVAR_ISPATH, "value FCVAR_ISPATH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c100)>, decltype(FCVAR_ISPATH)>, "type FCVAR_ISPATH");
    static_assert(c101 == FCVAR_CHEAT, "value FCVAR_CHEAT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c101)>, decltype(FCVAR_CHEAT)>, "type FCVAR_CHEAT");
    static_assert(c102 == COM_MAX_CMD_LINE, "value COM_MAX_CMD_LINE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c102)>, decltype(COM_MAX_CMD_LINE)>, "type COM_MAX_CMD_LINE");
    static_assert(c103 == NUM_SAFE_ARGVS, "value NUM_SAFE_ARGVS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c103)>, decltype(NUM_SAFE_ARGVS)>, "type NUM_SAFE_ARGVS");
    static_assert(c104 == IDPOLYHEADER, "value IDPOLYHEADER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c104)>, decltype(IDPOLYHEADER)>, "type IDPOLYHEADER");
    static_assert(c105 == MAX_KNOWN_MODELS, "value MAX_KNOWN_MODELS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c105)>, decltype(MAX_KNOWN_MODELS)>, "type MAX_KNOWN_MODELS");
    static_assert(c106 == SURF_PLANEBACK, "value SURF_PLANEBACK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c106)>, decltype(SURF_PLANEBACK)>, "type SURF_PLANEBACK");
    static_assert(c107 == SURF_DRAWSKY, "value SURF_DRAWSKY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c107)>, decltype(SURF_DRAWSKY)>, "type SURF_DRAWSKY");
    static_assert(c108 == SURF_DRAWSPRITE, "value SURF_DRAWSPRITE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c108)>, decltype(SURF_DRAWSPRITE)>, "type SURF_DRAWSPRITE");
    static_assert(c109 == SURF_DRAWTURB, "value SURF_DRAWTURB");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c109)>, decltype(SURF_DRAWTURB)>, "type SURF_DRAWTURB");
    static_assert(c110 == SURF_DRAWTILED, "value SURF_DRAWTILED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c110)>, decltype(SURF_DRAWTILED)>, "type SURF_DRAWTILED");
    static_assert(c111 == SURF_DRAWBACKGROUND, "value SURF_DRAWBACKGROUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c111)>, decltype(SURF_DRAWBACKGROUND)>, "type SURF_DRAWBACKGROUND");
    static_assert(c112 == SURF_UNDERWATER, "value SURF_UNDERWATER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c112)>, decltype(SURF_UNDERWATER)>, "type SURF_UNDERWATER");
    static_assert(c113 == MAX_MODEL_NAME, "value MAX_MODEL_NAME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c113)>, decltype(MAX_MODEL_NAME)>, "type MAX_MODEL_NAME");
    static_assert(c114 == SIZEBUF_CHECK_OVERFLOW, "value SIZEBUF_CHECK_OVERFLOW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c114)>, decltype(SIZEBUF_CHECK_OVERFLOW)>, "type SIZEBUF_CHECK_OVERFLOW");
    static_assert(c115 == SIZEBUF_ALLOW_OVERFLOW, "value SIZEBUF_ALLOW_OVERFLOW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c115)>, decltype(SIZEBUF_ALLOW_OVERFLOW)>, "type SIZEBUF_ALLOW_OVERFLOW");
    static_assert(c116 == SIZEBUF_OVERFLOWED, "value SIZEBUF_OVERFLOWED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c116)>, decltype(SIZEBUF_OVERFLOWED)>, "type SIZEBUF_OVERFLOWED");
    static_assert(c117 == FCUST_FROMHPAK, "value FCUST_FROMHPAK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c117)>, decltype(FCUST_FROMHPAK)>, "type FCUST_FROMHPAK");
    static_assert(c118 == FCUST_WIPEDATA, "value FCUST_WIPEDATA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c118)>, decltype(FCUST_WIPEDATA)>, "type FCUST_WIPEDATA");
    static_assert(c119 == FCUST_IGNOREINIT, "value FCUST_IGNOREINIT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c119)>, decltype(FCUST_IGNOREINIT)>, "type FCUST_IGNOREINIT");
    static_assert(c120 == MAX_RESOURCE_LIST, "value MAX_RESOURCE_LIST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c120)>, decltype(MAX_RESOURCE_LIST)>, "type MAX_RESOURCE_LIST");
    static_assert(c121 == RES_FATALIFMISSING, "value RES_FATALIFMISSING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c121)>, decltype(RES_FATALIFMISSING)>, "type RES_FATALIFMISSING");
    static_assert(c122 == RES_WASMISSING, "value RES_WASMISSING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c122)>, decltype(RES_WASMISSING)>, "type RES_WASMISSING");
    static_assert(c123 == RES_CUSTOM, "value RES_CUSTOM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c123)>, decltype(RES_CUSTOM)>, "type RES_CUSTOM");
    static_assert(c124 == RES_REQUESTED, "value RES_REQUESTED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c124)>, decltype(RES_REQUESTED)>, "type RES_REQUESTED");
    static_assert(c125 == RES_PRECACHED, "value RES_PRECACHED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c125)>, decltype(RES_PRECACHED)>, "type RES_PRECACHED");
    static_assert(c126 == RES_ALWAYS, "value RES_ALWAYS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c126)>, decltype(RES_ALWAYS)>, "type RES_ALWAYS");
    static_assert(c127 == RES_UNK_6, "value RES_UNK_6");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c127)>, decltype(RES_UNK_6)>, "type RES_UNK_6");
    static_assert(c128 == RES_CHECKFILE, "value RES_CHECKFILE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c128)>, decltype(RES_CHECKFILE)>, "type RES_CHECKFILE");
    static_assert(c129 == CHAN_AUTO, "value CHAN_AUTO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c129)>, decltype(CHAN_AUTO)>, "type CHAN_AUTO");
    static_assert(c130 == CHAN_WEAPON, "value CHAN_WEAPON");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c130)>, decltype(CHAN_WEAPON)>, "type CHAN_WEAPON");
    static_assert(c131 == CHAN_VOICE, "value CHAN_VOICE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c131)>, decltype(CHAN_VOICE)>, "type CHAN_VOICE");
    static_assert(c132 == CHAN_ITEM, "value CHAN_ITEM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c132)>, decltype(CHAN_ITEM)>, "type CHAN_ITEM");
    static_assert(c133 == CHAN_BODY, "value CHAN_BODY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c133)>, decltype(CHAN_BODY)>, "type CHAN_BODY");
    static_assert(c134 == CHAN_STREAM, "value CHAN_STREAM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c134)>, decltype(CHAN_STREAM)>, "type CHAN_STREAM");
    static_assert(c135 == CHAN_STATIC, "value CHAN_STATIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c135)>, decltype(CHAN_STATIC)>, "type CHAN_STATIC");
    static_assert(c136 == CHAN_NETWORKVOICE_BASE, "value CHAN_NETWORKVOICE_BASE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c136)>, decltype(CHAN_NETWORKVOICE_BASE)>, "type CHAN_NETWORKVOICE_BASE");
    static_assert(c137 == CHAN_NETWORKVOICE_END, "value CHAN_NETWORKVOICE_END");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c137)>, decltype(CHAN_NETWORKVOICE_END)>, "type CHAN_NETWORKVOICE_END");
    static_assert(c138 == CHAN_BOT, "value CHAN_BOT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c138)>, decltype(CHAN_BOT)>, "type CHAN_BOT");
    static_assert(c139 == ::CVOXFILESENTENCEMAX, "value CVOXFILESENTENCEMAX");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c139)>, std::remove_const_t<decltype(::CVOXFILESENTENCEMAX)>>,
        "type CVOXFILESENTENCEMAX"
    );
    static_assert(std::string_view(c140) == std::string_view(OFF_SVC_MESSAGES_P), "value OFF_SVC_MESSAGES_P");
    static_assert(c141 == EF_BRIGHTFIELD, "value EF_BRIGHTFIELD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c141)>, decltype(EF_BRIGHTFIELD)>, "type EF_BRIGHTFIELD");
    static_assert(c142 == EF_MUZZLEFLASH, "value EF_MUZZLEFLASH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c142)>, decltype(EF_MUZZLEFLASH)>, "type EF_MUZZLEFLASH");
    static_assert(c143 == EF_BRIGHTLIGHT, "value EF_BRIGHTLIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c143)>, decltype(EF_BRIGHTLIGHT)>, "type EF_BRIGHTLIGHT");
    static_assert(c144 == EF_DIMLIGHT, "value EF_DIMLIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c144)>, decltype(EF_DIMLIGHT)>, "type EF_DIMLIGHT");
    static_assert(c145 == EF_INVLIGHT, "value EF_INVLIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c145)>, decltype(EF_INVLIGHT)>, "type EF_INVLIGHT");
    static_assert(c146 == EF_NOINTERP, "value EF_NOINTERP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c146)>, decltype(EF_NOINTERP)>, "type EF_NOINTERP");
    static_assert(c147 == EF_LIGHT, "value EF_LIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c147)>, decltype(EF_LIGHT)>, "type EF_LIGHT");
    static_assert(c148 == EF_NODRAW, "value EF_NODRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c148)>, decltype(EF_NODRAW)>, "type EF_NODRAW");
    static_assert(c149 == EF_NIGHTVISION, "value EF_NIGHTVISION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c149)>, decltype(EF_NIGHTVISION)>, "type EF_NIGHTVISION");
    static_assert(c150 == EF_SNIPERLASER, "value EF_SNIPERLASER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c150)>, decltype(EF_SNIPERLASER)>, "type EF_SNIPERLASER");
    static_assert(c151 == EF_FIBERCAMERA, "value EF_FIBERCAMERA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c151)>, decltype(EF_FIBERCAMERA)>, "type EF_FIBERCAMERA");
    static_assert(c152 == FL_DORMANT, "value FL_DORMANT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c152)>, decltype(FL_DORMANT)>, "type FL_DORMANT");
    static_assert(c153 == FL_FLY, "value FL_FLY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c153)>, decltype(FL_FLY)>, "type FL_FLY");
    static_assert(c154 == FL_SWIM, "value FL_SWIM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c154)>, decltype(FL_SWIM)>, "type FL_SWIM");
    static_assert(c155 == FL_CONVEYOR, "value FL_CONVEYOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c155)>, decltype(FL_CONVEYOR)>, "type FL_CONVEYOR");
    static_assert(c156 == FL_CLIENT, "value FL_CLIENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c156)>, decltype(FL_CLIENT)>, "type FL_CLIENT");
    static_assert(c157 == FL_INWATER, "value FL_INWATER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c157)>, decltype(FL_INWATER)>, "type FL_INWATER");
    static_assert(c158 == FL_MONSTER, "value FL_MONSTER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c158)>, decltype(FL_MONSTER)>, "type FL_MONSTER");
    static_assert(c159 == FL_GODMODE, "value FL_GODMODE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c159)>, decltype(FL_GODMODE)>, "type FL_GODMODE");
    static_assert(c160 == FL_NOTARGET, "value FL_NOTARGET");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c160)>, decltype(FL_NOTARGET)>, "type FL_NOTARGET");
    static_assert(c161 == FL_SKIPLOCALHOST, "value FL_SKIPLOCALHOST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c161)>, decltype(FL_SKIPLOCALHOST)>, "type FL_SKIPLOCALHOST");
    static_assert(c162 == FL_ONGROUND, "value FL_ONGROUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c162)>, decltype(FL_ONGROUND)>, "type FL_ONGROUND");
    static_assert(c163 == FL_PARTIALGROUND, "value FL_PARTIALGROUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c163)>, decltype(FL_PARTIALGROUND)>, "type FL_PARTIALGROUND");
    static_assert(c164 == FL_WATERJUMP, "value FL_WATERJUMP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c164)>, decltype(FL_WATERJUMP)>, "type FL_WATERJUMP");
    static_assert(c165 == FL_FROZEN, "value FL_FROZEN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c165)>, decltype(FL_FROZEN)>, "type FL_FROZEN");
    static_assert(c166 == FL_FAKECLIENT, "value FL_FAKECLIENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c166)>, decltype(FL_FAKECLIENT)>, "type FL_FAKECLIENT");
    static_assert(c167 == FL_DUCKING, "value FL_DUCKING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c167)>, decltype(FL_DUCKING)>, "type FL_DUCKING");
    static_assert(c168 == FL_FLOAT, "value FL_FLOAT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c168)>, decltype(FL_FLOAT)>, "type FL_FLOAT");
    static_assert(c169 == FL_GRAPHED, "value FL_GRAPHED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c169)>, decltype(FL_GRAPHED)>, "type FL_GRAPHED");
    static_assert(c170 == FL_IMMUNE_WATER, "value FL_IMMUNE_WATER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c170)>, decltype(FL_IMMUNE_WATER)>, "type FL_IMMUNE_WATER");
    static_assert(c171 == FL_IMMUNE_SLIME, "value FL_IMMUNE_SLIME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c171)>, decltype(FL_IMMUNE_SLIME)>, "type FL_IMMUNE_SLIME");
    static_assert(c172 == FL_IMMUNE_LAVA, "value FL_IMMUNE_LAVA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c172)>, decltype(FL_IMMUNE_LAVA)>, "type FL_IMMUNE_LAVA");
    static_assert(c173 == FL_PROXY, "value FL_PROXY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c173)>, decltype(FL_PROXY)>, "type FL_PROXY");
    static_assert(c174 == FL_ALWAYSTHINK, "value FL_ALWAYSTHINK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c174)>, decltype(FL_ALWAYSTHINK)>, "type FL_ALWAYSTHINK");
    static_assert(c175 == FL_BASEVELOCITY, "value FL_BASEVELOCITY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c175)>, decltype(FL_BASEVELOCITY)>, "type FL_BASEVELOCITY");
    static_assert(c176 == FL_MONSTERCLIP, "value FL_MONSTERCLIP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c176)>, decltype(FL_MONSTERCLIP)>, "type FL_MONSTERCLIP");
    static_assert(c177 == FL_ONTRAIN, "value FL_ONTRAIN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c177)>, decltype(FL_ONTRAIN)>, "type FL_ONTRAIN");
    static_assert(c178 == FL_WORLDBRUSH, "value FL_WORLDBRUSH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c178)>, decltype(FL_WORLDBRUSH)>, "type FL_WORLDBRUSH");
    static_assert(c179 == FL_SPECTATOR, "value FL_SPECTATOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c179)>, decltype(FL_SPECTATOR)>, "type FL_SPECTATOR");
    static_assert(c180 == FL_CUSTOMENTITY, "value FL_CUSTOMENTITY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c180)>, decltype(FL_CUSTOMENTITY)>, "type FL_CUSTOMENTITY");
    static_assert(c181 == FL_KILLME, "value FL_KILLME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c181)>, decltype(FL_KILLME)>, "type FL_KILLME");
    static_assert(c182 == HISTORY_MASK, "value HISTORY_MASK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c182)>, decltype(HISTORY_MASK)>, "type HISTORY_MASK");
    static_assert(c183 == HISTORY_MAX, "value HISTORY_MAX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c183)>, decltype(HISTORY_MAX)>, "type HISTORY_MAX");
    static_assert(c184 == MOVETYPE_NONE, "value MOVETYPE_NONE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c184)>, decltype(MOVETYPE_NONE)>, "type MOVETYPE_NONE");
    static_assert(c185 == MOVETYPE_WALK, "value MOVETYPE_WALK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c185)>, decltype(MOVETYPE_WALK)>, "type MOVETYPE_WALK");
    static_assert(c186 == MOVETYPE_STEP, "value MOVETYPE_STEP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c186)>, decltype(MOVETYPE_STEP)>, "type MOVETYPE_STEP");
    static_assert(c187 == MOVETYPE_FLY, "value MOVETYPE_FLY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c187)>, decltype(MOVETYPE_FLY)>, "type MOVETYPE_FLY");
    static_assert(c188 == MOVETYPE_TOSS, "value MOVETYPE_TOSS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c188)>, decltype(MOVETYPE_TOSS)>, "type MOVETYPE_TOSS");
    static_assert(c189 == MOVETYPE_PUSH, "value MOVETYPE_PUSH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c189)>, decltype(MOVETYPE_PUSH)>, "type MOVETYPE_PUSH");
    static_assert(c190 == MOVETYPE_NOCLIP, "value MOVETYPE_NOCLIP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c190)>, decltype(MOVETYPE_NOCLIP)>, "type MOVETYPE_NOCLIP");
    static_assert(c191 == MOVETYPE_FLYMISSILE, "value MOVETYPE_FLYMISSILE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c191)>, decltype(MOVETYPE_FLYMISSILE)>, "type MOVETYPE_FLYMISSILE");
    static_assert(c192 == MOVETYPE_BOUNCE, "value MOVETYPE_BOUNCE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c192)>, decltype(MOVETYPE_BOUNCE)>, "type MOVETYPE_BOUNCE");
    static_assert(c193 == MOVETYPE_BOUNCEMISSILE, "value MOVETYPE_BOUNCEMISSILE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c193)>, decltype(MOVETYPE_BOUNCEMISSILE)>, "type MOVETYPE_BOUNCEMISSILE");
    static_assert(c194 == MOVETYPE_FOLLOW, "value MOVETYPE_FOLLOW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c194)>, decltype(MOVETYPE_FOLLOW)>, "type MOVETYPE_FOLLOW");
    static_assert(c195 == MOVETYPE_PUSHSTEP, "value MOVETYPE_PUSHSTEP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c195)>, decltype(MOVETYPE_PUSHSTEP)>, "type MOVETYPE_PUSHSTEP");
    static_assert(c196 == HLBSP_VERSION, "value HLBSP_VERSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c196)>, decltype(HLBSP_VERSION)>, "type HLBSP_VERSION");
    static_assert(c197 == Q1BSP_VERSION, "value Q1BSP_VERSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c197)>, decltype(Q1BSP_VERSION)>, "type Q1BSP_VERSION");
    static_assert(c198 == IDSPRITEHEADER, "value IDSPRITEHEADER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c198)>, decltype(IDSPRITEHEADER)>, "type IDSPRITEHEADER");
    static_assert(c199 == SPRITE_VERSION, "value SPRITE_VERSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c199)>, decltype(SPRITE_VERSION)>, "type SPRITE_VERSION");
    static_assert(c200 == MAXSTUDIOSKINS, "value MAXSTUDIOSKINS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c200)>, decltype(MAXSTUDIOSKINS)>, "type MAXSTUDIOSKINS");
    static_assert(c201 == STUDIO_HAS_NORMALS, "value STUDIO_HAS_NORMALS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c201)>, decltype(STUDIO_HAS_NORMALS)>, "type STUDIO_HAS_NORMALS");
    static_assert(c202 == STUDIO_LOOPING, "value STUDIO_LOOPING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c202)>, decltype(STUDIO_LOOPING)>, "type STUDIO_LOOPING");
    static_assert(c203 == STUDIO_NF_FLATSHADE, "value STUDIO_NF_FLATSHADE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c203)>, decltype(STUDIO_NF_FLATSHADE)>, "type STUDIO_NF_FLATSHADE");
    static_assert(c204 == STUDIO_X, "value STUDIO_X");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c204)>, decltype(STUDIO_X)>, "type STUDIO_X");
    static_assert(c205 == STUDIO_HAS_VERTICES, "value STUDIO_HAS_VERTICES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c205)>, decltype(STUDIO_HAS_VERTICES)>, "type STUDIO_HAS_VERTICES");
    static_assert(c206 == STUDIO_NF_CHROME, "value STUDIO_NF_CHROME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c206)>, decltype(STUDIO_NF_CHROME)>, "type STUDIO_NF_CHROME");
    static_assert(c207 == STUDIO_Y, "value STUDIO_Y");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c207)>, decltype(STUDIO_Y)>, "type STUDIO_Y");
    static_assert(c208 == STUDIO_HAS_BBOX, "value STUDIO_HAS_BBOX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c208)>, decltype(STUDIO_HAS_BBOX)>, "type STUDIO_HAS_BBOX");
    static_assert(c209 == STUDIO_NF_FULLBRIGHT, "value STUDIO_NF_FULLBRIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c209)>, decltype(STUDIO_NF_FULLBRIGHT)>, "type STUDIO_NF_FULLBRIGHT");
    static_assert(c210 == STUDIO_Z, "value STUDIO_Z");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c210)>, decltype(STUDIO_Z)>, "type STUDIO_Z");
    static_assert(c211 == STUDIO_HAS_CHROME, "value STUDIO_HAS_CHROME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c211)>, decltype(STUDIO_HAS_CHROME)>, "type STUDIO_HAS_CHROME");
    static_assert(c212 == STUDIO_NF_NOMIPS, "value STUDIO_NF_NOMIPS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c212)>, decltype(STUDIO_NF_NOMIPS)>, "type STUDIO_NF_NOMIPS");
    static_assert(c213 == STUDIO_XR, "value STUDIO_XR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c213)>, decltype(STUDIO_XR)>, "type STUDIO_XR");
    static_assert(c214 == STUDIO_NF_ALPHA, "value STUDIO_NF_ALPHA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c214)>, decltype(STUDIO_NF_ALPHA)>, "type STUDIO_NF_ALPHA");
    static_assert(c215 == STUDIO_YR, "value STUDIO_YR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c215)>, decltype(STUDIO_YR)>, "type STUDIO_YR");
    static_assert(c216 == STUDIO_NF_ADDITIVE, "value STUDIO_NF_ADDITIVE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c216)>, decltype(STUDIO_NF_ADDITIVE)>, "type STUDIO_NF_ADDITIVE");
    static_assert(c217 == STUDIO_ZR, "value STUDIO_ZR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c217)>, decltype(STUDIO_ZR)>, "type STUDIO_ZR");
    static_assert(c218 == STUDIO_LX, "value STUDIO_LX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c218)>, decltype(STUDIO_LX)>, "type STUDIO_LX");
    static_assert(c219 == STUDIO_NF_MASKED, "value STUDIO_NF_MASKED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c219)>, decltype(STUDIO_NF_MASKED)>, "type STUDIO_NF_MASKED");
    static_assert(c220 == STUDIO_LY, "value STUDIO_LY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c220)>, decltype(STUDIO_LY)>, "type STUDIO_LY");
    static_assert(c221 == STUDIO_DYNAMIC_LIGHT, "value STUDIO_DYNAMIC_LIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c221)>, decltype(STUDIO_DYNAMIC_LIGHT)>, "type STUDIO_DYNAMIC_LIGHT");
    static_assert(c222 == STUDIO_LZ, "value STUDIO_LZ");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c222)>, decltype(STUDIO_LZ)>, "type STUDIO_LZ");
    static_assert(c223 == STUDIO_AX, "value STUDIO_AX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c223)>, decltype(STUDIO_AX)>, "type STUDIO_AX");
    static_assert(c224 == STUDIO_TRACE_HITBOX, "value STUDIO_TRACE_HITBOX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c224)>, decltype(STUDIO_TRACE_HITBOX)>, "type STUDIO_TRACE_HITBOX");
    static_assert(c225 == STUDIO_AY, "value STUDIO_AY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c225)>, decltype(STUDIO_AY)>, "type STUDIO_AY");
    static_assert(c226 == STUDIO_AZ, "value STUDIO_AZ");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c226)>, decltype(STUDIO_AZ)>, "type STUDIO_AZ");
    static_assert(c227 == STUDIO_AXR, "value STUDIO_AXR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c227)>, decltype(STUDIO_AXR)>, "type STUDIO_AXR");
    static_assert(c228 == STUDIO_AYR, "value STUDIO_AYR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c228)>, decltype(STUDIO_AYR)>, "type STUDIO_AYR");
    static_assert(c229 == STUDIO_AZR, "value STUDIO_AZR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c229)>, decltype(STUDIO_AZR)>, "type STUDIO_AZR");
    static_assert(c230 == STUDIO_TYPES, "value STUDIO_TYPES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c230)>, decltype(STUDIO_TYPES)>, "type STUDIO_TYPES");
    static_assert(c231 == STUDIO_RLOOP, "value STUDIO_RLOOP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c231)>, decltype(STUDIO_RLOOP)>, "type STUDIO_RLOOP");
    static_assert(c232 == STUDIO_CACHE_SIZE, "value STUDIO_CACHE_SIZE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c232)>, decltype(STUDIO_CACHE_SIZE)>, "type STUDIO_CACHE_SIZE");
    static_assert(c233 == STUDIO_MOUTH, "value STUDIO_MOUTH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c233)>, decltype(STUDIO_MOUTH)>, "type STUDIO_MOUTH");
    static_assert(c234 == STUDIO_NUM_HULLS, "value STUDIO_NUM_HULLS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c234)>, decltype(STUDIO_NUM_HULLS)>, "type STUDIO_NUM_HULLS");
    static_assert(c235 == STUDIO_NUM_PLANES, "value STUDIO_NUM_PLANES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c235)>, decltype(STUDIO_NUM_PLANES)>, "type STUDIO_NUM_PLANES");
    static_assert(c236 == STUDIO_TO_RAD, "value STUDIO_TO_RAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c236)>, decltype(STUDIO_TO_RAD)>, "type STUDIO_TO_RAD");
    static_assert(c237 == DRC_CMD_NONE, "value DRC_CMD_NONE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c237)>, decltype(DRC_CMD_NONE)>, "type DRC_CMD_NONE");
    static_assert(c238 == DRC_CMD_START, "value DRC_CMD_START");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c238)>, decltype(DRC_CMD_START)>, "type DRC_CMD_START");
    static_assert(c239 == DRC_CMD_EVENT, "value DRC_CMD_EVENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c239)>, decltype(DRC_CMD_EVENT)>, "type DRC_CMD_EVENT");
    static_assert(c240 == DRC_CMD_MODE, "value DRC_CMD_MODE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c240)>, decltype(DRC_CMD_MODE)>, "type DRC_CMD_MODE");
    static_assert(c241 == DRC_CMD_CAMERA, "value DRC_CMD_CAMERA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c241)>, decltype(DRC_CMD_CAMERA)>, "type DRC_CMD_CAMERA");
    static_assert(c242 == DRC_CMD_TIMESCALE, "value DRC_CMD_TIMESCALE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c242)>, decltype(DRC_CMD_TIMESCALE)>, "type DRC_CMD_TIMESCALE");
    static_assert(c243 == DRC_CMD_MESSAGE, "value DRC_CMD_MESSAGE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c243)>, decltype(DRC_CMD_MESSAGE)>, "type DRC_CMD_MESSAGE");
    static_assert(c244 == DRC_CMD_SOUND, "value DRC_CMD_SOUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c244)>, decltype(DRC_CMD_SOUND)>, "type DRC_CMD_SOUND");
    static_assert(c245 == DRC_CMD_STATUS, "value DRC_CMD_STATUS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c245)>, decltype(DRC_CMD_STATUS)>, "type DRC_CMD_STATUS");
    static_assert(c246 == DRC_CMD_BANNER, "value DRC_CMD_BANNER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c246)>, decltype(DRC_CMD_BANNER)>, "type DRC_CMD_BANNER");
    static_assert(c247 == DRC_CMD_STUFFTEXT, "value DRC_CMD_STUFFTEXT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c247)>, decltype(DRC_CMD_STUFFTEXT)>, "type DRC_CMD_STUFFTEXT");
    static_assert(c248 == DRC_CMD_CHASE, "value DRC_CMD_CHASE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c248)>, decltype(DRC_CMD_CHASE)>, "type DRC_CMD_CHASE");
    static_assert(c249 == DRC_CMD_INEYE, "value DRC_CMD_INEYE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c249)>, decltype(DRC_CMD_INEYE)>, "type DRC_CMD_INEYE");
    static_assert(c250 == DRC_CMD_MAP, "value DRC_CMD_MAP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c250)>, decltype(DRC_CMD_MAP)>, "type DRC_CMD_MAP");
    static_assert(c251 == DRC_CMD_CAMPATH, "value DRC_CMD_CAMPATH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c251)>, decltype(DRC_CMD_CAMPATH)>, "type DRC_CMD_CAMPATH");
    static_assert(c252 == DRC_CMD_LAST, "value DRC_CMD_LAST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c252)>, decltype(DRC_CMD_LAST)>, "type DRC_CMD_LAST");
    static_assert(c253 == DRC_CMD_WAYPOINTS, "value DRC_CMD_WAYPOINTS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c253)>, decltype(DRC_CMD_WAYPOINTS)>, "type DRC_CMD_WAYPOINTS");
    static_assert(c254 == DRC_FLAG_STARTPATH, "value DRC_FLAG_STARTPATH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c254)>, decltype(DRC_FLAG_STARTPATH)>, "type DRC_FLAG_STARTPATH");
    static_assert(c255 == DRC_FLAG_SLOWSTART, "value DRC_FLAG_SLOWSTART");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c255)>, decltype(DRC_FLAG_SLOWSTART)>, "type DRC_FLAG_SLOWSTART");
    static_assert(c256 == DRC_FLAG_SLOWEND, "value DRC_FLAG_SLOWEND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c256)>, decltype(DRC_FLAG_SLOWEND)>, "type DRC_FLAG_SLOWEND");
    static_assert(c257 == DRC_FLAG_PRIO_MASK, "value DRC_FLAG_PRIO_MASK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c257)>, decltype(DRC_FLAG_PRIO_MASK)>, "type DRC_FLAG_PRIO_MASK");
    static_assert(c258 == DRC_FLAG_SIDE, "value DRC_FLAG_SIDE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c258)>, decltype(DRC_FLAG_SIDE)>, "type DRC_FLAG_SIDE");
    static_assert(c259 == DRC_FLAG_DRAMATIC, "value DRC_FLAG_DRAMATIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c259)>, decltype(DRC_FLAG_DRAMATIC)>, "type DRC_FLAG_DRAMATIC");
    static_assert(c260 == DRC_FLAG_SLOWMOTION, "value DRC_FLAG_SLOWMOTION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c260)>, decltype(DRC_FLAG_SLOWMOTION)>, "type DRC_FLAG_SLOWMOTION");
    static_assert(c261 == DRC_FLAG_FACEPLAYER, "value DRC_FLAG_FACEPLAYER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c261)>, decltype(DRC_FLAG_FACEPLAYER)>, "type DRC_FLAG_FACEPLAYER");
    static_assert(c262 == DRC_FLAG_INTRO, "value DRC_FLAG_INTRO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c262)>, decltype(DRC_FLAG_INTRO)>, "type DRC_FLAG_INTRO");
    static_assert(c263 == DRC_FLAG_FINAL, "value DRC_FLAG_FINAL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c263)>, decltype(DRC_FLAG_FINAL)>, "type DRC_FLAG_FINAL");
    static_assert(c264 == DRC_FLAG_NO_RANDOM, "value DRC_FLAG_NO_RANDOM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c264)>, decltype(DRC_FLAG_NO_RANDOM)>, "type DRC_FLAG_NO_RANDOM");
    static_assert(c265 == HLTV_ACTIVE, "value HLTV_ACTIVE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c265)>, decltype(HLTV_ACTIVE)>, "type HLTV_ACTIVE");
    static_assert(c266 == HLTV_STATUS, "value HLTV_STATUS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c266)>, decltype(HLTV_STATUS)>, "type HLTV_STATUS");
    static_assert(c267 == HLTV_LISTEN, "value HLTV_LISTEN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c267)>, decltype(HLTV_LISTEN)>, "type HLTV_LISTEN");
    static_assert(c268 == TYPE_CLIENT, "value TYPE_CLIENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c268)>, decltype(TYPE_CLIENT)>, "type TYPE_CLIENT");
    static_assert(c269 == TYPE_PROXY, "value TYPE_PROXY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c269)>, decltype(TYPE_PROXY)>, "type TYPE_PROXY");
    static_assert(c270 == TYPE_DIRECTOR, "value TYPE_DIRECTOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c270)>, decltype(TYPE_DIRECTOR)>, "type TYPE_DIRECTOR");
    static_assert(c271 == TYPE_COMMENTATOR, "value TYPE_COMMENTATOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c271)>, decltype(TYPE_COMMENTATOR)>, "type TYPE_COMMENTATOR");
    static_assert(c272 == TYPE_DEMO, "value TYPE_DEMO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c272)>, decltype(TYPE_DEMO)>, "type TYPE_DEMO");
    static_assert(c273 == K_TAB, "value K_TAB");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c273)>, decltype(K_TAB)>, "type K_TAB");
    static_assert(c274 == K_ENTER, "value K_ENTER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c274)>, decltype(K_ENTER)>, "type K_ENTER");
    static_assert(c275 == K_ESCAPE, "value K_ESCAPE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c275)>, decltype(K_ESCAPE)>, "type K_ESCAPE");
    static_assert(c276 == K_SPACE, "value K_SPACE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c276)>, decltype(K_SPACE)>, "type K_SPACE");
    static_assert(c277 == K_BACKSPACE, "value K_BACKSPACE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c277)>, decltype(K_BACKSPACE)>, "type K_BACKSPACE");
    static_assert(c278 == K_UPARROW, "value K_UPARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c278)>, decltype(K_UPARROW)>, "type K_UPARROW");
    static_assert(c279 == K_DOWNARROW, "value K_DOWNARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c279)>, decltype(K_DOWNARROW)>, "type K_DOWNARROW");
    static_assert(c280 == K_LEFTARROW, "value K_LEFTARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c280)>, decltype(K_LEFTARROW)>, "type K_LEFTARROW");
    static_assert(c281 == K_RIGHTARROW, "value K_RIGHTARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c281)>, decltype(K_RIGHTARROW)>, "type K_RIGHTARROW");
    static_assert(c282 == K_ALT, "value K_ALT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c282)>, decltype(K_ALT)>, "type K_ALT");
    static_assert(c283 == K_CTRL, "value K_CTRL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c283)>, decltype(K_CTRL)>, "type K_CTRL");
    static_assert(c284 == K_SHIFT, "value K_SHIFT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c284)>, decltype(K_SHIFT)>, "type K_SHIFT");
    static_assert(c285 == K_F1, "value K_F1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c285)>, decltype(K_F1)>, "type K_F1");
    static_assert(c286 == K_F2, "value K_F2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c286)>, decltype(K_F2)>, "type K_F2");
    static_assert(c287 == K_F3, "value K_F3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c287)>, decltype(K_F3)>, "type K_F3");
    static_assert(c288 == K_F4, "value K_F4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c288)>, decltype(K_F4)>, "type K_F4");
    static_assert(c289 == K_F5, "value K_F5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c289)>, decltype(K_F5)>, "type K_F5");
    static_assert(c290 == K_F6, "value K_F6");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c290)>, decltype(K_F6)>, "type K_F6");
    static_assert(c291 == K_F7, "value K_F7");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c291)>, decltype(K_F7)>, "type K_F7");
    static_assert(c292 == K_F8, "value K_F8");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c292)>, decltype(K_F8)>, "type K_F8");
    static_assert(c293 == K_F9, "value K_F9");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c293)>, decltype(K_F9)>, "type K_F9");
    static_assert(c294 == K_F10, "value K_F10");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c294)>, decltype(K_F10)>, "type K_F10");
    static_assert(c295 == K_F11, "value K_F11");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c295)>, decltype(K_F11)>, "type K_F11");
    static_assert(c296 == K_F12, "value K_F12");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c296)>, decltype(K_F12)>, "type K_F12");
    static_assert(c297 == K_INS, "value K_INS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c297)>, decltype(K_INS)>, "type K_INS");
    static_assert(c298 == K_DEL, "value K_DEL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c298)>, decltype(K_DEL)>, "type K_DEL");
    static_assert(c299 == K_PGDN, "value K_PGDN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c299)>, decltype(K_PGDN)>, "type K_PGDN");
    static_assert(c300 == K_PGUP, "value K_PGUP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c300)>, decltype(K_PGUP)>, "type K_PGUP");
    static_assert(c301 == K_HOME, "value K_HOME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c301)>, decltype(K_HOME)>, "type K_HOME");
    static_assert(c302 == K_END, "value K_END");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c302)>, decltype(K_END)>, "type K_END");
    static_assert(c303 == K_KP_HOME, "value K_KP_HOME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c303)>, decltype(K_KP_HOME)>, "type K_KP_HOME");
    static_assert(c304 == K_KP_UPARROW, "value K_KP_UPARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c304)>, decltype(K_KP_UPARROW)>, "type K_KP_UPARROW");
    static_assert(c305 == K_KP_PGUP, "value K_KP_PGUP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c305)>, decltype(K_KP_PGUP)>, "type K_KP_PGUP");
    static_assert(c306 == K_KP_LEFTARROW, "value K_KP_LEFTARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c306)>, decltype(K_KP_LEFTARROW)>, "type K_KP_LEFTARROW");
    static_assert(c307 == K_KP_5, "value K_KP_5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c307)>, decltype(K_KP_5)>, "type K_KP_5");
    static_assert(c308 == K_KP_RIGHTARROW, "value K_KP_RIGHTARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c308)>, decltype(K_KP_RIGHTARROW)>, "type K_KP_RIGHTARROW");
    static_assert(c309 == K_KP_END, "value K_KP_END");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c309)>, decltype(K_KP_END)>, "type K_KP_END");
    static_assert(c310 == K_KP_DOWNARROW, "value K_KP_DOWNARROW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c310)>, decltype(K_KP_DOWNARROW)>, "type K_KP_DOWNARROW");
    static_assert(c311 == K_KP_PGDN, "value K_KP_PGDN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c311)>, decltype(K_KP_PGDN)>, "type K_KP_PGDN");
    static_assert(c312 == K_KP_ENTER, "value K_KP_ENTER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c312)>, decltype(K_KP_ENTER)>, "type K_KP_ENTER");
    static_assert(c313 == K_KP_INS, "value K_KP_INS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c313)>, decltype(K_KP_INS)>, "type K_KP_INS");
    static_assert(c314 == K_KP_DEL, "value K_KP_DEL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c314)>, decltype(K_KP_DEL)>, "type K_KP_DEL");
    static_assert(c315 == K_KP_SLASH, "value K_KP_SLASH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c315)>, decltype(K_KP_SLASH)>, "type K_KP_SLASH");
    static_assert(c316 == K_KP_MINUS, "value K_KP_MINUS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c316)>, decltype(K_KP_MINUS)>, "type K_KP_MINUS");
    static_assert(c317 == K_KP_PLUS, "value K_KP_PLUS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c317)>, decltype(K_KP_PLUS)>, "type K_KP_PLUS");
    static_assert(c318 == K_CAPSLOCK, "value K_CAPSLOCK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c318)>, decltype(K_CAPSLOCK)>, "type K_CAPSLOCK");
    static_assert(c319 == K_JOY1, "value K_JOY1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c319)>, decltype(K_JOY1)>, "type K_JOY1");
    static_assert(c320 == K_JOY2, "value K_JOY2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c320)>, decltype(K_JOY2)>, "type K_JOY2");
    static_assert(c321 == K_JOY3, "value K_JOY3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c321)>, decltype(K_JOY3)>, "type K_JOY3");
    static_assert(c322 == K_JOY4, "value K_JOY4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c322)>, decltype(K_JOY4)>, "type K_JOY4");
    static_assert(c323 == K_AUX1, "value K_AUX1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c323)>, decltype(K_AUX1)>, "type K_AUX1");
    static_assert(c324 == K_AUX2, "value K_AUX2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c324)>, decltype(K_AUX2)>, "type K_AUX2");
    static_assert(c325 == K_AUX3, "value K_AUX3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c325)>, decltype(K_AUX3)>, "type K_AUX3");
    static_assert(c326 == K_AUX4, "value K_AUX4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c326)>, decltype(K_AUX4)>, "type K_AUX4");
    static_assert(c327 == K_AUX5, "value K_AUX5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c327)>, decltype(K_AUX5)>, "type K_AUX5");
    static_assert(c328 == K_AUX6, "value K_AUX6");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c328)>, decltype(K_AUX6)>, "type K_AUX6");
    static_assert(c329 == K_AUX7, "value K_AUX7");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c329)>, decltype(K_AUX7)>, "type K_AUX7");
    static_assert(c330 == K_AUX8, "value K_AUX8");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c330)>, decltype(K_AUX8)>, "type K_AUX8");
    static_assert(c331 == K_AUX9, "value K_AUX9");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c331)>, decltype(K_AUX9)>, "type K_AUX9");
    static_assert(c332 == K_AUX10, "value K_AUX10");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c332)>, decltype(K_AUX10)>, "type K_AUX10");
    static_assert(c333 == K_AUX11, "value K_AUX11");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c333)>, decltype(K_AUX11)>, "type K_AUX11");
    static_assert(c334 == K_AUX12, "value K_AUX12");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c334)>, decltype(K_AUX12)>, "type K_AUX12");
    static_assert(c335 == K_AUX13, "value K_AUX13");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c335)>, decltype(K_AUX13)>, "type K_AUX13");
    static_assert(c336 == K_AUX14, "value K_AUX14");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c336)>, decltype(K_AUX14)>, "type K_AUX14");
    static_assert(c337 == K_AUX15, "value K_AUX15");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c337)>, decltype(K_AUX15)>, "type K_AUX15");
    static_assert(c338 == K_AUX16, "value K_AUX16");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c338)>, decltype(K_AUX16)>, "type K_AUX16");
    static_assert(c339 == K_AUX17, "value K_AUX17");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c339)>, decltype(K_AUX17)>, "type K_AUX17");
    static_assert(c340 == K_AUX18, "value K_AUX18");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c340)>, decltype(K_AUX18)>, "type K_AUX18");
    static_assert(c341 == K_AUX19, "value K_AUX19");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c341)>, decltype(K_AUX19)>, "type K_AUX19");
    static_assert(c342 == K_AUX20, "value K_AUX20");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c342)>, decltype(K_AUX20)>, "type K_AUX20");
    static_assert(c343 == K_AUX21, "value K_AUX21");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c343)>, decltype(K_AUX21)>, "type K_AUX21");
    static_assert(c344 == K_AUX22, "value K_AUX22");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c344)>, decltype(K_AUX22)>, "type K_AUX22");
    static_assert(c345 == K_AUX23, "value K_AUX23");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c345)>, decltype(K_AUX23)>, "type K_AUX23");
    static_assert(c346 == K_AUX24, "value K_AUX24");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c346)>, decltype(K_AUX24)>, "type K_AUX24");
    static_assert(c347 == K_AUX25, "value K_AUX25");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c347)>, decltype(K_AUX25)>, "type K_AUX25");
    static_assert(c348 == K_AUX26, "value K_AUX26");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c348)>, decltype(K_AUX26)>, "type K_AUX26");
    static_assert(c349 == K_AUX27, "value K_AUX27");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c349)>, decltype(K_AUX27)>, "type K_AUX27");
    static_assert(c350 == K_AUX28, "value K_AUX28");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c350)>, decltype(K_AUX28)>, "type K_AUX28");
    static_assert(c351 == K_AUX29, "value K_AUX29");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c351)>, decltype(K_AUX29)>, "type K_AUX29");
    static_assert(c352 == K_AUX30, "value K_AUX30");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c352)>, decltype(K_AUX30)>, "type K_AUX30");
    static_assert(c353 == K_AUX31, "value K_AUX31");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c353)>, decltype(K_AUX31)>, "type K_AUX31");
    static_assert(c354 == K_AUX32, "value K_AUX32");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c354)>, decltype(K_AUX32)>, "type K_AUX32");
    static_assert(c355 == K_MWHEELDOWN, "value K_MWHEELDOWN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c355)>, decltype(K_MWHEELDOWN)>, "type K_MWHEELDOWN");
    static_assert(c356 == K_MWHEELUP, "value K_MWHEELUP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c356)>, decltype(K_MWHEELUP)>, "type K_MWHEELUP");
    static_assert(c357 == K_MOUSE1, "value K_MOUSE1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c357)>, decltype(K_MOUSE1)>, "type K_MOUSE1");
    static_assert(c358 == K_MOUSE2, "value K_MOUSE2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c358)>, decltype(K_MOUSE2)>, "type K_MOUSE2");
    static_assert(c359 == K_MOUSE3, "value K_MOUSE3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c359)>, decltype(K_MOUSE3)>, "type K_MOUSE3");
    static_assert(c360 == K_MOUSE4, "value K_MOUSE4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c360)>, decltype(K_MOUSE4)>, "type K_MOUSE4");
    static_assert(c361 == K_MOUSE5, "value K_MOUSE5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c361)>, decltype(K_MOUSE5)>, "type K_MOUSE5");
    static_assert(c362 == K_PAUSE, "value K_PAUSE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c362)>, decltype(K_PAUSE)>, "type K_PAUSE");
    static_assert(c363 == MAX_LIGHTSTYLE_INDEX_BITS, "value MAX_LIGHTSTYLE_INDEX_BITS");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c363)>, decltype(MAX_LIGHTSTYLE_INDEX_BITS)>,
        "type MAX_LIGHTSTYLE_INDEX_BITS"
    );
    static_assert(c364 == MAX_DECAL_INDEX_BITS, "value MAX_DECAL_INDEX_BITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c364)>, decltype(MAX_DECAL_INDEX_BITS)>, "type MAX_DECAL_INDEX_BITS");
    static_assert(c365 == MAX_GENERIC_INDEX_BITS, "value MAX_GENERIC_INDEX_BITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c365)>, decltype(MAX_GENERIC_INDEX_BITS)>, "type MAX_GENERIC_INDEX_BITS");
    static_assert(c366 == MAX_MODEL_INDEX_BITS, "value MAX_MODEL_INDEX_BITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c366)>, decltype(MAX_MODEL_INDEX_BITS)>, "type MAX_MODEL_INDEX_BITS");
    static_assert(c367 == MAX_SOUND_INDEX_BITS, "value MAX_SOUND_INDEX_BITS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c367)>, decltype(MAX_SOUND_INDEX_BITS)>, "type MAX_SOUND_INDEX_BITS");
    static_assert(c368 == MAX_NUM_ARGVS, "value MAX_NUM_ARGVS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c368)>, decltype(MAX_NUM_ARGVS)>, "type MAX_NUM_ARGVS");
    static_assert(c369 == MAX_LIGHTSTYLES, "value MAX_LIGHTSTYLES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c369)>, decltype(MAX_LIGHTSTYLES)>, "type MAX_LIGHTSTYLES");
    static_assert(c370 == MAX_QPATH, "value MAX_QPATH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c370)>, decltype(MAX_QPATH)>, "type MAX_QPATH");
    static_assert(c371 == MAX_EVENTS, "value MAX_EVENTS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c371)>, decltype(MAX_EVENTS)>, "type MAX_EVENTS");
    static_assert(c372 == MAX_PACKET_ENTITIES, "value MAX_PACKET_ENTITIES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c372)>, decltype(MAX_PACKET_ENTITIES)>, "type MAX_PACKET_ENTITIES");
    static_assert(c373 == MAX_OSPATH, "value MAX_OSPATH");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c373)>, decltype(MAX_OSPATH)>, "type MAX_OSPATH");
    static_assert(c374 == MAX_BASE_DECALS, "value MAX_BASE_DECALS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c374)>, decltype(MAX_BASE_DECALS)>, "type MAX_BASE_DECALS");
    static_assert(c375 == MAX_GENERIC, "value MAX_GENERIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c375)>, decltype(MAX_GENERIC)>, "type MAX_GENERIC");
    static_assert(c376 == MAX_MODELS, "value MAX_MODELS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c376)>, decltype(MAX_MODELS)>, "type MAX_MODELS");
    static_assert(c377 == MAX_SOUNDS, "value MAX_SOUNDS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c377)>, decltype(MAX_SOUNDS)>, "type MAX_SOUNDS");
    static_assert(c378 == MAX_SOUNDS_HASHLOOKUP_SIZE, "value MAX_SOUNDS_HASHLOOKUP_SIZE");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c378)>, decltype(MAX_SOUNDS_HASHLOOKUP_SIZE)>,
        "type MAX_SOUNDS_HASHLOOKUP_SIZE"
    );
    static_assert(c379 == MAX_CLIENTS, "value MAX_CLIENTS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c379)>, decltype(MAX_CLIENTS)>, "type MAX_CLIENTS");
    static_assert(c380 == PM_NORMAL, "value PM_NORMAL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c380)>, decltype(PM_NORMAL)>, "type PM_NORMAL");
    static_assert(c381 == PM_TRACELINE_PHYSENTSONLY, "value PM_TRACELINE_PHYSENTSONLY");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c381)>, decltype(PM_TRACELINE_PHYSENTSONLY)>,
        "type PM_TRACELINE_PHYSENTSONLY"
    );
    static_assert(c382 == PM_STUDIO_IGNORE, "value PM_STUDIO_IGNORE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c382)>, decltype(PM_STUDIO_IGNORE)>, "type PM_STUDIO_IGNORE");
    static_assert(c383 == PM_TRACELINE_ANYVISIBLE, "value PM_TRACELINE_ANYVISIBLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c383)>, decltype(PM_TRACELINE_ANYVISIBLE)>, "type PM_TRACELINE_ANYVISIBLE");
    static_assert(c384 == PM_STUDIO_BOX, "value PM_STUDIO_BOX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c384)>, decltype(PM_STUDIO_BOX)>, "type PM_STUDIO_BOX");
    static_assert(c385 == PM_GLASS_IGNORE, "value PM_GLASS_IGNORE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c385)>, decltype(PM_GLASS_IGNORE)>, "type PM_GLASS_IGNORE");
    static_assert(c386 == PM_WORLD_ONLY, "value PM_WORLD_ONLY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c386)>, decltype(PM_WORLD_ONLY)>, "type PM_WORLD_ONLY");
    static_assert(c387 == PM_CHECKSTUCK_MINTIME, "value PM_CHECKSTUCK_MINTIME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c387)>, decltype(PM_CHECKSTUCK_MINTIME)>, "type PM_CHECKSTUCK_MINTIME");
    static_assert(c388 == PM_DEAD_VIEWHEIGHT, "value PM_DEAD_VIEWHEIGHT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c388)>, decltype(PM_DEAD_VIEWHEIGHT)>, "type PM_DEAD_VIEWHEIGHT");
    static_assert(c389 == PM_PLAYER_FALL_PUNCH_THRESHHOLD, "value PM_PLAYER_FALL_PUNCH_THRESHHOLD");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c389)>, decltype(PM_PLAYER_FALL_PUNCH_THRESHHOLD)>,
        "type PM_PLAYER_FALL_PUNCH_THRESHHOLD"
    );
    static_assert(c390 == PM_PLAYER_MIN_BOUNCE_SPEED, "value PM_PLAYER_MIN_BOUNCE_SPEED");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c390)>, decltype(PM_PLAYER_MIN_BOUNCE_SPEED)>,
        "type PM_PLAYER_MIN_BOUNCE_SPEED"
    );
    static_assert(c391 == PM_PLAYER_MAX_SAFE_FALL_SPEED, "value PM_PLAYER_MAX_SAFE_FALL_SPEED");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c391)>, decltype(PM_PLAYER_MAX_SAFE_FALL_SPEED)>,
        "type PM_PLAYER_MAX_SAFE_FALL_SPEED"
    );
    static_assert(c392 == PM_VEC_HULL_MIN, "value PM_VEC_HULL_MIN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c392)>, decltype(PM_VEC_HULL_MIN)>, "type PM_VEC_HULL_MIN");
    static_assert(c393 == PM_VEC_DUCK_HULL_MIN, "value PM_VEC_DUCK_HULL_MIN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c393)>, decltype(PM_VEC_DUCK_HULL_MIN)>, "type PM_VEC_DUCK_HULL_MIN");
    static_assert(c394 == PM_VEC_DUCK_VIEW, "value PM_VEC_DUCK_VIEW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c394)>, decltype(PM_VEC_DUCK_VIEW)>, "type PM_VEC_DUCK_VIEW");
    static_assert(c395 == PM_VEC_VIEW, "value PM_VEC_VIEW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c395)>, decltype(PM_VEC_VIEW)>, "type PM_VEC_VIEW");
    static_assert(c396 == SVC_BAD, "value SVC_BAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c396)>, decltype(SVC_BAD)>, "type SVC_BAD");
    static_assert(c397 == SVC_NOP, "value SVC_NOP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c397)>, decltype(SVC_NOP)>, "type SVC_NOP");
    static_assert(c398 == SVC_DISCONNECT, "value SVC_DISCONNECT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c398)>, decltype(SVC_DISCONNECT)>, "type SVC_DISCONNECT");
    static_assert(c399 == SVC_EVENT, "value SVC_EVENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c399)>, decltype(SVC_EVENT)>, "type SVC_EVENT");
    static_assert(c400 == SVC_VERSION, "value SVC_VERSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c400)>, decltype(SVC_VERSION)>, "type SVC_VERSION");
    static_assert(c401 == SVC_SETVIEW, "value SVC_SETVIEW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c401)>, decltype(SVC_SETVIEW)>, "type SVC_SETVIEW");
    static_assert(c402 == SVC_SOUND, "value SVC_SOUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c402)>, decltype(SVC_SOUND)>, "type SVC_SOUND");
    static_assert(c403 == SVC_TIME, "value SVC_TIME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c403)>, decltype(SVC_TIME)>, "type SVC_TIME");
    static_assert(c404 == SVC_PRINT, "value SVC_PRINT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c404)>, decltype(SVC_PRINT)>, "type SVC_PRINT");
    static_assert(c405 == SVC_STUFFTEXT, "value SVC_STUFFTEXT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c405)>, decltype(SVC_STUFFTEXT)>, "type SVC_STUFFTEXT");
    static_assert(c406 == SVC_SETANGLE, "value SVC_SETANGLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c406)>, decltype(SVC_SETANGLE)>, "type SVC_SETANGLE");
    static_assert(c407 == SVC_SERVERINFO, "value SVC_SERVERINFO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c407)>, decltype(SVC_SERVERINFO)>, "type SVC_SERVERINFO");
    static_assert(c408 == SVC_LIGHTSTYLE, "value SVC_LIGHTSTYLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c408)>, decltype(SVC_LIGHTSTYLE)>, "type SVC_LIGHTSTYLE");
    static_assert(c409 == SVC_UPDATEUSERINFO, "value SVC_UPDATEUSERINFO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c409)>, decltype(SVC_UPDATEUSERINFO)>, "type SVC_UPDATEUSERINFO");
    static_assert(c410 == SVC_DELTADESCRIPTION, "value SVC_DELTADESCRIPTION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c410)>, decltype(SVC_DELTADESCRIPTION)>, "type SVC_DELTADESCRIPTION");
    static_assert(c411 == SVC_CLIENTDATA, "value SVC_CLIENTDATA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c411)>, decltype(SVC_CLIENTDATA)>, "type SVC_CLIENTDATA");
    static_assert(c412 == SVC_STOPSOUND, "value SVC_STOPSOUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c412)>, decltype(SVC_STOPSOUND)>, "type SVC_STOPSOUND");
    static_assert(c413 == SVC_PINGS, "value SVC_PINGS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c413)>, decltype(SVC_PINGS)>, "type SVC_PINGS");
    static_assert(c414 == SVC_PARTICLE, "value SVC_PARTICLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c414)>, decltype(SVC_PARTICLE)>, "type SVC_PARTICLE");
    static_assert(c415 == SVC_DAMAGE, "value SVC_DAMAGE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c415)>, decltype(SVC_DAMAGE)>, "type SVC_DAMAGE");
    static_assert(c416 == SVC_SPAWNSTATIC, "value SVC_SPAWNSTATIC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c416)>, decltype(SVC_SPAWNSTATIC)>, "type SVC_SPAWNSTATIC");
    static_assert(c417 == SVC_EVENT_RELIABLE, "value SVC_EVENT_RELIABLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c417)>, decltype(SVC_EVENT_RELIABLE)>, "type SVC_EVENT_RELIABLE");
    static_assert(c418 == SVC_SPAWNBASELINE, "value SVC_SPAWNBASELINE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c418)>, decltype(SVC_SPAWNBASELINE)>, "type SVC_SPAWNBASELINE");
    static_assert(c419 == SVC_TEMPENTITY, "value SVC_TEMPENTITY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c419)>, decltype(SVC_TEMPENTITY)>, "type SVC_TEMPENTITY");
    static_assert(c420 == SVC_SETPAUSE, "value SVC_SETPAUSE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c420)>, decltype(SVC_SETPAUSE)>, "type SVC_SETPAUSE");
    static_assert(c421 == SVC_SIGNONNUM, "value SVC_SIGNONNUM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c421)>, decltype(SVC_SIGNONNUM)>, "type SVC_SIGNONNUM");
    static_assert(c422 == SVC_CENTERPRINT, "value SVC_CENTERPRINT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c422)>, decltype(SVC_CENTERPRINT)>, "type SVC_CENTERPRINT");
    static_assert(c423 == SVC_KILLEDMONSTER, "value SVC_KILLEDMONSTER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c423)>, decltype(SVC_KILLEDMONSTER)>, "type SVC_KILLEDMONSTER");
    static_assert(c424 == SVC_FOUNDSECRET, "value SVC_FOUNDSECRET");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c424)>, decltype(SVC_FOUNDSECRET)>, "type SVC_FOUNDSECRET");
    static_assert(c425 == SVC_SPAWNSTATICSOUND, "value SVC_SPAWNSTATICSOUND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c425)>, decltype(SVC_SPAWNSTATICSOUND)>, "type SVC_SPAWNSTATICSOUND");
    static_assert(c426 == SVC_INTERMISSION, "value SVC_INTERMISSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c426)>, decltype(SVC_INTERMISSION)>, "type SVC_INTERMISSION");
    static_assert(c427 == SVC_FINALE, "value SVC_FINALE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c427)>, decltype(SVC_FINALE)>, "type SVC_FINALE");
    static_assert(c428 == SVC_CDTRACK, "value SVC_CDTRACK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c428)>, decltype(SVC_CDTRACK)>, "type SVC_CDTRACK");
    static_assert(c429 == SVC_RESTORE, "value SVC_RESTORE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c429)>, decltype(SVC_RESTORE)>, "type SVC_RESTORE");
    static_assert(c430 == SVC_CUTSCENE, "value SVC_CUTSCENE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c430)>, decltype(SVC_CUTSCENE)>, "type SVC_CUTSCENE");
    static_assert(c431 == SVC_WEAPONANIM, "value SVC_WEAPONANIM");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c431)>, decltype(SVC_WEAPONANIM)>, "type SVC_WEAPONANIM");
    static_assert(c432 == SVC_DECALNAME, "value SVC_DECALNAME");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c432)>, decltype(SVC_DECALNAME)>, "type SVC_DECALNAME");
    static_assert(c433 == SVC_ROOMTYPE, "value SVC_ROOMTYPE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c433)>, decltype(SVC_ROOMTYPE)>, "type SVC_ROOMTYPE");
    static_assert(c434 == SVC_ADDANGLE, "value SVC_ADDANGLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c434)>, decltype(SVC_ADDANGLE)>, "type SVC_ADDANGLE");
    static_assert(c435 == SVC_NEWUSERMSG, "value SVC_NEWUSERMSG");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c435)>, decltype(SVC_NEWUSERMSG)>, "type SVC_NEWUSERMSG");
    static_assert(c436 == SVC_PACKETENTITIES, "value SVC_PACKETENTITIES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c436)>, decltype(SVC_PACKETENTITIES)>, "type SVC_PACKETENTITIES");
    static_assert(c437 == SVC_DELTAPACKETENTITIES, "value SVC_DELTAPACKETENTITIES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c437)>, decltype(SVC_DELTAPACKETENTITIES)>, "type SVC_DELTAPACKETENTITIES");
    static_assert(c438 == SVC_CHOKE, "value SVC_CHOKE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c438)>, decltype(SVC_CHOKE)>, "type SVC_CHOKE");
    static_assert(c439 == SVC_RESOURCELIST, "value SVC_RESOURCELIST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c439)>, decltype(SVC_RESOURCELIST)>, "type SVC_RESOURCELIST");
    static_assert(c440 == SVC_NEWMOVEVARS, "value SVC_NEWMOVEVARS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c440)>, decltype(SVC_NEWMOVEVARS)>, "type SVC_NEWMOVEVARS");
    static_assert(c441 == SVC_RESOURCEREQUEST, "value SVC_RESOURCEREQUEST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c441)>, decltype(SVC_RESOURCEREQUEST)>, "type SVC_RESOURCEREQUEST");
    static_assert(c442 == SVC_CUSTOMIZATION, "value SVC_CUSTOMIZATION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c442)>, decltype(SVC_CUSTOMIZATION)>, "type SVC_CUSTOMIZATION");
    static_assert(c443 == SVC_CROSSHAIRANGLE, "value SVC_CROSSHAIRANGLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c443)>, decltype(SVC_CROSSHAIRANGLE)>, "type SVC_CROSSHAIRANGLE");
    static_assert(c444 == SVC_SOUNDFADE, "value SVC_SOUNDFADE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c444)>, decltype(SVC_SOUNDFADE)>, "type SVC_SOUNDFADE");
    static_assert(c445 == SVC_FILETXFERFAILED, "value SVC_FILETXFERFAILED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c445)>, decltype(SVC_FILETXFERFAILED)>, "type SVC_FILETXFERFAILED");
    static_assert(c446 == SVC_HLTV, "value SVC_HLTV");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c446)>, decltype(SVC_HLTV)>, "type SVC_HLTV");
    static_assert(c447 == SVC_DIRECTOR, "value SVC_DIRECTOR");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c447)>, decltype(SVC_DIRECTOR)>, "type SVC_DIRECTOR");
    static_assert(c448 == SVC_VOICEINIT, "value SVC_VOICEINIT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c448)>, decltype(SVC_VOICEINIT)>, "type SVC_VOICEINIT");
    static_assert(c449 == SVC_VOICEDATA, "value SVC_VOICEDATA");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c449)>, decltype(SVC_VOICEDATA)>, "type SVC_VOICEDATA");
    static_assert(c450 == SVC_SENDEXTRAINFO, "value SVC_SENDEXTRAINFO");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c450)>, decltype(SVC_SENDEXTRAINFO)>, "type SVC_SENDEXTRAINFO");
    static_assert(c451 == SVC_TIMESCALE, "value SVC_TIMESCALE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c451)>, decltype(SVC_TIMESCALE)>, "type SVC_TIMESCALE");
    static_assert(c452 == SVC_RESOURCELOCATION, "value SVC_RESOURCELOCATION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c452)>, decltype(SVC_RESOURCELOCATION)>, "type SVC_RESOURCELOCATION");
    static_assert(c453 == SVC_SENDCVARVALUE, "value SVC_SENDCVARVALUE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c453)>, decltype(SVC_SENDCVARVALUE)>, "type SVC_SENDCVARVALUE");
    static_assert(c454 == SVC_SENDCVARVALUE2, "value SVC_SENDCVARVALUE2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c454)>, decltype(SVC_SENDCVARVALUE2)>, "type SVC_SENDCVARVALUE2");
    static_assert(c455 == STUDIO_RENDER, "value STUDIO_RENDER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c455)>, decltype(STUDIO_RENDER)>, "type STUDIO_RENDER");
    static_assert(c456 == STUDIO_EVENTS, "value STUDIO_EVENTS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c456)>, decltype(STUDIO_EVENTS)>, "type STUDIO_EVENTS");
    static_assert(c457 == TRI_TRIANGLES, "value TRI_TRIANGLES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c457)>, decltype(TRI_TRIANGLES)>, "type TRI_TRIANGLES");
    static_assert(c458 == TRI_API_VERSION, "value TRI_API_VERSION");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c458)>, decltype(TRI_API_VERSION)>, "type TRI_API_VERSION");
    static_assert(c459 == TRI_TRIANGLE_FAN, "value TRI_TRIANGLE_FAN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c459)>, decltype(TRI_TRIANGLE_FAN)>, "type TRI_TRIANGLE_FAN");
    static_assert(c460 == TRI_QUADS, "value TRI_QUADS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c460)>, decltype(TRI_QUADS)>, "type TRI_QUADS");
    static_assert(c461 == TRI_POLYGON, "value TRI_POLYGON");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c461)>, decltype(TRI_POLYGON)>, "type TRI_POLYGON");
    static_assert(c462 == TRI_LINES, "value TRI_LINES");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c462)>, decltype(TRI_LINES)>, "type TRI_LINES");
    static_assert(c463 == TRI_TRIANGLE_STRIP, "value TRI_TRIANGLE_STRIP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c463)>, decltype(TRI_TRIANGLE_STRIP)>, "type TRI_TRIANGLE_STRIP");
    static_assert(c464 == TRI_QUAD_STRIP, "value TRI_QUAD_STRIP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c464)>, decltype(TRI_QUAD_STRIP)>, "type TRI_QUAD_STRIP");
    static_assert(static_cast<long long>(c465) == static_cast<long long>(::MAX_PLAYERS), "value ::MAX_PLAYERS");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c465)>, std::underlying_type_t<decltype(::MAX_PLAYERS)>>,
        "type ::MAX_PLAYERS"
    );
    static_assert(static_cast<long long>(c466) == static_cast<long long>(::MAX_TEAMS), "value ::MAX_TEAMS");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c466)>, std::underlying_type_t<decltype(::MAX_TEAMS)>>,
        "type ::MAX_TEAMS"
    );
    static_assert(static_cast<long long>(c467) == static_cast<long long>(::MAX_TEAM_NAME), "value ::MAX_TEAM_NAME");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c467)>, std::underlying_type_t<decltype(::MAX_TEAM_NAME)>>,
        "type ::MAX_TEAM_NAME"
    );
    static_assert(static_cast<long long>(c468) == static_cast<long long>(::MAX_LOCATION_NAME), "value ::MAX_LOCATION_NAME");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c468)>, std::underlying_type_t<decltype(::MAX_LOCATION_NAME)>>,
        "type ::MAX_LOCATION_NAME"
    );
    static_assert(static_cast<long long>(c469) == static_cast<long long>(::MAX_HOSTAGES), "value ::MAX_HOSTAGES");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c469)>, std::underlying_type_t<decltype(::MAX_HOSTAGES)>>,
        "type ::MAX_HOSTAGES"
    );
    static_assert(static_cast<long long>(c470) == static_cast<long long>(::WEAPON_NONE), "value ::WEAPON_NONE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c470)>, map_t<decltype(::WEAPON_NONE)>>, "type ::WEAPON_NONE");
    static_assert(static_cast<long long>(c471) == static_cast<long long>(::WEAPON_P228), "value ::WEAPON_P228");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c471)>, map_t<decltype(::WEAPON_P228)>>, "type ::WEAPON_P228");
    static_assert(static_cast<long long>(c472) == static_cast<long long>(::WEAPON_GLOCK), "value ::WEAPON_GLOCK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c472)>, map_t<decltype(::WEAPON_GLOCK)>>, "type ::WEAPON_GLOCK");
    static_assert(static_cast<long long>(c473) == static_cast<long long>(::WEAPON_SCOUT), "value ::WEAPON_SCOUT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c473)>, map_t<decltype(::WEAPON_SCOUT)>>, "type ::WEAPON_SCOUT");
    static_assert(static_cast<long long>(c474) == static_cast<long long>(::WEAPON_HEGRENADE), "value ::WEAPON_HEGRENADE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c474)>, map_t<decltype(::WEAPON_HEGRENADE)>>, "type ::WEAPON_HEGRENADE");
    static_assert(static_cast<long long>(c475) == static_cast<long long>(::WEAPON_XM1014), "value ::WEAPON_XM1014");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c475)>, map_t<decltype(::WEAPON_XM1014)>>, "type ::WEAPON_XM1014");
    static_assert(static_cast<long long>(c476) == static_cast<long long>(::WEAPON_C4), "value ::WEAPON_C4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c476)>, map_t<decltype(::WEAPON_C4)>>, "type ::WEAPON_C4");
    static_assert(static_cast<long long>(c477) == static_cast<long long>(::WEAPON_MAC10), "value ::WEAPON_MAC10");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c477)>, map_t<decltype(::WEAPON_MAC10)>>, "type ::WEAPON_MAC10");
    static_assert(static_cast<long long>(c478) == static_cast<long long>(::WEAPON_AUG), "value ::WEAPON_AUG");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c478)>, map_t<decltype(::WEAPON_AUG)>>, "type ::WEAPON_AUG");
    static_assert(static_cast<long long>(c479) == static_cast<long long>(::WEAPON_SMOKEGRENADE), "value ::WEAPON_SMOKEGRENADE");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c479)>, map_t<decltype(::WEAPON_SMOKEGRENADE)>>,
        "type ::WEAPON_SMOKEGRENADE"
    );
    static_assert(static_cast<long long>(c480) == static_cast<long long>(::WEAPON_ELITE), "value ::WEAPON_ELITE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c480)>, map_t<decltype(::WEAPON_ELITE)>>, "type ::WEAPON_ELITE");
    static_assert(static_cast<long long>(c481) == static_cast<long long>(::WEAPON_FIVESEVEN), "value ::WEAPON_FIVESEVEN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c481)>, map_t<decltype(::WEAPON_FIVESEVEN)>>, "type ::WEAPON_FIVESEVEN");
    static_assert(static_cast<long long>(c482) == static_cast<long long>(::WEAPON_UMP45), "value ::WEAPON_UMP45");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c482)>, map_t<decltype(::WEAPON_UMP45)>>, "type ::WEAPON_UMP45");
    static_assert(static_cast<long long>(c483) == static_cast<long long>(::WEAPON_SG550), "value ::WEAPON_SG550");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c483)>, map_t<decltype(::WEAPON_SG550)>>, "type ::WEAPON_SG550");
    static_assert(static_cast<long long>(c484) == static_cast<long long>(::WEAPON_GALIL), "value ::WEAPON_GALIL");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c484)>, map_t<decltype(::WEAPON_GALIL)>>, "type ::WEAPON_GALIL");
    static_assert(static_cast<long long>(c485) == static_cast<long long>(::WEAPON_FAMAS), "value ::WEAPON_FAMAS");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c485)>, map_t<decltype(::WEAPON_FAMAS)>>, "type ::WEAPON_FAMAS");
    static_assert(static_cast<long long>(c486) == static_cast<long long>(::WEAPON_USP), "value ::WEAPON_USP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c486)>, map_t<decltype(::WEAPON_USP)>>, "type ::WEAPON_USP");
    static_assert(static_cast<long long>(c487) == static_cast<long long>(::WEAPON_GLOCK18), "value ::WEAPON_GLOCK18");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c487)>, map_t<decltype(::WEAPON_GLOCK18)>>, "type ::WEAPON_GLOCK18");
    static_assert(static_cast<long long>(c488) == static_cast<long long>(::WEAPON_AWP), "value ::WEAPON_AWP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c488)>, map_t<decltype(::WEAPON_AWP)>>, "type ::WEAPON_AWP");
    static_assert(static_cast<long long>(c489) == static_cast<long long>(::WEAPON_MP5N), "value ::WEAPON_MP5N");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c489)>, map_t<decltype(::WEAPON_MP5N)>>, "type ::WEAPON_MP5N");
    static_assert(static_cast<long long>(c490) == static_cast<long long>(::WEAPON_M249), "value ::WEAPON_M249");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c490)>, map_t<decltype(::WEAPON_M249)>>, "type ::WEAPON_M249");
    static_assert(static_cast<long long>(c491) == static_cast<long long>(::WEAPON_M3), "value ::WEAPON_M3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c491)>, map_t<decltype(::WEAPON_M3)>>, "type ::WEAPON_M3");
    static_assert(static_cast<long long>(c492) == static_cast<long long>(::WEAPON_M4A1), "value ::WEAPON_M4A1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c492)>, map_t<decltype(::WEAPON_M4A1)>>, "type ::WEAPON_M4A1");
    static_assert(static_cast<long long>(c493) == static_cast<long long>(::WEAPON_TMP), "value ::WEAPON_TMP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c493)>, map_t<decltype(::WEAPON_TMP)>>, "type ::WEAPON_TMP");
    static_assert(static_cast<long long>(c494) == static_cast<long long>(::WEAPON_G3SG1), "value ::WEAPON_G3SG1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c494)>, map_t<decltype(::WEAPON_G3SG1)>>, "type ::WEAPON_G3SG1");
    static_assert(static_cast<long long>(c495) == static_cast<long long>(::WEAPON_FLASHBANG), "value ::WEAPON_FLASHBANG");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c495)>, map_t<decltype(::WEAPON_FLASHBANG)>>, "type ::WEAPON_FLASHBANG");
    static_assert(static_cast<long long>(c496) == static_cast<long long>(::WEAPON_DEAGLE), "value ::WEAPON_DEAGLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c496)>, map_t<decltype(::WEAPON_DEAGLE)>>, "type ::WEAPON_DEAGLE");
    static_assert(static_cast<long long>(c497) == static_cast<long long>(::WEAPON_SG552), "value ::WEAPON_SG552");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c497)>, map_t<decltype(::WEAPON_SG552)>>, "type ::WEAPON_SG552");
    static_assert(static_cast<long long>(c498) == static_cast<long long>(::WEAPON_AK47), "value ::WEAPON_AK47");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c498)>, map_t<decltype(::WEAPON_AK47)>>, "type ::WEAPON_AK47");
    static_assert(static_cast<long long>(c499) == static_cast<long long>(::WEAPON_KNIFE), "value ::WEAPON_KNIFE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c499)>, map_t<decltype(::WEAPON_KNIFE)>>, "type ::WEAPON_KNIFE");
    static_assert(static_cast<long long>(c500) == static_cast<long long>(::WEAPON_P90), "value ::WEAPON_P90");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c500)>, map_t<decltype(::WEAPON_P90)>>, "type ::WEAPON_P90");
    static_assert(static_cast<long long>(c501) == static_cast<long long>(::WEAPON_SHIELDGUN), "value ::WEAPON_SHIELDGUN");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c501)>, map_t<decltype(::WEAPON_SHIELDGUN)>>, "type ::WEAPON_SHIELDGUN");
    static_assert(static_cast<long long>(c502) == static_cast<long long>(::WPNSTATE_USP_SILENCED), "value ::WPNSTATE_USP_SILENCED");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c502)>, map_t<decltype(::WPNSTATE_USP_SILENCED)>>,
        "type ::WPNSTATE_USP_SILENCED"
    );
    static_assert(
        static_cast<long long>(c503) == static_cast<long long>(::WPNSTATE_GLOCK18_BURST_MODE),
        "value ::WPNSTATE_GLOCK18_BURST_MODE"
    );
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c503)>, map_t<decltype(::WPNSTATE_GLOCK18_BURST_MODE)>>,
        "type ::WPNSTATE_GLOCK18_BURST_MODE"
    );
    static_assert(static_cast<long long>(c504) == static_cast<long long>(::WPNSTATE_M4A1_SILENCED), "value ::WPNSTATE_M4A1_SILENCED");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c504)>, map_t<decltype(::WPNSTATE_M4A1_SILENCED)>>,
        "type ::WPNSTATE_M4A1_SILENCED"
    );
    static_assert(static_cast<long long>(c505) == static_cast<long long>(::WPNSTATE_ELITE_LEFT), "value ::WPNSTATE_ELITE_LEFT");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c505)>, map_t<decltype(::WPNSTATE_ELITE_LEFT)>>,
        "type ::WPNSTATE_ELITE_LEFT"
    );
    static_assert(static_cast<long long>(c506) == static_cast<long long>(::WPNSTATE_FAMAS_BURST_MODE), "value ::WPNSTATE_FAMAS_BURST_MODE");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c506)>, map_t<decltype(::WPNSTATE_FAMAS_BURST_MODE)>>,
        "type ::WPNSTATE_FAMAS_BURST_MODE"
    );
    static_assert(static_cast<long long>(c507) == static_cast<long long>(::WPNSTATE_SHIELD_DRAWN), "value ::WPNSTATE_SHIELD_DRAWN");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c507)>, map_t<decltype(::WPNSTATE_SHIELD_DRAWN)>>,
        "type ::WPNSTATE_SHIELD_DRAWN"
    );
    static_assert(static_cast<long long>(c508) == static_cast<long long>(::ELITE_IDLE), "value ::ELITE_IDLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c508)>, map_t<decltype(::ELITE_IDLE)>>, "type ::ELITE_IDLE");
    static_assert(static_cast<long long>(c509) == static_cast<long long>(::ELITE_IDLE_LEFTEMPTY), "value ::ELITE_IDLE_LEFTEMPTY");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c509)>, map_t<decltype(::ELITE_IDLE_LEFTEMPTY)>>,
        "type ::ELITE_IDLE_LEFTEMPTY"
    );
    static_assert(static_cast<long long>(c510) == static_cast<long long>(::ELITE_SHOOTLEFT1), "value ::ELITE_SHOOTLEFT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c510)>, map_t<decltype(::ELITE_SHOOTLEFT1)>>, "type ::ELITE_SHOOTLEFT1");
    static_assert(static_cast<long long>(c511) == static_cast<long long>(::ELITE_SHOOTLEFT2), "value ::ELITE_SHOOTLEFT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c511)>, map_t<decltype(::ELITE_SHOOTLEFT2)>>, "type ::ELITE_SHOOTLEFT2");
    static_assert(static_cast<long long>(c512) == static_cast<long long>(::ELITE_SHOOTLEFT3), "value ::ELITE_SHOOTLEFT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c512)>, map_t<decltype(::ELITE_SHOOTLEFT3)>>, "type ::ELITE_SHOOTLEFT3");
    static_assert(static_cast<long long>(c513) == static_cast<long long>(::ELITE_SHOOTLEFT4), "value ::ELITE_SHOOTLEFT4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c513)>, map_t<decltype(::ELITE_SHOOTLEFT4)>>, "type ::ELITE_SHOOTLEFT4");
    static_assert(static_cast<long long>(c514) == static_cast<long long>(::ELITE_SHOOTLEFT5), "value ::ELITE_SHOOTLEFT5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c514)>, map_t<decltype(::ELITE_SHOOTLEFT5)>>, "type ::ELITE_SHOOTLEFT5");
    static_assert(static_cast<long long>(c515) == static_cast<long long>(::ELITE_SHOOTLEFTLAST), "value ::ELITE_SHOOTLEFTLAST");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c515)>, map_t<decltype(::ELITE_SHOOTLEFTLAST)>>,
        "type ::ELITE_SHOOTLEFTLAST"
    );
    static_assert(static_cast<long long>(c516) == static_cast<long long>(::ELITE_SHOOTRIGHT1), "value ::ELITE_SHOOTRIGHT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c516)>, map_t<decltype(::ELITE_SHOOTRIGHT1)>>, "type ::ELITE_SHOOTRIGHT1");
    static_assert(static_cast<long long>(c517) == static_cast<long long>(::ELITE_SHOOTRIGHT2), "value ::ELITE_SHOOTRIGHT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c517)>, map_t<decltype(::ELITE_SHOOTRIGHT2)>>, "type ::ELITE_SHOOTRIGHT2");
    static_assert(static_cast<long long>(c518) == static_cast<long long>(::ELITE_SHOOTRIGHT3), "value ::ELITE_SHOOTRIGHT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c518)>, map_t<decltype(::ELITE_SHOOTRIGHT3)>>, "type ::ELITE_SHOOTRIGHT3");
    static_assert(static_cast<long long>(c519) == static_cast<long long>(::ELITE_SHOOTRIGHT4), "value ::ELITE_SHOOTRIGHT4");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c519)>, map_t<decltype(::ELITE_SHOOTRIGHT4)>>, "type ::ELITE_SHOOTRIGHT4");
    static_assert(static_cast<long long>(c520) == static_cast<long long>(::ELITE_SHOOTRIGHT5), "value ::ELITE_SHOOTRIGHT5");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c520)>, map_t<decltype(::ELITE_SHOOTRIGHT5)>>, "type ::ELITE_SHOOTRIGHT5");
    static_assert(static_cast<long long>(c521) == static_cast<long long>(::ELITE_SHOOTRIGHTLAST), "value ::ELITE_SHOOTRIGHTLAST");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c521)>, map_t<decltype(::ELITE_SHOOTRIGHTLAST)>>,
        "type ::ELITE_SHOOTRIGHTLAST"
    );
    static_assert(static_cast<long long>(c522) == static_cast<long long>(::ELITE_RELOAD), "value ::ELITE_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c522)>, map_t<decltype(::ELITE_RELOAD)>>, "type ::ELITE_RELOAD");
    static_assert(static_cast<long long>(c523) == static_cast<long long>(::ELITE_DRAW), "value ::ELITE_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c523)>, map_t<decltype(::ELITE_DRAW)>>, "type ::ELITE_DRAW");
    static_assert(static_cast<long long>(c524) == static_cast<long long>(::GLOCK18_IDLE1), "value ::GLOCK18_IDLE1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c524)>, map_t<decltype(::GLOCK18_IDLE1)>>, "type ::GLOCK18_IDLE1");
    static_assert(static_cast<long long>(c525) == static_cast<long long>(::GLOCK18_IDLE2), "value ::GLOCK18_IDLE2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c525)>, map_t<decltype(::GLOCK18_IDLE2)>>, "type ::GLOCK18_IDLE2");
    static_assert(static_cast<long long>(c526) == static_cast<long long>(::GLOCK18_IDLE3), "value ::GLOCK18_IDLE3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c526)>, map_t<decltype(::GLOCK18_IDLE3)>>, "type ::GLOCK18_IDLE3");
    static_assert(static_cast<long long>(c527) == static_cast<long long>(::GLOCK18_SHOOT), "value ::GLOCK18_SHOOT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c527)>, map_t<decltype(::GLOCK18_SHOOT)>>, "type ::GLOCK18_SHOOT");
    static_assert(static_cast<long long>(c528) == static_cast<long long>(::GLOCK18_SHOOT2), "value ::GLOCK18_SHOOT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c528)>, map_t<decltype(::GLOCK18_SHOOT2)>>, "type ::GLOCK18_SHOOT2");
    static_assert(static_cast<long long>(c529) == static_cast<long long>(::GLOCK18_SHOOT3), "value ::GLOCK18_SHOOT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c529)>, map_t<decltype(::GLOCK18_SHOOT3)>>, "type ::GLOCK18_SHOOT3");
    static_assert(static_cast<long long>(c530) == static_cast<long long>(::GLOCK18_SHOOT_EMPTY), "value ::GLOCK18_SHOOT_EMPTY");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c530)>, map_t<decltype(::GLOCK18_SHOOT_EMPTY)>>,
        "type ::GLOCK18_SHOOT_EMPTY"
    );
    static_assert(static_cast<long long>(c531) == static_cast<long long>(::GLOCK18_RELOAD), "value ::GLOCK18_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c531)>, map_t<decltype(::GLOCK18_RELOAD)>>, "type ::GLOCK18_RELOAD");
    static_assert(static_cast<long long>(c532) == static_cast<long long>(::GLOCK18_DRAW), "value ::GLOCK18_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c532)>, map_t<decltype(::GLOCK18_DRAW)>>, "type ::GLOCK18_DRAW");
    static_assert(static_cast<long long>(c533) == static_cast<long long>(::GLOCK18_HOLSTER), "value ::GLOCK18_HOLSTER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c533)>, map_t<decltype(::GLOCK18_HOLSTER)>>, "type ::GLOCK18_HOLSTER");
    static_assert(static_cast<long long>(c534) == static_cast<long long>(::GLOCK18_ADD_SILENCER), "value ::GLOCK18_ADD_SILENCER");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c534)>, map_t<decltype(::GLOCK18_ADD_SILENCER)>>,
        "type ::GLOCK18_ADD_SILENCER"
    );
    static_assert(static_cast<long long>(c535) == static_cast<long long>(::GLOCK18_DRAW2), "value ::GLOCK18_DRAW2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c535)>, map_t<decltype(::GLOCK18_DRAW2)>>, "type ::GLOCK18_DRAW2");
    static_assert(static_cast<long long>(c536) == static_cast<long long>(::GLOCK18_RELOAD2), "value ::GLOCK18_RELOAD2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c536)>, map_t<decltype(::GLOCK18_RELOAD2)>>, "type ::GLOCK18_RELOAD2");
    static_assert(static_cast<long long>(c537) == static_cast<long long>(::M4A1_IDLE), "value ::M4A1_IDLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c537)>, map_t<decltype(::M4A1_IDLE)>>, "type ::M4A1_IDLE");
    static_assert(static_cast<long long>(c538) == static_cast<long long>(::M4A1_SHOOT1), "value ::M4A1_SHOOT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c538)>, map_t<decltype(::M4A1_SHOOT1)>>, "type ::M4A1_SHOOT1");
    static_assert(static_cast<long long>(c539) == static_cast<long long>(::M4A1_SHOOT2), "value ::M4A1_SHOOT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c539)>, map_t<decltype(::M4A1_SHOOT2)>>, "type ::M4A1_SHOOT2");
    static_assert(static_cast<long long>(c540) == static_cast<long long>(::M4A1_SHOOT3), "value ::M4A1_SHOOT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c540)>, map_t<decltype(::M4A1_SHOOT3)>>, "type ::M4A1_SHOOT3");
    static_assert(static_cast<long long>(c541) == static_cast<long long>(::M4A1_RELOAD), "value ::M4A1_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c541)>, map_t<decltype(::M4A1_RELOAD)>>, "type ::M4A1_RELOAD");
    static_assert(static_cast<long long>(c542) == static_cast<long long>(::M4A1_DRAW), "value ::M4A1_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c542)>, map_t<decltype(::M4A1_DRAW)>>, "type ::M4A1_DRAW");
    static_assert(static_cast<long long>(c543) == static_cast<long long>(::M4A1_ATTACH_SILENCER), "value ::M4A1_ATTACH_SILENCER");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c543)>, map_t<decltype(::M4A1_ATTACH_SILENCER)>>,
        "type ::M4A1_ATTACH_SILENCER"
    );
    static_assert(static_cast<long long>(c544) == static_cast<long long>(::M4A1_UNSIL_IDLE), "value ::M4A1_UNSIL_IDLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c544)>, map_t<decltype(::M4A1_UNSIL_IDLE)>>, "type ::M4A1_UNSIL_IDLE");
    static_assert(static_cast<long long>(c545) == static_cast<long long>(::M4A1_UNSIL_SHOOT1), "value ::M4A1_UNSIL_SHOOT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c545)>, map_t<decltype(::M4A1_UNSIL_SHOOT1)>>, "type ::M4A1_UNSIL_SHOOT1");
    static_assert(static_cast<long long>(c546) == static_cast<long long>(::M4A1_UNSIL_SHOOT2), "value ::M4A1_UNSIL_SHOOT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c546)>, map_t<decltype(::M4A1_UNSIL_SHOOT2)>>, "type ::M4A1_UNSIL_SHOOT2");
    static_assert(static_cast<long long>(c547) == static_cast<long long>(::M4A1_UNSIL_SHOOT3), "value ::M4A1_UNSIL_SHOOT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c547)>, map_t<decltype(::M4A1_UNSIL_SHOOT3)>>, "type ::M4A1_UNSIL_SHOOT3");
    static_assert(static_cast<long long>(c548) == static_cast<long long>(::M4A1_UNSIL_RELOAD), "value ::M4A1_UNSIL_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c548)>, map_t<decltype(::M4A1_UNSIL_RELOAD)>>, "type ::M4A1_UNSIL_RELOAD");
    static_assert(static_cast<long long>(c549) == static_cast<long long>(::M4A1_UNSIL_DRAW), "value ::M4A1_UNSIL_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c549)>, map_t<decltype(::M4A1_UNSIL_DRAW)>>, "type ::M4A1_UNSIL_DRAW");
    static_assert(static_cast<long long>(c550) == static_cast<long long>(::M4A1_DETACH_SILENCER), "value ::M4A1_DETACH_SILENCER");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c550)>, map_t<decltype(::M4A1_DETACH_SILENCER)>>,
        "type ::M4A1_DETACH_SILENCER"
    );
    static_assert(static_cast<long long>(c551) == static_cast<long long>(::USP_IDLE), "value ::USP_IDLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c551)>, map_t<decltype(::USP_IDLE)>>, "type ::USP_IDLE");
    static_assert(static_cast<long long>(c552) == static_cast<long long>(::USP_SHOOT1), "value ::USP_SHOOT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c552)>, map_t<decltype(::USP_SHOOT1)>>, "type ::USP_SHOOT1");
    static_assert(static_cast<long long>(c553) == static_cast<long long>(::USP_SHOOT2), "value ::USP_SHOOT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c553)>, map_t<decltype(::USP_SHOOT2)>>, "type ::USP_SHOOT2");
    static_assert(static_cast<long long>(c554) == static_cast<long long>(::USP_SHOOT3), "value ::USP_SHOOT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c554)>, map_t<decltype(::USP_SHOOT3)>>, "type ::USP_SHOOT3");
    static_assert(static_cast<long long>(c555) == static_cast<long long>(::USP_SHOOT_EMPTY), "value ::USP_SHOOT_EMPTY");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c555)>, map_t<decltype(::USP_SHOOT_EMPTY)>>, "type ::USP_SHOOT_EMPTY");
    static_assert(static_cast<long long>(c556) == static_cast<long long>(::USP_RELOAD), "value ::USP_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c556)>, map_t<decltype(::USP_RELOAD)>>, "type ::USP_RELOAD");
    static_assert(static_cast<long long>(c557) == static_cast<long long>(::USP_DRAW), "value ::USP_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c557)>, map_t<decltype(::USP_DRAW)>>, "type ::USP_DRAW");
    static_assert(static_cast<long long>(c558) == static_cast<long long>(::USP_ATTACH_SILENCER), "value ::USP_ATTACH_SILENCER");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c558)>, map_t<decltype(::USP_ATTACH_SILENCER)>>,
        "type ::USP_ATTACH_SILENCER"
    );
    static_assert(static_cast<long long>(c559) == static_cast<long long>(::USP_UNSIL_IDLE), "value ::USP_UNSIL_IDLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c559)>, map_t<decltype(::USP_UNSIL_IDLE)>>, "type ::USP_UNSIL_IDLE");
    static_assert(static_cast<long long>(c560) == static_cast<long long>(::USP_UNSIL_SHOOT1), "value ::USP_UNSIL_SHOOT1");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c560)>, map_t<decltype(::USP_UNSIL_SHOOT1)>>, "type ::USP_UNSIL_SHOOT1");
    static_assert(static_cast<long long>(c561) == static_cast<long long>(::USP_UNSIL_SHOOT2), "value ::USP_UNSIL_SHOOT2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c561)>, map_t<decltype(::USP_UNSIL_SHOOT2)>>, "type ::USP_UNSIL_SHOOT2");
    static_assert(static_cast<long long>(c562) == static_cast<long long>(::USP_UNSIL_SHOOT3), "value ::USP_UNSIL_SHOOT3");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c562)>, map_t<decltype(::USP_UNSIL_SHOOT3)>>, "type ::USP_UNSIL_SHOOT3");
    static_assert(static_cast<long long>(c563) == static_cast<long long>(::USP_UNSIL_SHOOT_EMPTY), "value ::USP_UNSIL_SHOOT_EMPTY");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c563)>, map_t<decltype(::USP_UNSIL_SHOOT_EMPTY)>>,
        "type ::USP_UNSIL_SHOOT_EMPTY"
    );
    static_assert(static_cast<long long>(c564) == static_cast<long long>(::USP_UNSIL_RELOAD), "value ::USP_UNSIL_RELOAD");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c564)>, map_t<decltype(::USP_UNSIL_RELOAD)>>, "type ::USP_UNSIL_RELOAD");
    static_assert(static_cast<long long>(c565) == static_cast<long long>(::USP_UNSIL_DRAW), "value ::USP_UNSIL_DRAW");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c565)>, map_t<decltype(::USP_UNSIL_DRAW)>>, "type ::USP_UNSIL_DRAW");
    static_assert(static_cast<long long>(c566) == static_cast<long long>(::USP_DETACH_SILENCER), "value ::USP_DETACH_SILENCER");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c566)>, map_t<decltype(::USP_DETACH_SILENCER)>>,
        "type ::USP_DETACH_SILENCER"
    );
    static_assert(static_cast<long long>(c567) == static_cast<long long>(::CAREER_NONE), "value ::CAREER_NONE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c567)>, map_t<decltype(::CAREER_NONE)>>, "type ::CAREER_NONE");
    static_assert(static_cast<long long>(c568) == static_cast<long long>(::CAREER_LOADING), "value ::CAREER_LOADING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c568)>, map_t<decltype(::CAREER_LOADING)>>, "type ::CAREER_LOADING");
    static_assert(static_cast<long long>(c569) == static_cast<long long>(::CAREER_PLAYING), "value ::CAREER_PLAYING");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c569)>, map_t<decltype(::CAREER_PLAYING)>>, "type ::CAREER_PLAYING");
    static_assert(static_cast<long long>(c570) == static_cast<long long>(::ca_dedicated), "value ::ca_dedicated");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c570)>, map_t<decltype(::ca_dedicated)>>, "type ::ca_dedicated");
    static_assert(static_cast<long long>(c571) == static_cast<long long>(::ca_disconnected), "value ::ca_disconnected");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c571)>, map_t<decltype(::ca_disconnected)>>, "type ::ca_disconnected");
    static_assert(static_cast<long long>(c572) == static_cast<long long>(::ca_connecting), "value ::ca_connecting");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c572)>, map_t<decltype(::ca_connecting)>>, "type ::ca_connecting");
    static_assert(static_cast<long long>(c573) == static_cast<long long>(::ca_connected), "value ::ca_connected");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c573)>, map_t<decltype(::ca_connected)>>, "type ::ca_connected");
    static_assert(static_cast<long long>(c574) == static_cast<long long>(::ca_uninitialized), "value ::ca_uninitialized");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c574)>, map_t<decltype(::ca_uninitialized)>>, "type ::ca_uninitialized");
    static_assert(static_cast<long long>(c575) == static_cast<long long>(::ca_active), "value ::ca_active");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c575)>, map_t<decltype(::ca_active)>>, "type ::ca_active");
    static_assert(static_cast<long long>(c576) == static_cast<long long>(::src_client), "value ::src_client");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c576)>, map_t<decltype(::src_client)>>, "type ::src_client");
    static_assert(static_cast<long long>(c577) == static_cast<long long>(::src_command), "value ::src_command");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c577)>, map_t<decltype(::src_command)>>, "type ::src_command");
    static_assert(static_cast<long long>(c578) == static_cast<long long>(::mod_bad), "value ::mod_bad");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c578)>, map_t<decltype(::mod_bad)>>, "type ::mod_bad");
    static_assert(static_cast<long long>(c579) == static_cast<long long>(::mod_brush), "value ::mod_brush");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c579)>, map_t<decltype(::mod_brush)>>, "type ::mod_brush");
    static_assert(static_cast<long long>(c580) == static_cast<long long>(::mod_sprite), "value ::mod_sprite");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c580)>, map_t<decltype(::mod_sprite)>>, "type ::mod_sprite");
    static_assert(static_cast<long long>(c581) == static_cast<long long>(::mod_alias), "value ::mod_alias");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c581)>, map_t<decltype(::mod_alias)>>, "type ::mod_alias");
    static_assert(static_cast<long long>(c582) == static_cast<long long>(::mod_studio), "value ::mod_studio");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c582)>, map_t<decltype(::mod_studio)>>, "type ::mod_studio");
    static_assert(static_cast<long long>(c583) == static_cast<long long>(::clc_bad), "value ::clc_bad");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c583)>, map_t<decltype(::clc_bad)>>, "type ::clc_bad");
    static_assert(static_cast<long long>(c584) == static_cast<long long>(::clc_nop), "value ::clc_nop");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c584)>, map_t<decltype(::clc_nop)>>, "type ::clc_nop");
    static_assert(static_cast<long long>(c585) == static_cast<long long>(::clc_move), "value ::clc_move");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c585)>, map_t<decltype(::clc_move)>>, "type ::clc_move");
    static_assert(static_cast<long long>(c586) == static_cast<long long>(::clc_stringcmd), "value ::clc_stringcmd");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c586)>, map_t<decltype(::clc_stringcmd)>>, "type ::clc_stringcmd");
    static_assert(static_cast<long long>(c587) == static_cast<long long>(::clc_delta), "value ::clc_delta");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c587)>, map_t<decltype(::clc_delta)>>, "type ::clc_delta");
    static_assert(static_cast<long long>(c588) == static_cast<long long>(::clc_resourcelist), "value ::clc_resourcelist");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c588)>, map_t<decltype(::clc_resourcelist)>>, "type ::clc_resourcelist");
    static_assert(static_cast<long long>(c589) == static_cast<long long>(::clc_tmove), "value ::clc_tmove");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c589)>, map_t<decltype(::clc_tmove)>>, "type ::clc_tmove");
    static_assert(static_cast<long long>(c590) == static_cast<long long>(::clc_fileconsistency), "value ::clc_fileconsistency");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c590)>, map_t<decltype(::clc_fileconsistency)>>,
        "type ::clc_fileconsistency"
    );
    static_assert(static_cast<long long>(c591) == static_cast<long long>(::clc_voicedata), "value ::clc_voicedata");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c591)>, map_t<decltype(::clc_voicedata)>>, "type ::clc_voicedata");
    static_assert(static_cast<long long>(c592) == static_cast<long long>(::clc_hltv), "value ::clc_hltv");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c592)>, map_t<decltype(::clc_hltv)>>, "type ::clc_hltv");
    static_assert(static_cast<long long>(c593) == static_cast<long long>(::clc_cvarvalue), "value ::clc_cvarvalue");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c593)>, map_t<decltype(::clc_cvarvalue)>>, "type ::clc_cvarvalue");
    static_assert(static_cast<long long>(c594) == static_cast<long long>(::clc_cvarvalue2), "value ::clc_cvarvalue2");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c594)>, map_t<decltype(::clc_cvarvalue2)>>, "type ::clc_cvarvalue2");
    static_assert(static_cast<long long>(c595) == static_cast<long long>(::clc_endoflist), "value ::clc_endoflist");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c595)>, map_t<decltype(::clc_endoflist)>>, "type ::clc_endoflist");
    static_assert(static_cast<long long>(c596) == static_cast<long long>(::NA_UNUSED), "value ::NA_UNUSED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c596)>, map_t<decltype(::NA_UNUSED)>>, "type ::NA_UNUSED");
    static_assert(static_cast<long long>(c597) == static_cast<long long>(::NA_LOOPBACK), "value ::NA_LOOPBACK");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c597)>, map_t<decltype(::NA_LOOPBACK)>>, "type ::NA_LOOPBACK");
    static_assert(static_cast<long long>(c598) == static_cast<long long>(::NA_BROADCAST), "value ::NA_BROADCAST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c598)>, map_t<decltype(::NA_BROADCAST)>>, "type ::NA_BROADCAST");
    static_assert(static_cast<long long>(c599) == static_cast<long long>(::NA_IP), "value ::NA_IP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c599)>, map_t<decltype(::NA_IP)>>, "type ::NA_IP");
    static_assert(static_cast<long long>(c600) == static_cast<long long>(::NA_IPX), "value ::NA_IPX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c600)>, map_t<decltype(::NA_IPX)>>, "type ::NA_IPX");
    static_assert(static_cast<long long>(c601) == static_cast<long long>(::NA_BROADCAST_IPX), "value ::NA_BROADCAST_IPX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c601)>, map_t<decltype(::NA_BROADCAST_IPX)>>, "type ::NA_BROADCAST_IPX");
    static_assert(static_cast<long long>(c602) == static_cast<long long>(::NS_CLIENT), "value ::NS_CLIENT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c602)>, map_t<decltype(::NS_CLIENT)>>, "type ::NS_CLIENT");
    static_assert(static_cast<long long>(c603) == static_cast<long long>(::NS_SERVER), "value ::NS_SERVER");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c603)>, map_t<decltype(::NS_SERVER)>>, "type ::NS_SERVER");
    static_assert(static_cast<long long>(c604) == static_cast<long long>(::NS_MULTICAST), "value ::NS_MULTICAST");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c604)>, map_t<decltype(::NS_MULTICAST)>>, "type ::NS_MULTICAST");
    static_assert(static_cast<long long>(c605) == static_cast<long long>(::NS_MAX), "value ::NS_MAX");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c605)>, map_t<decltype(::NS_MAX)>>, "type ::NS_MAX");
    static_assert(static_cast<long long>(c606) == static_cast<long long>(::FRAG_NORMAL_STREAM), "value ::FRAG_NORMAL_STREAM");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c606)>, std::underlying_type_t<decltype(::FRAG_NORMAL_STREAM)>>,
        "type ::FRAG_NORMAL_STREAM"
    );
    static_assert(static_cast<long long>(c607) == static_cast<long long>(::FRAG_FILE_STREAM), "value ::FRAG_FILE_STREAM");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c607)>, std::underlying_type_t<decltype(::FRAG_FILE_STREAM)>>,
        "type ::FRAG_FILE_STREAM"
    );
    static_assert(static_cast<long long>(c608) == static_cast<long long>(::MAX_STREAMS), "value ::MAX_STREAMS");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c608)>, std::underlying_type_t<decltype(::MAX_STREAMS)>>,
        "type ::MAX_STREAMS"
    );
    static_assert(static_cast<long long>(c609) == static_cast<long long>(::t_sound), "value ::t_sound");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c609)>, map_t<decltype(::t_sound)>>, "type ::t_sound");
    static_assert(static_cast<long long>(c610) == static_cast<long long>(::t_skin), "value ::t_skin");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c610)>, map_t<decltype(::t_skin)>>, "type ::t_skin");
    static_assert(static_cast<long long>(c611) == static_cast<long long>(::t_model), "value ::t_model");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c611)>, map_t<decltype(::t_model)>>, "type ::t_model");
    static_assert(static_cast<long long>(c612) == static_cast<long long>(::t_decal), "value ::t_decal");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c612)>, map_t<decltype(::t_decal)>>, "type ::t_decal");
    static_assert(static_cast<long long>(c613) == static_cast<long long>(::t_generic), "value ::t_generic");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c613)>, map_t<decltype(::t_generic)>>, "type ::t_generic");
    static_assert(static_cast<long long>(c614) == static_cast<long long>(::t_eventscript), "value ::t_eventscript");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c614)>, map_t<decltype(::t_eventscript)>>, "type ::t_eventscript");
    static_assert(static_cast<long long>(c615) == static_cast<long long>(::t_world), "value ::t_world");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c615)>, map_t<decltype(::t_world)>>, "type ::t_world");
    static_assert(static_cast<long long>(c616) == static_cast<long long>(::rt_unk), "value ::rt_unk");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c616)>, map_t<decltype(::rt_unk)>>, "type ::rt_unk");
    static_assert(static_cast<long long>(c617) == static_cast<long long>(::rt_max), "value ::rt_max");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c617)>, map_t<decltype(::rt_max)>>, "type ::rt_max");
    static_assert(static_cast<long long>(c618) == static_cast<long long>(::nitroapi::VT_None), "value ::nitroapi::VT_None");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c618)>, map_t<decltype(::nitroapi::VT_None)>>, "type ::nitroapi::VT_None");
    static_assert(static_cast<long long>(c619) == static_cast<long long>(::nitroapi::VT_Software), "value ::nitroapi::VT_Software");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c619)>, map_t<decltype(::nitroapi::VT_Software)>>,
        "type ::nitroapi::VT_Software"
    );
    static_assert(static_cast<long long>(c620) == static_cast<long long>(::nitroapi::VT_OpenGL), "value ::nitroapi::VT_OpenGL");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c620)>, map_t<decltype(::nitroapi::VT_OpenGL)>>,
        "type ::nitroapi::VT_OpenGL"
    );
    static_assert(static_cast<long long>(c621) == static_cast<long long>(::nitroapi::VT_Direct3D), "value ::nitroapi::VT_Direct3D");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c621)>, map_t<decltype(::nitroapi::VT_Direct3D)>>,
        "type ::nitroapi::VT_Direct3D"
    );
    static_assert(static_cast<long long>(c622) == static_cast<long long>(::kRenderNormal), "value ::kRenderNormal");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c622)>, std::underlying_type_t<decltype(::kRenderNormal)>>,
        "type ::kRenderNormal"
    );
    static_assert(static_cast<long long>(c623) == static_cast<long long>(::kRenderTransColor), "value ::kRenderTransColor");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c623)>, std::underlying_type_t<decltype(::kRenderTransColor)>>,
        "type ::kRenderTransColor"
    );
    static_assert(static_cast<long long>(c624) == static_cast<long long>(::kRenderTransTexture), "value ::kRenderTransTexture");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c624)>, std::underlying_type_t<decltype(::kRenderTransTexture)>>,
        "type ::kRenderTransTexture"
    );
    static_assert(static_cast<long long>(c625) == static_cast<long long>(::kRenderGlow), "value ::kRenderGlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c625)>, std::underlying_type_t<decltype(::kRenderGlow)>>,
        "type ::kRenderGlow"
    );
    static_assert(static_cast<long long>(c626) == static_cast<long long>(::kRenderTransAlpha), "value ::kRenderTransAlpha");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c626)>, std::underlying_type_t<decltype(::kRenderTransAlpha)>>,
        "type ::kRenderTransAlpha"
    );
    static_assert(static_cast<long long>(c627) == static_cast<long long>(::kRenderTransAdd), "value ::kRenderTransAdd");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c627)>, std::underlying_type_t<decltype(::kRenderTransAdd)>>,
        "type ::kRenderTransAdd"
    );
    static_assert(static_cast<long long>(c628) == static_cast<long long>(::kRenderFxNone), "value ::kRenderFxNone");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c628)>, std::underlying_type_t<decltype(::kRenderFxNone)>>,
        "type ::kRenderFxNone"
    );
    static_assert(static_cast<long long>(c629) == static_cast<long long>(::kRenderFxPulseSlow), "value ::kRenderFxPulseSlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c629)>, std::underlying_type_t<decltype(::kRenderFxPulseSlow)>>,
        "type ::kRenderFxPulseSlow"
    );
    static_assert(static_cast<long long>(c630) == static_cast<long long>(::kRenderFxPulseFast), "value ::kRenderFxPulseFast");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c630)>, std::underlying_type_t<decltype(::kRenderFxPulseFast)>>,
        "type ::kRenderFxPulseFast"
    );
    static_assert(static_cast<long long>(c631) == static_cast<long long>(::kRenderFxPulseSlowWide), "value ::kRenderFxPulseSlowWide");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c631)>, std::underlying_type_t<decltype(::kRenderFxPulseSlowWide)>>,
        "type ::kRenderFxPulseSlowWide"
    );
    static_assert(static_cast<long long>(c632) == static_cast<long long>(::kRenderFxPulseFastWide), "value ::kRenderFxPulseFastWide");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c632)>, std::underlying_type_t<decltype(::kRenderFxPulseFastWide)>>,
        "type ::kRenderFxPulseFastWide"
    );
    static_assert(static_cast<long long>(c633) == static_cast<long long>(::kRenderFxFadeSlow), "value ::kRenderFxFadeSlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c633)>, std::underlying_type_t<decltype(::kRenderFxFadeSlow)>>,
        "type ::kRenderFxFadeSlow"
    );
    static_assert(static_cast<long long>(c634) == static_cast<long long>(::kRenderFxFadeFast), "value ::kRenderFxFadeFast");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c634)>, std::underlying_type_t<decltype(::kRenderFxFadeFast)>>,
        "type ::kRenderFxFadeFast"
    );
    static_assert(static_cast<long long>(c635) == static_cast<long long>(::kRenderFxSolidSlow), "value ::kRenderFxSolidSlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c635)>, std::underlying_type_t<decltype(::kRenderFxSolidSlow)>>,
        "type ::kRenderFxSolidSlow"
    );
    static_assert(static_cast<long long>(c636) == static_cast<long long>(::kRenderFxSolidFast), "value ::kRenderFxSolidFast");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c636)>, std::underlying_type_t<decltype(::kRenderFxSolidFast)>>,
        "type ::kRenderFxSolidFast"
    );
    static_assert(static_cast<long long>(c637) == static_cast<long long>(::kRenderFxStrobeSlow), "value ::kRenderFxStrobeSlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c637)>, std::underlying_type_t<decltype(::kRenderFxStrobeSlow)>>,
        "type ::kRenderFxStrobeSlow"
    );
    static_assert(static_cast<long long>(c638) == static_cast<long long>(::kRenderFxStrobeFast), "value ::kRenderFxStrobeFast");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c638)>, std::underlying_type_t<decltype(::kRenderFxStrobeFast)>>,
        "type ::kRenderFxStrobeFast"
    );
    static_assert(static_cast<long long>(c639) == static_cast<long long>(::kRenderFxStrobeFaster), "value ::kRenderFxStrobeFaster");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c639)>, std::underlying_type_t<decltype(::kRenderFxStrobeFaster)>>,
        "type ::kRenderFxStrobeFaster"
    );
    static_assert(static_cast<long long>(c640) == static_cast<long long>(::kRenderFxFlickerSlow), "value ::kRenderFxFlickerSlow");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c640)>, std::underlying_type_t<decltype(::kRenderFxFlickerSlow)>>,
        "type ::kRenderFxFlickerSlow"
    );
    static_assert(static_cast<long long>(c641) == static_cast<long long>(::kRenderFxFlickerFast), "value ::kRenderFxFlickerFast");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c641)>, std::underlying_type_t<decltype(::kRenderFxFlickerFast)>>,
        "type ::kRenderFxFlickerFast"
    );
    static_assert(static_cast<long long>(c642) == static_cast<long long>(::kRenderFxNoDissipation), "value ::kRenderFxNoDissipation");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c642)>, std::underlying_type_t<decltype(::kRenderFxNoDissipation)>>,
        "type ::kRenderFxNoDissipation"
    );
    static_assert(static_cast<long long>(c643) == static_cast<long long>(::kRenderFxDistort), "value ::kRenderFxDistort");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c643)>, std::underlying_type_t<decltype(::kRenderFxDistort)>>,
        "type ::kRenderFxDistort"
    );
    static_assert(static_cast<long long>(c644) == static_cast<long long>(::kRenderFxHologram), "value ::kRenderFxHologram");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c644)>, std::underlying_type_t<decltype(::kRenderFxHologram)>>,
        "type ::kRenderFxHologram"
    );
    static_assert(static_cast<long long>(c645) == static_cast<long long>(::kRenderFxDeadPlayer), "value ::kRenderFxDeadPlayer");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c645)>, std::underlying_type_t<decltype(::kRenderFxDeadPlayer)>>,
        "type ::kRenderFxDeadPlayer"
    );
    static_assert(static_cast<long long>(c646) == static_cast<long long>(::kRenderFxExplode), "value ::kRenderFxExplode");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c646)>, std::underlying_type_t<decltype(::kRenderFxExplode)>>,
        "type ::kRenderFxExplode"
    );
    static_assert(static_cast<long long>(c647) == static_cast<long long>(::kRenderFxGlowShell), "value ::kRenderFxGlowShell");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c647)>, std::underlying_type_t<decltype(::kRenderFxGlowShell)>>,
        "type ::kRenderFxGlowShell"
    );
    static_assert(static_cast<long long>(c648) == static_cast<long long>(::kRenderFxClampMinScale), "value ::kRenderFxClampMinScale");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c648)>, std::underlying_type_t<decltype(::kRenderFxClampMinScale)>>,
        "type ::kRenderFxClampMinScale"
    );
    static_assert(static_cast<long long>(c649) == static_cast<long long>(::kRenderFxLightMultiplier), "value ::kRenderFxLightMultiplier");
    static_assert(
        std::is_same_v<std::underlying_type_t<decltype(c649)>, std::underlying_type_t<decltype(::kRenderFxLightMultiplier)>>,
        "type ::kRenderFxLightMultiplier"
    );
    static_assert(static_cast<long long>(c650) == static_cast<long long>(::SPR_SINGLE), "value ::SPR_SINGLE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c650)>, map_t<decltype(::SPR_SINGLE)>>, "type ::SPR_SINGLE");
    static_assert(static_cast<long long>(c651) == static_cast<long long>(::SPR_GROUP), "value ::SPR_GROUP");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c651)>, map_t<decltype(::SPR_GROUP)>>, "type ::SPR_GROUP");
    static_assert(static_cast<long long>(c652) == static_cast<long long>(::SPR_ANGLED), "value ::SPR_ANGLED");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c652)>, map_t<decltype(::SPR_ANGLED)>>, "type ::SPR_ANGLED");
    static_assert(static_cast<long long>(c653) == static_cast<long long>(::ST_SYNC), "value ::ST_SYNC");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c653)>, map_t<decltype(::ST_SYNC)>>, "type ::ST_SYNC");
    static_assert(static_cast<long long>(c654) == static_cast<long long>(::ST_RAND), "value ::ST_RAND");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c654)>, map_t<decltype(::ST_RAND)>>, "type ::ST_RAND");
    static_assert(static_cast<long long>(c655) == static_cast<long long>(::key_game), "value ::key_game");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c655)>, map_t<decltype(::key_game)>>, "type ::key_game");
    static_assert(static_cast<long long>(c656) == static_cast<long long>(::key_message), "value ::key_message");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c656)>, map_t<decltype(::key_message)>>, "type ::key_message");
    static_assert(static_cast<long long>(c657) == static_cast<long long>(::key_menu), "value ::key_menu");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c657)>, map_t<decltype(::key_menu)>>, "type ::key_menu");
    static_assert(static_cast<long long>(c658) == static_cast<long long>(::at_notice), "value ::at_notice");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c658)>, map_t<decltype(::at_notice)>>, "type ::at_notice");
    static_assert(static_cast<long long>(c659) == static_cast<long long>(::at_console), "value ::at_console");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c659)>, map_t<decltype(::at_console)>>, "type ::at_console");
    static_assert(static_cast<long long>(c660) == static_cast<long long>(::at_aiconsole), "value ::at_aiconsole");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c660)>, map_t<decltype(::at_aiconsole)>>, "type ::at_aiconsole");
    static_assert(static_cast<long long>(c661) == static_cast<long long>(::at_warning), "value ::at_warning");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c661)>, map_t<decltype(::at_warning)>>, "type ::at_warning");
    static_assert(static_cast<long long>(c662) == static_cast<long long>(::at_error), "value ::at_error");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c662)>, map_t<decltype(::at_error)>>, "type ::at_error");
    static_assert(static_cast<long long>(c663) == static_cast<long long>(::at_logged), "value ::at_logged");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c663)>, map_t<decltype(::at_logged)>>, "type ::at_logged");
    static_assert(static_cast<long long>(c664) == static_cast<long long>(::force_exactfile), "value ::force_exactfile");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c664)>, map_t<decltype(::force_exactfile)>>, "type ::force_exactfile");
    static_assert(static_cast<long long>(c665) == static_cast<long long>(::force_model_samebounds), "value ::force_model_samebounds");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c665)>, map_t<decltype(::force_model_samebounds)>>,
        "type ::force_model_samebounds"
    );
    static_assert(static_cast<long long>(c666) == static_cast<long long>(::force_model_specifybounds), "value ::force_model_specifybounds");
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c666)>, map_t<decltype(::force_model_specifybounds)>>,
        "type ::force_model_specifybounds"
    );
    static_assert(
        static_cast<long long>(c667) == static_cast<long long>(::force_model_specifybounds_if_avail),
        "value ::force_model_specifybounds_if_avail"
    );
    static_assert(
        std::is_same_v<std::remove_const_t<decltype(c667)>, map_t<decltype(::force_model_specifybounds_if_avail)>>,
        "type ::force_model_specifybounds_if_avail"
    );
    static_assert(static_cast<long long>(c668) == static_cast<long long>(::print_console), "value ::print_console");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c668)>, map_t<decltype(::print_console)>>, "type ::print_console");
    static_assert(static_cast<long long>(c669) == static_cast<long long>(::print_center), "value ::print_center");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c669)>, map_t<decltype(::print_center)>>, "type ::print_center");
    static_assert(static_cast<long long>(c670) == static_cast<long long>(::print_chat), "value ::print_chat");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c670)>, map_t<decltype(::print_chat)>>, "type ::print_chat");
    static_assert(static_cast<long long>(c671) == static_cast<long long>(::TRI_FRONT), "value ::TRI_FRONT");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c671)>, map_t<decltype(::TRI_FRONT)>>, "type ::TRI_FRONT");
    static_assert(static_cast<long long>(c672) == static_cast<long long>(::TRI_NONE), "value ::TRI_NONE");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c672)>, map_t<decltype(::TRI_NONE)>>, "type ::TRI_NONE");
    static_assert(static_cast<long long>(c673) == static_cast<long long>(::MicrophoneVolume), "value ::MicrophoneVolume");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c673)>, map_t<decltype(::MicrophoneVolume)>>, "type ::MicrophoneVolume");
    static_assert(static_cast<long long>(c674) == static_cast<long long>(::OtherSpeakerScale), "value ::OtherSpeakerScale");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c674)>, map_t<decltype(::OtherSpeakerScale)>>, "type ::OtherSpeakerScale");
    static_assert(static_cast<long long>(c675) == static_cast<long long>(::MicBoost), "value ::MicBoost");
    static_assert(std::is_same_v<std::remove_const_t<decltype(c675)>, map_t<decltype(::MicBoost)>>, "type ::MicBoost");
    struct p0 : ::CHudAmmoSecondary
    {
        static_assert(
            static_cast<long long>(c676) == static_cast<long long>(MAX_SEC_AMMO_VALUES),
            "value ::CHudAmmoSecondary::MAX_SEC_AMMO_VALUES"
        );
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c676)>, std::underlying_type_t<decltype(MAX_SEC_AMMO_VALUES)>>,
            "type ::CHudAmmoSecondary::MAX_SEC_AMMO_VALUES"
        );
    };
    struct p1 : ::CHudMessage
    {
        static_assert(static_cast<long long>(c677) == static_cast<long long>(MESSAGE_ARG_LEN), "value ::CHudMessage::MESSAGE_ARG_LEN");
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c677)>, std::underlying_type_t<decltype(MESSAGE_ARG_LEN)>>,
            "type ::CHudMessage::MESSAGE_ARG_LEN"
        );
        static_assert(static_cast<long long>(c678) == static_cast<long long>(MAX_MESSAGE_ARGS), "value ::CHudMessage::MAX_MESSAGE_ARGS");
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c678)>, std::underlying_type_t<decltype(MAX_MESSAGE_ARGS)>>,
            "type ::CHudMessage::MAX_MESSAGE_ARGS"
        );
    };
    struct p2 : ::CHudStatusBar
    {
        static_assert(
            static_cast<long long>(c679) == static_cast<long long>(MAX_STATUSTEXT_LENGTH),
            "value ::CHudStatusBar::MAX_STATUSTEXT_LENGTH"
        );
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c679)>, std::underlying_type_t<decltype(MAX_STATUSTEXT_LENGTH)>>,
            "type ::CHudStatusBar::MAX_STATUSTEXT_LENGTH"
        );
        static_assert(
            static_cast<long long>(c680) == static_cast<long long>(MAX_STATUSBAR_VALUES),
            "value ::CHudStatusBar::MAX_STATUSBAR_VALUES"
        );
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c680)>, std::underlying_type_t<decltype(MAX_STATUSBAR_VALUES)>>,
            "type ::CHudStatusBar::MAX_STATUSBAR_VALUES"
        );
        static_assert(
            static_cast<long long>(c681) == static_cast<long long>(MAX_STATUSBAR_LINES),
            "value ::CHudStatusBar::MAX_STATUSBAR_LINES"
        );
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c681)>, std::underlying_type_t<decltype(MAX_STATUSBAR_LINES)>>,
            "type ::CHudStatusBar::MAX_STATUSBAR_LINES"
        );
    };
    struct p3 : ::CHudStatusIcons
    {
        static_assert(
            static_cast<long long>(c682) == static_cast<long long>(MAX_ICONSPRITENAME_LENGTH),
            "value ::CHudStatusIcons::MAX_ICONSPRITENAME_LENGTH"
        );
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c682)>, std::underlying_type_t<decltype(MAX_ICONSPRITENAME_LENGTH)>>,
            "type ::CHudStatusIcons::MAX_ICONSPRITENAME_LENGTH"
        );
        static_assert(static_cast<long long>(c683) == static_cast<long long>(MAX_ICONSPRITES), "value ::CHudStatusIcons::MAX_ICONSPRITES");
        static_assert(
            std::is_same_v<std::underlying_type_t<decltype(c683)>, std::underlying_type_t<decltype(MAX_ICONSPRITES)>>,
            "type ::CHudStatusIcons::MAX_ICONSPRITES"
        );
    };
} // namespace goldsrc_equivalence

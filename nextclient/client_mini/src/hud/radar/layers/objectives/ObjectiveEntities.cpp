#include "ObjectiveEntities.h"

#include <cstring>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>

#include "main.h"

#include "hud/radar/map/MapObjectives.h"
#include "hud/radar/integration/RadarUtil.h"

void ObjectiveEntities::OnVidInit()
{
    model_kind_.clear(); // model pointers are only stable within a level
}

ObjectiveEntities::ModelKind ObjectiveEntities::ClassifyModel(const goldsrc::model_t* model, const RadarMapObjectives& map)
{
    auto it = model_kind_.find(model);

    if (it != model_kind_.end())
    {
        return it->second;
    }

    // Substring, not exact match, so custom bomb model paths still classify.
    ModelKind kind = ModelKind::Other;

    if (RadarUtil_IsHostageModel(model->name, map.get_hostage_models()))
    {
        kind = ModelKind::Hostage;
    }
    else if (std::strstr(model->name, "backpack"))
    {
        kind = ModelKind::BombDropped;
    }
    else if (std::strstr(model->name, "w_c4"))
    {
        kind = ModelKind::BombPlanted;
    }

    model_kind_.emplace(model, kind);
    return kind;
}

void ObjectiveEntities::CollectEntities(const RadarFrame& frame, const RadarMapObjectives& map)
{
    test_due_ = (frame.time - last_test_ >= kTestInterval) || frame.time < last_test_;

    if (test_due_)
    {
        last_test_ = frame.time;
    }

    bomb_ = BombEnt{};
    hostage_count_ = 0;

    goldsrc::cl_entity_t* local = frame.ctx.local;

    if (!local)
    {
        return;
    }

    auto consider = [&](int i, goldsrc::cl_entity_t* ent) {
        if (!ent || !ent->model || !RadarFrame_IsEntityCurrent(frame.ctx, ent) || (ent->curstate.effects & goldsrc::EF_NODRAW))
        {
            return false;
        }

        ModelKind kind = ClassifyModel(ent->model, map);

        if (kind == ModelKind::Other)
        {
            return false;
        }

        if (kind == ModelKind::Hostage)
        {
            // CHostage leaves a killed hostage's body in the snapshot until the round restarts and
            // makes it non-solid; a living one is SOLID_SLIDEBOX, a rescued one is not sent at all.
            if (ent->curstate.solid == goldsrc::SOLID_NOT)
            {
                return false;
            }

            if (hostage_count_ < kRadarMaxHostages)
            {
                HostageEnt& h = hostages_[hostage_count_++];
                h.index = i;
                h.origin = ncl_math::Vector3::Ref(ent->origin);
                h.discontinuity = (ent->curstate.effects & goldsrc::EF_NOINTERP) != 0;
            }
        }
        else if (!bomb_.found)
        {
            bomb_.found = true;
            bomb_.ent = ent;
            bomb_.planted = (kind == ModelKind::BombPlanted);
            bomb_.pos = ncl_math::Vector3::Ref(ent->origin);
        }

        return true;
    };

    // Rediscovering the set walks every edict above the player slots, so it rides the same
    // kTestInterval cadence as the sight tests instead of running per frame. Only the membership
    // goes stale in between: an objective that spawns then reaches the radar one sweep late, while
    // the entities already found are re-read every frame.
    if (test_due_)
    {
        // GetMaxClients()+1, not goldsrc::cstrike::MAX_PLAYERS+1: fewer slots puts objectives at or below goldsrc::cstrike::MAX_PLAYERS.
        objective_ents_.clear();

        for (int i = gEngfuncs.GetMaxClients() + 1; goldsrc::cl_entity_t* ent = gEngfuncs.GetEntityByIndex(i); i++)
        {
            if (consider(i, ent))
            {
                objective_ents_.push_back(i);
            }
        }
    }
    else
    {
        for (int i : objective_ents_)
        {
            consider(i, gEngfuncs.GetEntityByIndex(i));
        }
    }
}

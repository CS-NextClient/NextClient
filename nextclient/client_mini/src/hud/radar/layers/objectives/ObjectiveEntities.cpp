#include "ObjectiveEntities.h"

#include <cstring>
#include <goldsrc/client_api.h>
#include <goldsrc/engine/model.h>
#include <goldsrc/entity.h>

#include "main.h"

#include "hud/radar/RadarConfig.h"

void ObjectiveEntities::OnVidInit()
{
    model_kind_.clear(); // model pointers are only stable within a level
}

ObjectiveEntities::ModelKind ObjectiveEntities::ClassifyModel(const goldsrc::model_t* model)
{
    auto it = model_kind_.find(model);

    if (it != model_kind_.end())
    {
        return it->second;
    }

    // Substring, not exact match, so custom hostage/bomb model paths still classify.
    ModelKind kind = ModelKind::Other;

    if (std::strstr(model->name, "hostage"))
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

void ObjectiveEntities::CollectEntities(const RadarFrame& frame)
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

    int message_num = local->curstate.messagenum;

    auto consider = [&](int i, goldsrc::cl_entity_t* ent) {
        if (!ent || !ent->model || ent->curstate.messagenum != message_num)
        {
            return false;
        }

        ModelKind kind = ClassifyModel(ent->model);

        if (kind == ModelKind::Other)
        {
            return false;
        }

        if (kind == ModelKind::Hostage)
        {
            if (hostage_count_ < kRadarMaxHostages)
            {
                HostageEnt& h = hostages_[hostage_count_++];
                h.index = i;
                h.origin = ncl_math::Vector3::Ref(ent->origin);
            }
        }
        else if (!bomb_.found)
        {
            bomb_.found = true;
            bomb_.ent = ent;
            bomb_.planted = (kind == ModelKind::BombPlanted);
            bomb_.pos = {ent->origin[0], ent->origin[1]};
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

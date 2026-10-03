#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

#include <ncl_math/vec3.h>
#include <goldsrc/fwd.h>

#include "hud/radar/RadarFrame.h"
#include "hud/radar/RadarLimits.h"

class RadarMapObjectives;

// How long an objective marker outlives the sight of it, and how much of that it stays opaque.
// Distinct from RadarVisibility's enemy spot memory.
inline constexpr float kObjectiveSightMemory = 1.5f; // seconds
inline constexpr float kObjectiveSightHold = 0.4f; // seconds, inside kObjectiveSightMemory

// The bomb and hostage entities the radar draws from, resolved once a frame. The edict sweep is
// throttled and its result cached, so most frames only re-read the entities found last time.
class ObjectiveEntities
{
public:
    struct BombEnt
    {
        bool found;
        goldsrc::cl_entity_t* ent;
        bool planted;
        ncl_math::Vector2 pos; // snapshot origin; the consumer interpolates
    };

    struct HostageEnt
    {
        int index;
        ncl_math::Vector3 origin;
    };

private:
    enum class ModelKind : uint8_t
    {
        Other,
        Hostage,
        BombDropped,
        BombPlanted,
    };

    // Keyed on the model pointer: valid within one level, cleared in OnVidInit.
    std::unordered_map<const goldsrc::model_t*, ModelKind> model_kind_;

    BombEnt bomb_{};
    std::array<HostageEnt, kRadarMaxHostages> hostages_{};
    int hostage_count_{};

    std::vector<int> objective_ents_;

    float last_test_{};
    bool test_due_{};

    static constexpr float kTestInterval = 0.05f;

public:
    void CollectEntities(const RadarFrame& frame, const RadarMapObjectives& map);

    void OnVidInit();

    const BombEnt& get_bomb() const
    {
        return bomb_;
    }

    std::span<const HostageEnt> get_hostages() const
    {
        return {hostages_.data(), (size_t)hostage_count_};
    }

    // False on the frames between sight tests, where the marker keeps its last answer.
    bool is_test_due() const
    {
        return test_due_;
    }

private:
    ModelKind ClassifyModel(const goldsrc::model_t* model, const RadarMapObjectives& map);
};

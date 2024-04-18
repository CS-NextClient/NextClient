#pragma once

#include "../HudBase.h"
#include "hud_sprite.h"
#include <array>
#include <algorithm>

class HudSpriteStore : public HudBase
{
public:
    static constexpr auto MESSAGE_NAME = "HudSprite";
    static constexpr auto MAX_HUD_SPRITES = 32;

private:
    std::array<HudSprite*, MAX_HUD_SPRITES> g_hudSprites_{};

public:
    void Init() override;
    void VidInit() override;
    void Draw(float time) override;
    void Think(float time) override;

    void SetSprite(int channel, HudSprite* hudSprite);
    void ClearSprite(int channel);
    void forEachActive(const std::function<void(HudSprite*)>& action);
    void forEachActiveIndexed(const std::function<void(int, HudSprite*)>& action);
};

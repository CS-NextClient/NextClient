#include "../../main.h"
#include "hud_sprite_store.h"
#include "hud_sprite_parser.h"

void HudSpriteStore::Init()
{
    gEngfuncs.pfnHookUserMsg(MESSAGE_NAME, HudSprite_Parser);
}

void HudSpriteStore::VidInit()
{
    forEachActiveIndexed([&](int channel, const auto hudSprite) {
        ClearSprite(channel);
    });
}

void HudSpriteStore::Draw(float time)
{
    forEachActive([](const auto hudSprite) {
        hudSprite->Draw();
    });
}

void HudSpriteStore::Think(float time)
{
    forEachActiveIndexed([&](int channel, auto hudSprite) {
        if (hudSprite->getEndTime() <= time) {
            ClearSprite(channel);
            return;
        }
        hudSprite->Update(time);
    });
}

void HudSpriteStore::ClearSprite(int channel)
{
    delete g_hudSprites_[channel];
    g_hudSprites_[channel] = nullptr;
}

void HudSpriteStore::SetSprite(int channel, HudSprite* hudSprite)
{
    if(g_hudSprites_[channel]) ClearSprite(channel);
    g_hudSprites_[channel] = hudSprite;
}

void HudSpriteStore::forEachActive(const std::function<void(HudSprite*)>& action)
{
    for (auto& hudSprite: g_hudSprites_) {
        if (hudSprite == nullptr) {
            continue;
        }
        action(hudSprite);
    }
}

void HudSpriteStore::forEachActiveIndexed(const std::function<void(int, HudSprite*)>& action)
{
    for (auto channel = 0; channel < g_hudSprites_.size(); ++channel) {
        if (g_hudSprites_[channel] == nullptr) {
            continue;
        }
        action(channel, g_hudSprites_[channel]);
    }
}

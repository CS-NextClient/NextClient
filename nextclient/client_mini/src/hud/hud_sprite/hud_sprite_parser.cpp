#include "../../main.h"
#include "../../hlsdk/sys_dll.h"
#include "../../utils.h"
#include "hud_sprite_store.h"
#include "hud_sprite_parser.h"
#include "parsemsg.h"

int HudSprite_Parser(const char* name, int size, void* buffer)
{
    BEGIN_READ(buffer, size);
    HudSprite_ParseParams();
    return 1;
}

void HudSprite_ParseParams()
{
    int channel = std::clamp(READ_BYTE(), 0, HudSpriteStore::MAX_HUD_SPRITES - 1);
    const char* spritePath = READ_STRING();
    if (spritePath == nullptr || !spritePath[0] || !IsSafeSpriteFilePath(spritePath)) {
        Con_DPrintf("HudSprite_ParseParams: invalid spritePath\n");
        g_GameHud->get_sprite_store()->ClearSprite(channel);
        return;
    }
    const bool isFullScreen = std::clamp(READ_BYTE(), 0, 1);
    const auto red = READ_BYTE();
    const auto green = READ_BYTE();
    const auto blue = READ_BYTE();
    const auto alpha = READ_BYTE();
    const auto frame = READ_SHORT();
    const auto frameRate = READ_FLOAT();
    const auto inTime = READ_FLOAT();
    const auto holdTime = READ_FLOAT();
    const auto outTime = READ_FLOAT();
    auto hudSprite = (new HudSprite())
        ->setSpritePath(spritePath)
        ->setFullScreen(isFullScreen)
        ->setSpriteColor(red, green, blue)
        ->setAlpha(alpha)
        ->setFrame(static_cast<float>(frame == -1 ? 0 : frame))
        ->setFrameRate(frameRate)
        ->setTimings(inTime, holdTime, outTime)
        ->setLooping(frame == -1);
    if (!isFullScreen) {
        const auto x = READ_FLOAT();
        const auto y = READ_FLOAT();
        const auto left = READ_SHORT();
        const auto top = READ_SHORT();
        const auto right = READ_SHORT();
        const auto bottom = READ_SHORT();
        const auto scaleX = READ_FLOAT();
        const auto scaleY = READ_FLOAT();
        hudSprite
            ->setPosition(x, y)
            ->setSpriteRect(left, top, right, bottom)
            ->setScale(scaleX, scaleY);
    }

    const auto renderMode = std::clamp(READ_BYTE(), (int)kRenderNormal, (int)kRenderTransAdd);

    hudSprite->setRenderMode(renderMode);

    g_GameHud->get_sprite_store()->SetSprite(channel, hudSprite);
}

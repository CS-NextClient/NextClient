#include "../../main.h"
#include "../../hlsdk/sys_dll.h"
#include "../../utils.h"
#include "hud_sprite_store.h"
#include "hud_sprite_parser.h"
#include <data_types/MessageReader.h>
#include <goldsrc/entity.h>

static void HudSprite_ParseParams(MessageReader& reader);

int HudSprite_Parser(const char* name, int size, void* buffer)
{
    MessageReader reader(buffer, size);
    HudSprite_ParseParams(reader);
    return 1;
}

static void HudSprite_ParseParams(MessageReader& reader)
{
    int channel = std::clamp(reader.ReadByte(), 0, HudSpriteStore::MAX_HUD_SPRITES - 1);
    std::string spritePath = reader.ReadString();
    if (spritePath.empty() || !IsSafeSpriteFilePath(spritePath))
    {
        Con_DPrintf("HudSprite_ParseParams: invalid spritePath\n");
        g_GameHud->get_sprite_store()->ClearSprite(channel);
        return;
    }
    const bool isFullScreen = std::clamp(reader.ReadByte(), 0, 1);
    const auto red = reader.ReadByte();
    const auto green = reader.ReadByte();
    const auto blue = reader.ReadByte();
    const auto alpha = reader.ReadByte();
    const auto frame = reader.ReadShort();
    const auto frameRate = reader.ReadFloat();
    const auto inTime = reader.ReadFloat();
    const auto holdTime = reader.ReadFloat();
    const auto outTime = reader.ReadFloat();
    auto hudSprite = (new HudSprite())
                         ->setSpritePath(spritePath.c_str())
                         ->setFullScreen(isFullScreen)
                         ->setSpriteColor(red, green, blue)
                         ->setAlpha(alpha)
                         ->setFrame(static_cast<float>(frame == -1 ? 0 : frame))
                         ->setFrameRate(frameRate)
                         ->setTimings(inTime, holdTime, outTime)
                         ->setLooping(frame == -1);
    if (!isFullScreen) {
        const auto x = reader.ReadFloat();
        const auto y = reader.ReadFloat();
        const auto left = reader.ReadShort();
        const auto top = reader.ReadShort();
        const auto right = reader.ReadShort();
        const auto bottom = reader.ReadShort();
        const auto scaleX = reader.ReadFloat();
        const auto scaleY = reader.ReadFloat();
        hudSprite
            ->setPosition(x, y)
            ->setSpriteRect(left, top, right, bottom)
            ->setScale(scaleX, scaleY);
    }

    const auto renderMode = std::clamp(reader.ReadByte(), (int)goldsrc::kRenderNormal, (int)goldsrc::kRenderTransAdd);

    hudSprite->setRenderMode(renderMode);

    g_GameHud->get_sprite_store()->SetSprite(channel, hudSprite);
}

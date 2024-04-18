#pragma once

#include "../../hlsdk.h"
#include "sprite_rect.h"
#include "sprite_color.h"
#include "hud_sprite_state.h"
#include <array>

class HudSprite
{
private:
    using StateTimes = std::array<float, static_cast<size_t>(HudSpriteState::MAX_STATES)>;

    bool isFullScreen_{};
    HSPRITE_t sprite_{};
    SpriteRect spriteRect_{};
    SpriteColor spriteColor_{};
    HudSpriteState spriteState_{};
    float frame_{};
    float frameRate_{};
    float inTime_{};
    float holdTime_{};
    float outTime_{};
    float x_{};
    float y_{};
    float scaleX_{};
    float scaleY_{};

    bool isLooped_{};
    float currentAlpha_{};
    float maxFrames_{};
    float nextStateTime_{};
    float animTime_{};
    float endTime_{};
    StateTimes stateEndTimes_{};
    int renderMode_{};

public:
    void Draw() const;

    void Update(float time);

    void UpdateState(float time);

    void Animation(float time);

    float XPosition(float spriteWidth, float screenWidth) const;

    float YPosition(float spriteHeight, float screenHeight) const;

    bool isFullScreen() const;

    const HSPRITE_t& getSprite() const;

    const SpriteRect& getSpriteRect() const;

    const SpriteColor& getColor() const;

    const HudSpriteState& getSpriteState() const;

    void setSpriteState(HudSpriteState state);

    float getFrame() const;

    int getFrameInt() const;

    float getFrameRate() const;

    float getInTime() const;

    float getHoldTime() const;

    float getOutTime() const;

    float getX() const;

    float getY() const;

    float getScaleX() const;

    float getScaleY() const;

    bool isLooped() const;

    float getCurrentAlpha() const;

    void setCurrentAlpha(float alpha);

    float getMaxFrames() const;

    void setMaxFrames(float maxFrames);

    float getNextStateTime() const;

    void setNextStateTime(float time);

    float getAnimTime() const;

    void setAnimTime(float animTime);

    float getEndTime() const;

    void setEndTime(float endTime);

    float getStateEndTime(HudSpriteState state) const;

    void setStateEndTime(HudSpriteState hudSpriteState, float time);

    float getFullSpriteWidth() const;

    float getFullSpriteHeight() const;

    HudSprite* setFullScreen(bool isFullScreen);

    HudSprite* setSpritePath(const char* spritePath);

    HudSprite* setSpriteRect(int left, int top, int right, int bottom);

    HudSprite* setSpriteColor(byte red, byte green, byte blue);

    HudSprite* setAlpha(byte alpha);

    HudSprite* setScale(float scaleX, float scaleY);

    HudSprite* setFrame(float frame);

    HudSprite* setFrameRate(float frameRate);

    HudSprite* setTimings(float inTime, float holdTime, float outTime);

    HudSprite* setPosition(float x, float y);

    HudSprite* setLooping(bool isLooped);

    HudSprite* setRenderMode(int renderMode);

    int getRenderMode() const;

    static float normalizeColor(byte color);
};

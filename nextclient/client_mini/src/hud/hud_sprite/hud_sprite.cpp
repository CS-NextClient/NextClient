#include "../../main.h"
#include "../../hlsdk.h"
#include "hud_sprite.h"
#include "triangleapi.h"

void HudSprite::Draw() const
{
    SCREENINFO screen;
    screen.iSize = sizeof(SCREENINFO);
    gEngfuncs.pfnGetScreenInfo(&screen);

    auto screenWidth = static_cast<float>(screen.iWidth);
    auto screenHeight = static_cast<float>(screen.iHeight);

    const auto sprite = gEngfuncs.GetSpritePointer(getSprite());
    if(sprite == nullptr) return;

    const auto ta = gEngfuncs.pTriAPI;
    ta->SpriteTexture(const_cast<model_s*>(sprite), getFrameInt());
    ta->RenderMode(getRenderMode());
    ta->Color4f(getColor().red, getColor().green, getColor().blue, getCurrentAlpha());
    ta->CullFace(TRI_NONE);
    if (isFullScreen()) {
        ta->Begin(TRI_QUADS);
        ta->TexCoord2f(0, 0);
        ta->Vertex3f(0, 0, 0);
        ta->TexCoord2f(0, 1);
        ta->Vertex3f(0, screenHeight, 0);
        ta->TexCoord2f(1, 1);
        ta->Vertex3f(screenWidth, screenHeight, 0);
        ta->TexCoord2f(1, 0);
        ta->Vertex3f(screenWidth, 0, 0);
        ta->End();
    } else {
        const auto spriteWidth = (getSpriteRect().right - getSpriteRect().left) * getScaleX();
        const auto spriteHeight = (getSpriteRect().bottom - getSpriteRect().top) * getScaleY();
        const auto fullSpriteWidth = getFullSpriteWidth() * getScaleX();
        const auto fullSpriteHeight = getFullSpriteHeight() * getScaleY();

        const auto x = XPosition(spriteWidth, screenWidth);
        const auto y = YPosition(spriteHeight, screenHeight);

        const auto left = getSpriteRect().left / fullSpriteWidth * getScaleX();
        const auto top = getSpriteRect().top / fullSpriteHeight * getScaleY();
        const auto right = getSpriteRect().right / fullSpriteWidth * getScaleX();
        const auto bottom = getSpriteRect().bottom / fullSpriteHeight * getScaleY();

        ta->Begin(TRI_QUADS);
        ta->TexCoord2f(left, top);
        ta->Vertex3f(x, y, 0);
        ta->TexCoord2f(left, bottom);
        ta->Vertex3f(x, y + spriteHeight, 0);
        ta->TexCoord2f(right, bottom);
        ta->Vertex3f(x + spriteWidth, y + spriteHeight, 0);
        ta->TexCoord2f(right, top);
        ta->Vertex3f(x + spriteWidth, y, 0);
        ta->End();
    }
    ta->RenderMode(kRenderNormal);
}

void HudSprite::Update(float time)
{
    UpdateState(time);
    Animation(time);
}

void HudSprite::UpdateState(float time)
{
    const auto alpha = getColor().alpha;
    switch (getSpriteState()) {
        case HudSpriteState::In: {
            const auto newAlpha = (getStateEndTime(HudSpriteState::In) - time) / getInTime();
            setCurrentAlpha((1 - newAlpha) * alpha);
            if (getNextStateTime() <= time) {
                if (getHoldTime() != 0) {
                    setNextStateTime(getStateEndTime(HudSpriteState::Hold));
                    setSpriteState(HudSpriteState::Hold);
                } else if (getOutTime() != 0) {
                    setNextStateTime(getStateEndTime(HudSpriteState::Out));
                    setSpriteState(HudSpriteState::Out);
                }
            }
            break;
        }
        case HudSpriteState::Hold: {
            setCurrentAlpha(alpha);
            if (getNextStateTime() <= time && getOutTime() != 0) {
                setNextStateTime(getStateEndTime(HudSpriteState::Out));
                setSpriteState(HudSpriteState::Out);
            }
            break;
        }
        case HudSpriteState::Out: {
            const auto newAlpha = (getStateEndTime(HudSpriteState::Out) - time) / getOutTime();
            setCurrentAlpha(newAlpha * alpha);
            break;
        }
        default: {
            // Nothing
            break;
        }
    }
}

void HudSprite::Animation(float time)
{
    if (!isLooped() || getMaxFrames() <= 1 || getFrameRate() <= 0) {
        return;
    }
    const auto interval = time - getAnimTime();
    if (interval <= 0.001f) {
        setAnimTime(time);
        return;
    }
    setFrame(getFrame() + (interval * getFrameRate() * getMaxFrames()));
    setAnimTime(time);
    if (getFrame() < 0 || getFrame() >= getMaxFrames()) {
        setFrame(getFrame() - (getFrame() / getMaxFrames() * getMaxFrames()));
    }
}

float HudSprite::XPosition(float spriteWidth, float screenWidth) const
{
    float x;
    if (getX() == -1) {
        x = (screenWidth - spriteWidth) / 2;
    } else {
        if (getX() < 0) {
            x = (1.f + getX()) * screenWidth - spriteWidth;
        } else {
            x = getX() * screenWidth;
        }
    }
    if (x + spriteWidth > screenWidth) {
        x = screenWidth - spriteWidth;
    } else if (x < 0) {
        x = 0;
    }
    return x;
}

float HudSprite::YPosition(float spriteHeight, float screenHeight) const
{
    float y;
    if (getY() == -1) {
        y = (screenHeight - spriteHeight) / 2;
    } else {
        if (getY() < 0) {
            y = (1.f + getY()) * screenHeight - spriteHeight;
        } else {
            y = getY() * screenHeight;
        }
    }
    if (y + spriteHeight > screenHeight) {
        y = screenHeight - spriteHeight;
    } else if (y < 0) {
        y = 0;
    }
    return y;
}

bool HudSprite::isFullScreen() const
{
    return isFullScreen_;
}

const HSPRITE_t& HudSprite::getSprite() const
{
    return sprite_;
}

const SpriteRect& HudSprite::getSpriteRect() const
{
    return spriteRect_;
}

const SpriteColor& HudSprite::getColor() const
{
    return spriteColor_;
}

const HudSpriteState& HudSprite::getSpriteState() const
{
    return spriteState_;
}

void HudSprite::setSpriteState(HudSpriteState state)
{
    spriteState_ = state;
}

float HudSprite::getFrame() const
{
    return frame_;
}

int HudSprite::getFrameInt() const
{
    return static_cast<int>(frame_);
}

float HudSprite::getFrameRate() const
{
    return frameRate_;
}

float HudSprite::getInTime() const
{
    return inTime_;
}

float HudSprite::getHoldTime() const
{
    return holdTime_;
}

float HudSprite::getOutTime() const
{
    return outTime_;
}

float HudSprite::getX() const
{
    return x_;
}

float HudSprite::getY() const
{
    return y_;
}

float HudSprite::getScaleX() const
{
    return scaleX_;
}

float HudSprite::getScaleY() const
{
    return scaleY_;
}

bool HudSprite::isLooped() const
{
    return isLooped_;
}

float HudSprite::getCurrentAlpha() const
{
    return currentAlpha_;
}

void HudSprite::setCurrentAlpha(float alpha)
{
    currentAlpha_ = alpha;
}

float HudSprite::getMaxFrames() const
{
    return maxFrames_;
}

void HudSprite::setMaxFrames(float maxFrames)
{
    maxFrames_ = maxFrames;
}

float HudSprite::getNextStateTime() const
{
    return nextStateTime_;
}

void HudSprite::setNextStateTime(float time)
{
    nextStateTime_ = time;
}

float HudSprite::getAnimTime() const
{
    return animTime_;
}

void HudSprite::setAnimTime(float animTime)
{
    animTime_ = animTime;
}

float HudSprite::getEndTime() const
{
    return endTime_;
}

void HudSprite::setEndTime(float endTime)
{
    endTime_ = endTime;
}

float HudSprite::getStateEndTime(HudSpriteState state) const
{
    return stateEndTimes_[static_cast<int>(state)];
}

void HudSprite::setStateEndTime(HudSpriteState hudSpriteState, float time)
{
    stateEndTimes_[static_cast<int>(hudSpriteState)] = time;
}

float HudSprite::getFullSpriteWidth() const
{
    return static_cast<float>(gEngfuncs.pfnSPR_Width(getSprite(), getFrameInt()));
}

float HudSprite::getFullSpriteHeight() const
{
    return static_cast<float>(gEngfuncs.pfnSPR_Height(getSprite(), getFrameInt()));
}

HudSprite* HudSprite::setFullScreen(bool isFullScreen)
{
    isFullScreen_ = isFullScreen;
    setSpriteRect(0, 0, 0, 0);
    return this;
}

HudSprite* HudSprite::setSpritePath(const char* spritePath)
{
    sprite_ = gEngfuncs.pfnSPR_Load(spritePath);
    setMaxFrames(static_cast<float>(gEngfuncs.pfnSPR_Frames(sprite_)));
    return this;
}

HudSprite* HudSprite::setSpriteRect(int left, int top, int right, int bottom)
{
    if (left + top + right + bottom > 0.f) {
        spriteRect_.left = static_cast<float>(left);
        spriteRect_.top = static_cast<float>(top);
        spriteRect_.right = static_cast<float>(right);
        spriteRect_.bottom = static_cast<float>(bottom);
    } else {
        spriteRect_.left = 0;
        spriteRect_.top = 0;
        spriteRect_.right = getFullSpriteWidth();
        spriteRect_.bottom = getFullSpriteHeight();
    }
    return this;
}

HudSprite* HudSprite::setSpriteColor(byte red, byte green, byte blue)
{
    spriteColor_.red = normalizeColor(red);
    spriteColor_.green = normalizeColor(green);
    spriteColor_.blue = normalizeColor(blue);
    return this;
}

HudSprite* HudSprite::setAlpha(byte alpha)
{
    spriteColor_.alpha = normalizeColor(alpha);
    return this;
}

HudSprite* HudSprite::setScale(float scaleX, float scaleY)
{
    scaleX_ = scaleX;
    scaleY_ = scaleY;
    return this;
}

HudSprite* HudSprite::setFrame(float frame)
{
    frame_ = frame;
    return this;
}

HudSprite* HudSprite::setFrameRate(float frameRate)
{
    frameRate_ = frameRate;
    return this;
}

HudSprite* HudSprite::setTimings(float inTime, float holdTime, float outTime)
{
    const auto time = gEngfuncs.GetClientTime();
    inTime_ = inTime;
    holdTime_ = holdTime;
    outTime_ = outTime;
    setEndTime(time + inTime + holdTime + outTime);
    setStateEndTime(HudSpriteState::In, time + inTime);
    setStateEndTime(HudSpriteState::Hold, time + inTime + holdTime);
    setStateEndTime(HudSpriteState::Out, time + inTime + holdTime + outTime);
    if (inTime != 0) {
        setNextStateTime(getStateEndTime(HudSpriteState::In));
        setSpriteState(HudSpriteState::In);
    } else if (holdTime != 0) {
        setNextStateTime(getStateEndTime(HudSpriteState::Hold));
        setSpriteState(HudSpriteState::Hold);
    } else if (outTime != 0) {
        setNextStateTime(getStateEndTime(HudSpriteState::Out));
        setSpriteState(HudSpriteState::Out);
    } else {
        setNextStateTime(getEndTime());
        setSpriteState(HudSpriteState::Hold);
    }
    UpdateState(0.0);
    return this;
}

HudSprite* HudSprite::setPosition(float x, float y)
{
    x_ = x;
    y_ = y;
    return this;
}

HudSprite* HudSprite::setLooping(bool isLooped)
{
    isLooped_ = isLooped;
    return this;
}

HudSprite* HudSprite::setRenderMode(int renderMode)
{
    renderMode_ = renderMode;
    return this;
}

int HudSprite::getRenderMode() const
{
    return renderMode_;
}

float HudSprite::normalizeColor(byte color)
{
    return static_cast<float>(color) / 255.f;
}

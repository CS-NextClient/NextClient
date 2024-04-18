#pragma once

class HudBase
{
public:
    virtual ~HudBase() = default;

    virtual void Init() { }
    virtual void VidInit() { }
    virtual void Draw(float time) { }
    virtual void Think(float time) {}
    virtual void Reset() {}
    virtual void InitHUDData() {}		// called every time a server is connected to
};
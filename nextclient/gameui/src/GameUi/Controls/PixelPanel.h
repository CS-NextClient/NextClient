#pragma once

#include <cstdint>
#include <vector>

#include <ncl_math/color.h>
#include <vgui_controls/Panel.h>

// Draws an RGBA image its owner fills in: a gradient is something the scheme cannot draw,
// and hundreds of filled rectangles would cost more than uploading an image.
class CPixelPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CPixelPanel, vgui2::Panel);

private:
    int m_TextureId;    // the surface's, held for the life of the panel and released with it
    int m_Width;
    int m_Height;
    std::vector<uint8_t> m_Pixels;

    // True until the next paint has re-uploaded what SetPixels changed.
    bool m_Changed;

public:
    CPixelPanel(vgui2::Panel* parent, const char* name, int width, int height);
    ~CPixelPanel() override;

    // RGBA, width * height * 4 bytes, as passed to the constructor.
    void SetPixels(const uint8_t* rgba);

    void SetSolidColor(const ncl_math::Color& color);

    void Paint() override;

    void OnThink() override;
};

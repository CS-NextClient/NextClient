#pragma once

#include <vector>

#include <vgui_controls/Panel.h>

// Draws pixels the caller generates. Everything the colour picker shows - the shade square,
// the hue bar, the alpha bar - is a gradient nothing in the scheme can draw, and building
// them out of hundreds of filled rectangles would cost more than uploading an image.
//
// One texture id for the life of the panel, updated in place. The surface hands out texture
// ids and never takes them back, so a panel that made a new one every time its contents
// changed would leak one per frame of a drag.
class CPixelPanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CPixelPanel, vgui2::Panel);

public:
    CPixelPanel(vgui2::Panel* parent, const char* name, int width, int height);

    // RGBA, width * height * 4 bytes, as passed to the constructor.
    void SetPixels(const unsigned char* rgba);

    int image_width() const { return m_Width; }
    int image_height() const { return m_Height; }

    void Paint() override;

    // Marks itself dirty every frame, for the same reason the image panel does: these are
    // painted when something has marked them so, and a gradient that only changes when it
    // is set has nothing to mark it.
    void OnThink() override;

private:
    int m_TextureId;
    int m_Width;
    int m_Height;

    // Kept rather than handed over once: the surface is given the pixels again on every
    // paint, which is what puts the draw state back where an RGBA image needs it.
    std::vector<unsigned char> m_Pixels;

    // True until the next paint has re-uploaded what SetPixels changed.
    bool m_Changed;
};

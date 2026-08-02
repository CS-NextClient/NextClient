#include "PixelPanel.h"

#include <vgui/ISurfaceNext.h>

using namespace vgui2;

CPixelPanel::CPixelPanel(Panel* parent, const char* name, int width, int height) :
    BaseClass(parent, name)
{
    m_Width = width;
    m_Height = height;
    m_Changed = false;

    // Procedural: the id is claimed once here and the contents are replaced through it.
    m_TextureId = surface()->CreateNewTextureID(true);

    SetPaintBackgroundEnabled(false);
}

void CPixelPanel::SetPixels(const unsigned char* rgba)
{
    if (rgba == nullptr)
    {
        return;
    }

    m_Pixels.assign(rgba, rgba + static_cast<size_t>(m_Width) * m_Height * 4);
    m_Changed = true;

    // Panels are painted when they are marked dirty, not once a frame: without this the
    // shade square would keep the hue it was built with until something else invalidated
    // it, which is the one thing a colour picker must not do.
    Repaint();
}

void CPixelPanel::OnThink()
{
    BaseClass::OnThink();

    if (!m_Pixels.empty())
    {
        Repaint();
    }
}

void CPixelPanel::Paint()
{
    if (m_Pixels.empty())
    {
        return;
    }

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    // Re-set on every paint, and reloaded outright only when the contents actually changed.
    // Handing the surface a texture id it already holds is what puts the draw state back
    // where an RGBA image needs it; binding by id alone leaves whatever the previous panel
    // was drawing with.
    surface()->DrawSetColor(255, 255, 255, 255);
    surface()->DrawSetTextureRGBA(m_TextureId, m_Pixels.data(), m_Width, m_Height, 1, m_Changed);
    surface()->DrawTexturedRect(0, 0, wide, tall);

    m_Changed = false;
}

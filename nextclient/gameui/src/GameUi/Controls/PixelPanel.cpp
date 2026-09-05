#include "PixelPanel.h"

#include <cstdint>

#include <vgui/ISurfaceNext.h>

using namespace vgui2;

CPixelPanel::CPixelPanel(Panel* parent, const char* name, int width, int height) :
    BaseClass(parent, name)
{
    m_Width = width;
    m_Height = height;
    m_Changed = false;
    m_TextureId = surface()->CreateNewTextureID(true);

    SetPaintBackgroundEnabled(false);
}

CPixelPanel::~CPixelPanel()
{
    surface()->DeleteTextureByID(m_TextureId);
}

void CPixelPanel::SetPixels(const uint8_t* rgba)
{
    if (rgba == nullptr)
    {
        return;
    }

    m_Pixels.assign(rgba, rgba + static_cast<size_t>(m_Width) * m_Height * 4);
    m_Changed = true;

    // Panels are painted when they are marked dirty, not once a frame, so a new image would
    // otherwise wait for whatever invalidates the panel next.
    Repaint();
}

void CPixelPanel::SetSolidColor(const ncl_math::Color& color)
{
    m_Pixels.resize(static_cast<size_t>(m_Width) * m_Height * 4);

    for (size_t i = 0; i < m_Pixels.size(); i += 4)
    {
        m_Pixels[i + 0] = color.r;
        m_Pixels[i + 1] = color.g;
        m_Pixels[i + 2] = color.b;
        m_Pixels[i + 3] = color.a;
    }

    m_Changed = true;
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

    // Handed the id again on every paint rather than bound by id alone: that is what puts
    // the surface's draw state back where an RGBA image needs it. The pixels themselves
    // are reloaded only when they changed.
    surface()->DrawSetColor(255, 255, 255, 255);
    surface()->DrawSetTextureRGBA(m_TextureId, m_Pixels.data(), m_Width, m_Height, 1, m_Changed);
    surface()->DrawTexturedRect(0, 0, wide, tall);

    m_Changed = false;
}

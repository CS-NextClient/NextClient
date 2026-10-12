#pragma once

#include <vgui_controls/Panel.h>

// A decoded image, cached per path for the life of the process. Opaque: what a caller needs
// is to hold one and draw it.
struct ImageFilePixels;

// Null when the path is empty, unreadable or not an image the client would accept. The
// failure is cached too, so a caller naming a file that is not there does not hit the
// filesystem on every frame.
const ImageFilePixels* ImageFile_Get(const char* path);

// Draws it into the panel currently being painted, fitted into a square of the given side.
// Available apart from any panel so a control that is already something else - a button, in
// particular - can carry an image without a second panel over it, which is an overlap and
// therefore a question of painting order that nothing needs to have.
void ImageFile_Draw(const ImageFilePixels* image, int x, int y, int side);

// Draws an image file from the game directory. The stock vgui image system is built around
// the materials a scheme names and does not take an arbitrary path, so this decodes the
// file itself and hands the pixels to the surface.
//
// The renderer reads the same formats through the same decoder, so what this shows and
// what the game draws are one file rather than two representations of one.
class CImageFilePanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CImageFilePanel, vgui2::Panel);

private:
    // Points into the decoded image kept for this path, or null when there is none. The
    // pixels are cached per path and outlive the panel: texture ids are handed out by the
    // surface and never given back, so one is kept per distinct path for the life of the
    // process and creating one per paint would leak.
    const ImageFilePixels* m_pImage;

public:
    CImageFilePanel(vgui2::Panel* parent, const char* name);

    // Game-directory-relative, as a material's own paths are. An empty or unreadable path
    // leaves the panel showing nothing rather than showing the previous image.
    void SetImageFile(const char* path);

    void Paint() override;

    // Marks itself dirty every frame. Panels here are painted when something has marked
    // them so, not once a frame, and an image that only changes when it is set has nothing
    // to mark it: it draws once and is gone the moment anything else repaints over it. The
    // cost is a handful of small panels in an authoring dialog.
    void OnThink() override;
};

// What the cache holds and how often each entry has actually been painted. Told apart
// because the three ways this can fail look identical on screen: a file that did not
// decode, an image that decoded but is never painted, and one that is painted and still
// cannot be seen.
void ImageFilePanel_Report();

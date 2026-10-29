#pragma once

#include <memory>
#include <string>

#include <vgui_controls/Panel.h>

#include "GameUi/Controls/ImageFileCache.h"

class IFileSystem;

namespace goldsrc
{
    struct cl_enginefunc_t;
}

namespace vgui2
{
    class ISurface;
}

std::shared_ptr<ImageFileCache> ImageFile_CreateCache(IFileSystem& filesystem, vgui2::ISurface& surface);
void ImageFile_Draw(ImageFileCache& images, vgui2::ISurface& surface, const std::string& path, int x, int y, int side);

class CImageFilePanel : public vgui2::Panel
{
    DECLARE_CLASS_SIMPLE(CImageFilePanel, vgui2::Panel);

private:
    std::shared_ptr<ImageFileCache> m_Images;
    vgui2::ISurface& m_Surface;
    std::string m_Path;

public:
    CImageFilePanel(vgui2::Panel* parent, const char* name, std::shared_ptr<ImageFileCache> images, vgui2::ISurface& surface);
    void SetImageFile(const char* path);
    void Paint() override;
    void OnThink() override;
};

void ImageFilePanel_Report(const ImageFileCache* cache, goldsrc::cl_enginefunc_t& engine);

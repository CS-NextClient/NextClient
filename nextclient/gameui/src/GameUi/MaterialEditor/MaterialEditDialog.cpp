#include "GameUi/MaterialEditor/MaterialEditDialog.h"

#include <algorithm>
#include <utility>

#include <FileSystem.h>
#include <ncl_material/MaterialDoc.h>
#include <vgui/ILocalize.h>
#include <vgui/IInput.h>
#include <vgui/IInputInternal.h>
#include <vgui/ISystem.h>
#include <vgui/IScheme.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/QueryBox.h>
#include <vgui_controls/MessageBox.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>

#include "GameUi/Controls/ImageFilePanel.h"
#include "GameUi/MaterialEditor/MaterialPreviewPanel.h"
#include "GameUi/MaterialEditor/MaterialLightPanel.h"
#include "GameUi/MaterialEditor/MaterialModelPicker.h"
#include "GameUi/MaterialEditor/MaterialModelList.h"
#include "GameUi/GameConsole.h"
#include "GameUi/GameUi.h"
#include "GameUi/MaterialEditor/MaterialParamRow.h"
#include "GameUi/MaterialEditor/MaterialChangeQuery.h"
#include "GameUi/MaterialEditor/MaterialResourceCatalog.h"
#include "GameUi/MaterialEditor/MaterialSaveWorkflow.h"
#include "GameUi/PanelListPanel.h"

namespace material_editor
{

using namespace vgui2;

namespace
{
    constexpr int kGroupHeaderHeight = 22;
    constexpr int kSceneControlsPadding = 12;
    constexpr int kSceneControlsGap = 8;

    constexpr int kThumbSize = 40;
    constexpr int kBrowserRowHeight = 44;
    constexpr int kBrowserPreviewSize = 150;

    // The tab strip sits under the title bar, and every page starts below it.
    constexpr int kTabTop = 28;
    constexpr int kTabHeight = 22;
    constexpr int kPageTop = kTabTop + kTabHeight + 6;
    constexpr const char* kLabels[] = {"ModelLabel", "TextureLabel", "ProgramLabel"};

    constexpr int kButtonWide = 76;
    constexpr int kButtonGap = 4;
    constexpr float kPlaybackRates[] = {0.25f, 0.5f, 1.0f, 1.5f, 2.0f};

    // Focus repainting would cover an image painted by a separate overlapping panel.
    class BrowserRow : public Button
    {
        DECLARE_CLASS_SIMPLE(BrowserRow, Button);

    private:
        std::shared_ptr<ImageFileCache> m_Images;
        ISurface& m_Surface;
        std::string m_Path;

    public:
        BrowserRow(
            Panel* parent,
            Panel* owner,
            std::shared_ptr<ImageFileCache> images,
            ISurface& surface,
            const std::string& path,
            const char* command
        ) :
            BaseClass(parent, "BrowserRow", NameOf(path), owner, command),
            m_Images(std::move(images)),
            m_Surface(surface),
            m_Path(path)
        {
            SetContentAlignment(a_west);
            SetTextInset(kThumbSize + 6, 0);
        }

        void Paint() override
        {
            BaseClass::Paint();

            ImageFile_Draw(*m_Images, m_Surface, m_Path, 2, 2, kThumbSize);
        }

        // Panels here are painted when something has marked them dirty rather than once a
        // frame, and an image that never changes has nothing to mark it.
        void OnThink() override
        {
            BaseClass::OnThink();

            if (!m_Path.empty())
            {
                Repaint();
            }
        }

    private:
        static const char* NameOf(const std::string& path)
        {
            const char* slash = V_strrchr(path.c_str(), '/');

            return slash != nullptr ? slash + 1 : path.c_str();
        }
    };

    const char* SourceToken(render::MaterialEditSource source)
    {
        switch (source)
        {
        case render::MaterialEditSource::Instance: return "#GameUI_MatEdit_SourceInstance";
        case render::MaterialEditSource::ModelDocument: return "#GameUI_MatEdit_SourceDocument";
        case render::MaterialEditSource::Implicit: return "#GameUI_MatEdit_SourceImplicit";
        default: return "#GameUI_MatEdit_SourceNone";
        }
    }

    const char* SaveFailureToken(SaveFailureReason reason)
    {
        switch (reason)
        {
            case SaveFailureReason::ModelCapacity:
                return "#GameUI_MatEdit_ModelCapacity";
            case SaveFailureReason::EditorUnavailable:
            case SaveFailureReason::RendererUnavailable:
                return "#GameUI_MatEdit_PreviewUnavailable";
            case SaveFailureReason::NoMatchingTexture:
                return "#GameUI_MatEdit_NoMatchingTexture";
            default:
                return "#GameUI_MatEdit_ModelLoadFailed";
        }
    }

    // A section header inside the parameter list. Its own panel because the list stacks
    // panels and a header is not a row with a value.
    class GroupHeader : public Label
    {
        DECLARE_CLASS_SIMPLE(GroupHeader, Label);

    private:
        ISurface& surface_;

    public:
        GroupHeader(Panel* parent, ISurface& surface, const char* text) :
            BaseClass(parent, "GroupHeader", text),
            surface_(surface)
        {
            SetContentAlignment(a_west);
        }

        void ApplySchemeSettings(IScheme* scheme) override
        {
            BaseClass::ApplySchemeSettings(scheme);

            HFont bold = scheme->GetFont("DefaultBold", IsProportional());

            if (bold != INVALID_FONT)
            {
                SetFont(bold);
            }

            // Measured in the font actually in use rather than guessed in pixels, so the
            // indent stays three spaces whatever the scheme and the ui scale make of them.
            int wide = 0;
            int tall = 0;
            surface_.GetTextSize(GetFont(), L"   ", wide, tall);

            SetTextInset(wide, 0);
        }
    };
}

CMaterialEditDialog::CMaterialEditDialog(
    Panel* parent,
    CGameUI& host,
    render::MaterialEditInterface& editor,
    ScenePreviewInterface& preview,
    IFileSystem& filesystem,
    ISurface& surface,
    std::shared_ptr<ImageFileCache> images
) :
    BaseClass(parent, "CMaterialEditDialog"),
    m_Host(host),
    m_Editor(editor),
    m_Preview(preview),
    m_Surface(surface),
    m_Images(std::move(images)),
    m_ResourceSource(filesystem),
    m_ResourceCatalog(m_ResourceSource)
{
    SetTitle("#GameUI_MatEdit_Title", true);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);
    SetMoveable(false);
    DisableFadeEffect();
    m_pControls = new Panel(this, "MaterialControls");
    m_pPreview = new CMaterialPreviewPanel(this, m_PreviewState, preview, editor);
    m_pPreview->AddActionSignalTarget(this);
    m_pSceneControls = new Panel(this, "ModelControls");
    m_pLighting = new CMaterialLightPanel(m_pControls, m_PreviewState);

    m_pTabMaterial = new Button(m_pControls, "TabMaterial", "#GameUI_MatEdit_TabMaterial", this, "tab_material");
    m_pTabLighting = new Button(m_pControls, "TabLighting", "#GameUI_MatEdit_TabLighting", this, "tab_lighting");

    m_pModel = new CMaterialModelPicker(m_pControls, "Model");
    m_pTexture = new ComboBox(m_pControls, "Texture", 16, false);
    m_pProgram = new ComboBox(m_pControls, "Program", 8, false);


    m_pTexture->SetEditable(false);
    m_pProgram->SetEditable(false);

    m_pModel->AddActionSignalTarget(this);
    m_pTexture->AddActionSignalTarget(this);
    m_pTextureGroup = new Button(m_pControls, "TextureGroup", "#GameUI_MatEdit_TextureGroup", this, "texture_group");
    m_pTextureList = new CPanelListPanel(m_pControls, "TextureGroupList");
    m_pTextureAll = new Button(m_pControls, "TextureAll", "#GameUI_MatEdit_SelectAll", this, "texture_all");
    m_pTextureApply = new Button(m_pControls, "TextureApply", "#GameUI_MatEdit_ApplyGroup", this, "texture_apply");
    m_pTextureBack = new Button(m_pControls, "TextureBack", "#GameUI_MatEdit_Back", this, "texture_back");
    m_pTextureGroupCount = new Label(m_pControls, "TextureGroupCount", "#GameUI_MatEdit_SelectTextures");
    m_pProgram->AddActionSignalTarget(this);

    new Label(m_pControls, "ModelLabel", "#GameUI_MatEdit_Model");
    new Label(m_pControls, "TextureLabel", "#GameUI_MatEdit_Texture");
    new Label(m_pControls, "ProgramLabel", "#GameUI_MatEdit_Program");

    new Label(m_pControls, "SkinLabel", "#GameUI_MatEdit_Skin");
    new Label(m_pControls, "BodyLabel", "#GameUI_MatEdit_Body");

    m_pSkin = new ComboBox(m_pControls, "Skin", 12, false);
    m_pBody = new ComboBox(m_pControls, "Body", 12, false);
    m_pSkin->SetEditable(false);
    m_pBody->SetEditable(false);
    m_pSkin->AddActionSignalTarget(this);
    m_pBody->AddActionSignalTarget(this);

    m_pSource = new Label(m_pControls, "Source", "");
    m_pValidation = new Label(m_pControls, "Validation", "");

    m_pRows = new CPanelListPanel(m_pControls, "Rows");

    // Suppresses every material at once, which is the only comparison that means anything:
    // "with" against "without" has to differ for a reason the author chose.
    m_pCompare = new CheckButton(m_pControls, "Compare", "#GameUI_MatEdit_Compare");
    m_pCompare->AddActionSignalTarget(this);

    new Label(m_pControls, "RelatedTargetsLabel", "#GameUI_MatEdit_SaveTargets");

    for (size_t index = 0; index < m_SiblingTargets.size(); index++)
    {
        char name[32];
        V_snprintf(name, sizeof(name), "RelatedModel%u", static_cast<unsigned int>(index));
        m_SiblingTargets[index] = new CheckButton(m_pControls, name, "");
    }

    m_pSave = new Button(m_pControls, "Save", "#GameUI_MatEdit_Save", this, "save");
    m_pStub = new Button(m_pControls, "Stub", "#GameUI_MatEdit_Stub", this, "stub");


    m_pEmpty = new Label(m_pControls, "Empty", "");
    m_pEmpty->SetWrap(true);

    m_pState = new Button(m_pControls, "State", "#GameUI_MatEdit_State", this, "state");

    new Label(m_pControls, "StandInTargetLabel", "#GameUI_MatEdit_Target");

    m_pStandInTarget = new ComboBox(m_pControls, "StandInTargetBox", 4, false);
    m_pStandInTarget->SetEditable(false);
    m_pStandInTarget->AddItem("#GameUI_MatEdit_ModelView", nullptr);
    m_pStandInTarget->AddItem("#GameUI_MatEdit_WeaponView", nullptr);
    m_pStandInTarget->AddActionSignalTarget(this);


    m_pMirror = new CheckButton(m_pControls, "Mirror", "#GameUI_MatEdit_Mirror");
    m_pMirror->AddActionSignalTarget(this);

    new Label(m_pControls, "StandInSequenceLabel", "#GameUI_MatEdit_StandInSequence");
    new Label(m_pControls, "StandInFrameLabel", "#GameUI_MatEdit_StandInFrame");
    new Label(m_pControls, "PlaybackRateLabel", "#GameUI_MatEdit_PlaybackRate");
    m_pPlaybackRate = new ComboBox(m_pControls, "PlaybackRate", 5, false);

    for (const char* rate : {"0.25x", "0.5x", "1x", "1.5x", "2x"})
    {
        m_pPlaybackRate->AddItem(rate, nullptr);
    }

    m_pPlaybackRate->SilentActivateItemByRow(2);
    m_pPlaybackRate->AddActionSignalTarget(this);

    m_pStandInSequence = new ComboBox(m_pControls, "StandInSequenceBox", 16, false);

    m_pStandInSequence->SetEditable(false);
    m_pStandInSequence->AddActionSignalTarget(this);

    m_pResetView = new Button(m_pSceneControls, "ResetView", "#GameUI_MatEdit_ResetView", this, "reset_view");

    m_pStandInFrame = new Slider(m_pControls, "StandInFrame");
    m_pStandInFrame->SetRange(0, 1000);
    m_pStandInFrame->AddActionSignalTarget(this);

    m_pStandInPlaying = new CheckButton(m_pControls, "StandInPlaying", "#GameUI_MatEdit_StandInPlaying");
    m_pStandInPlaying->AddActionSignalTarget(this);

    m_pStandInPlace = new Button(m_pControls, "StandInPlace", "#GameUI_MatEdit_FrameModel", this, "standin_place");

    m_BrowserHighlighted = -1;

    m_pBrowserFolder = new ComboBox(m_pControls, "BrowserFolder", 12, false);
    m_pBrowserFolder->SetEditable(false);
    m_pBrowserFolder->AddActionSignalTarget(this);

    m_pBrowserList = new CPanelListPanel(m_pControls, "BrowserList");
    m_pBrowserPreview = new CImageFilePanel(m_pControls, "BrowserPreview", m_Images, surface);
    m_pBrowserPath = new Label(m_pControls, "BrowserPath", "");

    m_pBrowserChoose = new Button(m_pControls, "BrowserChoose", "#GameUI_MatEdit_Choose", this, "browse_choose");
    m_pBrowserClear = new Button(m_pControls, "BrowserClear", "#GameUI_MatEdit_Clear", this, "browse_clear");
    m_pBrowserBack = new Button(m_pControls, "BrowserBack", "#GameUI_MatEdit_Back", this, "browse_back");

    m_pCatalogFilter = new TextEntry(m_pControls, "CatalogFilter");
    m_pCatalogFilter->AddActionSignalTarget(this);

    m_pCatalogList = new CMaterialModelList(m_pControls, this, filesystem, surface);
    m_pCatalogCount = new Label(m_pControls, "CatalogCount", "");

    m_pCatalogClear = new Button(m_pControls, "CatalogClear", "#GameUI_MatEdit_Clear", this, "catalog_clear");
    m_pCatalogRescan = new Button(m_pControls, "CatalogRescan", "#GameUI_MatEdit_Rescan", this, "catalog_rescan");
    m_pCatalogBack = new Button(m_pControls, "CatalogBack", "#GameUI_MatEdit_Back", this, "catalog_back");

    m_CatalogScanned = false;
    Panel* const scene_controls[] = {
        m_pStandInTarget,
        m_pStandInSequence,
        m_pStandInFrame,
        m_pStandInPlaying,
        m_pStandInPlace,
        m_pPlaybackRate,
        m_pMirror,
        m_pControls->FindChildByName("PlaybackRateLabel"),
        m_pControls->FindChildByName("StandInTargetLabel"),
        m_pControls->FindChildByName("StandInSequenceLabel"),
        m_pControls->FindChildByName("StandInFrameLabel")
    };

    for (Panel* control : scene_controls)
    {
        control->SetParent(m_pSceneControls);
    }

    ShowPage(Page::Parameters);

    // After the children exist: sizing the frame invalidates the layout, and a layout pass
    // reaching a half-built panel would be reading pointers that are not set yet.
    SetSize(960, 640);
}

void CMaterialEditDialog::Activate()
{
    if (m_Closing)
    {
        return;
    }

    BaseClass::Activate();
    m_pPreview->SetVisible(true);

    int wide = 0;
    int tall = 0;
    m_Surface.GetScreenSize(wide, tall);
    SetBounds(0, 0, wide, tall);
    input()->SetAppModalSurface(GetVPanel());
    m_pLighting->Synchronize();
    m_pCompare->SilentSetSelected(m_State.is_comparing());
    ReloadModels();
    ReloadStandIn();
}

void CMaterialEditDialog::OnClose()
{
    if (m_Closing)
    {
        return;
    }

    m_Closing = true;
    m_Transitions.Reset();
    m_pPreview->CancelInteraction();
    m_pPreview->SetVisible(false);
    m_pCatalogList->ClearThumbnails();

    if (m_hConfirmClose.Get() != nullptr)
    {
        m_hConfirmClose->DisableFadeEffect();
        m_hConfirmClose->Close();
        m_hConfirmClose = nullptr;
    }

    if (m_hColorPicker.Get() != nullptr)
    {
        m_hColorPicker->Close();
        m_hColorPicker = nullptr;
    }

    if (m_hSaveError.Get() != nullptr)
    {
        m_hSaveError->Close();
        m_hSaveError = nullptr;
    }

    m_Editor.End();

    m_State.End();
    m_PreviewState.Reset();

    m_Preview.CloseAuthoring();

    m_Host.OnMaterialEditDialogClosed(this);
    BaseClass::OnClose();
}

void CMaterialEditDialog::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);
    int controls_wide = std::clamp(wide / 3, 360, 460);
    int scene_wide = wide - controls_wide;
    bool compact = scene_wide >= 540;
    int scene_controls_tall = (compact ? 150 : 204) + kSceneControlsPadding;
    m_pControls->SetBounds(scene_wide, 24, controls_wide, tall - 24);
    m_pPreview->SetBounds(0, 24, scene_wide, std::max(tall - 24 - scene_controls_tall - kSceneControlsGap, 32));
    m_pSceneControls->SetBounds(0, tall - scene_controls_tall, scene_wide, scene_controls_tall);
    const auto label = [this](const char* name, int x, int y, int width) {
        m_pSceneControls->FindChildByName(name)->SetBounds(x, kSceneControlsPadding + y, width, 20);
    };
    label("StandInTargetLabel", 8, 0, 76);
    m_pStandInTarget->SetBounds(88, kSceneControlsPadding, 154, 22);
    m_pMirror->SetBounds(compact ? 252 : 8, kSceneControlsPadding + (compact ? 0 : 28), scene_wide - (compact ? 260 : 16), 20);
    int animation_y = compact ? 30 : 54;
    label("StandInSequenceLabel", 8, animation_y, 76);
    m_pStandInSequence->SetBounds(88, kSceneControlsPadding + animation_y, scene_wide - (compact ? 304 : 96), 22);
    label("StandInFrameLabel", 8, animation_y + 30, 76);
    m_pStandInFrame->SetBounds(88, kSceneControlsPadding + animation_y + 28, scene_wide - 96, 26);
    label("PlaybackRateLabel", compact ? scene_wide - 202 : 8, compact ? animation_y : animation_y + 60, compact ? 76 : 104);
    m_pPlaybackRate->SetBounds(
        compact ? scene_wide - 112 : 120,
        kSceneControlsPadding + (compact ? animation_y : animation_y + 60),
        compact ? 104 : scene_wide - 128,
        22
    );
    int play_y = kSceneControlsPadding + (compact ? 90 : 144);
    m_pStandInPlaying->SetBounds(8, play_y, scene_wide - 16, 20);
    int view_y = kSceneControlsPadding + (compact ? 116 : 170);
    m_pStandInPlace->SetBounds(8, view_y, 150, 24);
    m_pResetView->SetBounds(8, view_y, 190, 24);
    wide = controls_wide;
    tall -= 24;

    int label_width = 90;
    int field_x = 10 + label_width;
    int field_width = wide - field_x - 10;

    int tab_wide = (wide - 24) / 2;

    m_pTabMaterial->SetBounds(10, kTabTop, tab_wide, kTabHeight);
    m_pTabLighting->SetBounds(14 + tab_wide, kTabTop, tab_wide, kTabHeight);

    for (int i = 0; i < 3; i++)
    {
        if (Panel* label = m_pControls->FindChildByName(kLabels[i]))
        {
            label->SetBounds(10, kPageTop + 2 + i * 26, label_width - 4, 18);
        }
    }

    m_pModel->SetBounds(field_x, kPageTop, field_width, 22);
    m_pTexture->SetBounds(field_x, kPageTop + 26, field_width - 80, 22);
    m_pTextureGroup->SetBounds(field_x + field_width - 76, kPageTop + 26, 76, 22);
    m_pTextureGroup->SetEnabled(!m_Model.empty());
    m_pTextureGroupCount->SetBounds(10, kPageTop, wide - 20, 40);
    m_pTextureList->SetBounds(10, kPageTop + 44, wide - 20, std::max(tall - kPageTop - 94, 40));
    m_pTextureAll->SetBounds(10, tall - 40, 100, 24);
    m_pTextureBack->SetBounds(wide - 170, tall - 40, 76, 24);
    m_pTextureApply->SetBounds(wide - 90, tall - 40, 76, 24);
    m_pProgram->SetBounds(field_x, kPageTop + 52, field_width, 22);

    m_pSource->SetBounds(10, kPageTop + 78, wide - 20, 18);
    m_pEmpty->SetBounds(16, kPageTop + 106, wide - 32, 60);

    int buttons_y = tall - 34;
    int siblings_y = buttons_y - 68;
    int compare_y = siblings_y - 22;
    int shown_y = compare_y - 28;
    int validation_y = shown_y - 34;

    m_pRows->SetBounds(10, kPageTop + 100, wide - 20, std::max(validation_y - 6 - (kPageTop + 100), 40));
    m_pValidation->SetBounds(10, validation_y, wide - 20, 30);

    int half = (wide - 20) / 2;

    if (Panel* label = m_pControls->FindChildByName("SkinLabel"))
    {
        label->SetBounds(10, shown_y + 2, 46, 18);
    }

    if (Panel* label = m_pControls->FindChildByName("BodyLabel"))
    {
        label->SetBounds(10 + half, shown_y + 2, 46, 18);
    }

    m_pSkin->SetBounds(58, shown_y, half - 54, 22);
    m_pBody->SetBounds(58 + half, shown_y, half - 54, 22);

    m_pCompare->SetBounds(10, compare_y, 220, 20);
    if (Panel* label = m_pControls->FindChildByName("RelatedTargetsLabel"))
    {
        label->SetBounds(10, siblings_y, wide - 20, 20);
    }

    for (size_t index = 0; index < m_SiblingTargets.size(); index++)
    {
        m_SiblingTargets[index]->SetBounds(10, siblings_y + 22 + static_cast<int>(index) * 20, wide - 20, 20);
    }

    m_pStub->SetBounds(10, buttons_y, kButtonWide, 24);
    m_pSave->SetBounds(10 + (kButtonWide + kButtonGap), buttons_y, kButtonWide, 24);

    m_pLighting->SetBounds(10, kPageTop, wide - 20, tall - kPageTop - 44);
    // Wherever the page it is shown over leaves room for it.
    switch (m_Page)
    {
        case Page::Lighting:
            m_pState->SetBounds(10, tall - 40, kButtonWide, 24);
            break;
        case Page::Browser:
        case Page::Catalog:
            m_pState->SetBounds(10, tall - 40, kButtonWide, 24);
            break;
        default:
            m_pState->SetBounds(10 + 2 * (kButtonWide + kButtonGap), buttons_y, kButtonWide, 24);
            break;
    }

    int list_width = wide - kBrowserPreviewSize - 30;

    m_pBrowserFolder->SetBounds(10, kPageTop, list_width, 22);
    m_pBrowserList->SetBounds(10, kPageTop + 28, list_width, std::max(tall - kPageTop - 78, 40));

    m_pBrowserPreview->SetBounds(wide - kBrowserPreviewSize - 10, kPageTop + 28, kBrowserPreviewSize, kBrowserPreviewSize);
    m_pBrowserPath->SetBounds(wide - kBrowserPreviewSize - 10, kPageTop + 34 + kBrowserPreviewSize, kBrowserPreviewSize, 40);

    m_pBrowserBack->SetBounds(wide - 250, tall - 40, 76, 24);
    m_pBrowserClear->SetBounds(wide - 170, tall - 40, 76, 24);
    m_pBrowserChoose->SetBounds(wide - 90, tall - 40, 76, 24);


    m_pCatalogFilter->SetBounds(10, kPageTop, wide - 20, 22);
    m_pCatalogCount->SetBounds(10, kPageTop + 26, wide - 20, 18);
    m_pCatalogList->SetBounds(10, kPageTop + 48, wide - 20, std::max(tall - kPageTop - 98, 40));

    m_pCatalogRescan->SetBounds(wide - 250, tall - 40, 76, 24);
    m_pCatalogClear->SetBounds(wide - 170, tall - 40, 76, 24);
    m_pCatalogBack->SetBounds(wide - 90, tall - 40, 76, 24);
}

void CMaterialEditDialog::ShowPage(Page page)
{
    m_Page = page;
    Panel* const parameters[] = {
        m_pModel,
        m_pTexture,
        m_pTextureGroup,
        m_pProgram,
        m_pSource,
        m_pRows,
        m_pValidation,
        m_pCompare,
        m_pSave,
        m_pStub,
        m_pSkin,
        m_pBody,
        m_pControls->FindChildByName("ModelLabel"),
        m_pControls->FindChildByName("TextureLabel"),
        m_pControls->FindChildByName("ProgramLabel"),
        m_pControls->FindChildByName("SkinLabel"),
        m_pControls->FindChildByName("BodyLabel")
    };
    Panel* const browser[] = {
        m_pBrowserFolder, m_pBrowserList, m_pBrowserPreview, m_pBrowserPath,
        m_pBrowserChoose, m_pBrowserClear, m_pBrowserBack
    };
    Panel* const catalog[] = {m_pCatalogFilter, m_pCatalogList, m_pCatalogCount, m_pCatalogClear, m_pCatalogRescan, m_pCatalogBack};
    Panel* const textures[] = {m_pTextureList, m_pTextureAll, m_pTextureApply, m_pTextureBack, m_pTextureGroupCount};
    for (Panel* panel : textures)
    {
        panel->SetVisible(page == Page::Textures);
    }

    for (Panel* panel : parameters)
    {
        panel->SetVisible(page == Page::Parameters);
    }

    for (Panel* panel : browser)
    {
        panel->SetVisible(page == Page::Browser);
    }

    for (Panel* panel : catalog)
    {
        panel->SetVisible(page == Page::Catalog);
    }

    m_pLighting->SetVisible(page == Page::Lighting);
    m_pEmpty->SetVisible(false);
    m_pState->SetVisible(page != Page::Textures);
    bool lighting = page == Page::Lighting || (page == Page::Catalog && m_CatalogReturnPage == Page::Lighting);
    m_pTabMaterial->SetSelected(!lighting);
    m_pTabLighting->SetSelected(lighting);

    switch (page)
    {
    case Page::Browser: SetTitle("#GameUI_MatEdit_PickTitle", true); break;
    case Page::Lighting:
        SetTitle("#GameUI_MatEdit_TabLighting", true);
        break;
    case Page::Textures:
        SetTitle("#GameUI_MatEdit_SelectTextures", true);
        break;
    case Page::Catalog:
        SetTitle("#GameUI_MatEdit_CatalogueTitle", true);
        break;
    default: SetTitle("#GameUI_MatEdit_Title", true); break;
    }

    UpdateTargetControls();

    for (size_t index = 0; index < m_SiblingTargets.size(); index++)
    {
        m_SiblingTargets[index]->SetVisible(page == Page::Parameters && index < m_RelatedModels.size());
    }

    m_pControls->FindChildByName("RelatedTargetsLabel")->SetVisible(page == Page::Parameters && !m_RelatedModels.empty());
    InvalidateLayout();
}

void CMaterialEditDialog::ShowTab(Page page)
{
    if (m_Page == Page::Browser)
    {
        CloseBrowser();
        if (page == Page::Parameters)
        {
            return;
        }
    }

    ShowPage(page);

    if (page == Page::Parameters)
    {
        ReloadModels();
    }
    else
    {
        m_pLighting->Synchronize();
    }
}

void CMaterialEditDialog::OnCloseFrameButtonPressed()
{
    if (m_Closing)
    {
        return;
    }

    if (m_Page == Page::Browser)
    {
        CloseBrowser();
        return;
    }

    if (m_Page == Page::Catalog)
    {
        ShowTab(m_CatalogReturnPage);
        return;
    }

    if (m_Page == Page::Textures)
    {
        ShowPage(Page::Parameters);
        return;
    }

    RequestChange({TransitionKind::Close, {}});
}

void CMaterialEditDialog::UpdateTargetControls()
{
    m_pMirror->SetVisible(m_PreviewState.is_weapon_view());
    m_pResetView->SetVisible(m_PreviewState.is_weapon_view());
    m_pStandInPlace->SetVisible(!m_PreviewState.is_weapon_view());
}

void CMaterialEditDialog::ReloadStandIn()
{
    m_pStandInTarget->SilentActivateItemByRow(m_PreviewState.is_weapon_view() ? 1 : 0);
    ReloadStandInSequences();
    m_pStandInFrame->SetValue(static_cast<int>(m_PreviewState.get_frame() * 1000.0f), false);
    m_pStandInPlaying->SilentSetSelected(m_PreviewState.is_playing());
    m_pMirror->SilentSetSelected(m_PreviewState.BuildModel().mirror);
    UpdateTargetControls();
}

void CMaterialEditDialog::ReloadStandInSequences()
{
    m_pStandInSequence->RemoveAll();
    int count = m_PreviewState.get_model_info().sequence_count;

    for (int index = 0; index < count; index++)
    {
        PreviewSequenceInfo sequence{};

        if (m_Preview.GetSequenceInfo(m_Model.c_str(), index, &sequence))
        {
            char name[112];
            V_snprintf(name, sizeof(name), "%d: %s", index, sequence.name);
            m_pStandInSequence->AddItem(name, nullptr);
        }
    }

    if (m_pStandInSequence->GetItemCount() > 0)
    {
        m_pStandInSequence->SilentActivateItemByRow(std::clamp(m_PreviewState.get_sequence(), 0, m_pStandInSequence->GetItemCount() - 1));
    }
}

void CMaterialEditDialog::StepStandIn()
{
    m_pStandInFrame->SetValue(static_cast<int>(m_PreviewState.get_frame() * 1000.0f), false);
}



void CMaterialEditDialog::PushStandIn()
{
    m_PreviewState.SetPlaying(m_pStandInPlaying->IsSelected());
    m_PreviewState.SetMirror(m_pMirror->IsSelected());
}

void CMaterialEditDialog::ShowBrowser(const char* param)
{
    if (!m_Editor.BeginParamPreview(param))
    {
        return;
    }

    m_BrowserParam = param;

    ScanBrowserFolders();
    ShowPage(Page::Browser);
}

void CMaterialEditDialog::CloseBrowser(bool accept)
{
    m_Editor.EndParamPreview(accept);

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->get_name() == m_BrowserParam)
        {
            row->Refresh();
        }
    }
    m_BrowserParam.clear();
    OnRowChanged();

    // Only the page goes back. Reloading the lists here restarts the editing session, which
    // drops the editor's layer and re-seeds it from the file: every unsaved change gone and
    // the program back to whatever the file says. "Refresh" is for rescanning.
    ShowPage(Page::Parameters);
}

void CMaterialEditDialog::ScanBrowserFolders()
{
    m_BrowserFolders = m_ResourceCatalog.ScanImageFolders();

    m_pBrowserFolder->RemoveAll();

    for (const std::string& folder : m_BrowserFolders)
    {
        m_pBrowserFolder->AddItem(folder.c_str(), nullptr);
    }

    if (m_BrowserFolders.empty())
    {
        // Said out loud rather than shown as an empty list: an installation missing the
        // shipped collection looks exactly like a browser that failed to read anything.
        m_pBrowserPath->SetText("#GameUI_MatEdit_NoCollection");
    }
    else
    {
        m_pBrowserFolder->ActivateItemByRow(0);
    }

    ReloadBrowserImages();
}

void CMaterialEditDialog::ReloadBrowserImages()
{
    m_pBrowserList->DeleteAllItems();
    m_BrowserImages.clear();
    m_BrowserHighlighted = -1;
    m_pBrowserPreview->SetImageFile("");

    int row = m_pBrowserFolder->GetActiveItem();

    if (row < 0 || row >= static_cast<int>(m_BrowserFolders.size()))
    {
        return;
    }

    m_BrowserImages = m_ResourceCatalog.ScanImages(m_BrowserFolders[row]);

    for (size_t i = 0; i < m_BrowserImages.size(); i++)
    {
        char command[32];
        Q_snprintf(command, sizeof(command), "highlight %d", static_cast<int>(i));

        BrowserRow* entry = new BrowserRow(m_pBrowserList->GetEmbedded(), this, m_Images, m_Surface, m_BrowserImages[i], command);

        entry->SetSize(1, kBrowserRowHeight);
        m_pBrowserList->AddItem(entry);
    }

    m_pBrowserPath->SetText("");
    InvalidateLayout();
}

void CMaterialEditDialog::HighlightBrowserImage(int index)
{
    if (index < 0 || index >= static_cast<int>(m_BrowserImages.size()))
    {
        return;
    }

    m_BrowserHighlighted = index;
    m_pBrowserPreview->SetImageFile(m_BrowserImages[index].c_str());
    m_pBrowserPath->SetText(m_BrowserImages[index].c_str());

    // Put on the model at once rather than on the way out: a mask is judged on the weapon,
    // not on a thumbnail, and the way back is the button that says so.
    SetTextureParam(m_BrowserParam.c_str(), m_BrowserImages[index].c_str());
}

void CMaterialEditDialog::ShowCatalog()
{
    if (m_Page != Page::Catalog)
    {
        m_CatalogReturnPage = m_Page == Page::Lighting ? Page::Lighting : Page::Parameters;
    }

    if (!m_CatalogScanned)
    {
        ScanCatalog();
    }

    ReloadCatalogRows();
    ShowPage(Page::Catalog);

    m_pCatalogFilter->RequestFocus();
}

void CMaterialEditDialog::ScanCatalog()
{
    m_CatalogScanned = true;
    m_pCatalogList->ClearThumbnails();
    m_Catalog.clear();

    m_Catalog = m_ResourceCatalog.ScanModels();

    m_pCatalogList->SetModels(m_Catalog);
}

void CMaterialEditDialog::ReloadCatalogRows()
{
    char filter[128]{};
    m_pCatalogFilter->GetText(filter, sizeof(filter));
    m_pCatalogList->SetFilter(filter);

    if (m_Catalog.empty())
    {
        m_pCatalogCount->SetText("#GameUI_MatEdit_CatalogueEmpty");
    }
    else
    {
        char count[64];
        V_snprintf(count, sizeof(count), "%d / %d", m_pCatalogList->get_match_count(), static_cast<int>(m_Catalog.size()));
        m_pCatalogCount->SetText(count);
    }

    InvalidateLayout();
}

void CMaterialEditDialog::ShowTextureGroup()
{
    if (m_Model.empty())
    {
        return;
    }
    m_TextureChecks.clear();
    m_TextureNames.clear();
    m_pTextureList->DeleteAllItems();
    for (int index = 0; index < m_Editor.get_texture_count(m_Model.c_str()); index++)
    {
        const std::string name = m_Editor.get_texture_name(m_Model.c_str(), index);
        CheckButton* check = new CheckButton(m_pTextureList->GetEmbedded(), "Texture", name.c_str());
        check->SetSize(1, 24);
        for (int selected = 0; selected < m_Editor.get_selection_count(); selected++)
        {
            if (V_stricmp(name.c_str(), m_Editor.get_selected_texture(selected)) == 0)
            {
                check->SilentSetSelected(true);
                break;
            }
        }
        m_pTextureList->AddItem(check);
        m_TextureChecks.push_back(check);
        m_TextureNames.push_back(name);
    }
    ShowPage(Page::Textures);
}

void CMaterialEditDialog::ApplyTextureGroup()
{
    std::vector<std::string> textures;

    for (size_t index = 0; index < m_TextureChecks.size(); index++)
    {
        if (m_TextureChecks[index]->IsSelected())
        {
            textures.push_back(m_TextureNames[index]);
        }
    }

    if (textures.empty() || textures.size() > ncl_material::kMaxEntries)
    {
        m_pTextureGroupCount->SetText("#GameUI_MatEdit_SelectTextures");
        return;
    }

    TransitionRequest request{
        TransitionKind::TextureGroup, {m_Model, textures.front(), std::move(textures)}
    };
    RequestChange(std::move(request));

    if (!m_Transitions.is_pending())
    {
        ShowPage(Page::Parameters);
    }
}

void CMaterialEditDialog::ReloadModels()
{
    m_StandInModels = m_ResourceCatalog.GetStandInModels(m_Model);

    m_pModel->SetModels(m_Model, m_StandInModels);
    bool has_model = !m_Model.empty();
    m_pEmpty->SetVisible(m_Page == Page::Parameters && !has_model);
    m_pEmpty->SetText("#GameUI_MatEdit_SelectModel");
    m_pRows->SetVisible(m_Page == Page::Parameters && has_model);
    ReloadTextures();
}

const char* CMaterialEditDialog::SelectedText(ComboBox* combo, std::string& storage)
{
    char text[256] = {};
    combo->GetText(text, sizeof(text));
    storage = text;

    return storage.c_str();
}

void CMaterialEditDialog::ReloadTextures()
{
    m_pTexture->RemoveAll();

    int active = 0;

    for (int index = 0; index < m_Editor.get_texture_count(m_Model.c_str()); index++)
    {
        const char* name = m_Editor.get_texture_name(m_Model.c_str(), index);
        m_pTexture->AddItem(name, nullptr);

        if (V_stricmp(name, m_Texture.c_str()) == 0)
        {
            active = index;
        }
    }

    if (m_pTexture->GetItemCount() > 0)
    {
        m_pTexture->SilentActivateItemByRow(active);
    }

    BeginEditing();
}

void CMaterialEditDialog::ReloadPreview()
{
    m_pSkin->RemoveAll();
    m_pBody->RemoveAll();

    int families = m_Editor.GetSkinFamilyCount(m_Model.c_str());

    for (int i = 0; i < families; i++)
    {
        char text[32];
        Q_snprintf(text, sizeof(text), "%d", i);
        m_pSkin->AddItem(text, nullptr);
    }

    int bodies = m_Editor.GetBodyCount(m_Model.c_str());

    for (int i = 0; i < bodies; i++)
    {
        // Named by the submodels the value selects rather than by the number itself: the
        // number says nothing, and the names are what the model's author wrote.
        char name[160];

        if (!m_Editor.GetBodyName(m_Model.c_str(), i, name, sizeof(name)) || *name == '\0')
        {
            Q_snprintf(name, sizeof(name), "%d", i);
        }

        m_pBody->AddItem(name, nullptr);
    }

    // A model with one of each has nothing to switch between, and a control that can only
    // be left alone is a control that has to be explained for no reason.
    m_pSkin->SetEnabled(families > 1);
    m_pBody->SetEnabled(bodies > 1);

    if (families > 0)
    {
        m_pSkin->SilentActivateItemByRow(std::clamp(m_PreviewState.BuildModel().skin, 0, families - 1));
    }

    if (bodies > 0)
    {
        m_pBody->SilentActivateItemByRow(std::clamp(m_PreviewState.BuildModel().body, 0, bodies - 1));
    }

    ApplyPreview();
    ReloadRelatedTargets();
}

void CMaterialEditDialog::ApplyPreview()
{
    m_PreviewState.SetPresentation(
        m_pSkin->GetActiveItem(), m_pBody->GetActiveItem(), m_PreviewState.get_pitch(), m_PreviewState.get_yaw(), m_PreviewState.get_scale()
    );
}

void CMaterialEditDialog::ReloadPrograms()
{
    m_pProgram->RemoveAll();

    // The material that does nothing beyond the model's own flags. Offered first because
    // going back to it is how an author undoes a material rather than emptying its values.
    const bool mixed = m_Editor.has_mixed_programs();
    if (mixed)
    {
        m_pProgram->AddItem("#GameUI_MatEdit_MixedPrograms", nullptr);
    }
    m_pProgram->AddItem("studio_legacy", nullptr);

    int active = 0;

    for (int i = 0; i < m_Editor.GetProgramCount(); i++)
    {
        const char* name = m_Editor.get_program_name(i);
        m_pProgram->AddItem(name, nullptr);

        if (m_Program == name)
        {
            active = i + 1 + (mixed ? 1 : 0);
        }
    }

    m_pProgram->SilentActivateItemByRow(active);
}

void CMaterialEditDialog::BeginEditing()
{
    SelectedText(m_pTexture, m_Texture);

    if (m_Editor.is_editing() && m_Model == m_Editor.get_edited_model() && V_stricmp(m_Texture.c_str(), m_Editor.get_edited_texture()) == 0)
    {
        return;
    }

    m_Editor.End();

    if (!m_Model.empty() && !m_Texture.empty() && m_Editor.Begin(m_Model.c_str(), m_Texture.c_str(), 0))
    {
        m_State.Begin(m_Editor.GetPresetStub());
    }
    else
    {
        m_State.End();
    }

    RefreshEditingControls();
}

void CMaterialEditDialog::RefreshEditingControls()
{
    m_Program = m_Editor.get_edited_program();
    ReloadPreview();
    ReloadPrograms();
    BuildRows();
    UpdateSource();
    UpdateValidation();
    MarkMaterialChanged();
}

void CMaterialEditDialog::BuildRows()
{
    m_Rows.clear();
    m_pRows->DeleteAllItems();

    if (!m_Editor.is_editing())
    {
        return;
    }

    const char* program = m_Editor.get_edited_program();

    for (int group = 0; group < m_Editor.GetGroupCount(program); group++)
    {
        const char* label = m_Editor.GetGroupLabel(program, group);

        // The group holding what the manifest placed nowhere carries no label, and a header
        // there would name a section the program never declared.
        if (*label != '\0')
        {
            GroupHeader* header = new GroupHeader(m_pRows->GetEmbedded(), m_Surface, label);
            header->SetSize(1, kGroupHeaderHeight);
            m_pRows->AddItem(header);
        }

        for (int i = 0; i < m_Editor.GetGroupParamCount(program, group); i++)
        {
            render::MaterialEditParam param;

            if (!m_Editor.GetParam(program, m_Editor.GetGroupParam(program, group, i), &param))
            {
                continue;
            }

            CMaterialParamRow* row = new CMaterialParamRow(m_pRows->GetEmbedded(), this, m_Editor, m_Images, m_Surface, param);
            row->SetSize(1, CMaterialParamRow::HeightFor(param));
            m_pRows->AddItem(row);
            m_Rows.push_back(row);

            row->Refresh();
        }
    }

    InvalidateLayout();
}

void CMaterialEditDialog::UpdateSource()
{
    // What the target resolves to with the editor's own layer taken away. Without this an
    // author edits a model the server describes, saves a file, reloads and finds the table
    // still winning, with nothing on screen having said so.
    if (m_Editor.get_selection_count() > 1)
    {
        wchar_t count[32]{};
        V_snwprintf(count, std::size(count), L"%d", m_Editor.get_selection_count());
        wchar_t text[256]{};
        g_pVGuiLocalize->ConstructString(text, sizeof(text), g_pVGuiLocalize->Find("#GameUI_MatEdit_GroupCount"), 1, count);
        m_pSource->SetText(text);
    }
    else
    {
        m_pSource->SetText(SourceToken(m_Editor.GetMaterialSource(m_Model.c_str(), m_Texture.c_str(), 0)));
    }
}

void CMaterialEditDialog::UpdateValidation()
{
    const char* problem = m_Editor.Validate();

    if (*problem == '\0')
    {
        m_pValidation->SetText("");
        m_pSave->SetEnabled(true);
        return;
    }

    // The localized explanation and the client's own message together: the first says what
    // happened, the second is the text an author carries to a search or to support.
    wchar_t* head = g_pVGuiLocalize->Find("#GameUI_MatEdit_Invalid");
    wchar_t detail[512];
    g_pVGuiLocalize->ConvertANSIToUnicode(problem, detail, sizeof(detail));

    wchar_t text[768];
    V_snwprintf(text, sizeof(text) / sizeof(wchar_t), L"%s%s", head != nullptr ? head : L"", detail);

    m_pValidation->SetText(text);
    m_pSave->SetEnabled(false);
}

void CMaterialEditDialog::OnRowChanged()
{
    if (m_Closing)
    {
        return;
    }

    UpdateValidation();
    MarkMaterialChanged();
}

void CMaterialEditDialog::SetTextureParam(const char* param, const char* path)
{
    if (m_Closing)
    {
        return;
    }

    m_Editor.SetTexture(param, path);

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->get_name() == param)
        {
            row->Refresh();
        }
    }

    UpdateValidation();
    MarkMaterialChanged();
}

void CMaterialEditDialog::OnColorPicked(const char* param, const float rgba[4])
{
    if (m_Closing)
    {
        return;
    }

    m_Editor.SetVec4(param, rgba);

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->get_name() == param)
        {
            row->Refresh();
        }
    }

    UpdateValidation();
    MarkMaterialChanged();
}

void CMaterialEditDialog::OnColorPickEnded(const char* param, const float original[4], bool accepted)
{
    if (m_Closing)
    {
        return;
    }

    m_Editor.EndParamPreview(accepted);

    m_hColorPicker = nullptr;
    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->get_name() == param)
        {
            row->Refresh();
        }
    }
    OnRowChanged();
}

bool CMaterialEditDialog::FindParamSchema(const char* name, render::MaterialEditParam* out)
{
    if (!m_Editor.is_editing())
    {
        return false;
    }

    const char* program = m_Editor.get_edited_program();

    for (int i = 0; i < m_Editor.GetParamCount(program); i++)
    {
        if (m_Editor.GetParam(program, i, out) && V_strcmp(out->name, name) == 0)
        {
            return true;
        }
    }

    return false;
}

void CMaterialEditDialog::OnTextChanged(Panel* panel)
{
    if (m_Closing)
    {
        return;
    }

    if (panel == m_pBrowserFolder)
    {
        ReloadBrowserImages();
    }
    else if (panel == m_pCatalogFilter)
    {
        ReloadCatalogRows();
    }
    else if (panel == m_pStandInTarget)
    {
        m_PreviewState.SetWeaponView(m_pStandInTarget->GetActiveItem() == 1);
        UpdateTargetControls();
    }
    else if (panel == m_pPlaybackRate)
    {
        m_PreviewState.SetPlaybackRate(kPlaybackRates[m_pPlaybackRate->GetActiveItem()]);
    }

    else if (panel == m_pStandInSequence)
    {
        int sequence = m_pStandInSequence->GetActiveItem();
        PreviewSequenceInfo info{};

        if (m_Preview.GetSequenceInfo(m_Model.c_str(), sequence, &info))
        {
            m_PreviewState.SetSequence(sequence, info.duration);
        }
    }
    else if (panel == m_pSkin || panel == m_pBody)
    {
        ApplyPreview();
    }

    else if (panel == m_pTexture)
    {
        std::string texture;
        SelectedText(m_pTexture, texture);
        RequestChange({TransitionKind::Texture, {m_Model, texture, {}}});
    }
    else if (panel == m_pProgram)
    {
        std::string chosen;
        SelectedText(m_pProgram, chosen);

        if (m_Editor.is_editing() && chosen != m_Editor.get_edited_program())
        {
            if (!m_Editor.SetEditedProgram(chosen.c_str()))
            {
                return;
            }
            m_Program = chosen;
            ReloadPrograms();
            BuildRows();
            UpdateValidation();
            MarkMaterialChanged();
        }
    }
}

void CMaterialEditDialog::OnCommand(const char* command)
{
    if (m_Closing)
    {
        return;
    }

    if (V_stricmp(command, "texture_group") == 0)
    {
        ShowTextureGroup();
        return;
    }
    if (V_stricmp(command, "texture_back") == 0)
    {
        ShowPage(Page::Parameters);
        return;
    }
    if (V_stricmp(command, "texture_all") == 0)
    {
        for (CheckButton* check : m_TextureChecks)
        {
            check->SilentSetSelected(true);
        }
        return;
    }
    if (V_stricmp(command, "texture_apply") == 0)
    {
        ApplyTextureGroup();
        return;
    }

    if (V_strncmp(command, "pick ", 5) == 0)
    {
        ShowBrowser(command + 5);
        return;
    }

    if (V_strncmp(command, "highlight ", 10) == 0)
    {
        HighlightBrowserImage(atoi(command + 10));
        return;
    }

    if (V_stricmp(command, "catalog") == 0)
    {
        ShowCatalog();
        return;
    }

    if (V_stricmp(command, "catalog_clear") == 0)
    {
        RequestChange({TransitionKind::Model, {}});
        return;
    }

    if (V_stricmp(command, "catalog_rescan") == 0)
    {
        ScanCatalog();
        ReloadCatalogRows();
        return;
    }

    if (V_stricmp(command, "catalog_back") == 0)
    {
        ShowTab(m_CatalogReturnPage);
        return;
    }

    // Choosing is already done: the image went onto the model when it was highlighted, so
    // this only leaves the collection.
    if (V_stricmp(command, "browse_choose") == 0)
    {
        CloseBrowser(true);
        return;
    }

    if (V_stricmp(command, "browse_clear") == 0)
    {
        SetTextureParam(m_BrowserParam.c_str(), "");
        CloseBrowser(true);
        return;
    }

    if (V_stricmp(command, "browse_back") == 0)
    {
        CloseBrowser();
        return;
    }

    if (V_strncmp(command, "color ", 6) == 0)
    {
        if (m_Editor.BeginParamPreview(command + 6))
        {
            float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            m_Editor.get_vec4(command + 6, rgba);

            // The program's own default is what "use defaults" means here; nothing else
            // knows what this parameter is supposed to be.
            render::MaterialEditParam schema;
            float defaults[4] = {1.0f, 1.0f, 1.0f, 1.0f};

            if (FindParamSchema(command + 6, &schema) && schema.has_default)
            {
                for (int i = 0; i < 4; i++)
                {
                    defaults[i] = schema.default_vec4[i];
                }
            }

            CColorPickerDialog* picker = new CColorPickerDialog(this, this, command + 6, rgba, defaults);
            m_hColorPicker = picker;
            picker->Activate();
        }

        return;
    }

    if (V_stricmp(command, "state") == 0)
    {
        m_Host.RequestMaterialStateDump();

        return;
    }

    if (V_stricmp(command, "tab_material") == 0)
    {
        ShowTab(Page::Parameters);
        return;
    }

    if (V_stricmp(command, "tab_lighting") == 0)
    {
        ShowTab(Page::Lighting);
        return;
    }

    if (V_stricmp(command, "Close") == 0)
    {
        OnCloseFrameButtonPressed();
        return;
    }

    if (V_stricmp(command, "reset_view") == 0)
    {
        m_PreviewState.ResetView();
        ReloadStandIn();
        return;
    }

    if (V_stricmp(command, "standin_place") == 0)
    {
        m_PreviewState.FrameModel();

        return;
    }

    if (V_stricmp(command, "refresh") == 0)
    {
        ReloadModels();
        return;
    }

    if (V_stricmp(command, "save") == 0)
    {
        RenderSaveResult(Save());
        return;
    }

    if (V_stricmp(command, "stub") == 0)
    {
        // To the console rather than to a file: this is text a plugin author pastes
        // into a table of their own, and where that table lives is their business.
        m_Host.PrintMaterialReport(m_Editor.GetPresetStub());
        m_Host.PrintMaterialReport("\n");

        return;
    }

    BaseClass::OnCommand(command);
}

std::vector<SaveFailure> CMaterialEditDialog::Save()
{
    if (m_Model.empty() || !m_Editor.is_editing())
    {
        return {{m_Model, SaveFailureReason::EditorUnavailable, {}}};
    }

    std::vector<std::string> targets{m_Model};

    for (size_t index = 0; index < m_RelatedModels.size(); index++)
    {
        if (m_SiblingTargets[index]->IsSelected())
        {
            targets.push_back(m_RelatedModels[index]);
        }
    }

    std::vector<std::string> selected_textures;
    const int selected_count = m_Editor.get_selection_count();

    for (int selected = 0; selected < selected_count; selected++)
    {
        selected_textures.emplace_back(m_Editor.get_selected_texture(selected));
    }

    const std::vector<SaveFailure> save_failures =
        SaveMaterialCopies(m_Editor, m_Preview, targets, selected_textures);

    if (save_failures.empty())
    {
        m_State.MarkMaterialSaved(m_Editor.GetPresetStub());
    }

    return save_failures;
}

void CMaterialEditDialog::RenderSaveResult(const std::vector<SaveFailure>& save_failures)
{
    if (save_failures.empty())
    {
        m_pValidation->SetText("#GameUI_MatEdit_Saved");
        return;
    }

    std::wstring failures;

    for (const SaveFailure& failure : save_failures)
    {
        wchar_t detail[1024]{};

        if (failure.reason == SaveFailureReason::SaveFailed)
        {
            g_pVGuiLocalize->ConvertANSIToUnicode(failure.error.c_str(), detail, sizeof(detail));
        }
        else
        {
            const wchar_t* text = g_pVGuiLocalize->Find(SaveFailureToken(failure.reason));

            if (text != nullptr)
            {
                V_wcsncpy(detail, text, sizeof(detail));
            }
        }

        wchar_t path[256]{};
        g_pVGuiLocalize->ConvertANSIToUnicode(failure.model.c_str(), path, sizeof(path));
        failures += std::wstring(path) + L": " + detail + L"\n";
    }

    m_pValidation->SetText("#GameUI_MatEdit_SaveFailed");
    const wchar_t* title = g_pVGuiLocalize->Find("#GameUI_MatEdit_SaveFailedTitle");
    MessageBox* box = new MessageBox(title != nullptr ? title : L"", failures.c_str(), this);
    m_hSaveError = box;
    box->DoModal();
}

void CMaterialEditDialog::OnThink()
{
    if (m_Closing)
    {
        return;
    }

    if (m_Page == Page::Catalog)
    {
        m_pCatalogList->ProcessOneThumbnail();
    }

    BaseClass::OnThink();
    m_PreviewState.Advance(system()->GetCurrentTime());

    if (m_Page == Page::Lighting && m_PreviewState.is_sun_animating())
    {
        m_pLighting->UpdateAnimatedAngles();
    }

    StepStandIn();

    if (IsVisible() && input()->GetAppModalSurface() == 0 && !GameConsole().IsConsoleVisible())
    {
        input()->SetAppModalSurface(GetVPanel());
    }
}

void CMaterialEditDialog::OnSliderMoved(Panel* panel)
{
    if (m_Closing)
    {
        return;
    }

    if (panel == m_pStandInFrame)
    {
        m_PreviewState.SetFrame(m_pStandInFrame->GetValue() / 1000.0f);
    }
}

void CMaterialEditDialog::OnCheckButtonChecked(Panel* panel)
{
    if (m_Closing)
    {
        return;
    }

    if (panel == m_pCompare)
    {
        m_State.SetComparison(m_pCompare->IsSelected());
        m_pPreview->SetComparison(m_State.is_comparing());
    }
    else
    {
        PushStandIn();
    }
}


void CMaterialEditDialog::ForceClose()
{
    DisableFadeEffect();
    Close();
}

void CMaterialEditDialog::MarkMaterialChanged()
{
    if (m_Editor.is_editing())
    {
        m_State.MarkMaterialChanged(m_Editor.GetPresetStub());
    }
}

void CMaterialEditDialog::RequestChange(TransitionRequest request)
{
    MarkMaterialChanged();
    RestoreSelections();

    DocumentSelection current{m_Model, m_Texture, {}};

    for (int index = 0; index < m_Editor.get_selection_count(); index++)
    {
        current.textures.emplace_back(m_Editor.get_selected_texture(index));
    }

    HandleTransitionResult(m_Transitions.Request(std::move(request), current, m_State.is_material_dirty()));
}

void CMaterialEditDialog::HandleTransitionResult(const TransitionResult& result)
{
    if (result.decision == TransitionDecision::Confirm)
    {
        m_pPreview->CancelInteraction();
        CMaterialChangeQuery* box = new CMaterialChangeQuery(this, result.token);
        m_hConfirmClose = box;
        box->DoModal();
    }
    else if (result.decision == TransitionDecision::Retain)
    {
        RestoreSelections();
    }
    else if (result.decision == TransitionDecision::Apply)
    {
        ApplyTransition(*result.request);
    }
}

void CMaterialEditDialog::OnTransitionResolved(KeyValues* params)
{
    if (m_Closing)
    {
        return;
    }

    const int choice = params->GetInt("choice", -1);

    if (choice < static_cast<int>(TransitionChoice::Save) ||
        choice > static_cast<int>(TransitionChoice::Cancel))
    {
        return;
    }

    const TransitionResult result =
        m_Transitions.Resolve(params->GetUint64("token"), static_cast<TransitionChoice>(choice), *this);

    if (result.decision == TransitionDecision::Ignored)
    {
        return;
    }

    m_hConfirmClose = nullptr;

    if (choice == static_cast<int>(TransitionChoice::Save))
    {
        RenderSaveResult(result.failures);
    }

    HandleTransitionResult(result);
}

void CMaterialEditDialog::ApplyTransition(const TransitionRequest& request)
{
    if (request.kind == TransitionKind::Close)
    {
        ForceClose();
        return;
    }

    if (request.kind == TransitionKind::Model)
    {
        const std::string& model = request.target.model;
        PreviewModelInfo info{};

        if (!model.empty())
        {
            const PreviewModelStatus status = m_Preview.PrepareModel(model.c_str(), &info);

            if (status != PreviewModelStatus::Ready)
            {
                const char* token = status == PreviewModelStatus::CapacityExceeded      ? "#GameUI_MatEdit_ModelCapacity"
                                    : status == PreviewModelStatus::RendererUnavailable ? "#GameUI_MatEdit_PreviewUnavailable"
                                                                                        : "#GameUI_MatEdit_ModelLoadFailed";
                m_pValidation->SetText(token);
                m_pPreview->SetLoadError(token);
                RestoreSelections();
                return;
            }
        }

        m_Editor.End();

        m_Model = model;
        m_Texture.clear();
        m_PreviewState.SetModel(model.c_str(), info);
        m_pPreview->SetLoadError("");

        if (model.empty())
        {
            m_State.End();
            m_pCompare->SilentSetSelected(false);
            m_pPreview->SetComparison(false);
        }
        else
        {
            PreviewSequenceInfo sequence{};

            if (m_Preview.GetSequenceInfo(model.c_str(), 0, &sequence))
            {
                m_PreviewState.SetSequence(0, sequence.duration);
            }
        }

        ReloadModels();
        ReloadStandIn();
        return;
    }

    if (request.kind == TransitionKind::TextureGroup)
    {
        std::vector<const char*> names;

        for (const std::string& selected : request.target.textures)
        {
            names.push_back(selected.c_str());
        }

        if (m_Editor.BeginGroup(request.target.model.c_str(), names.data(), static_cast<int>(names.size()), 0))
        {
            m_Texture = m_Editor.get_selected_texture(0);
            RestoreSelections();
            m_State.Begin(m_Editor.GetPresetStub());
            RefreshEditingControls();
            ShowPage(Page::Parameters);
        }
        else
        {
            RestoreSelections();
        }

        return;
    }

    if (request.kind == TransitionKind::Texture)
    {
        m_Texture = request.target.texture;
        RestoreSelections();
        m_Editor.End();
        BeginEditing();
    }
}

void CMaterialEditDialog::RestoreSelections()
{
    m_pModel->SetModels(m_Model, m_StandInModels);

    for (int row = 0; row < m_pTexture->GetItemCount(); row++)
    {
        char text[256]{};
        m_pTexture->GetItemText(row, text, sizeof(text));

        if (V_stricmp(text, m_Texture.c_str()) == 0)
        {
            m_pTexture->SilentActivateItemByRow(row);
            break;
        }
    }
}

void CMaterialEditDialog::OnModelPicked(KeyValues* params)
{
    if (m_Closing)
    {
        return;
    }

    RequestChange({TransitionKind::Model, {params->GetString("model"), {}, {}}});
}

void CMaterialEditDialog::OnTexturePicked(KeyValues* params)
{
    if (m_Closing)
    {
        return;
    }

    if (m_Model == params->GetString("model"))
    {
        RequestChange({TransitionKind::Texture, {m_Model, params->GetString("texture"), {}}});
    }
}

void CMaterialEditDialog::ReloadRelatedTargets()
{
    m_RelatedModels.clear();

    if (!m_Model.empty())
    {
        m_RelatedModels = m_ResourceCatalog.GetRelatedModels(m_Model);
    }

    for (size_t index = 0; index < m_SiblingTargets.size(); index++)
    {
        bool present = index < m_RelatedModels.size();
        m_SiblingTargets[index]->SetText(present ? m_RelatedModels[index].c_str() : "");
        m_SiblingTargets[index]->SilentSetSelected(false);
        m_SiblingTargets[index]->SetVisible(m_Page == Page::Parameters && present);
    }
}

void CMaterialEditDialog::OnKeyCodePressed(KeyCode code)
{
    if (m_Closing)
    {
        return;
    }

    if (code == KEY_BACKQUOTE)
    {
        input()->ReleaseAppModalSurface();
        GameConsole().Activate();
        return;
    }

    BaseClass::OnKeyCodePressed(code);
}

void CMaterialEditDialog::OnKeyCodeTyped(KeyCode code)
{
    if (m_Closing)
    {
        return;
    }

    if (code == KEY_ESCAPE)
    {
        OnCloseFrameButtonPressed();
        return;
    }

    if (code != KEY_BACKQUOTE)
    {
        BaseClass::OnKeyCodeTyped(code);
    }
}

void CMaterialEditDialog::DumpState()
{
    PreviewCamera camera = m_PreviewState.BuildCamera();
    PreviewModel model = m_PreviewState.BuildModel();
    PreviewLight light = m_PreviewState.BuildLight();
    char report[2048];
    V_snprintf(
        report,
        sizeof(report),
        "material scene: model=%s, weapon_view=%d, mirror=%d, skin=%d, body=%d, scale=%.3f\n"
        "animation: sequence=%d, frame=%.4f, playing=%d, dirty=%d, comparison=%d\n"
        "camera: origin=(%.3f %.3f %.3f), angles=(%.3f %.3f %.3f), fov=%.3f\n"
        "light: direction=(%.4f %.4f %.4f), color=(%.4f %.4f %.4f), ambient=%.3f, directional=%.3f\n",
        model.path,
        model.camera_space,
        model.mirror,
        model.skin,
        model.body,
        model.scale,
        model.sequence,
        model.frame,
        m_PreviewState.is_playing(),
        m_State.is_material_dirty(),
        m_State.is_comparing(),
        camera.origin[0],
        camera.origin[1],
        camera.origin[2],
        camera.angles[0],
        camera.angles[1],
        camera.angles[2],
        camera.fov,
        light.direction[0],
        light.direction[1],
        light.direction[2],
        light.color[0],
        light.color[1],
        light.color[2],
        light.ambient,
        light.diffuse
    );
    m_Host.PrintMaterialReport(report);
}

} // namespace material_editor

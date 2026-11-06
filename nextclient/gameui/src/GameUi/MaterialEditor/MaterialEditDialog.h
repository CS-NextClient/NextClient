#pragma once

#include <array>
#include <string>
#include <memory>
#include <vector>

#include <vgui_controls/Frame.h>
#include <vgui_controls/PHandle.h>

#include <next_engine_mini/MaterialEditInterface.h>

#include "GameUi/ColorPickerDialog.h"
#include "GameUi/MaterialEditor/MaterialEditDialogState.h"
#include "GameUi/MaterialEditor/MaterialPreviewState.h"
#include "GameUi/MaterialEditor/MaterialResourceCatalog.h"
#include "GameUi/MaterialEditor/MaterialTransitionController.h"

class CGameUI;
class ScenePreviewInterface;
class ImageFileCache;
class CImageFilePanel;
class CPanelListPanel;

namespace vgui2
{
    class ISurface;
    class Button;
    class CheckButton;
    class ComboBox;
    class Label;
    class QueryBox;
    class Slider;
    class TextEntry;
} // namespace vgui2

namespace material_editor
{
class CMaterialParamRow;
class CMaterialPreviewPanel;
class CMaterialLightPanel;
class CMaterialModelPicker;
class CMaterialModelList;

class CMaterialEditDialog : public vgui2::Frame, public CColorPickerDialog::IListener, private TransitionSaveService
{
    DECLARE_CLASS_SIMPLE(CMaterialEditDialog, vgui2::Frame);

private:
    enum class Page
    {
        Parameters,
        Browser,
        Lighting,
        Catalog,
        Textures
    };

    CGameUI& m_Host;
    render::MaterialEditInterface& m_Editor;
    ScenePreviewInterface& m_Preview;
    vgui2::ISurface& m_Surface;
    std::shared_ptr<ImageFileCache> m_Images;
    FileSystemResourceSource m_ResourceSource;
    ResourceCatalog m_ResourceCatalog;
    bool m_Closing{};
    MaterialPreviewState m_PreviewState;
    MaterialEditDialogState m_State;
    TransitionController m_Transitions;
    vgui2::Panel* m_pControls{};
    vgui2::Panel* m_pSceneControls{};
    CMaterialLightPanel* m_pLighting{};
    CMaterialPreviewPanel* m_pPreview{};
    vgui2::Button* m_pTabMaterial{};
    vgui2::Button* m_pTabLighting{};
    vgui2::DHANDLE<vgui2::QueryBox> m_hConfirmClose;
    vgui2::DHANDLE<CColorPickerDialog> m_hColorPicker;
    vgui2::DHANDLE<vgui2::Frame> m_hSaveError;
    CMaterialModelPicker* m_pModel{};
    vgui2::ComboBox* m_pTexture{};
    vgui2::Button* m_pTextureGroup{};
    CPanelListPanel* m_pTextureList{};
    vgui2::Button* m_pTextureAll{};
    vgui2::Button* m_pTextureApply{};
    vgui2::Button* m_pTextureBack{};
    vgui2::Label* m_pTextureGroupCount{};
    std::vector<vgui2::CheckButton*> m_TextureChecks;
    std::vector<std::string> m_TextureNames;
    vgui2::ComboBox* m_pProgram{};
    vgui2::Label* m_pSource{};
    vgui2::Label* m_pValidation{};
    CPanelListPanel* m_pRows{};
    std::vector<CMaterialParamRow*> m_Rows;
    vgui2::ComboBox* m_pSkin{};
    vgui2::ComboBox* m_pBody{};
    vgui2::CheckButton* m_pCompare{};
    std::array<vgui2::CheckButton*, 2> m_SiblingTargets{};
    std::vector<std::string> m_RelatedModels;

    vgui2::Button* m_pSave{};
    vgui2::Button* m_pStub{};

    vgui2::Label* m_pEmpty{};
    vgui2::ComboBox* m_pBrowserFolder{};
    CPanelListPanel* m_pBrowserList{};
    CImageFilePanel* m_pBrowserPreview{};
    vgui2::Label* m_pBrowserPath{};
    vgui2::Button* m_pBrowserChoose{};
    vgui2::Button* m_pBrowserClear{};
    vgui2::Button* m_pBrowserBack{};
    vgui2::ComboBox* m_pStandInTarget{};
    vgui2::CheckButton* m_pMirror{};
    vgui2::Button* m_pResetView{};
    vgui2::ComboBox* m_pStandInSequence{};
    vgui2::Slider* m_pStandInFrame{};
    vgui2::ComboBox* m_pPlaybackRate{};
    vgui2::CheckButton* m_pStandInPlaying{};
    vgui2::Button* m_pStandInPlace{};
    vgui2::Button* m_pState{};
    std::vector<std::string> m_StandInModels;
    std::string m_BrowserParam;
    std::vector<std::string> m_BrowserFolders;
    std::vector<std::string> m_BrowserImages;
    int m_BrowserHighlighted{};
    vgui2::TextEntry* m_pCatalogFilter{};
    CMaterialModelList* m_pCatalogList{};
    vgui2::Label* m_pCatalogCount{};
    vgui2::Button* m_pCatalogClear{};
    vgui2::Button* m_pCatalogRescan{};
    vgui2::Button* m_pCatalogBack{};
    std::vector<std::string> m_Catalog;
    bool m_CatalogScanned{};
    Page m_Page{};
    Page m_CatalogReturnPage{};
    std::string m_Model;
    std::string m_Texture;
    std::string m_Program;

public:
    CMaterialEditDialog(
        vgui2::Panel* parent,
        CGameUI& host,
        render::MaterialEditInterface& editor,
        ScenePreviewInterface& preview,
        IFileSystem& filesystem,
        vgui2::ISurface& surface,
        std::shared_ptr<ImageFileCache> images
    );

    bool is_closing() const
    {
        return m_Closing;
    }

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void OnThink() override;
    void PerformLayout() override;
    void OnCloseFrameButtonPressed() override;
    void OnKeyCodePressed(vgui2::KeyCode code) override;
    void OnKeyCodeTyped(vgui2::KeyCode code) override;
    void ForceClose();
    void DumpState();
    void OnRowChanged();
    void SetTextureParam(const char* param, const char* path);
    void OnColorPicked(const char* param, const float rgba[4]) override;
    void OnColorPickEnded(const char* param, const float original[4], bool accepted) override;

private:
    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
    MESSAGE_FUNC_PARAMS(OnModelPicked, "ModelPicked", params);
    MESSAGE_FUNC_PARAMS(OnTexturePicked, "TexturePicked", params);
    MESSAGE_FUNC_PARAMS(OnTransitionResolved, "MaterialTransitionResolved", params);
    MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR(OnSliderMoved, "SliderMoved", panel);

    void ShowPage(Page page);
    void ShowTab(Page page);

    void ReloadStandIn();
    void ReloadStandInSequences();
    void StepStandIn();
    void PushStandIn();
    void UpdateTargetControls();
    void ReloadModels();
    void ReloadTextures();
    void ReloadPreview();
    void ApplyPreview();
    void ReloadPrograms();
    void BeginEditing();
    void RefreshEditingControls();
    void ShowTextureGroup();
    void ApplyTextureGroup();
    void BuildRows();
    void UpdateSource();
    void UpdateValidation();
    std::vector<SaveFailure> Save() override;
    void RenderSaveResult(const std::vector<SaveFailure>& failures);
    void ShowBrowser(const char* param);
    void CloseBrowser(bool accept = false);
    void ScanBrowserFolders();
    void ReloadBrowserImages();
    void HighlightBrowserImage(int index);
    void ShowCatalog();
    void ScanCatalog();
    void ReloadCatalogRows();

    void RequestChange(TransitionRequest request);
    void HandleTransitionResult(const TransitionResult& result);
    void ApplyTransition(const TransitionRequest& request);
    void RestoreSelections();
    void MarkMaterialChanged();
    void ReloadRelatedTargets();
    bool FindParamSchema(const char* name, render::MaterialEditParam* out);
    const char* SelectedText(vgui2::ComboBox* combo, std::string& storage);

};

} // namespace material_editor

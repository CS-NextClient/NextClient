#pragma once

#include <string>
#include <vector>

#include <vgui_controls/PHandle.h>

#include <next_engine_mini/MaterialEditInterface.h>

#include "ColorPickerDialog.h"
#include "Controls/DragValueLabel.h"
#include "Controls/EditorPanel.h"

class CImageFilePanel;
class CPanelListPanel;
class CMaterialParamRow;

namespace vgui2
{
    class Button;
    class CheckButton;
    class ComboBox;
    class Label;
    class Slider;
} // namespace vgui2

// Edits the material of one texture of one model on the model in front of the author, and
// writes it beside the model when it is right.
//
// Nothing here names a parameter. The dialog asks the engine's editing interface for the
// program's schema and builds a row per declared parameter, which is what lets it work for
// a program it has never heard of.
class CMaterialEditDialog :
    public CEditorPanel,
    public CColorPickerDialog::IListener,
    public CDragValueLabel::IListener
{
    DECLARE_CLASS_SIMPLE(CMaterialEditDialog, CEditorPanel);

public:
    explicit CMaterialEditDialog(vgui2::Panel* parent);

    void Activate() override;
    void OnClose() override;
    void OnCommand(const char* command) override;
    void OnThink() override;
    void PerformLayout() override;

    MESSAGE_FUNC_PTR(OnTextChanged, "TextChanged", panel);
    MESSAGE_FUNC_PTR(OnCheckButtonChecked, "CheckButtonChecked", panel);
    MESSAGE_FUNC_PTR(OnSliderMoved, "SliderMoved", panel);

    // A row changed a value: push it into the editor's layer, which is what the model in
    // front of the author is drawn from.
    void OnRowChanged();

    // The browser chose an image for a texture parameter.
    void SetTextureParam(const char* param, const char* path);

    void OnColorPicked(const char* param, const float rgba[4]) override;

    // The stand-in's yaw and pitch are turned the same way every other number in this panel
    // is: by dragging its caption.
    void OnDragValue(int index, int pixels, bool fine, bool coarse) override;

private:
    // What the panel is showing. One value rather than a flag per page: with a flag each
    // page only knows how to hide the page it was written against, and a page added later
    // is a page the others leave on screen underneath.
    enum class Page
    {
        Parameters,
        Browser,
        StandIn
    };

    void ShowPage(Page page);

    // The copy of a model put into the world to be judged from outside. A page of its own
    // rather than another row: it has as many controls as a small dialog, and the panel
    // already knows how to swap what it is showing.
    void ShowStandIn();
    void CloseStandIn();
    void ReloadStandIn();
    void ReloadStandInSequences();
    void StepStandIn();
    void PushStandIn();

    // Which of the two models on that page is being talked about.
    void UpdateTargetControls();

    void UpdateScaleCaption(float scale);
    void TakeOverViewmodel();

    // A model in the scene was clicked: it becomes the one being edited.
    void OnScenePicked(const char* model) override;

    void ReloadModels();
    void ReloadTextures();
    void ReloadPreview();
    void ApplyPreview();
    void ReloadPrograms();
    void BeginEditing();
    void BuildRows();
    void UpdateSource();
    void UpdateValidation();
    void SaveDocument();
    void StepSweep();

    // Flies the camera while a mouse button is held over the world: the mouse turns it and
    // the movement keys move it. Polled rather than handled: the panel holds the modal
    // surface, so vgui drops every click that lands outside it and there is nothing to
    // route. The keyboard is part of the same held gesture because the panel owns it the
    // rest of the time, and a W meant for a texture path must not be a step forward.
    // The shipped collection, shown in place of the parameter list rather than in a window
    // of its own. This panel already floats over the running game; a second floating window
    // on top of it is one more thing to place, to raise and to lose behind the first.
    void ShowBrowser(const char* param);
    void CloseBrowser();
    void ScanBrowserFolders();
    void ReloadBrowserImages();
    void HighlightBrowserImage(int index);

    bool FindParamSchema(const char* name, MaterialEditParam* out);

    const char* SelectedText(vgui2::ComboBox* combo, std::string& storage);

    vgui2::ComboBox* m_pModel;
    vgui2::ComboBox* m_pTexture;
    vgui2::ComboBox* m_pProgram;

    vgui2::Label* m_pSource;
    vgui2::Label* m_pValidation;

    CPanelListPanel* m_pRows;
    std::vector<CMaterialParamRow*> m_Rows;

    // What the model is shown as. Neither picks what the material applies to - that is the
    // texture name - but both decide which meshes are on screen and which texture each of
    // them resolves to, so an author can look at the rest without leaving the panel.
    vgui2::ComboBox* m_pSkin;
    vgui2::ComboBox* m_pBody;

    vgui2::CheckButton* m_pCompare;
    vgui2::CheckButton* m_pSiblings;
    vgui2::Button* m_pSweep;
    vgui2::Button* m_pSave;
    vgui2::Button* m_pStub;
    vgui2::Button* m_pRefresh;

    // Shown in place of the parameter list when there is nothing to edit. Models appear as
    // the renderer prepares them, which it does not do at all while the shader path is off,
    // and an empty list that does not say which of the two it is wastes an evening.
    vgui2::Label* m_pEmpty;
    vgui2::Button* m_pEnableShader;

    vgui2::ComboBox* m_pBrowserFolder;
    CPanelListPanel* m_pBrowserList;
    CImageFilePanel* m_pBrowserPreview;
    vgui2::Label* m_pBrowserPath;
    vgui2::Button* m_pBrowserChoose;
    vgui2::Button* m_pBrowserClear;
    vgui2::Button* m_pBrowserBack;

    vgui2::ComboBox* m_pStandInTarget;
    vgui2::CheckButton* m_pViewmodelShown;
    vgui2::CheckButton* m_pViewmodelOverride;

    vgui2::ComboBox* m_pStandInModel;
    vgui2::ComboBox* m_pStandInSequence;
    CDragValueLabel* m_pStandInYaw;
    CDragValueLabel* m_pStandInPitch;
    CDragValueLabel* m_pStandInScale;
    vgui2::Slider* m_pStandInFrame;
    vgui2::CheckButton* m_pStandInPlaying;
    vgui2::CheckButton* m_pStandInSpinning;
    vgui2::Button* m_pStandInPlace;
    vgui2::Button* m_pStandInBack;
    vgui2::Button* m_pStandIn;
    vgui2::Button* m_pState;

    MaterialTarget m_Target;

    std::vector<std::string> m_StandInModels;

    // What the animation list was built from, kept to notice when the client has answered
    // with a different model's animations.
    std::string m_StandInSequences;

    bool m_LoadingStandIn;

    std::string m_BrowserParam;

    // What the parameter held when the collection was opened. Everything looked at on the
    // way through is applied to the model as it is highlighted, so leaving without choosing
    // has to put this back.
    std::string m_BrowserOriginal;

    std::vector<std::string> m_BrowserFolders;
    std::vector<std::string> m_BrowserImages;
    int m_BrowserHighlighted;

    Page m_Page;

    std::string m_Model;
    std::string m_Texture;
    std::string m_Program;

    // Walks every instance parameter across its range while it runs. The server sets these
    // per entity, so a material that only looks right at the value currently in the editor
    // is a material the author has not actually checked.
    bool m_Sweeping;
    float m_SweepPhase;
};

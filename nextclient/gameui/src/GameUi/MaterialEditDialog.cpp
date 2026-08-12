#include "MaterialEditDialog.h"

#include "Controls/ImageFilePanel.h"
#include "GameUi.h"
#include "MaterialParamRow.h"
#include "PanelListPanel.h"

#include <algorithm>
#include <cmath>

#include <FileSystem.h>
// studio.h uses vec3_t without declaring it.
#include <tier0/basetypes.h>
#include <studio.h>
#include <vgui/ILocalize.h>
#include <vgui/IScheme.h>
#include <vgui/ISurfaceNext.h>
#include <vgui/MouseCode.h>
#include <vgui_controls/Button.h>
#include <vgui_controls/CheckButton.h>
#include <vgui_controls/ComboBox.h>
#include <vgui_controls/Label.h>
#include <vgui_controls/Slider.h>
#include <vgui_controls/TextEntry.h>

using namespace vgui2;

namespace
{
    constexpr int kGroupHeaderHeight = 22;

    // Where the shipped collection lives. A folder is part of a path a material references,
    // so this root and the folders under it are as permanent as the file names.
    constexpr const char* kCollectionRoot = "materials";

    // Models are looked for under this one root, which the filesystem resolves in every
    // search path it has: cstrike, cstrike_downloads and the private one engine_mini adds.
    constexpr const char* kCatalogueRoot = "models";

    // A downloaded pack is models/<pack>/<file>, and a few nest one deeper. Bounded rather
    // than exhaustive: this walks a thousand directories as it is.
    constexpr int kCatalogueDepth = 3;

    // The walk stops here, and the count on screen says so. An installation with more models
    // than this is not one an author picks from by scrolling anyway.
    constexpr size_t kMaxCatalogueModels = 16384;

    // How many rows are built at once. Each is a panel, and building thousands of them is
    // both slow and pointless: what narrows the list is the filter.
    constexpr int kMaxCatalogueRows = 200;

    constexpr int kCatalogueRowHeight = 20;

    // What a stand-in can be scaled between. Below the lower bound the model is a speck and
    // the drag cannot get back out of it; above the upper one the engine ignores the scale.
    constexpr float kMinimumScale = 0.05f;
    constexpr float kMaximumScale = 10.0f;

    constexpr int kThumbSize = 40;
    constexpr int kBrowserRowHeight = 44;
    constexpr int kBrowserPreviewSize = 150;

    bool IsImage(const char* name)
    {
        const char* dot = V_strrchr(name, '.');

        return dot != nullptr && (V_stricmp(dot, ".png") == 0 || V_stricmp(dot, ".tga") == 0);
    }

    // The filesystem walks every search path, so a directory present in more than one of
    // them comes back once per path. What matters to a material is the relative path, and
    // that is the same thing however many places it was found in.
    void AddUnique(std::vector<std::string>& out, const std::string& value)
    {
        for (const std::string& existing : out)
        {
            if (V_stricmp(existing.c_str(), value.c_str()) == 0)
            {
                return;
            }
        }

        out.push_back(value);
    }

    void CollectFiles(const std::string& folder, const char* pattern, std::vector<std::string>& out)
    {
        std::string wildcard = folder + "/" + pattern;

        // Initialised, and closed only when the search started: the filesystem is not
        // required to write the handle when the first call finds nothing.
        FileFindHandle_t handle = {};
        const char* name = g_pFullFileSystem->FindFirst(wildcard.c_str(), &handle);

        if (name == nullptr)
        {
            return;
        }

        while (name != nullptr)
        {
            if (!g_pFullFileSystem->FindIsDirectory(handle) && IsImage(name))
            {
                AddUnique(out, folder + "/" + name);
            }

            name = g_pFullFileSystem->FindNext(handle);
        }

        g_pFullFileSystem->FindClose(handle);
    }

    // Depth-limited: the collection is two levels deep by design and an unbounded walk over
    // whatever else a player installed under the root is not what this is for.
    void CollectFolders(const std::string& folder, int depth, std::vector<std::string>& out)
    {
        std::vector<std::string> images;
        CollectFiles(folder, "*.png", images);
        CollectFiles(folder, "*.tga", images);

        if (!images.empty())
        {
            AddUnique(out, folder);
        }

        if (depth <= 0)
        {
            return;
        }

        FileFindHandle_t handle = {};
        std::string wildcard = folder + "/*";
        const char* name = g_pFullFileSystem->FindFirst(wildcard.c_str(), &handle);

        if (name == nullptr)
        {
            return;
        }

        std::vector<std::string> children;

        while (name != nullptr)
        {
            if (g_pFullFileSystem->FindIsDirectory(handle) && V_strcmp(name, ".") != 0 && V_strcmp(name, "..") != 0)
            {
                AddUnique(children, name);
            }

            name = g_pFullFileSystem->FindNext(handle);
        }

        g_pFullFileSystem->FindClose(handle);

        // Collected before recursing: FindFirst/FindNext share one handle per walk and
        // starting a nested one inside the loop would step on it.
        for (const std::string& child : children)
        {
            CollectFolders(folder + "/" + child, depth - 1, out);
        }
    }

    // A browser entry: the whole row is one button carrying its own image, so a click
    // anywhere on it selects and these can be chosen by eye.
    //
    // The image is drawn by the button itself rather than by a panel laid over it. A second
    // panel is an overlap, an overlap is a question of painting order, and that question
    // has a wrong answer here: a button repaints its background when it takes focus, and a
    // clicked entry went blank while an unclicked one did not.
    class BrowserRow : public Button
    {
        DECLARE_CLASS_SIMPLE(BrowserRow, Button);

    public:
        BrowserRow(Panel* parent, Panel* owner, const std::string& path, const char* command) :
            BaseClass(parent, "BrowserRow", NameOf(path), owner, command)
        {
            SetContentAlignment(a_west);
            SetTextInset(kThumbSize + 6, 0);

            m_pImage = ImageFile_Get(path.c_str());
        }

        void Paint() override
        {
            BaseClass::Paint();

            ImageFile_Draw(m_pImage, 2, 2, kThumbSize);
        }

        // Panels here are painted when something has marked them dirty rather than once a
        // frame, and an image that never changes has nothing to mark it.
        void OnThink() override
        {
            BaseClass::OnThink();

            if (m_pImage != nullptr)
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

        const ImageFilePixels* m_pImage;
    };

    bool IsModelFile(const char* name)
    {
        const char* dot = V_strrchr(name, '.');

        return dot != nullptr && V_stricmp(dot, ".mdl") == 0;
    }

    // Not deduplicated as it goes, unlike the image collection: the same folder comes back
    // once per search path it exists in, and comparing every name against everything found so
    // far is quadratic over thousands of files. Sorted and uniqued once at the end instead.
    void CollectModels(const std::string& folder, int depth, std::vector<std::string>& out)
    {
        FileFindHandle_t handle = {};
        std::string wildcard = folder + "/*";
        const char* name = g_pFullFileSystem->FindFirst(wildcard.c_str(), &handle);

        if (name == nullptr)
        {
            return;
        }

        std::vector<std::string> children;

        while (name != nullptr)
        {
            if (g_pFullFileSystem->FindIsDirectory(handle))
            {
                if (V_strcmp(name, ".") != 0 && V_strcmp(name, "..") != 0)
                {
                    // These are deduplicated, because a folder present in two search paths
                    // would otherwise be walked twice over.
                    AddUnique(children, name);
                }
            }
            else if (IsModelFile(name) && out.size() < kMaxCatalogueModels)
            {
                out.push_back(folder + "/" + name);
            }

            name = g_pFullFileSystem->FindNext(handle);
        }

        g_pFullFileSystem->FindClose(handle);

        if (depth <= 0)
        {
            return;
        }

        // Collected before recursing: FindFirst/FindNext share one handle per walk and
        // starting a nested one inside the loop would step on it.
        for (const std::string& child : children)
        {
            CollectModels(folder + "/" + child, depth - 1, out);
        }
    }

    bool LessIgnoreCase(const std::string& left, const std::string& right)
    {
        return V_stricmp(left.c_str(), right.c_str()) < 0;
    }

    bool IsDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    // The file identifiers as studiomdl writes them, least significant byte first.
    constexpr int kStudioModelId = ('T' << 24) | ('S' << 16) | ('D' << 8) | 'I';
    constexpr int kSequenceGroupId = ('Q' << 24) | ('S' << 16) | ('D' << 8) | 'I';

    // Whether the file is one of the pieces the engine loads on a model's behalf rather than
    // a model. A sequence group says so with an identifier of its own; a texture file carries
    // the model identifier and is told apart by having no geometry at all.
    bool IsModelPart(const std::string& path)
    {
        FileHandle_t file = g_pFullFileSystem->Open(path.c_str(), "rb");

        if (file == nullptr)
        {
            return false;
        }

        studiohdr_t header = {};
        int read = g_pFullFileSystem->Read(&header, sizeof(header), file);

        g_pFullFileSystem->Close(file);

        if (read != static_cast<int>(sizeof(header)))
        {
            return false;
        }

        return header.id != kStudioModelId || header.numbodyparts <= 0;
    }

    // studiomdl puts a model's textures in <stem>T.mdl and every sequence group past the
    // first in <stem>NN.mdl, and the engine builds those names from the base name when it
    // loads the model. Neither is a model of its own: loading one draws nothing.
    //
    // The name is only what makes a file worth asking about, and the header is what answers:
    // "w_ak47" ends in two digits without being anybody's sequence group, and a downloaded
    // pack holding both tree.mdl and tree02.mdl has two trees in it, not one. Opening the few
    // files the name catches costs nothing; opening all three thousand would.
    bool IsPartOfAnotherModel(const std::vector<std::string>& sorted, const std::string& path)
    {
        if (path.size() < 5)
        {
            return false;
        }

        size_t stem = path.size() - 4;
        std::string base;

        if (path[stem - 1] == 'T' || path[stem - 1] == 't')
        {
            base = path.substr(0, stem - 1) + ".mdl";
        }
        else if (stem >= 2 && IsDigit(path[stem - 1]) && IsDigit(path[stem - 2]))
        {
            base = path.substr(0, stem - 2) + ".mdl";
        }
        else
        {
            return false;
        }

        return std::binary_search(sorted.begin(), sorted.end(), base, LessIgnoreCase) && IsModelPart(path);
    }

    bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle)
    {
        return needle.empty() || V_stristr(haystack.c_str(), needle.c_str()) != nullptr;
    }

    // One row of the model catalogue. A plain button rather than the collection's row: there
    // is no thumbnail to draw for a model, and what shows it is the world behind the panel.
    class CatalogueRow : public Button
    {
        DECLARE_CLASS_SIMPLE(CatalogueRow, Button);

    public:
        CatalogueRow(Panel* parent, Panel* owner, const char* text, const char* command) :
            BaseClass(parent, "CatalogueRow", text, owner, command)
        {
            SetContentAlignment(a_west);
            SetTextInset(4, 0);
        }
    };

    // One full pass of every instance parameter over its range, in seconds. Slow enough to
    // watch, short enough that an author does not walk away.
    constexpr float kSweepSeconds = 6.0f;

    const char* SourceToken(MaterialEditSource source)
    {
        switch (source)
        {
        case MaterialEditSource::Instance: return "#GameUI_MatEdit_SourceInstance";
        case MaterialEditSource::ModelDocument: return "#GameUI_MatEdit_SourceDocument";
        case MaterialEditSource::Implicit: return "#GameUI_MatEdit_SourceImplicit";
        default: return "#GameUI_MatEdit_SourceNone";
        }
    }

    // A section header inside the parameter list. Its own panel because the list stacks
    // panels and a header is not a row with a value.
    class GroupHeader : public Label
    {
        DECLARE_CLASS_SIMPLE(GroupHeader, Label);

    public:
        GroupHeader(Panel* parent, const char* text) :
            BaseClass(parent, "GroupHeader", text)
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
            surface()->GetTextSize(GetFont(), L"   ", wide, tall);

            SetTextInset(wide, 0);
        }
    };
}

CMaterialEditDialog::CMaterialEditDialog(Panel* parent) :
    BaseClass(parent, "CMaterialEditDialog")
{
    m_Sweeping = false;
    m_SweepPhase = 0.0f;

    SetTitle("#GameUI_MatEdit_Title", true);
    SetSizeable(false);
    SetDeleteSelfOnClose(true);

    m_pModel = new ComboBox(this, "Model", 16, false);
    m_pTexture = new ComboBox(this, "Texture", 16, false);
    m_pProgram = new ComboBox(this, "Program", 8, false);

    m_pModel->SetEditable(false);
    m_pTexture->SetEditable(false);
    m_pProgram->SetEditable(false);

    m_pModel->AddActionSignalTarget(this);
    m_pTexture->AddActionSignalTarget(this);
    m_pProgram->AddActionSignalTarget(this);

    new Label(this, "ModelLabel", "#GameUI_MatEdit_Model");
    new Label(this, "TextureLabel", "#GameUI_MatEdit_Texture");
    new Label(this, "ProgramLabel", "#GameUI_MatEdit_Program");

    new Label(this, "SkinLabel", "#GameUI_MatEdit_Skin");
    new Label(this, "BodyLabel", "#GameUI_MatEdit_Body");

    m_pSkin = new ComboBox(this, "Skin", 12, false);
    m_pBody = new ComboBox(this, "Body", 12, false);
    m_pSkin->SetEditable(false);
    m_pBody->SetEditable(false);
    m_pSkin->AddActionSignalTarget(this);
    m_pBody->AddActionSignalTarget(this);

    m_pSource = new Label(this, "Source", "");
    m_pValidation = new Label(this, "Validation", "");

    m_pRows = new CPanelListPanel(this, "Rows");

    // Suppresses every material at once, which is the only comparison that means anything:
    // "with" against "without" has to differ for a reason the author chose.
    m_pCompare = new CheckButton(this, "Compare", "#GameUI_MatEdit_Compare");
    m_pCompare->AddActionSignalTarget(this);

    m_pSiblings = new CheckButton(this, "Siblings", "#GameUI_MatEdit_Siblings");
    m_pSiblings->SetSelected(true);

    m_pSweep = new Button(this, "Sweep", "#GameUI_MatEdit_Sweep", this, "sweep");
    m_pSave = new Button(this, "Save", "#GameUI_MatEdit_Save", this, "save");
    m_pStub = new Button(this, "Stub", "#GameUI_MatEdit_Stub", this, "stub");
    m_pRefresh = new Button(this, "Refresh", "#GameUI_MatEdit_Refresh", this, "refresh");

    m_pEmpty = new Label(this, "Empty", "");
    m_pEmpty->SetWrap(true);
    m_pEnableShader = new Button(this, "EnableShader", "#GameUI_MatEdit_EnableShader", this, "enable_shader");

    m_pStandIn = new Button(this, "StandIn", "#GameUI_MatEdit_StandIn", this, "standin");
    m_pState = new Button(this, "State", "#GameUI_MatEdit_State", this, "state");

    m_LoadingStandIn = false;

    new Label(this, "StandInTargetLabel", "#GameUI_MatEdit_Target");

    m_pStandInTarget = new ComboBox(this, "StandInTargetBox", 4, false);
    m_pStandInTarget->SetEditable(false);
    m_pStandInTarget->AddItem("#GameUI_MatEdit_TargetStandIn", nullptr);
    m_pStandInTarget->AddItem("#GameUI_MatEdit_TargetViewmodel", nullptr);
    m_pStandInTarget->AddActionSignalTarget(this);

    m_Target = MaterialTarget::StandIn;

    m_pViewmodelShown = new CheckButton(this, "ViewmodelShown", "#GameUI_MatEdit_ViewmodelShown");
    m_pViewmodelOverride = new CheckButton(this, "ViewmodelOverride", "#GameUI_MatEdit_ViewmodelOverride");
    m_pViewmodelShown->AddActionSignalTarget(this);
    m_pViewmodelOverride->AddActionSignalTarget(this);

    new Label(this, "StandInModelLabel", "#GameUI_MatEdit_StandInModel");
    new Label(this, "StandInSequenceLabel", "#GameUI_MatEdit_StandInSequence");
    new Label(this, "StandInFrameLabel", "#GameUI_MatEdit_StandInFrame");

    m_pStandInModel = new ComboBox(this, "StandInModelBox", 8, false);
    m_pStandInSequence = new ComboBox(this, "StandInSequenceBox", 16, false);
    m_pStandInModel->SetEditable(false);
    m_pStandInSequence->SetEditable(false);
    m_pStandInModel->AddActionSignalTarget(this);
    m_pStandInSequence->AddActionSignalTarget(this);

    // The combo offers the model being edited and its siblings, which is the answer nine
    // times out of ten; this is the tenth, and it opens everything installed.
    m_pStandInBrowse = new Button(this, "StandInBrowse", "...", this, "catalogue");

    // Indices 0 and 1: the two angles an author turns it by. Roll is not offered - a weapon
    // lying on its side tells nobody anything about its finish. Index 2 is how large it is.
    m_pStandInYaw = new CDragValueLabel(this, "StandInYaw", "#GameUI_MatEdit_StandInYaw", this, 0);
    m_pStandInPitch = new CDragValueLabel(this, "StandInPitch", "#GameUI_MatEdit_StandInPitch", this, 1);
    m_pStandInScale = new CDragValueLabel(this, "StandInScale", "#GameUI_MatEdit_StandInScale", this, 2);

    m_pStandInFrame = new Slider(this, "StandInFrame");
    m_pStandInFrame->SetRange(0, 1000);
    m_pStandInFrame->AddActionSignalTarget(this);

    m_pStandInPlaying = new CheckButton(this, "StandInPlaying", "#GameUI_MatEdit_StandInPlaying");
    m_pStandInSpinning = new CheckButton(this, "StandInSpinning", "#GameUI_MatEdit_StandInSpinning");
    m_pStandInPlaying->AddActionSignalTarget(this);
    m_pStandInSpinning->AddActionSignalTarget(this);

    m_pStandInPlace = new Button(this, "StandInPlace", "#GameUI_MatEdit_StandInPlace", this, "standin_place");
    m_pStandInBack = new Button(this, "StandInBack", "#GameUI_MatEdit_Back", this, "standin_back");

    m_BrowserHighlighted = -1;

    m_pBrowserFolder = new ComboBox(this, "BrowserFolder", 12, false);
    m_pBrowserFolder->SetEditable(false);
    m_pBrowserFolder->AddActionSignalTarget(this);

    m_pBrowserList = new CPanelListPanel(this, "BrowserList");
    m_pBrowserPreview = new CImageFilePanel(this, "BrowserPreview");
    m_pBrowserPath = new Label(this, "BrowserPath", "");

    m_pBrowserChoose = new Button(this, "BrowserChoose", "#GameUI_MatEdit_Choose", this, "browse_choose");
    m_pBrowserClear = new Button(this, "BrowserClear", "#GameUI_MatEdit_Clear", this, "browse_clear");
    m_pBrowserBack = new Button(this, "BrowserBack", "#GameUI_MatEdit_Back", this, "browse_back");

    m_pCatalogueFilter = new TextEntry(this, "CatalogueFilter");
    m_pCatalogueFilter->AddActionSignalTarget(this);

    m_pCatalogueList = new CPanelListPanel(this, "CatalogueList");
    m_pCatalogueCount = new Label(this, "CatalogueCount", "");

    m_pCatalogueClear = new Button(this, "CatalogueClear", "#GameUI_MatEdit_Clear", this, "catalogue_clear");
    m_pCatalogueRescan = new Button(this, "CatalogueRescan", "#GameUI_MatEdit_Rescan", this, "catalogue_rescan");
    m_pCatalogueBack = new Button(this, "CatalogueBack", "#GameUI_MatEdit_Back", this, "catalogue_back");

    m_CatalogueScanned = false;

    ShowPage(Page::Parameters);

    // After the children exist: sizing the frame invalidates the layout, and a layout pass
    // reaching a half-built panel would be reading pointers that are not set yet.
    DockToScreen();
}

// Selected by name, through the list the panel already has: a model that is drawn but not
// prepared is not one this can edit, and the list is what says which is which.
void CMaterialEditDialog::OnScenePicked(const char* model)
{
    if (model == nullptr || *model == '\0')
    {
        return;
    }

    std::string picked = model;

    for (int i = 0; i < m_pModel->GetItemCount(); i++)
    {
        char text[256];
        m_pModel->GetItemText(i, text, sizeof(text));

        if (picked == text)
        {
            m_pModel->ActivateItemByRow(i);
            return;
        }
    }
}

void CMaterialEditDialog::Activate()
{
    BaseClass::Activate();

    ReloadModels();
}

void CMaterialEditDialog::OnClose()
{
    // The editor's layer is what the model in front of the author is drawn from, so it has
    // to go when the panel does; otherwise a closed panel keeps overriding the material and
    // the model never shows what its own file says.
    if (Editor() != nullptr)
    {
        // The stand-in goes with the panel. It is not something the game put in the world
        // and nothing else can take it out: the only control that clears it is on the page
        // that is closing.
        Editor()->SetStandInModel("");

        // The hands go back to the game's own for the same reason: closed with them hidden
        // or held on one frame, they would stay that way with nothing left to switch back.
        MaterialViewmodel viewmodel;
        Editor()->GetViewmodel(&viewmodel);
        viewmodel.shown = true;
        viewmodel.overriding = false;
        Editor()->SetViewmodel(&viewmodel);

        Editor()->End();
    }

    BaseClass::OnClose();
}

void CMaterialEditDialog::PerformLayout()
{
    BaseClass::PerformLayout();

    int wide = 0;
    int tall = 0;
    GetSize(wide, tall);

    int label_width = 90;
    int field_x = 10 + label_width;
    int field_width = wide - field_x - 10;

    const char* const kLabels[] = {"ModelLabel", "TextureLabel", "ProgramLabel"};

    for (int i = 0; i < 3; i++)
    {
        if (Panel* label = FindChildByName(kLabels[i]))
        {
            label->SetBounds(10, 34 + i * 26, label_width - 4, 18);
        }
    }

    constexpr int kRefreshWidth = 74;

    m_pModel->SetBounds(field_x, 32, field_width - kRefreshWidth - 4, 22);
    m_pRefresh->SetBounds(wide - 10 - kRefreshWidth, 32, kRefreshWidth, 22);
    m_pTexture->SetBounds(field_x, 58, field_width, 22);
    m_pProgram->SetBounds(field_x, 84, field_width, 22);

    m_pSource->SetBounds(10, 110, wide - 20, 18);
    m_pEmpty->SetBounds(16, 138, wide - 32, 60);
    m_pEnableShader->SetBounds(16, 206, 220, 24);

    // The bottom of the panel, stacked upwards: the four buttons on a row of their own, the
    // two switches above them, then what the model is shown as, then what the material has
    // to say for itself. The panel is docked and narrow, so a row of buttons beside a row of
    // switches does not fit and putting them there drew one over the other.
    constexpr int kButtonWide = 76;
    constexpr int kButtonGap = 4;

    int buttons_y = tall - 34;
    int siblings_y = buttons_y - 24;
    int compare_y = siblings_y - 22;
    int shown_y = compare_y - 28;
    int validation_y = shown_y - 34;

    m_pRows->SetBounds(10, 132, wide - 20, std::max(validation_y - 6 - 132, 40));
    m_pValidation->SetBounds(10, validation_y, wide - 20, 30);

    int half = (wide - 20) / 2;

    if (Panel* label = FindChildByName("SkinLabel"))
    {
        label->SetBounds(10, shown_y + 2, 46, 18);
    }

    if (Panel* label = FindChildByName("BodyLabel"))
    {
        label->SetBounds(10 + half, shown_y + 2, 46, 18);
    }

    m_pSkin->SetBounds(58, shown_y, half - 54, 22);
    m_pBody->SetBounds(58 + half, shown_y, half - 54, 22);

    m_pCompare->SetBounds(10, compare_y, 220, 20);
    m_pSiblings->SetBounds(10, siblings_y, 260, 20);

    m_pStandIn->SetBounds(10, buttons_y, kButtonWide, 24);
    m_pSweep->SetBounds(10 + (kButtonWide + kButtonGap), buttons_y, kButtonWide, 24);
    m_pStub->SetBounds(10 + 2 * (kButtonWide + kButtonGap), buttons_y, kButtonWide, 24);
    m_pSave->SetBounds(10 + 3 * (kButtonWide + kButtonGap), buttons_y, kButtonWide, 24);

    // The stand-in page, laid out over the same area the parameter list uses. What belongs
    // to one target only shares its rows with the other target's: exactly one of the two is
    // on screen, and stacking them instead would leave a hole in whichever is shown.
    int row = 40;

    if (Panel* label = FindChildByName("StandInTargetLabel"))
    {
        label->SetBounds(10, row + 2, 110, 18);
    }

    m_pStandInTarget->SetBounds(124, row, wide - 134, 22);
    row += 30;

    if (Panel* label = FindChildByName("StandInModelLabel"))
    {
        label->SetBounds(10, row + 2, 110, 18);
    }

    constexpr int kBrowseWidth = 30;

    m_pStandInModel->SetBounds(124, row, wide - 134 - kBrowseWidth - 4, 22);
    m_pStandInBrowse->SetBounds(wide - 10 - kBrowseWidth, row, kBrowseWidth, 22);
    row += 30;

    if (Panel* label = FindChildByName("StandInSequenceLabel"))
    {
        label->SetBounds(10, row + 2, 110, 18);
    }

    m_pStandInSequence->SetBounds(124, row, wide - 134, 22);
    row += 30;

    if (Panel* label = FindChildByName("StandInFrameLabel"))
    {
        label->SetBounds(10, row + 2, 110, 18);
    }

    m_pStandInFrame->SetBounds(124, row - 2, wide - 134, 26);
    row += 32;

    m_pStandInPlaying->SetBounds(10, row, 240, 20);
    row += 26;

    m_pStandInSpinning->SetBounds(10, row, 240, 20);
    m_pViewmodelShown->SetBounds(10, row, 240, 20);
    row += 26;

    m_pViewmodelOverride->SetBounds(10, row, 240, 20);

    m_pStandInYaw->SetBounds(10, row, 200, 20);
    row += 24;
    m_pStandInPitch->SetBounds(10, row, 200, 20);
    row += 24;
    m_pStandInScale->SetBounds(10, row, 200, 20);

    m_pStandInPlace->SetBounds(10, tall - 32, 150, 24);
    m_pStandInBack->SetBounds(wide - 90, tall - 32, 76, 24);

    // Wherever the page it is shown over leaves room for it.
    switch (m_Page)
    {
    case Page::StandIn: m_pState->SetBounds(170, tall - 32, kButtonWide, 24); break;
    case Page::Browser:
    case Page::Catalogue: m_pState->SetBounds(10, tall - 40, kButtonWide, 24); break;
    default: m_pState->SetBounds(10, buttons_y - 26, kButtonWide, 22); break;
    }

    int list_width = wide - kBrowserPreviewSize - 30;

    m_pBrowserFolder->SetBounds(10, 32, list_width, 22);
    m_pBrowserList->SetBounds(10, 60, list_width, tall - 110);

    m_pBrowserPreview->SetBounds(wide - kBrowserPreviewSize - 10, 60, kBrowserPreviewSize, kBrowserPreviewSize);
    m_pBrowserPath->SetBounds(wide - kBrowserPreviewSize - 10, 60 + kBrowserPreviewSize + 6, kBrowserPreviewSize, 40);

    m_pBrowserBack->SetBounds(wide - 250, tall - 40, 76, 24);
    m_pBrowserClear->SetBounds(wide - 170, tall - 40, 76, 24);
    m_pBrowserChoose->SetBounds(wide - 90, tall - 40, 76, 24);

    // The catalogue: what narrows it on top, what it found under that, and the rest of the
    // panel is the list. Nothing is previewed here beside it - the model goes into the world
    // as it is picked, and the world is already on screen.
    m_pCatalogueFilter->SetBounds(10, 32, wide - 20, 22);
    m_pCatalogueCount->SetBounds(10, 58, wide - 20, 18);
    m_pCatalogueList->SetBounds(10, 80, wide - 20, std::max(tall - 130, 40));

    m_pCatalogueRescan->SetBounds(wide - 250, tall - 40, 76, 24);
    m_pCatalogueClear->SetBounds(wide - 170, tall - 40, 76, 24);
    m_pCatalogueBack->SetBounds(wide - 90, tall - 40, 76, 24);
}

void CMaterialEditDialog::ShowPage(Page page)
{
    m_Page = page;

    // Every control is named on exactly one of these lists, and the lists are walked in
    // full every time. A page shown by hiding "the other one" stops being true the moment a
    // third page exists, and what an author sees then is both at once.
    Panel* const parameters[] = {
        m_pModel, m_pTexture, m_pProgram, m_pSource, m_pRows, m_pValidation,
        m_pCompare, m_pSiblings, m_pSweep, m_pSave, m_pStub, m_pRefresh, m_pSkin, m_pBody,
        m_pStandIn,
        FindChildByName("ModelLabel"), FindChildByName("TextureLabel"), FindChildByName("ProgramLabel"),
        FindChildByName("SkinLabel"), FindChildByName("BodyLabel")
    };

    Panel* const browser[] = {
        m_pBrowserFolder, m_pBrowserList, m_pBrowserPreview, m_pBrowserPath,
        m_pBrowserChoose, m_pBrowserClear, m_pBrowserBack
    };

    Panel* const stand_in[] = {
        m_pStandInTarget, m_pStandInModel, m_pStandInBrowse, m_pStandInSequence, m_pStandInYaw,
        m_pStandInPitch, m_pStandInScale, m_pStandInFrame, m_pStandInPlaying, m_pStandInSpinning,
        m_pStandInPlace, m_pStandInBack,
        m_pViewmodelShown, m_pViewmodelOverride,
        FindChildByName("StandInTargetLabel"), FindChildByName("StandInModelLabel"),
        FindChildByName("StandInSequenceLabel"), FindChildByName("StandInFrameLabel")
    };

    Panel* const catalogue[] = {
        m_pCatalogueFilter, m_pCatalogueList, m_pCatalogueCount,
        m_pCatalogueClear, m_pCatalogueRescan, m_pCatalogueBack
    };

    for (Panel* panel : parameters)
    {
        if (panel != nullptr)
        {
            panel->SetVisible(page == Page::Parameters);
        }
    }

    for (Panel* panel : browser)
    {
        if (panel != nullptr)
        {
            panel->SetVisible(page == Page::Browser);
        }
    }

    for (Panel* panel : stand_in)
    {
        if (panel != nullptr)
        {
            panel->SetVisible(page == Page::StandIn);
        }
    }

    for (Panel* panel : catalogue)
    {
        if (panel != nullptr)
        {
            panel->SetVisible(page == Page::Catalogue);
        }
    }

    // The explanation belongs to the parameter page and is shown only when it has something
    // to explain, so whoever returns to that page decides it rather than this.
    m_pEmpty->SetVisible(false);
    m_pEnableShader->SetVisible(false);

    // On every page: it reports what the whole editor believes, and the page an author is on
    // when something goes wrong is not something they should have to leave first.
    m_pState->SetVisible(true);

    switch (page)
    {
    case Page::Browser: SetTitle("#GameUI_MatEdit_PickTitle", true); break;
    case Page::StandIn: SetTitle("#GameUI_MatEdit_StandInTitle", true); break;
    case Page::Catalogue: SetTitle("#GameUI_MatEdit_CatalogueTitle", true); break;
    default: SetTitle("#GameUI_MatEdit_Title", true); break;
    }

    if (page == Page::StandIn)
    {
        UpdateTargetControls();
    }

    InvalidateLayout();
}

// Half of this page belongs to the copy in the world and half to the weapon in the author's
// hands; what they share is one model on one frame of one animation. The two halves sit on
// the same rows because only one of them is ever being talked about.
void CMaterialEditDialog::UpdateTargetControls()
{
    bool stand_in = m_Target == MaterialTarget::StandIn;

    Panel* const only_stand_in[] = {
        m_pStandInModel, m_pStandInBrowse, m_pStandInYaw, m_pStandInPitch, m_pStandInScale,
        m_pStandInSpinning, m_pStandInPlace, FindChildByName("StandInModelLabel")
    };

    for (Panel* panel : only_stand_in)
    {
        if (panel != nullptr)
        {
            panel->SetVisible(stand_in);
        }
    }

    m_pViewmodelShown->SetVisible(!stand_in);
    m_pViewmodelOverride->SetVisible(!stand_in);
}

namespace
{
    // v_ak47.mdl, p_ak47.mdl, w_ak47.mdl: one weapon is three files, and the one other
    // players see is not the one in the author's hands.
    void CollectSiblings(const std::string& model, std::vector<std::string>& out)
    {
        out.clear();

        size_t slash = model.find_last_of('/');
        std::string directory = slash != std::string::npos ? model.substr(0, slash + 1) : std::string();
        std::string file = slash != std::string::npos ? model.substr(slash + 1) : model;

        // Only the usual three prefixes are swapped, and only when the name carries one:
        // anything else is a model whose siblings we would be guessing at.
        static const char* const kPrefixes[] = {"v_", "p_", "w_"};

        std::string stem = file;

        for (const char* prefix : kPrefixes)
        {
            if (file.rfind(prefix, 0) == 0)
            {
                stem = file.substr(2);
                break;
            }
        }

        if (stem == file)
        {
            out.push_back(model);
            return;
        }

        for (const char* prefix : kPrefixes)
        {
            out.push_back(directory + prefix + stem);
        }
    }
}

void CMaterialEditDialog::ShowStandIn()
{
    ReloadStandIn();
    ShowPage(Page::StandIn);
}

void CMaterialEditDialog::CloseStandIn()
{
    ShowPage(Page::Parameters);

    // The list can have grown while the stand-in was up: putting one into the world is what
    // makes its model prepared, and a prepared model is one the panel can edit.
    ReloadModels();
}

void CMaterialEditDialog::ReloadStandIn()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    m_LoadingStandIn = true;

    m_pStandInTarget->ActivateItemByRow(m_Target == MaterialTarget::StandIn ? 0 : 1);

    CollectSiblings(m_Model, m_StandInModels);

    m_pStandInModel->RemoveAll();
    m_pStandInModel->AddItem("#GameUI_MatEdit_StandInNone", nullptr);

    std::string current = editor->GetStandInModel();
    int active = 0;

    // A model taken from the catalogue is nobody's sibling, and a combo that cannot show what
    // is in the world would read as "nothing" over a model standing right there.
    if (!current.empty())
    {
        AddUnique(m_StandInModels, current);
    }

    for (size_t i = 0; i < m_StandInModels.size(); i++)
    {
        m_pStandInModel->AddItem(m_StandInModels[i].c_str(), nullptr);

        if (current == m_StandInModels[i])
        {
            active = static_cast<int>(i) + 1;
        }
    }

    m_pStandInModel->ActivateItemByRow(active);

    ReloadStandInSequences();

    MaterialStandIn state;
    editor->GetStandIn(&state);

    MaterialViewmodel viewmodel;
    editor->GetViewmodel(&viewmodel);

    MaterialAnimation animation;
    editor->GetAnimation(m_Target, &animation);

    m_pStandInFrame->SetValue(static_cast<int>(animation.frame * 1000.0f), false);
    m_pStandInPlaying->SetSelected(animation.playing);
    m_pStandInSpinning->SetSelected(state.spinning);
    UpdateScaleCaption(state.scale > 0.0f ? state.scale : 1.0f);
    m_pViewmodelShown->SetSelected(viewmodel.shown);
    m_pViewmodelOverride->SetSelected(viewmodel.overriding);

    m_LoadingStandIn = false;
}

void CMaterialEditDialog::ReloadStandInSequences()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    m_StandInSequences = editor->GetSequenceNames(m_Target);

    MaterialAnimation animation;
    editor->GetAnimation(m_Target, &animation);

    // Saved rather than set: this is called both on its own and from the middle of a reload
    // that is already holding the flag, and clearing it there would let the rest of that
    // reload push half a state back at the model.
    bool loading = m_LoadingStandIn;
    m_LoadingStandIn = true;

    m_pStandInSequence->RemoveAll();

    for (size_t start = 0; start < m_StandInSequences.size();)
    {
        size_t end = m_StandInSequences.find('\n', start);

        if (end == std::string::npos)
        {
            end = m_StandInSequences.size();
        }

        m_pStandInSequence->AddItem(m_StandInSequences.substr(start, end - start).c_str(), nullptr);
        start = end + 1;
    }

    if (animation.sequence_count > 0)
    {
        m_pStandInSequence->ActivateItemByRow(std::clamp(animation.sequence, 0, animation.sequence_count - 1));
    }

    m_LoadingStandIn = loading;
}

void CMaterialEditDialog::StepStandIn()
{
    MaterialEditInterface* editor = Editor();

    if (editor == nullptr || m_Page != Page::StandIn)
    {
        return;
    }

    // Which animations a model has is answered by the client, and only once it has loaded
    // the model, which is a frame or more after the panel asked for it. Nothing announces
    // that moment, so the answer is watched rather than asked for.
    if (m_StandInSequences != editor->GetSequenceNames(m_Target))
    {
        ReloadStandInSequences();
    }

    MaterialAnimation animation;
    editor->GetAnimation(m_Target, &animation);

    // While something is playing the frame belongs to the client, and the slider follows it
    // so that stopping the playback stops on the frame that is on screen.
    if (animation.playing)
    {
        m_pStandInFrame->SetValue(static_cast<int>(animation.frame * 1000.0f), false);
    }
}

// Touching an animation control while the hands are the target means the panel is driving
// them: the game drives that model otherwise, and an author who asked for frame 12 and got
// whatever the weapon felt like has been told nothing about their material. The switch stays
// on the page as the way back.
void CMaterialEditDialog::TakeOverViewmodel()
{
    if (m_Target != MaterialTarget::Viewmodel || m_pViewmodelOverride->IsSelected())
    {
        return;
    }

    // Held over the message this sends: the push that follows reads the switch itself.
    bool loading = m_LoadingStandIn;
    m_LoadingStandIn = true;

    m_pViewmodelOverride->SetSelected(true);

    m_LoadingStandIn = loading;
}

void CMaterialEditDialog::PushStandIn()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr || m_LoadingStandIn)
    {
        return;
    }

    if (m_Target == MaterialTarget::StandIn)
    {
        int row = m_pStandInModel->GetActiveItem();

        // Row zero is "nothing", so the list of models starts one later.
        editor->SetStandInModel(
            row > 0 && row - 1 < static_cast<int>(m_StandInModels.size())
                ? m_StandInModels[row - 1].c_str()
                : ""
        );

        MaterialStandIn state;
        editor->GetStandIn(&state);
        state.spinning = m_pStandInSpinning->IsSelected();
        editor->SetStandIn(&state);
    }
    else
    {
        MaterialViewmodel viewmodel;
        editor->GetViewmodel(&viewmodel);
        viewmodel.shown = m_pViewmodelShown->IsSelected();
        viewmodel.overriding = m_pViewmodelOverride->IsSelected();
        editor->SetViewmodel(&viewmodel);
    }

    MaterialAnimation animation;
    editor->GetAnimation(m_Target, &animation);

    animation.sequence = std::max(m_pStandInSequence->GetActiveItem(), 0);
    animation.playing = m_pStandInPlaying->IsSelected();

    // While something is playing the frame belongs to the client, and writing the slider
    // back each time the panel touched anything would fight it.
    if (!animation.playing)
    {
        animation.frame = m_pStandInFrame->GetValue() / 1000.0f;
    }

    editor->SetAnimation(m_Target, &animation);
}

void CMaterialEditDialog::OnDragValue(int index, int pixels, bool fine, bool coarse)
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    MaterialStandIn state;
    editor->GetStandIn(&state);

    if (index == 2)
    {
        // Zero means "as it was built", so it is read as one before anything is added to it.
        float scale = state.scale > 0.0f ? state.scale : 1.0f;
        float step = (fine ? 0.002f : (coarse ? 0.05f : 0.01f)) * pixels;

        state.scale = std::clamp(scale + step, kMinimumScale, kMaximumScale);

        editor->SetStandIn(&state);
        UpdateScaleCaption(state.scale);
        return;
    }

    float step = (fine ? 0.05f : (coarse ? 5.0f : 0.5f)) * pixels;

    if (index == 0)
    {
        state.angles[1] = std::fmod(state.angles[1] + step, 360.0f);
    }
    else
    {
        state.angles[0] = std::clamp(state.angles[0] + step, -89.0f, 89.0f);
    }

    editor->SetStandIn(&state);
}

// The one draggable caption whose value an author cannot see anywhere else: an angle is
// visible on the model, a size is only visible against a size it is not.
void CMaterialEditDialog::UpdateScaleCaption(float scale)
{
    wchar_t* caption = g_pVGuiLocalize->Find("#GameUI_MatEdit_StandInScale");

    wchar_t text[128];
    V_snwprintf(text, sizeof(text) / sizeof(wchar_t), L"%s  %.2f",
                caption != nullptr ? caption : L"Scale", scale);

    m_pStandInScale->SetText(text);
}

void CMaterialEditDialog::ShowBrowser(const char* param)
{
    m_BrowserParam = param;

    // Copied, not held: the interface hands out a pointer into engine storage that the very
    // next call may rewrite, and this one has to outlive the whole browsing session.
    MaterialEditInterface* editor = Editor();
    m_BrowserOriginal = editor != nullptr ? editor->GetTexture(param) : "";

    ScanBrowserFolders();
    ShowPage(Page::Browser);
}

void CMaterialEditDialog::CloseBrowser()
{
    // Only the page goes back. Reloading the lists here restarts the editing session, which
    // drops the editor's layer and re-seeds it from the file: every unsaved change gone and
    // the program back to whatever the file says. "Refresh" is for rescanning.
    ShowPage(Page::Parameters);
}

void CMaterialEditDialog::ScanBrowserFolders()
{
    m_BrowserFolders.clear();
    CollectFolders(kCollectionRoot, 2, m_BrowserFolders);
    std::sort(m_BrowserFolders.begin(), m_BrowserFolders.end());

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

    CollectFiles(m_BrowserFolders[row], "*.png", m_BrowserImages);
    CollectFiles(m_BrowserFolders[row], "*.tga", m_BrowserImages);
    std::sort(m_BrowserImages.begin(), m_BrowserImages.end());

    for (size_t i = 0; i < m_BrowserImages.size(); i++)
    {
        char command[32];
        Q_snprintf(command, sizeof(command), "highlight %d", static_cast<int>(i));

        BrowserRow* entry = new BrowserRow(m_pBrowserList->GetEmbedded(), this, m_BrowserImages[i], command);

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

void CMaterialEditDialog::ShowCatalogue()
{
    if (!m_CatalogueScanned)
    {
        ScanCatalogue();
    }

    ReloadCatalogueRows();
    ShowPage(Page::Catalogue);

    m_pCatalogueFilter->RequestFocus();
}

void CMaterialEditDialog::ScanCatalogue()
{
    m_CatalogueScanned = true;
    m_Catalogue.clear();

    std::vector<std::string> found;
    CollectModels(kCatalogueRoot, kCatalogueDepth, found);

    std::sort(found.begin(), found.end(), LessIgnoreCase);
    found.erase(
        std::unique(
            found.begin(),
            found.end(),
            [](const std::string& left, const std::string& right)
            {
                return V_stricmp(left.c_str(), right.c_str()) == 0;
            }
        ),
        found.end()
    );

    for (const std::string& path : found)
    {
        if (!IsPartOfAnotherModel(found, path))
        {
            m_Catalogue.push_back(path);
        }
    }
}

void CMaterialEditDialog::ReloadCatalogueRows()
{
    m_pCatalogueList->DeleteAllItems();
    m_CatalogueShown.clear();

    char filter[128] = {};
    m_pCatalogueFilter->GetText(filter, sizeof(filter));

    std::string needle(filter);

    for (size_t i = 0; i < m_Catalogue.size() && m_CatalogueShown.size() < static_cast<size_t>(kMaxCatalogueRows); i++)
    {
        if (ContainsIgnoreCase(m_Catalogue[i], needle))
        {
            m_CatalogueShown.push_back(static_cast<int>(i));
        }
    }

    for (size_t row = 0; row < m_CatalogueShown.size(); row++)
    {
        const std::string& path = m_Catalogue[m_CatalogueShown[row]];

        char command[32];
        Q_snprintf(command, sizeof(command), "catalogue_pick %d", static_cast<int>(row));

        // Shown without the root every one of them shares, which is the part of the path a
        // narrow docked panel can least afford to spend.
        const char* text = path.c_str();
        if (V_strnicmp(text, "models/", 7) == 0)
        {
            text += 7;
        }

        CatalogueRow* entry = new CatalogueRow(m_pCatalogueList->GetEmbedded(), this, text, command);

        entry->SetSize(1, kCatalogueRowHeight);
        m_pCatalogueList->AddItem(entry);
    }

    if (m_Catalogue.empty())
    {
        // An empty list on its own does not say whether nothing matched or nothing was
        // found at all, and the two have different answers.
        m_pCatalogueCount->SetText("#GameUI_MatEdit_CatalogueEmpty");
    }
    else
    {
        // Two numbers rather than a sentence: how many are on screen and how many exist. A
        // shown count that stops at the row limit is the sign to type more into the filter.
        char count[64];
        Q_snprintf(
            count, sizeof(count), "%d / %d",
            static_cast<int>(m_CatalogueShown.size()),
            static_cast<int>(m_Catalogue.size())
        );

        m_pCatalogueCount->SetText(count);
    }

    InvalidateLayout();
}

void CMaterialEditDialog::ChooseCatalogueModel(int index)
{
    MaterialEditInterface* editor = Editor();

    if (editor == nullptr || index < 0 || index >= static_cast<int>(m_CatalogueShown.size()))
    {
        return;
    }

    // Into the world at once and the page stays open, as in the collection: what tells an
    // author whether this is the model they meant is the model, not its file name.
    editor->SetStandInModel(m_Catalogue[m_CatalogueShown[index]].c_str());
}

void CMaterialEditDialog::ReloadModels()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    // Kept across the refill: the list is rebuilt whenever it might have grown, and moving
    // the author back to the first model every time would restart what they were editing.
    std::string previous = m_Model;

    m_pModel->RemoveAll();

    int count = editor->GetModelCount();
    int active = 0;

    for (int i = 0; i < count; i++)
    {
        const char* name = editor->GetModelName(i);
        m_pModel->AddItem(name, nullptr);

        if (previous == name)
        {
            active = i;
        }
    }

    // The shader path is what prepares studio models, and it is off by default and not
    // saved with the configuration, so it is off again after every restart. An empty list
    // that does not say which of the two reasons it is costs an evening.
    bool shader_path_on = engine->pfnGetCvarFloat("r_studio_shader") != 0.0f;

    m_pEmpty->SetVisible(count == 0);
    m_pEnableShader->SetVisible(count == 0 && !shader_path_on);
    m_pRows->SetVisible(count != 0);

    if (count == 0)
    {
        m_pEmpty->SetText(shader_path_on ? "#GameUI_MatEdit_NoModelsYet" : "#GameUI_MatEdit_ShaderOff");
        m_pSource->SetText("");
        m_pValidation->SetText("");
    }

    if (m_pModel->GetItemCount() > 0)
    {
        m_pModel->ActivateItemByRow(active);
    }

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
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    SelectedText(m_pModel, m_Model);

    // Kept across the refill for the same reason the model is: rebuilding the list must not
    // move the author off what they were editing.
    std::string previous = m_Texture;
    int active = 0;

    m_pTexture->RemoveAll();

    for (int i = 0; i < editor->GetTextureCount(m_Model.c_str()); i++)
    {
        const char* name = editor->GetTextureName(m_Model.c_str(), i);
        m_pTexture->AddItem(name, nullptr);

        if (V_stricmp(previous.c_str(), name) == 0)
        {
            active = i;
        }
    }

    if (m_pTexture->GetItemCount() > 0)
    {
        m_pTexture->ActivateItemByRow(active);
    }

    BeginEditing();
}

void CMaterialEditDialog::ReloadPreview()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    m_pSkin->RemoveAll();
    m_pBody->RemoveAll();

    int families = editor->GetSkinFamilyCount(m_Model.c_str());

    for (int i = 0; i < families; i++)
    {
        char text[32];
        Q_snprintf(text, sizeof(text), "%d", i);
        m_pSkin->AddItem(text, nullptr);
    }

    int bodies = editor->GetBodyCount(m_Model.c_str());

    for (int i = 0; i < bodies; i++)
    {
        // Named by the submodels the value selects rather than by the number itself: the
        // number says nothing, and the names are what the model's author wrote.
        char name[160];
        if (!editor->GetBodyName(m_Model.c_str(), i, name, sizeof(name)) || *name == '\0')
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
        m_pSkin->ActivateItemByRow(0);
    }

    if (bodies > 0)
    {
        m_pBody->ActivateItemByRow(0);
    }

    ApplyPreview();
}

void CMaterialEditDialog::ApplyPreview()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    editor->SetShownAs(m_pSkin->GetActiveItem(), m_pBody->GetActiveItem());
}

void CMaterialEditDialog::ReloadPrograms()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    m_pProgram->RemoveAll();

    // The material that does nothing beyond the model's own flags. Offered first because
    // going back to it is how an author undoes a material rather than emptying its values.
    m_pProgram->AddItem("studio_legacy", nullptr);

    int active = 0;

    for (int i = 0; i < editor->GetProgramCount(); i++)
    {
        const char* name = editor->GetProgramName(i);
        m_pProgram->AddItem(name, nullptr);

        if (m_Program == name)
        {
            active = i + 1;
        }
    }

    m_pProgram->ActivateItemByRow(active);
}

void CMaterialEditDialog::BeginEditing()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    SelectedText(m_pModel, m_Model);
    SelectedText(m_pTexture, m_Texture);

    // A session already open on this very target is left alone. Restarting it drops the
    // editor's layer and re-seeds from what the target resolves to on disk, which throws
    // away every unsaved change and puts the program back to whatever the file says - and
    // the lists are rebuilt often enough that this would happen on the way past.
    if (editor->IsEditing() &&
        m_Model == editor->GetEditedModel() &&
        V_stricmp(m_Texture.c_str(), editor->GetEditedTexture()) == 0)
    {
        return;
    }

    editor->End();

    if (!m_Model.empty() && !m_Texture.empty())
    {
        editor->Begin(m_Model.c_str(), m_Texture.c_str());
    }

    m_Program = editor->GetEditedProgram();

    ReloadPreview();
    ReloadPrograms();
    BuildRows();
    UpdateSource();
    UpdateValidation();
}

void CMaterialEditDialog::BuildRows()
{
    m_Rows.clear();
    m_pRows->DeleteAllItems();

    MaterialEditInterface* editor = Editor();
    if (editor == nullptr || !editor->IsEditing())
    {
        return;
    }

    const char* program = editor->GetEditedProgram();

    for (int group = 0; group < editor->GetGroupCount(program); group++)
    {
        const char* label = editor->GetGroupLabel(program, group);

        // The group holding what the manifest placed nowhere carries no label, and a header
        // there would name a section the program never declared.
        if (*label != '\0')
        {
            GroupHeader* header = new GroupHeader(m_pRows->GetEmbedded(), label);
            header->SetSize(1, kGroupHeaderHeight);
            m_pRows->AddItem(header);
        }

        for (int i = 0; i < editor->GetGroupParamCount(program, group); i++)
        {
            MaterialEditParam param;
            if (!editor->GetParam(program, editor->GetGroupParam(program, group, i), &param))
            {
                continue;
            }

            CMaterialParamRow* row = new CMaterialParamRow(m_pRows->GetEmbedded(), this, param);
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
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    // What the target resolves to with the editor's own layer taken away. Without this an
    // author edits a model the server describes, saves a file, reloads and finds the table
    // still winning, with nothing on screen having said so.
    m_pSource->SetText(SourceToken(editor->GetMaterialSource(m_Model.c_str(), m_Texture.c_str())));
}

void CMaterialEditDialog::UpdateValidation()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    const char* problem = editor->Validate();

    if (*problem == '\0')
    {
        m_pValidation->SetText("");
        m_pSave->SetEnabled(true);
        return;
    }

    // The localised explanation and the client's own message together: the first says what
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
    UpdateValidation();
}

void CMaterialEditDialog::SetTextureParam(const char* param, const char* path)
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    editor->SetTexture(param, path);

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->name() == param)
        {
            row->Refresh();
        }
    }

    UpdateValidation();
}

void CMaterialEditDialog::OnColorPicked(const char* param, const float rgba[4])
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr)
    {
        return;
    }

    editor->SetVec4(param, rgba);

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->name() == param)
        {
            row->Refresh();
        }
    }

    UpdateValidation();
}

bool CMaterialEditDialog::FindParamSchema(const char* name, MaterialEditParam* out)
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr || !editor->IsEditing())
    {
        return false;
    }

    const char* program = editor->GetEditedProgram();

    for (int i = 0; i < editor->GetParamCount(program); i++)
    {
        if (editor->GetParam(program, i, out) && V_strcmp(out->name, name) == 0)
        {
            return true;
        }
    }

    return false;
}

void CMaterialEditDialog::OnTextChanged(Panel* panel)
{
    if (panel == m_pBrowserFolder)
    {
        ReloadBrowserImages();
        return;
    }

    if (panel == m_pCatalogueFilter)
    {
        ReloadCatalogueRows();
        return;
    }

    if (panel == m_pStandInTarget)
    {
        m_Target = m_pStandInTarget->GetActiveItem() == 1 ? MaterialTarget::Viewmodel : MaterialTarget::StandIn;

        UpdateTargetControls();
        ReloadStandIn();
        return;
    }

    if (panel == m_pStandInModel || panel == m_pStandInSequence)
    {
        if (panel == m_pStandInSequence)
        {
            TakeOverViewmodel();
        }

        PushStandIn();
        return;
    }

    if (panel == m_pSkin || panel == m_pBody)
    {
        ApplyPreview();
        return;
    }

    if (panel == m_pModel)
    {
        ReloadTextures();
        return;
    }

    if (panel == m_pTexture)
    {
        BeginEditing();
        return;
    }

    if (panel == m_pProgram)
    {
        MaterialEditInterface* editor = Editor();
        std::string chosen;
        SelectedText(m_pProgram, chosen);

        if (editor != nullptr && editor->IsEditing() && chosen != editor->GetEditedProgram())
        {
            editor->SetEditedProgram(chosen.c_str());
            m_Program = chosen;

            BuildRows();
            UpdateValidation();
        }
    }
}

void CMaterialEditDialog::OnCommand(const char* command)
{
    // A child of this dialog. A frame is a popup, so being a child costs it nothing in
    // clipping, and vgui delivers input only to descendants of the modal surface: a window
    // opened as a sibling of a modal dialog receives no clicks at all, which looks exactly
    // like a window that refuses to open.
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

    if (V_strncmp(command, "catalogue_pick ", 15) == 0)
    {
        ChooseCatalogueModel(atoi(command + 15));
        return;
    }

    if (V_stricmp(command, "catalogue") == 0)
    {
        ShowCatalogue();
        return;
    }

    if (V_stricmp(command, "catalogue_clear") == 0)
    {
        if (MaterialEditInterface* editor = Editor())
        {
            editor->SetStandInModel("");
        }

        return;
    }

    if (V_stricmp(command, "catalogue_rescan") == 0)
    {
        ScanCatalogue();
        ReloadCatalogueRows();
        return;
    }

    if (V_stricmp(command, "catalogue_back") == 0)
    {
        ShowStandIn();
        return;
    }

    // Choosing is already done: the image went onto the model when it was highlighted, so
    // this only leaves the collection.
    if (V_stricmp(command, "browse_choose") == 0)
    {
        CloseBrowser();
        return;
    }

    if (V_stricmp(command, "browse_clear") == 0)
    {
        SetTextureParam(m_BrowserParam.c_str(), "");
        CloseBrowser();
        return;
    }

    if (V_stricmp(command, "browse_back") == 0)
    {
        // Back puts the previous image back, because everything looked at on the way here
        // was already applied to the model.
        SetTextureParam(m_BrowserParam.c_str(), m_BrowserOriginal.c_str());
        CloseBrowser();
        return;
    }

    if (V_strncmp(command, "color ", 6) == 0)
    {
        MaterialEditInterface* editor = Editor();

        if (editor != nullptr)
        {
            float rgba[4] = {1.0f, 1.0f, 1.0f, 1.0f};
            editor->GetVec4(command + 6, rgba);

            // The program's own default is what "use defaults" means here; nothing else
            // knows what this parameter is supposed to be.
            MaterialEditParam schema;
            float defaults[4] = {1.0f, 1.0f, 1.0f, 1.0f};

            if (FindParamSchema(command + 6, &schema) && schema.has_default)
            {
                for (int i = 0; i < 4; i++)
                {
                    defaults[i] = schema.default_vec4[i];
                }
            }

            CColorPickerDialog* picker = new CColorPickerDialog(this, this, command + 6, rgba, defaults);
            picker->Activate();
        }

        return;
    }

    if (V_stricmp(command, "state") == 0)
    {
        // Written to a file as well as printed, and that is the half that matters: the
        // console is not always reachable from over the game.
        if (Editor() != nullptr)
        {
            Editor()->DumpState();
        }

        return;
    }

    if (V_stricmp(command, "standin") == 0)
    {
        ShowStandIn();
        return;
    }

    if (V_stricmp(command, "standin_back") == 0)
    {
        CloseStandIn();
        return;
    }

    if (V_stricmp(command, "standin_place") == 0)
    {
        if (Editor() != nullptr)
        {
            Editor()->RequestStandInPlacement();
        }

        return;
    }

    if (V_stricmp(command, "refresh") == 0)
    {
        ReloadModels();
        return;
    }

    if (V_stricmp(command, "enable_shader") == 0)
    {
        engine->pfnClientCmd("r_studio_shader 1\n");

        // The command is buffered, and a model is prepared when it is next drawn, so the
        // list cannot be right yet. Saying what to do next beats an empty list that just
        // stopped explaining itself.
        m_pEmpty->SetText("#GameUI_MatEdit_NoModelsYet");
        m_pEnableShader->SetVisible(false);
        return;
    }

    if (V_stricmp(command, "sweep") == 0)
    {
        m_Sweeping = !m_Sweeping;
        m_SweepPhase = 0.0f;

        if (!m_Sweeping)
        {
            for (CMaterialParamRow* row : m_Rows)
            {
                row->Refresh();
            }
        }

        return;
    }

    if (V_stricmp(command, "save") == 0)
    {
        SaveDocument();
        return;
    }

    if (V_stricmp(command, "stub") == 0)
    {
        MaterialEditInterface* editor = Editor();

        if (editor != nullptr)
        {
            // To the console rather than to a file: this is text a plugin author pastes
            // into a table of their own, and where that table lives is their business.
            engine->pfnConsolePrint(editor->GetPresetStub());
            engine->pfnConsolePrint("\n");
        }

        return;
    }

    BaseClass::OnCommand(command);
}

void CMaterialEditDialog::SaveDocument()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr || !editor->IsEditing())
    {
        return;
    }

    char error[256] = {};

    if (!editor->Save(m_Model.c_str(), error, sizeof(error)))
    {
        wchar_t detail[512];
        g_pVGuiLocalize->ConvertANSIToUnicode(error, detail, sizeof(detail));
        m_pValidation->SetText(detail);
        return;
    }

    if (m_pSiblings->IsSelected())
    {
        // One weapon is three model files, and an author who saves only the viewmodel finds
        // out from somebody else's screenshot. Every other prepared model carrying a texture
        // of this name gets the same material.
        for (int i = 0; i < editor->GetModelCount(); i++)
        {
            const char* other = editor->GetModelName(i);

            if (m_Model == other)
            {
                continue;
            }

            for (int t = 0; t < editor->GetTextureCount(other); t++)
            {
                if (V_stricmp(editor->GetTextureName(other, t), m_Texture.c_str()) == 0)
                {
                    editor->Save(other, error, sizeof(error));
                    break;
                }
            }
        }
    }

    m_pValidation->SetText("#GameUI_MatEdit_Saved");
}

void CMaterialEditDialog::StepSweep()
{
    MaterialEditInterface* editor = Editor();
    if (editor == nullptr || !editor->IsEditing())
    {
        return;
    }

    const char* program = editor->GetEditedProgram();

    for (int i = 0; i < editor->GetParamCount(program); i++)
    {
        MaterialEditParam param;
        if (!editor->GetParam(program, i, &param) || !param.instance || !param.has_range)
        {
            continue;
        }

        // A triangle rather than a saw: the way back is as informative as the way out, and
        // a jump from full wear to none reads as a glitch rather than as a new pass.
        float t = m_SweepPhase > 1.0f ? 2.0f - m_SweepPhase : m_SweepPhase;

        editor->SetScalar(param.name, param.min_value + (param.max_value - param.min_value) * t);
    }

    for (CMaterialParamRow* row : m_Rows)
    {
        if (row->is_instance())
        {
            row->Refresh();
        }
    }
}

void CMaterialEditDialog::OnThink()
{
    BaseClass::OnThink();

    StepStandIn();

    if (!m_Sweeping)
    {
        return;
    }

    m_SweepPhase += 1.0f / (kSweepSeconds * 30.0f);

    if (m_SweepPhase > 2.0f)
    {
        m_SweepPhase -= 2.0f;
    }

    StepSweep();
}

// The frame slider is the one control here that sends this, and it sent it to nobody: the
// panel listened for a text change and for a check button, so scrubbing through an animation
// did nothing until something else was touched.
void CMaterialEditDialog::OnSliderMoved(Panel* panel)
{
    MaterialEditInterface* editor = Editor();

    if (panel != m_pStandInFrame || editor == nullptr || m_LoadingStandIn)
    {
        return;
    }

    TakeOverViewmodel();

    MaterialAnimation animation;
    editor->GetAnimation(m_Target, &animation);

    animation.frame = m_pStandInFrame->GetValue() / 1000.0f;

    editor->SetAnimation(m_Target, &animation);

    // The switch may have just been turned on, and it is the panel that owns what it says.
    MaterialViewmodel viewmodel;
    editor->GetViewmodel(&viewmodel);
    viewmodel.overriding = m_pViewmodelOverride->IsSelected();
    editor->SetViewmodel(&viewmodel);
}

void CMaterialEditDialog::OnCheckButtonChecked(Panel* panel)
{
    if (panel == m_pStandInPlaying || panel == m_pStandInSpinning ||
        panel == m_pViewmodelShown || panel == m_pViewmodelOverride)
    {
        if (panel == m_pStandInPlaying && m_pStandInPlaying->IsSelected())
        {
            TakeOverViewmodel();
        }

        PushStandIn();
        return;
    }

    if (panel != m_pCompare)
    {
        return;
    }

    // The cvar suppresses every material source at once, which is the only comparison that
    // means anything: guarding one source would leave the model still described by another.
    engine->pfnClientCmd(m_pCompare->IsSelected() ? "r_studio_materials 0\n" : "r_studio_materials 1\n");
}

#ifndef GAMEUI_INTERFACE_H
#define GAMEUI_INTERFACE_H
#pragma once

#include <memory>

#include <vgui/VGUI2.h>
#include <IGameUI.h>
#include <goldsrc/client_api.h>
#include <GameUINext.h>
#include <ISystemModule.h>
#include <vgui_controls/Panel.h>
#include <vgui_controls/PHandle.h>
#include <utils/TaskRun.h>
#include <IGameClientExports.h>
#include <next_engine_mini/engine_mini.h>
#include <next_engine_mini/ScenePreviewInterface.h>

#include "LoadingDialog.h"
#include "GameUi/Controls/ImageFileCache.h"

namespace render
{
    class ShaderRendererInterface;
    class MaterialEditInterface;
} // namespace render

namespace material_editor
{
    class CMaterialEditDialog;
}
class IFileSystem;
namespace vgui2
{
    class ISurface;
}

class CGameUI : public IGameUI
{
private:
    enum
    {
        MAX_NUM_FACTORIES = 5
    };

public:
    CGameUI();
    ~CGameUI();

public:
    virtual void Initialize(CreateInterfaceFn *factories, int count);
    virtual void Start(goldsrc::cl_enginefunc_t* engineFuncs, int interfaceVersion, void* system);
    virtual void Shutdown();
    virtual int ActivateGameUI();
    virtual int ActivateDemoUI();
    virtual int HasExclusiveInput();
    virtual void RunFrame();
    virtual void ConnectToServer(const char *game, int IP, int port);
    virtual void DisconnectFromServer();
    virtual void HideGameUI();
    virtual bool IsGameUIActive();
    virtual void LoadingStarted(const char *resourceType, const char *resourceName);
    virtual void LoadingFinished(const char *resourceType, const char *resourceName);
    virtual void StartProgressBar(const char *progressType, int progressSteps);
    virtual int ContinueProgressBar(int progressPoint, float progressFraction);
    virtual void StopProgressBar(bool bError, const char *failureReason, const char *extendedReason = NULL);
    virtual int SetProgressBarStatusText(const char *statusText);
    virtual void SetSecondaryProgressBar(float progress);
    virtual void SetSecondaryProgressBarText(const char *statusText);
    virtual void ValidateCDKey(bool force, bool inConnect);
    virtual void OnDisconnectFromServer(int eSteamLoginFailure, const char *username);

public:
    bool IsServerBrowserValid();
    void ActivateServerBrowser();
    bool FindPlatformDirectory(char* platformDir, int bufferSize);
    void NeedApplyMultiplayerGameSettings();

    // Opens the advanced-radar settings frame: a normal dialog from the menu, or a
    // frame floating over the running game while in a level. Called directly from the
    // Options button (there is no console command for it).
    void OpenHudRadarDialog();
    void OpenMaterialEditDialog();
    void CloseMaterialEditDialog();
    void OnMaterialEditDialogClosed(material_editor::CMaterialEditDialog* dialog);
    bool is_material_editor_open() const;
    void DumpMaterialImages();
    void RequestMaterialStateDump();
    void PrintMaterialReport(const char* report);
    void DumpMaterialSceneState();

    // Shows a VGUI panel over the running game instead of the menu: reparents it to
    // the client panel the engine paints while the GameUI menu is hidden, hides the
    // GameUI so the engine resumes drawing the HUD, and enables the mouse cursor.
    // While a panel is registered this way, Esc closes only it (ActivateGameUI is
    // suppressed). The registration is a safe panel handle, so a destroyed panel
    // clears it on its own; a level transition closes the dialog before releasing
    // its registration.
    // Returns false when not in a level (the caller shows a normal dialog instead).
    bool BeginPanelOverGame(vgui2::Panel* panel);

    // Teardown for the panel shown over the running game. Mirrors the engine-side
    // game-ui hide that the second Esc of an open/close cycle performs, so the client
    // restores its HUD after the panel closes - notably the spectator letterbox and
    // bottom menu, which a plain close leaves hidden. No-op unless a panel is over the
    // game.
    void EndPanelOverGame();

public:
    bool IsInLevel();
    bool IsInMultiplayer();

private:
    void OpenServerBrowserIfNeeded();
    void CloseHudRadarDialog();
    void ClosePanelOverGame();

private:
    bool m_bFirstActivatePassed;
    bool m_bServBrowserFirstActivatePassed;
    bool m_bLoadlingLevel;
    // The panel currently shown over the running game (see BeginPanelOverGame); a safe
    // handle so a destroyed panel deregisters itself. Empty when the menu owns input.
    vgui2::PHandle m_hPanelOverGame;
    char m_szPreviousStatusText[128];
    char m_szPlatformDir[MAX_PATH];
    bool m_bNeedApplyMultiplayerGameSettings;

    int m_iNumFactories;
    CreateInterfaceFn m_FactoryList[MAX_NUM_FACTORIES];

    std::shared_ptr<TaskCoroImpl> task_run_impl_;
    // VGUI safe-handle lookup has no const overload.
    mutable vgui2::DHANDLE<material_editor::CMaterialEditDialog> m_hMaterialEditDialog;
    std::shared_ptr<ImageFileCache> m_ImageFileCache;
    render::ShaderRendererInterface* m_ShaderRenderer{};
    render::MaterialEditInterface* m_MaterialEditor{};
    IFileSystem* m_FileSystem{};
    vgui2::ISurface* m_Surface{};
    bool m_ShuttingDown{};

    class ContainerExtensionGameUiApi* browserExtensionGameUiApi = nullptr;
};

extern CGameUI& GameUI();
extern vgui2::Panel* StaticPanel();
extern IBaseSystem* SystemWrapper();
extern goldsrc::cl_enginefunc_t* engine;
extern vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;
extern IGameClientExports* GameClientExports();
extern EngineMiniInterface* EngineMini();
extern ScenePreviewInterface* ScenePreview();

#endif

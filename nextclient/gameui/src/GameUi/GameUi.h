#ifndef GAMEUI_INTERFACE_H
#define GAMEUI_INTERFACE_H
#pragma once

#include <vgui/VGUI2.h>
#include <IGameUI.h>
#include <GameUINext.h>
#include <ISystemModule.h>
#include <vgui_controls/Panel.h>
#include <concurrencpp/concurrencpp.h>
#include <IGameClientExports.h>
#include "LoadingDialog.h"

class TaskRunImpl;

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
    virtual void Start(cl_enginefuncs_s *engineFuncs, int interfaceVersion, void *system);
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

public:
    bool IsInLevel();
    bool IsInMultiplayer();

private:
    void OpenServerBrowserIfNeeded();

private:
    bool m_bFirstActivatePassed;
    bool m_bServBrowserFirstActivatePassed;
    bool m_bLoadlingLevel;
    char m_szPreviousStatusText[128];
    char m_szPlatformDir[MAX_PATH];
    bool m_bNeedApplyMultiplayerGameSettings;

    int m_iNumFactories;
    CreateInterfaceFn m_FactoryList[MAX_NUM_FACTORIES];

    std::shared_ptr<TaskRunImpl> task_run_impl_;

    class ContainerExtensionGameUiApi* browserExtensionGameUiApi;
};

extern CGameUI& GameUI();
extern vgui2::Panel* StaticPanel();
extern IBaseSystem* SystemWrapper();
extern cl_enginefunc_t* engine;
extern vgui2::DHANDLE<CLoadingDialog> g_hLoadingDialog;
extern IGameClientExports* GameClientExports();

#endif
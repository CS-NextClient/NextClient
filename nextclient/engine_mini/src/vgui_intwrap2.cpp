#include "engine.h"
#include "vgui_int.h"
#include "client/cl_scrn.h"

void StartLoadingProgressBar(const char *loadingType, int numProgressPoints)
{
    eng()->StartLoadingProgressBar.InvokeChained(loadingType, numProgressPoints);
}

void SetLoadingProgressBarStatusText(const char *statusText)
{
    if (!g_pGameUi)
        return;

    if (g_pGameUi->SetProgressBarStatusText(statusText))
        SCR_UpdateScreen();
}

void SetSecondaryProgressBar(float progress)
{
    if (!g_pGameUi)
        return;

    g_pGameUi->SetSecondaryProgressBar(progress);
}

void SetSecondaryProgressBarText(const char *statusText)
{
    if (!g_pGameUi)
        return;

    g_pGameUi->SetSecondaryProgressBarText(statusText);
}

void ContinueLoadingProgressBar(const char *loadingType, int progressPoint, float progressFraction)
{
    if (!g_pGameUi)
        return;

    if (g_pGameUi->ContinueProgressBar(progressPoint, progressFraction))
        SCR_UpdateScreen();
}

void VGuiWrap2_LoadingStarted(const char *resourceType, const char *resourceName)
{
    if (!g_pGameUi)
        return;

    g_pGameUi->LoadingStarted(resourceType, resourceName);
}

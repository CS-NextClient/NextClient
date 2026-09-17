#pragma once

void StartLoadingProgressBar(const char *loadingType, int numProgressPoints);
void StopLoadingProgressBar();
void SetLoadingProgressBarStatusText(const char *statusText);
void SetSecondaryProgressBar(float progress);
void SetSecondaryProgressBarText(const char *statusText);
void ContinueLoadingProgressBar(const char *loadingType, int progressPoint, float progressFraction);

void VGuiWrap2_LoadingStarted(const char* resourceType, const char* resourceName);
void VGuiWrap2_NotifyOfServerDisconnect();

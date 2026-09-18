#pragma once

namespace goldsrc
{
    enum VoiceTweakControl
    {
        MicrophoneVolume = 0,
        OtherSpeakerScale = 1,
        MicBoost = 2
    };

    struct IVoiceTweak_s
    {
        int (*StartVoiceTweakMode)();
        void (*EndVoiceTweakMode)();
        void (*SetControlFloat)(VoiceTweakControl, float);
        float (*GetControlFloat)(VoiceTweakControl);
        int (*GetSpeakingVolume)();
    };
} // namespace goldsrc

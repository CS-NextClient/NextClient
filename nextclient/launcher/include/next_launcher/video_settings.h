#pragma once

#include <icommandline.h>

namespace video_settings
{
    inline constexpr char kMsaaSamplesKey[] = "msaa_samples";
    inline constexpr int kDefaultMsaaSamples = -1;

    constexpr int NormalizeMsaaMode(int samples)
    {
        return samples == 0 || samples == 4 ? samples : kDefaultMsaaSamples;
    }

    inline void PrepareEngineCommandLine(ICommandLine& session, const ICommandLine& source, int msaa_samples)
    {
        session.CreateCmdLine(source.GetCmdLine());

        switch (NormalizeMsaaMode(msaa_samples))
        {
            case 0:
                session.AppendParm("-nomsaa", nullptr);
                break;

            case 4:
                session.RemoveParm("-nomsaa");
                session.RemoveParm("-nofbo");
                break;
        }
    }
} // namespace video_settings

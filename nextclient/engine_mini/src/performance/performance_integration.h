#pragma once

#include <cstdint>

namespace nitroapi
{
    class NitroApiInterface;
}

namespace performance
{
    namespace runtime
    {
        void Bind(nitroapi::NitroApiInterface* nitro_api);
        void ContextCreated();
        void ContextChanging();
        void ReleaseContext();
        void AbandonContext();
        void MapChanged();
        void RegisterControls();
        void PresentationInitialized();
        void BindPresentation();
        void Shutdown();
    } // namespace runtime

    namespace presentation
    {
        bool UsesLegacyWindowSize(uint8_t major, uint8_t minor, uint8_t patch, bool drawable_size);
    } // namespace presentation
} // namespace performance

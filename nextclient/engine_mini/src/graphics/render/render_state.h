#pragma once

namespace render
{
    class ServerMaterials;
    class MaterialEditor;
    class MaterialDocumentStore;
    struct RendererState;
    RendererState& GetRendererState();
    ServerMaterials& GetServerMaterials();
    MaterialEditor& GetMaterialEditor();
    MaterialDocumentStore& GetMaterialDocuments();
    void Renderer_InitializeMaterialFiles(const char* game_directory);
} // namespace render

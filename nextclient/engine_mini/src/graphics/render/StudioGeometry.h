#pragma once

#include <ncl_material/MaterialResolve.h>
#include <ncl_studio/StudioMesh.h>

#include <goldsrc/format/studio.h>
#include <goldsrc/fwd.h>

namespace render
{
    // One studio model uploaded to the GPU: the whole model shares a single vertex and
    // index buffer, and every mesh is drawn as a range inside them.
    struct StudioGpuModel
    {
        // GLuint, spelled out so this header does not drag in a GL loader; engine_mini
        // translation units disagree on which GL header comes first.
        unsigned int vao;
        const ncl_studio::StudioMesh* mesh;
    };

    // Converts and uploads on first use, then returns the cached model. Returns nullptr
    // when the pipeline is unavailable or the model cannot be converted; a failure is
    // cached so a broken model is not retried every frame.
    const StudioGpuModel* Geometry_Get(goldsrc::model_t* model, const goldsrc::studiohdr_t* header);

    // The material for one texture of a model, resolved once per model rather than per
    // draw: resolution copies a material, and doing that per submodel would undo the
    // work that made this path cheap. Never null once the model is cached; a model with
    // no authored description still gets the material implied by its texture flags.
    const ncl_material::Material* Geometry_Material(
        goldsrc::model_t* model,
        const goldsrc::studiohdr_t* header,
        const goldsrc::studiohdr_t* texture_header,
        int texture_index
    );

    // The models prepared so far, which is what an author can currently see and therefore
    // what an author can judge a material on. Sorted by name, so a list built from this
    // does not reorder itself as models are prepared.
    int Geometry_ModelCount();
    const char* Geometry_ModelName(int index);

    // The texture names of a prepared model. Not a convenience: a material is addressed by
    // texture name rather than by mesh, so this is how a target is named at all.
    int Geometry_TextureCount(const char* model);
    const char* Geometry_TextureName(const char* model, int index);

    // What a prepared model can be shown as. Neither changes what a material applies to -
    // a material belongs to a texture name - but both change which meshes are on screen and
    // which texture each of them resolves to, and an author has to be able to look.
    int Geometry_SkinFamilyCount(const char* model);
    int Geometry_BodyCount(const char* model);

    // The submodels a body value selects, joined by a slash. False when the model has no
    // body parts worth naming or the value is out of range.
    bool Geometry_BodyName(const char* model, int body, char* out, int out_len);

    // The material one texture of a prepared model resolved to, and where it came from.
    // Returns nullptr when the model has not been prepared or names no such texture.
    //
    // This is the model's own resolution and nothing above it. What a server bound belongs
    // to an entity rather than to a model, and is applied in the draw path; ask
    // Geometry_DrawnBy for the entity and ServerMaterials for the binding.
    const ncl_material::Material* Geometry_MaterialByName(
        const char* model,
        const char* texture_name,
        ncl_material::MaterialOrigin* origin
    );

    // Which entity index last drew this model, or 0 for a model nothing has drawn. The one
    // link between a question about a model and an answer that belongs to an entity: the
    // editor asks about the model in front of the author, and the entity that drew it is
    // what put it there.
    void Geometry_NoteDrawnBy(const char* model, int entity_index);
    int Geometry_DrawnBy(const char* model);

    // Makes the next resolution re-read the documents beside the models. The manifests and
    // the shaders are dropped by their own reload; this is the third input to what a model
    // is baked from, and an editor that wrote a file needs all three.
    void Geometry_ReloadDocuments();

    // Drops every cached model. Bound to map load, since the engine reuses its model
    // slots and a stale entry would render the wrong geometry.
    void Geometry_Clear();
}

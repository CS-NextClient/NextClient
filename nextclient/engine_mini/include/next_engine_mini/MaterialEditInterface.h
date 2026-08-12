#pragma once

// Material authoring, exported so a panel can be built over it without knowing a single
// parameter name. The panel asks a program for its schema and shows what it is told;
// what a parameter means stays with the program that declared it.
//
// Nothing here carries a container across the module boundary: the CRT is linked
// statically and the two modules own separate heaps. Enumeration goes by index, text
// comes back as a pointer into engine storage that stays valid until the next call which
// could change it, and a caller keeping any of it copies it.

// What kind of value a parameter holds. The same three the material format carries.
enum class MaterialEditParamType
{
    Scalar,
    Vec4,
    Texture
};

// Which control a program would like one of its values edited with. Not a role: it says
// nothing about what the value means, only how it is comfortable to enter. A program that
// declares nothing gets plain numeric fields.
enum class MaterialEditParamWidget
{
    Default,
    Color
};

// Where a target's material comes from with the editor's own layer taken away. An author
// whose model is described by the server's table has to learn that before saving a file
// which will lose to it.
enum class MaterialEditSource
{
    None,
    Instance,
    ModelDocument,
    Implicit
};

struct MaterialEditParam
{
    const char* name;

    // What to show instead of the identifier, and what to explain beside it. A leading
    // '#' is a localisation token; label is never null and never empty, hint may be empty.
    const char* label;
    const char* hint;

    MaterialEditParamType type;
    MaterialEditParamWidget widget;

    // The server may set this one per entity, so the material has to look right across
    // the whole range rather than at the value currently in the editor.
    bool instance;

    // Step the value has to sit on, or 0 when it sits on none.
    float quantum;

    // The span the program declares for a scalar. Without one there is nothing to lay a
    // slider over, and a display shows a typed number rather than inventing a range.
    bool has_range;
    float min_value;
    float max_value;

    bool has_default;
    float default_scalar;
    float default_vec4[4];
    const char* default_texture;
};

// Which model the panel is talking about. Both are one model held on one frame of one
// animation, so everything that is true of an animation is addressed through this rather
// than written twice.
enum class MaterialTarget
{
    StandIn,
    Viewmodel,

    Count
};

// What a model is doing while it is being looked at.
struct MaterialAnimation
{
    int sequence;

    // Where in the sequence, from 0 to 1. Held there when nothing is playing, which is what
    // makes a single frame of a reload something an author can look at.
    float frame;

    bool playing;

    // Published by the client once the model is loaded; nothing else knows how many
    // animations it has.
    int sequence_count;
};

// Everything about the stand-in except which model it is. Plain data on purpose: it crosses
// the module boundary in both directions, the panel writing what an author asked for and
// the client writing back what only it can know.
struct MaterialStandIn
{
    float origin[3];

    // Pitch and yaw as an author turned them; roll is left at zero.
    float angles[3];

    // Turns it slowly on the spot. A finish is judged by how it moves under the light, and
    // holding a drag while looking at it is not the same as watching it turn.
    bool spinning;

    // How large, with zero meaning "as the model was built". Zero rather than one so that a
    // struct nobody has filled in asks for nothing, which is what every field here has to do.
    float scale;

    MaterialAnimation animation;
};

// The weapon in the author's own hands, which is where most of a finish is looked at.
struct MaterialViewmodel
{
    // Off takes it out of the frame entirely, which is what an author wants when what they
    // are judging is behind it.
    bool shown;

    // Off leaves it animating as the game drives it. On holds it wherever the panel puts it,
    // which is the only way to stand on the frame where a bolt is back.
    bool overriding;

    MaterialAnimation animation;
};

// The camera the panel flies while it is open. It is nobody's entity: the player stays where
// they were standing, and this only decides what the frame is drawn from, so it goes through
// walls, costs the server nothing and needs no cheat to be allowed.
struct MaterialCamera
{
    float origin[3];

    // Pitch, yaw, roll; roll is left at zero.
    float angles[3];

    bool active;

    // Asks to start from where the author is looking now. Answered by the client, which is
    // the only side that knows where that is.
    bool seed_requested;
};

class MaterialEditInterface
{
public:
    virtual ~MaterialEditInterface() = default;

    // --- what programs there are, and what each declares ---

    virtual int GetProgramCount() = 0;
    virtual const char* GetProgramName(int index) = 0;

    virtual int GetParamCount(const char* program) = 0;
    virtual bool GetParam(const char* program, int index, MaterialEditParam* out) = 0;

    // Sections, in the order a panel should lay them out. Every parameter belongs to
    // exactly one, so walking the groups walks the whole schema; a group whose label is
    // empty is the one holding what the manifest did not place, and gets no header.
    virtual int GetGroupCount(const char* program) = 0;
    virtual const char* GetGroupLabel(const char* program, int group) = 0;
    virtual int GetGroupParamCount(const char* program, int group) = 0;
    virtual int GetGroupParam(const char* program, int group, int index) = 0;

    // --- what can be edited ---

    // The studio models the renderer has prepared this map, which is what an author can
    // see and therefore what an author can judge.
    virtual int GetModelCount() = 0;
    virtual const char* GetModelName(int index) = 0;

    // A material is addressed by texture name rather than by mesh, so this list is not a
    // convenience: it is how a target is named.
    virtual int GetTextureCount(const char* model) = 0;
    virtual const char* GetTextureName(const char* model, int index) = 0;

    virtual MaterialEditSource GetMaterialSource(const char* model, const char* texture_name) = 0;

    // --- what the model can be shown as ---
    //
    // Neither of these changes what a material applies to: a material belongs to a texture
    // name, and every mesh of every body part that resolves to that texture gets it. What
    // they change is which meshes are on screen and which texture each of them resolves to,
    // and an author has to be able to look at the other ones without leaving the panel.

    virtual int GetSkinFamilyCount(const char* model) = 0;
    virtual int GetBodyCount(const char* model) = 0;
    virtual bool GetBodyName(const char* model, int body, char* out, int out_len) = 0;

    // Applied to the edited model alone, and only while the panel holds a session. A
    // negative value asks for nothing and leaves the entity's own state alone.
    virtual void SetShownAs(int skin_family, int body) = 0;
    virtual int GetShownSkin() = 0;
    virtual int GetShownBody() = 0;

    // --- the stand-in ---
    //
    // A copy of a model put into the world so it can be judged the way other players see
    // it. Two things make this necessary rather than convenient: the model other players
    // see is a different file (p_ beside v_), and a view model is glued to the camera, so
    // turning the camera says nothing about how the weapon looks from another angle. The
    // stand-in can be turned, walked around and stopped on any frame of any animation.
    //
    // It is an ordinary entity drawn by the game, not a preview of our own: the lighting,
    // the chrome and the animation are the real ones because nothing about them is
    // reimplemented here.

    // Game-directory-relative, empty to show nothing.
    virtual void SetStandInModel(const char* path) = 0;
    virtual const char* GetStandInModel() = 0;

    // The model put in the author's hands, when a particular one has been asked for. Empty is
    // the weapon the game gave them. Any model will do: what a viewmodel is, to everything
    // below the panel, is the model the engine hands over as the view entity.
    virtual void SetViewmodelModel(const char* path) = 0;
    virtual const char* GetViewmodelModel() = 0;

    virtual void GetStandIn(MaterialStandIn* out) = 0;
    virtual void SetStandIn(const MaterialStandIn* state) = 0;

    // --- the weapon in the author's hands ---

    virtual void GetViewmodel(MaterialViewmodel* out) = 0;
    virtual void SetViewmodel(const MaterialViewmodel* viewmodel) = 0;

    // --- what either of them is doing ---

    virtual void GetAnimation(MaterialTarget target, MaterialAnimation* out) = 0;
    virtual void SetAnimation(MaterialTarget target, const MaterialAnimation* animation) = 0;

    // A model's animations by name, one per line, in the order the model declares them.
    // Written by the client, which is the only side that can turn a model into a studio
    // header, and it can only do it once the model is loaded - which is a frame or more
    // after the panel asked for it, so the panel waits for this rather than asking.
    virtual void SetSequenceNames(MaterialTarget target, const char* names) = 0;
    virtual const char* GetSequenceNames(MaterialTarget target) = 0;

    // Asks for it to be put where the author is looking from. Answered by the client, which
    // is the only side that knows where that is.
    virtual void RequestStandInPlacement() = 0;
    virtual bool TakeStandInPlacement() = 0;

    // --- the panel itself ---

    // Whether a panel is open. Everything the client applies every frame - the camera, the
    // weapon in the author's hands, the copy in the world - is gated on this, so a closed
    // panel is a panel that changes nothing about the running game. Without it the editor's
    // idea of what a field means leaks into every session: an all-zero "shown" once took the
    // weapon out of every player's hands.
    virtual void SetPanelOpen(bool open) = 0;
    virtual bool IsPanelOpen() = 0;

    // Writes everything the editor believes to the console and to materials_state.txt in the
    // game directory. The file is the point: the console is a panel of the game ui, and the
    // game ui is not always reachable from where the editor is.
    virtual void DumpState() = 0;

    // One line from the client about what it last applied, for that report. The engine holds
    // what a panel asked for; only the client knows what became of it.
    virtual void SetClientReport(const char* text) = 0;

    // --- the viewport and the camera ---

    // How much of the screen the docked panel covers, as the panel's own width and the
    // screen width it was measured against. Not pixels: vgui coordinates stop being frame
    // pixels the moment the interface is scaled, and only the renderer knows its own frame.
    // Zero width gives the world the whole frame back.
    virtual void SetViewportInset(int panel_wide, int screen_wide) = 0;

    // What the renderer actually took off the frame last time it drew the world, in frame
    // pixels. Anything drawn over the world afterwards is placed against the whole screen,
    // and has to move by half of this to sit where the world ended up.
    virtual int GetAppliedInset() = 0;

    // What a click over the scene landed on, as the model's path, or empty for nothing. The
    // point is given as a fraction of the part of the screen the world is drawn into, not in
    // pixels: only the renderer knows how large its frame is, and only the panel knows how
    // much of the screen it is covering.
    virtual const char* PickModel(float u, float v) = 0;

    virtual void GetCamera(MaterialCamera* out) = 0;
    virtual void SetCamera(const MaterialCamera* camera) = 0;

    // --- the editing session ---

    // Seeds the editor with whatever that target resolves to now and puts the editor's
    // copy above every other source, so a slider changes what is on screen without
    // writing a file and reloading.
    virtual bool Begin(const char* model, const char* texture_name) = 0;
    virtual void End() = 0;
    virtual bool IsEditing() = 0;

    virtual const char* GetEditedModel() = 0;
    virtual const char* GetEditedTexture() = 0;

    virtual const char* GetEditedProgram() = 0;

    // Drops whatever the previous program declared and takes the new one's defaults: a
    // value belonging to a program no longer named is a value nothing will read.
    virtual bool SetEditedProgram(const char* program) = 0;

    virtual bool GetScalar(const char* name, float* out) = 0;
    virtual bool GetVec4(const char* name, float* out) = 0;
    virtual const char* GetTexture(const char* name) = 0;

    virtual bool SetScalar(const char* name, float value) = 0;
    virtual bool SetVec4(const char* name, const float* value) = 0;
    virtual bool SetTexture(const char* name, const char* path) = 0;
    virtual bool ClearParam(const char* name) = 0;

    // --- checking and saving ---

    // Empty when the material matches its program. Otherwise the message the client
    // itself would print, unlocalised: a panel shows its own explanation of what went
    // wrong and this beside it, because this is the text an author carries to a search.
    virtual const char* Validate() = 0;

    // Folds the editor's copy into that model's document and writes it, leaving the
    // entries for other textures as they were. One weapon is three model files, so the
    // model is a parameter rather than the one being edited.
    virtual bool Save(const char* model, char* error_out, int error_out_len) = 0;

    // The same material as an entry of a server's table, for a plugin author to paste.
    virtual const char* GetPresetStub() = 0;

    // Re-reads documents and manifests from disk, the same path r_reload_shaders takes.
    // A panel that wrote a file and did not do this leaves the author looking at what was
    // already on screen.
    virtual void Reload() = 0;
};

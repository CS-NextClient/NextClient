#pragma once

#include <cstdint>

// Engine-owned glyph service. Rasterization and atlas packing live here, as they do in stock
// GoldSrc, and consumers ask for cells rather than owning a glyph store of their own.
// This is the query half: it hands out atlas cells and metrics. Drawing stays with the caller,
// because the radar composites its glyphs inside its own shader batch.

typedef int TextFont_t;
constexpr TextFont_t kInvalidTextFont = 0;

enum TextFontFlag
{
    kTextFontNone = 0,
    kTextFontDropShadow = 1 << 0,
    // Off by default: crisp bitmap rasterization, matching the engine's per-font FONTFLAG_ANTIALIAS.
    kTextFontAntialias = 1 << 1,
};

// Texel layout of the atlas pages (LUMINANCE_ALPHA): alpha carries coverage, luminance carries the
// fill so GL_MODULATE shows the caller's colour, and a baked drop shadow reads as black.
enum class TextGlyphFormat
{
    Coverage,
};

struct TextFontDesc
{
    const char* face;
    int tall; // cell height in pixels
    int weight;
    int flags;
    TextGlyphFormat format;
};

struct TextGlyph
{
    unsigned int texture; // GL name of the page holding this cell
    float u0, v0, u1, v1;
    int wide, tall; // cell size in pixels
    int bearing_x, bearing_y; // pen origin to cell corner
    int advance;
    bool renderable; // false when the glyph has no ink (a space) or no face covers it
};

class TextRenderInterface
{
public:
    virtual ~TextRenderInterface() = default;

    // Memoized: repeated calls with the same descriptor return the same handle. Fonts are never
    // released, so a caller that picks a size per frame must not create one per frame.
    virtual TextFont_t FindOrCreateFont(const TextFontDesc& desc) = 0;

    // Two-phase creation, matching how a scheme builds a font: an empty handle first, then one
    // glyph set per face. The first set fixes the size and weight of the font.
    virtual TextFont_t CreateEmptyFont() = 0;
    virtual bool AddGlyphSet(TextFont_t font, const char* face, int tall, int weight, int flags) = 0;

    // Appends a face to the fallback chain. GetGlyph walks the chain in insertion order and takes
    // the first face that covers the codepoint.
    virtual bool AddFallbackFace(TextFont_t font, const char* face) = 0;

    // Drops the chain so it can be rebuilt; the handle stays valid.
    virtual void ResetFaces(TextFont_t font) = 0;

    // nullptr means a transient failure (the atlas had no room) and the caller should retry on a
    // later frame. A cell with renderable == false is the permanent answer for a codepoint the
    // chain does not cover.
    virtual const TextGlyph* GetGlyph(TextFont_t font, uint32_t codepoint) = 0;

    virtual int GetLineHeight(TextFont_t font) = 0;
    virtual int GetAscent(TextFont_t font) = 0;

    // Metrics in the shape the vgui surface asks for them. GetGlyphAscent reports the ascent of
    // the face that actually covers the codepoint, which is not the primary face for a fallback.
    virtual void GetCharABCwide(TextFont_t font, uint32_t codepoint, int* a, int* b, int* c) = 0;
    virtual int GetCharacterWidth(TextFont_t font, uint32_t codepoint) = 0;
    virtual void GetTextSize(TextFont_t font, const wchar_t* text, int* wide, int* tall) = 0;
    virtual int GetGlyphAscent(TextFont_t font, uint32_t codepoint) = 0;

    // Last-resort face for text the shipped faces do not cover, chosen from the system language.
    virtual const char* GetForeignFallbackFace() = 0;

    // Changes when cells are repacked, invalidating every texture coordinate handed out before it.
    // Callers caching a laid-out run must carry it and re-shape when it moves.
    virtual uint32_t Generation() = 0;

    // Reads back an existing font's creation descriptor so a caller can build a related font (e.g. a
    // slightly smaller variant of a vgui scheme font, whose HFont is the same handle as a TextFont_t
    // in this service). The face name is copied into face_out; tall/weight/flags each receive the
    // stored value when non-null. Returns false for an invalid handle.
    virtual bool GetFontDesc(TextFont_t font, char* face_out, int face_out_len, int* tall, int* weight, int* flags) = 0;
};

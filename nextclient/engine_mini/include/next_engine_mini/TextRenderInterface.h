#pragma once

#include <cstdint>

typedef int TextFont_t;
constexpr TextFont_t kInvalidTextFont = 0;

enum TextFontFlag
{
    kTextFontNone = 0,
    kTextFontDropShadow = 1 << 0,
    // Mirrors the engine's per-font FONTFLAG_ANTIALIAS; off means crisp bitmaps.
    kTextFontAntialias = 1 << 1,
};

// LUMINANCE_ALPHA pages: alpha is coverage, luminance the fill GL_MODULATE tints; shadow texels are 0.
enum class TextGlyphFormat
{
    Coverage,
};

struct TextFontDesc
{
    const char* face;
    int tall; // pixels
    int weight;
    int flags;
    TextGlyphFormat format;
};

struct TextGlyph
{
    unsigned int texture; // GL name of the atlas page
    float u0, v0, u1, v1;
    int wide, tall; // pixels
    int bearing_x, bearing_y; // pen origin to cell corner
    int advance;
    bool renderable;
};

class TextRenderInterface
{
public:
    virtual ~TextRenderInterface() = default;

    // Memoized, and fonts are never released: a caller picking a size per frame must not create one.
    virtual TextFont_t FindOrCreateFont(const TextFontDesc& desc) = 0;

    // The first glyph set fixes the font's size and weight; a set added after the font has drawn
    // starts the chain over, so a scheme reload rebuilds on the same handle.
    virtual TextFont_t CreateEmptyFont() = 0;
    virtual bool AddGlyphSet(TextFont_t font, const char* face, int tall, int weight, int flags) = 0;

    // GetGlyph walks the chain in insertion order and takes the first face covering the codepoint.
    virtual bool AddFallbackFace(TextFont_t font, const char* face) = 0;

    // nullptr is transient (no atlas room), retry on a later frame; renderable == false is the
    // permanent answer for a codepoint with no ink or no covering face.
    virtual const TextGlyph* GetGlyph(TextFont_t font, uint32_t codepoint) = 0;

    virtual int GetLineHeight(TextFont_t font) = 0;
    virtual int GetAscent(TextFont_t font) = 0;

    // GetGlyphAscent reports the ascent of the face covering the codepoint, not of the primary face.
    virtual void GetCharABCwide(TextFont_t font, uint32_t codepoint, int* a, int* b, int* c) = 0;
    virtual int GetCharacterWidth(TextFont_t font, uint32_t codepoint) = 0;
    virtual void GetTextSize(TextFont_t font, const wchar_t* text, int* wide, int* tall) = 0;
    virtual int GetGlyphAscent(TextFont_t font, uint32_t codepoint) = 0;

    // Changes when cells are repacked; a cached laid-out run must carry it and re-shape when it moves.
    virtual uint32_t Generation() = 0;

    // face_out is truncated to face_out_len and always terminated; tall/weight/flags are optional.
    virtual bool GetFontDesc(TextFont_t font, char* face_out, int face_out_len, int* tall, int* weight, int* flags) = 0;
};

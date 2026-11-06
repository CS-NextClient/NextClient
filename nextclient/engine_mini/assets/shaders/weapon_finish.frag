#version 330 compatibility

#include "material_coverage.glsl"
#include "material_glitch_surface.glsl"
#include "studio_fog.glsl"

// Layered composition over the model's own skin. Nothing here is named after a game
// concept: the program sees a base layer, a pattern layer, a layer that shows through as
// the surface wears, a threshold map that decides where it shows through first, and a
// sphere map for the reflective response. What those mean is the material's business.

in vec2 vUv;
in vec3 vColor;
in vec2 vSphereUv;
in vec3 vNormal;
in vec3 vToView;

// How much light reaches this entity: x the direct part, y all of it. A highlight must not
// obey the diffuse cosine, or it disappears on every surface that faces away from the
// light, but it does have to obey this - a weapon standing in shadow has nothing to catch.
in vec2 vLightLevel;

uniform sampler2D uTexture;   // the model's own skin, always on unit 0
uniform float uBlend;         // the entity's translucency, folded into the output alpha

uniform sampler2D uPattern;     // material artwork
uniform sampler2D uUnderlay;    // shows through where the surface has worn away
uniform sampler2D uWearMap;     // per texel threshold at which the underlay appears
uniform sampler2D uSphereMap;   // reflection, sampled by the view derived sphere map
uniform sampler2D uReflectMask; // per texel, how much of the surface reflects at all

uniform vec4 uBaseTint;       // multiplies the base layer
uniform vec4 uPatternTint;    // multiplies the pattern layer
uniform vec4 uUnderlayTint;   // multiplies what the wear exposes
uniform vec4 uReflectTint;    // multiplies both kinds of highlight
uniform float uLayerMix;      // 0 keeps the base, 1 keeps the pattern

// xy tiles a layer over the skin's own coordinates, zw shifts it. Packed into one vec4
// because the material format carries scalars, vec4s and textures and nothing in between;
// a two component type would be a type added for one shader's convenience.
//
// One per layer sampled in the model's coordinates, because a supplied image has a density
// of its own and the model's UV density varies between weapons by several times: without
// this the same image is coarse on one and fine on another and the author cannot correct
// it. uSphereMap gets none: it is sampled by the view derived coordinates, where a tiling
// factor means nothing.
uniform vec4 uPatternTransform;
uniform vec4 uWearMapTransform;
uniform vec4 uUnderlayTransform;
uniform vec4 uReflectMaskTransform;

uniform float uWear;          // 0 untouched, 1 worn through everywhere
uniform float uWearSoftness;  // width of the transition around the threshold
uniform float uReflectivity;  // how much of the sphere map the underlay picks up

uniform float uSpecular;      // strength of the highlight the scene's own light throws
uniform float uGloss;         // how tight that highlight is
uniform float uReflectWear;   // how much the highlight follows what the wear has bared

// The direction the engine derived for the light falling on this entity: outdoors that is
// the map's sun. It points the way the light travels, so the direction towards it is its
// negation - the vertex stage's own shading is brightest where this dot goes negative.
uniform vec3 uLightVec;

// The color of that light, so a highlight under colored lighting is that color rather
// than white.
uniform vec3 uColorMix;

uniform int uShadeMode;

out vec4 oColor;

void main()
{
    bool visible = MaterialCoverage_Evaluate(vUv);
    vec4 base = MaterialGlitch_Sample(uTexture, vUv) * uBaseTint;
    vec4 pattern = texture(uPattern, vUv * uPatternTransform.xy + uPatternTransform.zw) * uPatternTint;

    // The pattern's own alpha gates it as well as the mix amount, so artwork can cover
    // part of the skin without needing a separate mask.
    float amount = clamp(uLayerMix, 0.0, 1.0) * pattern.a;

    vec3 finish = mix(base.rgb, pattern.rgb, amount);

    // Kept out of the paint and added after the lighting, because that is what a
    // reflection is. Folded into the diffuse term it gets multiplied by the vertex color
    // and disappears in exactly the places a reflection is meant to be seen: a weapon in a
    // dark corner has no diffuse light to carry it.
    vec3 highlight = vec3(0.0);

    // How much of this texel has worn through. Hoisted out of the block below because the
    // highlight can be told to follow it: bare metal is usually shinier than the coat that
    // was over it, and a material that wants that should not have to author a second mask
    // that duplicates the wear it already described.
    float worn = 0.0;

    // Guarded rather than sampled unconditionally: a material that asks for no wear
    // supplies no wear map either, and a sampler nobody bound reads texture unit 0,
    // which is the model's own skin. A program that offers optional layers has to not
    // read them when the value that switches them on is off.
    if (uWear > 0.0)
    {
        // A threshold map rather than a second alpha: it says where the surface gives way
        // first, so one scalar walks the whole surface from untouched to worn through,
        // and an author controls the order rather than only the amount.
        float threshold = texture(uWearMap, vUv * uWearMapTransform.xy + uWearMapTransform.zw).r;
        float softness = max(uWearSoftness, 0.001);
        worn = smoothstep(threshold - softness, threshold + softness, uWear);

        vec3 underlay = texture(uUnderlay, vUv * uUnderlayTransform.xy + uUnderlayTransform.zw).rgb;
        underlay *= uUnderlayTint.rgb;

        finish = mix(finish, underlay, worn);
    }

    // Which parts of the surface reflect at all, on its own map rather than tied to the
    // wear: what is polished and what is matte is a property of the weapon, not of how far
    // it has worn down. Sampled only when something reflects, for the same reason the wear
    // map is - a material that asks for no highlight supplies no mask either.
    if (uReflectivity > 0.0 || uSpecular > 0.0)
    {
        float reflective = texture(
            uReflectMask,
            vUv * uReflectMaskTransform.xy + uReflectMaskTransform.zw
        ).r;

        // Optionally follows the wear on top of the mask. Positive sends the shine to what
        // the wear has bared, which is the usual way round: paint is dull and the metal
        // under it is not. Negative sends it the other way, for a lacquer that has worn to
        // something matte. Zero, the default, leaves the mask to decide alone.
        //
        // A parameter rather than a fixed coupling, and not something an author can get by
        // pointing the mask at the wear map instead: the wear map is a threshold compared
        // against how far the wear has gone, so using it directly would shine along the
        // order in which the surface gives way rather than on the part already bare.
        if (uReflectWear != 0.0)
        {
            float follows = uReflectWear > 0.0 ? worn : 1.0 - worn;

            reflective *= mix(1.0, follows, abs(uReflectWear));
        }

        // Scaled by all the light in the place: a matcap is authored as a lit surround, and
        // a surround that is not lit reflects nothing.
        highlight += texture(uSphereMap, vSphereUv).rgb * uReflectivity * (uColorMix * vLightLevel.y);

        // A real highlight from the scene's own light, rather than one baked into an image:
        // this is what makes a finish read as metal when the weapon turns under the sun.
        // Scaled by the direct light alone, so it goes out in shadow where there is no sun
        // to catch, and left out of the modes that carry no lighting to reflect.
        if (uSpecular > 0.0 && (uShadeMode == kShadeLit || uShadeMode == kShadeFlat))
        {
            vec3 normal = normalize(vNormal);
            vec3 to_view = normalize(vToView);
            vec3 to_light = normalize(-uLightVec);

            float facing = max(dot(normal, normalize(to_light + to_view)), 0.0);

            highlight += uColorMix * vLightLevel.x * pow(facing, max(uGloss, 1.0)) * uSpecular;
        }

        highlight *= reflective * uReflectTint.rgb;
    }

    if (!visible)
    {
        discard;
    }

    oColor = Studio_ApplyFog(vec4(finish * vColor + highlight, base.a * uBlend));
}

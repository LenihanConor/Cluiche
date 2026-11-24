$input v_texcoord1, v_texcoord2, v_color0

// CellField fragment shader. Uniform / sampler / varying layout (renderer must match exactly):
//
//   Uniform           Type  Contents
//   u_params0         vec4  (rampMin, rampMax, inset, cellSize)
//   u_params1         vec4  (hillshadeStrength, elevationScale, mapMode, hasElevation)
//                           mapMode: 0 = palette, 1 = ramp, 2 = solid backdrop (outputs u_backdropColour)
//   u_params2         vec4  (smooth 0/1, ambient, unused, unused)
//   u_gridInfo        vec4  (width, height, 1/width, 1/height)
//   u_fallbackColour  vec4  palette fallback for ids < 0, >= 64, non-finite/none
//   u_backdropColour  vec4  mapMode 2 colour
//   u_lightDir        vec4  xyz = unit vector pointing toward the light (from azimuth/elevation)
//
//   Sampler (stage)          Texture
//   u_paletteOrRamp (0)      64x1 RGBA8 palette (point) or 256x1 RGBA8 ramp (linear), clamped
//   u_valueTexture (1)       R32F width x height, point, clamp; texel (x,y) = cell y*width+x
//   u_elevationTexture (2)   R32F same grid, point, clamp (only read when hasElevation > 0.5)
//
// Hillshade: the elevation is blended through the same four-cell weights as smooth shading.
// Normal = cross(dFdx(P), dFdy(P)), P = (world.xy, elevation*elevationScale). Because it is
// built from ratios of height to world-position derivatives it is independent of zoom.
// Backdrop: draw any geometry with the same program and mapMode = 2.

#include <bgfx_shader.sh>

uniform vec4 u_params0;
uniform vec4 u_params1;
uniform vec4 u_params2;
uniform vec4 u_gridInfo;
uniform vec4 u_fallbackColour;
uniform vec4 u_backdropColour;
uniform vec4 u_lightDir;

SAMPLER2D(u_paletteOrRamp, 0);
SAMPLER2D(u_valueTexture, 1);
SAMPLER2D(u_elevationTexture, 2);

float fetchCell(sampler2D tex, float idx)
{
    float i = floor(idx + 0.5);
    float w = u_gridInfo.x;
    float y = floor(i / w);
    float x = i - y * w;
    vec2 uv = vec2((x + 0.5) * u_gridInfo.z, (y + 0.5) * u_gridInfo.w);
    return texture2DLod(tex, uv, 0.0).x;
}

vec4 paletteColour(float id)
{
    float i = floor(id + 0.5);
    if (i < 0.0 || i > 63.0)
    {
        return u_fallbackColour;
    }
    return texture2DLod(u_paletteOrRamp, vec2((i + 0.5) / 64.0, 0.5), 0.0);
}

vec4 rampColour(float v)
{
    float range = u_params0.y - u_params0.x;
    float t = clamp((v - u_params0.x) / (abs(range) > 1e-20 ? range : 1.0), 0.0, 1.0);
    return texture2DLod(u_paletteOrRamp, vec2((t * 255.0 + 0.5) / 256.0, 0.5), 0.0);
}

void main()
{
    float mapMode = u_params1.z;
    if (mapMode > 1.5)
    {
        gl_FragColor = u_backdropColour;
        return;
    }

    vec4 idx = v_texcoord1;
    vec4 w   = v_color0;
    bool isSmooth = u_params2.x > 0.5;

    // Flat: own cell only. Smooth: weights across the four cells; -1 (none) is never sampled.
    bool use0 = idx.x > -0.5;
    bool use1 = isSmooth && idx.y > -0.5 && w.y > 0.0;
    bool use2 = isSmooth && idx.z > -0.5 && w.z > 0.0;
    bool use3 = isSmooth && idx.w > -0.5 && w.w > 0.0;

    float w0 = isSmooth ? w.x : 1.0;
    float w1 = use1 ? w.y : 0.0;
    float w2 = use2 ? w.z : 0.0;
    float w3 = use3 ? w.w : 0.0;
    float wsum = (use0 ? w0 : 0.0) + w1 + w2 + w3;
    float inv = wsum > 1e-6 ? 1.0 / wsum : 0.0;

    vec4 colour = u_fallbackColour;
    if (!use0 && wsum <= 1e-6)
    {
        colour = u_fallbackColour;
    }
    else if (mapMode < 0.5)
    {
        vec4 c = vec4(0.0, 0.0, 0.0, 0.0);
        if (use0) { c += paletteColour(fetchCell(u_valueTexture, idx.x)) * (w0 * inv); }
        if (use1) { c += paletteColour(fetchCell(u_valueTexture, idx.y)) * (w1 * inv); }
        if (use2) { c += paletteColour(fetchCell(u_valueTexture, idx.z)) * (w2 * inv); }
        if (use3) { c += paletteColour(fetchCell(u_valueTexture, idx.w)) * (w3 * inv); }
        colour = c;
    }
    else
    {
        float v = 0.0;
        if (use0) { v += fetchCell(u_valueTexture, idx.x) * (w0 * inv); }
        if (use1) { v += fetchCell(u_valueTexture, idx.y) * (w1 * inv); }
        if (use2) { v += fetchCell(u_valueTexture, idx.z) * (w2 * inv); }
        if (use3) { v += fetchCell(u_valueTexture, idx.w) * (w3 * inv); }
        colour = rampColour(v);
    }

    float strength = u_params1.x;
    bool shade = strength > 0.0 && u_params1.w > 0.5;

    // Derivatives must be evaluated in uniform control flow (before any divergent use).
    // Elevation is ALWAYS blended through the four-cell weights, even when colour shading is Flat:
    // with own-cell-only elevation the derivatives are zero inside a cell and the relief is lost.
    float eh = 0.0;
    float ew = 0.0;
    if (idx.x > -0.5)               { eh += fetchCell(u_elevationTexture, idx.x) * w.x; ew += w.x; }
    if (idx.y > -0.5 && w.y > 0.0)  { eh += fetchCell(u_elevationTexture, idx.y) * w.y; ew += w.y; }
    if (idx.z > -0.5 && w.z > 0.0)  { eh += fetchCell(u_elevationTexture, idx.z) * w.z; ew += w.z; }
    if (idx.w > -0.5 && w.w > 0.0)  { eh += fetchCell(u_elevationTexture, idx.w) * w.w; ew += w.w; }
    float h = ew > 1e-6 ? eh / ew : 0.0;
    vec3 P = vec3(v_texcoord2.xy, h * u_params1.y);
    vec3 n = cross(dFdx(P), dFdy(P));
    float nl = length(n);
    n = nl > 1e-20 ? n / nl : vec3(0.0, 0.0, 1.0);
    if (n.z < 0.0) { n = -n; }

    if (shade)
    {
        float lit = u_params2.y + strength * max(0.0, dot(n, u_lightDir.xyz));
        colour.xyz *= lit;
    }

    gl_FragColor = colour;
}

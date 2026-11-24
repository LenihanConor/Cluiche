$input a_texcoord0, a_texcoord1, a_texcoord2, a_color0
$output v_texcoord1, v_texcoord2, v_color0

// CellField vertex shader. Layout must match VertexCellField (36 bytes):
//   a_texcoord0 (2 x float)  : cell centre (world)
//   a_texcoord1 (2 x float)  : unit-hex corner offset from the centre
//   a_texcoord2 (4 x float)  : cell indices [own, N(k-1), N(k), N(k+1)], -1 = none
//   a_color0    (4 x uint8, normalised) : blend weights (sum to 1)
//
// Uniform layout (shared with fs_cellfield.sc; vec4 uniforms):
//   u_params0 = (rampMin, rampMax, inset, cellSize)             [VS uses inset, cellSize]
//   u_params1 = (hillshadeStrength, elevationScale, mapMode, hasElevation)
//               mapMode: 0 = palette, 1 = ramp, 2 = solid backdrop colour
//   u_params2 = (smooth 0/1, ambient, unused, unused)
//   u_gridInfo = (width, height, 1/width, 1/height)
//   u_fallbackColour, u_backdropColour, u_lightDir (xyz = unit vector toward the light)
// Samplers: u_paletteOrRamp = 0, u_valueTexture = 1, u_elevationTexture = 2
//
// Varyings:
//   v_texcoord1 = cellIndices (identical on all 3 vertices of a triangle; interpolation is safe,
//                 the fragment shader rounds)
//   v_texcoord2 = (world.x, world.y, 0, 0)
//   v_color0    = weights

#include <bgfx_shader.sh>

uniform vec4 u_params0;

void main()
{
    float inset    = u_params0.z;
    float cellSize = u_params0.w;
    vec2 world = a_texcoord0 + a_texcoord1 * (cellSize * (1.0 - inset));

    gl_Position = mul(u_modelViewProj, vec4(world, 0.0, 1.0));
    v_texcoord1 = a_texcoord2;
    v_texcoord2 = vec4(world, 0.0, 0.0);
    v_color0    = a_color0;
}

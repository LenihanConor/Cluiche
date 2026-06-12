$input a_position, a_normal, a_texcoord0, a_color0, a_indices, a_weight
$output v_worldPos, v_normal, v_texcoord0, v_color0, v_shadowCoord

#include <bgfx_shader.sh>

uniform mat4 u_lightViewProj;
uniform vec4 u_skinningPalette[256*3]; // mat3x4[256] packed as 3 vec4 per bone

void main()
{
    // Blend 4 bone influences
    ivec4 indices = a_indices;
    vec4  weights = a_weight;

    vec4 skinnedPos = vec4(0.0, 0.0, 0.0, 0.0);
    vec3 skinnedNrm = vec3(0.0, 0.0, 0.0);

    for (int i = 0; i < 4; ++i)
    {
        float w = weights[i];
        if (w <= 0.0) continue;

        int idx = indices[i] * 3;
        vec4 row0 = u_skinningPalette[idx + 0];
        vec4 row1 = u_skinningPalette[idx + 1];
        vec4 row2 = u_skinningPalette[idx + 2];

        // mat3x4: rows are (x,y,z,tx), (x,y,z,ty), (x,y,z,tz)
        vec4 pos4 = vec4(a_position, 1.0);
        skinnedPos.x += w * dot(row0, pos4);
        skinnedPos.y += w * dot(row1, pos4);
        skinnedPos.z += w * dot(row2, pos4);

        vec3 nrm3 = a_normal;
        skinnedNrm.x += w * dot(row0.xyz, nrm3);
        skinnedNrm.y += w * dot(row1.xyz, nrm3);
        skinnedNrm.z += w * dot(row2.xyz, nrm3);
    }
    skinnedPos.w = 1.0;

    vec4 worldPos = mul(u_model[0], skinnedPos);
    gl_Position   = mul(u_viewProj, worldPos);

    v_worldPos    = worldPos.xyz;
    v_normal      = normalize(mul(u_model[0], vec4(skinnedNrm, 0.0)).xyz);
    v_texcoord0   = a_texcoord0;
    v_color0      = a_color0;
    v_shadowCoord = mul(u_lightViewProj, worldPos);
}

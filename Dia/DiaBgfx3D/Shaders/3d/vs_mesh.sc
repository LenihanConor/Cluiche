$input a_position, a_normal, a_texcoord0, a_color0
$output v_worldPos, v_normal, v_texcoord0, v_color0, v_shadowCoord

#include <bgfx_shader.sh>

uniform mat4 u_lightViewProj;

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0));
    gl_Position   = mul(u_viewProj, worldPos);

    v_worldPos    = worldPos.xyz;
    v_normal      = normalize(mul(u_model[0], vec4(a_normal, 0.0)).xyz);
    v_texcoord0   = a_texcoord0;
    v_color0      = a_color0;
    v_shadowCoord = mul(u_lightViewProj, worldPos);
}

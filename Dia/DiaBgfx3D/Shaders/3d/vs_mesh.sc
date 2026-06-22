$input a_position, a_normal, a_tangent, a_texcoord0, a_color0
$output v_worldPos, v_normal, v_texcoord0, v_color0, v_shadowCoord, v_tangent, v_bitangent

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

    vec3 worldTangent  = normalize(mul(u_model[0], vec4(a_tangent.xyz, 0.0)).xyz);
    float bitangentSign = a_tangent.w;
    v_tangent   = worldTangent;
    v_bitangent = cross(v_normal, worldTangent) * bitangentSign;
}

$input v_worldPos, v_normal, v_texcoord0, v_color0, v_shadowCoord, v_tangent, v_bitangent

#include <bgfx_shader.sh>

uniform vec4 u_directionalLightDir;    // xyz = normalized direction (toward light)
uniform vec4 u_directionalLightColour; // rgb = colour, a = intensity
uniform vec4 u_ambient;                // rgb = ambient colour, a = intensity
uniform vec4 u_baseColour;             // rgba from MaterialDescriptor

SAMPLER2D(s_albedo,    0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_shadowMap, 2);

float sampleShadow(vec4 shadowCoord)
{
    vec3 projCoords = shadowCoord.xyz / shadowCoord.w;
    projCoords = projCoords * 0.5 + 0.5;

    if (projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0 ||
        projCoords.z > 1.0)
    {
        return 1.0; // outside shadow map — fully lit
    }

    float closestDepth = texture2D(s_shadowMap, projCoords.xy).r;
    float currentDepth = projCoords.z;
    float bias = 0.005;
    return (currentDepth - bias > closestDepth) ? 0.3 : 1.0;
}

void main()
{
    vec3 T = normalize(v_tangent);
    vec3 B = normalize(v_bitangent);
    vec3 N = normalize(v_normal);
    vec4 normalSample = texture2D(s_normalMap, v_texcoord0);
    vec3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    vec3 normal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);
    vec3 lightDir = normalize(u_directionalLightDir.xyz);

    // Lambert diffuse
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = u_directionalLightColour.rgb * u_directionalLightColour.a * NdotL;

    // Ambient
    vec3 ambient = u_ambient.rgb * u_ambient.a;

    // Shadow
    float shadow = sampleShadow(v_shadowCoord);

    // Final colour
    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec3 baseCol = u_baseColour.rgb * v_color0.rgb * albedoSample.rgb;
    vec3 finalColour = baseCol * (ambient + diffuse * shadow);
    float alpha = u_baseColour.a * v_color0.a * albedoSample.a;

    gl_FragColor = vec4(finalColour, alpha);
}

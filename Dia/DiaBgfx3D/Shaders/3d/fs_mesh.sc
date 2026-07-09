$input v_worldPos, v_normal, v_texcoord0, v_color0, v_shadowCoord, v_tangent, v_bitangent

#include <bgfx_shader.sh>

uniform vec4 u_directionalLightDir;    // xyz = normalized direction (toward light)
uniform vec4 u_directionalLightColour; // rgb = colour, a = intensity
uniform vec4 u_ambient;                // rgb = ambient colour, a = intensity
uniform vec4 u_baseColour;             // rgba from MaterialDescriptor
uniform vec4 u_cameraPos;             // xyz = camera world position
uniform vec4 u_pbrParams;             // x = metallic fallback, y = roughness fallback

SAMPLER2D(s_albedo,    0);
SAMPLER2D(s_normalMap, 1);
SAMPLER2D(s_shadowMap, 2);
SAMPLER2D(s_orm,       3);

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

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = NdotH2 * (a2 - 1.0) + 1.0;
    denom = 3.14159265 * denom * denom;

    return a2 / max(denom, 0.0000001);
}

float GeometrySchlickGGX(float NdotX, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotX / (NdotX * (1.0 - k) + k);
}

float GeometrySmith(float NdotV, float NdotL, float roughness)
{
    float ggx1 = GeometrySchlickGGX(NdotV, roughness);
    float ggx2 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (vec3(1.0, 1.0, 1.0) - F0) * pow(max(1.0 - cosTheta, 0.0), 5.0);
}

void main()
{
    // TBN normal mapping
    vec3 T = normalize(v_tangent);
    vec3 B = normalize(v_bitangent);
    vec3 N = normalize(v_normal);
    vec4 normalSample = texture2D(s_normalMap, v_texcoord0);
    vec3 tangentNormal = normalSample.rgb * 2.0 - 1.0;
    vec3 normal = normalize(T * tangentNormal.x + B * tangentNormal.y + N * tangentNormal.z);

    // ORM texture sampling
    vec4 ormSample = texture2D(s_orm, v_texcoord0);
    float roughness = ormSample.g > 0.0 ? ormSample.g : u_pbrParams.y;
    float metallic = ormSample.b > 0.0 ? ormSample.b : u_pbrParams.x;

    // Albedo and base colour
    vec4 albedoSample = texture2D(s_albedo, v_texcoord0);
    vec3 baseCol = u_baseColour.rgb * v_color0.rgb * albedoSample.rgb;

    // F0 reflectance at normal incidence
    vec3 F0 = mix(vec3(0.04, 0.04, 0.04), baseCol, metallic);

    // Vectors
    vec3 V = normalize(u_cameraPos.xyz - v_worldPos.xyz);
    vec3 L = normalize(-u_directionalLightDir.xyz);
    vec3 H = normalize(V + L);

    // Dot products (clamped to avoid divide-by-zero)
    float NdotL = max(dot(normal, L), 0.001);
    float NdotV = max(dot(normal, V), 0.001);

    // Cook-Torrance BRDF
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
    float D = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(NdotV, NdotL, roughness);

    vec3 specular = (D * F * G) / max(4.0 * NdotV * NdotL, 0.001);

    // Diffuse (energy-conserving Lambertian)
    vec3 diffuse = (vec3(1.0, 1.0, 1.0) - F) * (1.0 - metallic) * baseCol / 3.14159265;

    // Light radiance
    vec3 lightRadiance = u_directionalLightColour.rgb * u_directionalLightColour.a * NdotL;

    // Ambient
    vec3 ambient = u_ambient.rgb * u_ambient.a * baseCol;

    // Shadow
    float shadow = sampleShadow(v_shadowCoord);

    // Final composition
    vec3 finalColour = ambient + (diffuse + specular) * lightRadiance * shadow;
    float alpha = u_baseColour.a * v_color0.a * albedoSample.a;

    gl_FragColor = vec4(finalColour, alpha);
}

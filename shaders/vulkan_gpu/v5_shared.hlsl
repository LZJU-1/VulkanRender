#ifndef V5_SHARED_HLSL
#define V5_SHARED_HLSL

static const float kPi = 3.14159265;

struct Surface {
    float3 worldPos;
    float3 normal;
    float3 base;
    float roughness;
    float metalness;
    float materialKind;
};

struct SceneLight {
    float4 positionRadius;
    float4 colorIntensity;
};

float3 skyRadiance(float3 dir) {
    dir = normalize(dir);
    float horizon = saturate(dir.z * 0.5 + 0.5);
    float3 ground = float3(0.18, 0.17, 0.15);
    float3 lowSky = float3(1.04, 0.82, 0.56);
    float3 highSky = float3(0.18, 0.38, 0.78);
    float sun = pow(saturate(dot(dir, normalize(float3(-0.35, -0.48, 0.80)))), 360.0);
    return lerp(ground, lerp(lowSky, highSky, horizon), horizon) + sun.xxx * float3(7.0, 5.5, 3.4);
}

float3 toneMap(float3 color) {
    color = max(color, 0.0.xxx);
    color = color / (color + 1.0.xxx);
    return pow(saturate(color), 1.0 / 2.2);
}

float3 fresnelSchlick(float cosTheta, float3 f0) {
    return f0 + (1.0.xxx - f0) * pow(1.0 - saturate(cosTheta), 5.0);
}

float3 pbrDirectional(Surface surface, float3 view, float3 lightDir, float3 lightColor, float lightIntensity, float visibility) {
    float ndotl = saturate(dot(surface.normal, lightDir));
    float ndotv = max(0.04, saturate(dot(surface.normal, view)));
    if (ndotl <= 0.0) {
        return 0.0.xxx;
    }

    float3 halfVector = normalize(lightDir + view);
    float ndoth = saturate(dot(surface.normal, halfVector));
    float alpha = max(0.035, surface.roughness * surface.roughness);
    float alpha2 = alpha * alpha;
    float denom = ndoth * ndoth * (alpha2 - 1.0) + 1.0;
    float distribution = alpha2 / max(0.001, kPi * denom * denom);
    float k = (surface.roughness + 1.0) * (surface.roughness + 1.0) * 0.125;
    float geometry = ndotl / max(0.001, ndotl * (1.0 - k) + k);
    geometry *= ndotv / max(0.001, ndotv * (1.0 - k) + k);
    float3 f0 = lerp(0.04.xxx, surface.base, surface.metalness);
    float3 fresnel = fresnelSchlick(saturate(dot(halfVector, view)), f0);
    float3 specular = distribution * geometry * fresnel / max(0.001, 4.0 * ndotl * ndotv);
    float3 diffuse = surface.base * (1.0 - surface.metalness) * (1.0.xxx - fresnel) / kPi;
    return lightColor * lightIntensity * visibility * ndotl * (diffuse + specular);
}

float3 importedLightsContribution(Surface surface, float3 view, StructuredBuffer<SceneLight> sceneLights, float4 v4Flags) {
    uint lightCount = min((uint)max(0.0, v4Flags.y), 128u);
    float3 radiance = 0.0.xxx;
    [loop]
    for (uint i = 0; i < lightCount; ++i) {
        SceneLight light = sceneLights[i];
        float3 toLight = light.positionRadius.xyz - surface.worldPos;
        float distanceToLight = length(toLight);
        float radius = max(0.25, light.positionRadius.w);
        float3 lightDir = toLight / max(distanceToLight, 0.001);
        if (distanceToLight >= radius || dot(surface.normal, lightDir) <= 0.0) {
            continue;
        }
        float falloff = saturate(1.0 - distanceToLight / radius);
        falloff = falloff * falloff * (3.0 - 2.0 * falloff);
        radiance += pbrDirectional(surface, view, lightDir, saturate(light.colorIntensity.rgb), light.colorIntensity.w * falloff, 1.0);
    }
    return radiance;
}

#endif

//#pragma pack_matrix(row_major)

Texture2D t_diffuse : register(t0);
Texture2D t_specular : register(t1);
Texture2D t_emissive : register(t2);
Texture2D t_normal : register(t3);
SamplerState t_sampler : register(s0);

struct OutputVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
    float3 worldposition : WORDLPOS;
    float3 cameraposition : CAMPOS;
};

struct Light
{
    float4 position, lightDirection;
    float4 ambientUp, ambientDown, diffuse, specular;
    unsigned int lightType;
    float lightRadius;
    float cosineInnerCone, cosineOuterCone;
    float ambientIntensityUp, ambientIntensityDown, diffuseIntensity, specularIntensity;
    float lightLength, p1, p2, p3;
};

cbuffer LIGHTS
{
    Light lights[3];
};
#define SPEC_EXP 100
#define PI 3.14159265359

float3 CalcAmbient(Light light, float3 normal, float3 color)
{
    // Convert from [-1, 1] to [0, 1]
    float up = normal.y * 0.5 + 0.5;
    // Calculate the ambient value
    float3 Ambient = (lights[0].ambientDown.xyz * light.ambientIntensityDown) + up * (light.ambientUp.xyz * light.ambientIntensityUp);

    // Apply the ambient value to the color
    return Ambient * color;
}

float4 computePointLight(Light plight, OutputVertex input)
{
    // Texture color
    float4 color = saturate(t_diffuse.Sample(t_sampler, input.tex));
    float3 normal = normalize(input.normal);
    
    // Ambient color
    float3 ambientColor = CalcAmbient(plight, normal, color.xyz);
    
    // Diffuse color
    float3 lightDirection = (plight.position.xyz - input.worldposition);
    float diffuseFactor = saturate(dot(normal, lightDirection));
    float3 diffuseColor = saturate(diffuseFactor * plight.diffuseIntensity * plight.diffuse);
    
    // Specular color
    // The view direction needs to be added to the positive light direction here
    float3 viewDirection = normalize(input.cameraposition - input.worldposition);
    float3 halfVector = normalize(lightDirection + viewDirection);
    float specularFactor = pow(max(dot(normal, halfVector), 0), SPEC_EXP);
    float3 specularColor = saturate(specularFactor * plight.specularIntensity * t_specular.Sample(t_sampler, input.tex).rgb);
    
    float3 emissiveColor = t_emissive.Sample(t_sampler, input.tex).rgb;
    
    // Attenuation
    float distance = length(plight.position.xyz - input.worldposition);
    float attenuation = 1.0 - saturate(distance / plight.lightRadius);
    attenuation *= attenuation;
    
    // Apply attenuation
    diffuseColor = saturate(diffuseColor * attenuation);
    specularColor = saturate(specularColor * attenuation);
    
    // Combine phong components
    color = saturate(float4((diffuseColor + specularColor + ambientColor + emissiveColor) * color.rgb, color.a));
    return color;
}

float4 main(OutputVertex input) : SV_TARGET
{
    return computePointLight(lights[0], input);
    return t_diffuse.Sample(t_sampler, input.tex);
}
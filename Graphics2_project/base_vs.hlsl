
// Use row major matrices
#pragma pack_matrix(row_major)

struct InputVertex
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    int4 joints : JOINTS;
    float4 weights : WEIGHTS;
    float2 tex : TEXCOORD;
};

cbuffer SHADER_VARIABLES : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 cameraPosition;
};

cbuffer JOINT_DATA : register(b1)
{
    float4x4 joints[30];
}

struct OutputVertex
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
    float3 worldposition : WORDLPOS;
    float3 cameraposition : CAMPOS;
};

OutputVertex main(InputVertex input)
{
    OutputVertex output;
    
    output.position = mul(float4(input.position, 1), worldMatrix);
    output.worldposition = output.position.xyz;
    output.position = mul(output.position, viewMatrix);
    output.position = mul(output.position, projectionMatrix);
    
    output.normal = mul(float4(input.normal, 0), worldMatrix).xyz;
    output.tex = input.tex;
    
    output.cameraposition = cameraPosition;
    
    bool APPLY_SKINNED_ANIMATION = true;
    
    if (APPLY_SKINNED_ANIMATION)
    {
        // Skinned position
        float4 skinned_position = float4(0, 0, 0, 0);
        float4 skinned_normal = float4(0, 0, 0, 0);
        
        int miss_count = 0;
    
        for (int i = 0; i < 4; i++)
        {
            if (input.joints[i] != -1)
            {
                skinned_position += mul(float4(input.position, 1), joints[input.joints[i]]) * input.weights[i];
                skinned_normal += mul(float4(input.normal, 0), joints[input.joints[i]]) * input.weights[i];
                continue;
            }
            miss_count += 1;
        }
        
        skinned_position.w = 1;
        skinned_normal.w = 0;
    
        // Applymatrices
        skinned_position = mul(skinned_position, worldMatrix);
        skinned_position = mul(skinned_position, viewMatrix);
        skinned_position = mul(skinned_position, projectionMatrix);
    
        skinned_normal = mul(float4(skinned_normal.xyz, 0), worldMatrix);
    
        output.normal = skinned_normal;
        output.position = skinned_position;
    }
    
    return output;
}
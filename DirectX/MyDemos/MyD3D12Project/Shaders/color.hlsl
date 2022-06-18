//***************************************************************************************
// color.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Transforms and colors geometry.
//***************************************************************************************

#include "Common.hlsl"

struct VertexIn
{
	float3 PosL     : POSITION;
    float3 NormalL  : NORMAL;
    float3 TangentL : TANGENT;
    float2 TexC     : TEXCOORD;
#ifdef _PMX_FORMAT
    float4 BoneWeights : WEIGHTS;
    int4 BoneIndices  : BONEINDICES;
#endif
};

struct VertexOut
{
	float4 PosH     : SV_POSITION;
	float4 ShadowPosH : POSITION0;
    float3 PosW     : POSITIONT;
    float3 NormalW  : NORMAL;
	float3 TangentW : TANGENT;
    float2 TexC     : TEXCOORD;
    
    // 각 인스턴스 당 텍스쳐 인덱스가 겹치지 않도록 보간
    nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
	VertexOut vout = (VertexOut) 0.0f;

	InstanceData instData	= gInstanceData[instanceID];

	float4x4 world = instData.World;
    float4x4 texTransform = instData.TexTransform;
    uint matIndex = instData.MaterialIndex;
    vout.MatIndex = matIndex;
    
    MaterialData matData = gMaterialData[matIndex];
    
#ifdef _PMX_FORMAT
    float4 posW = float4(vin.PosL, 1.0f);
    
    if (vin.BoneIndices.y == -1) {
        pmxBoneData boneData =  gPmxBoneData[vin.BoneIndices.x];
        posW = mul(posW, (boneData.gOriginMatrix));
        posW = mul(posW, boneData.gMatrix);
    }
    else if (vin.BoneIndices.z == -1) {
        pmxBoneData boneData =  gPmxBoneData[vin.BoneIndices.x];
        float4 posX = mul(posW, (boneData.gOriginMatrix));
        posX = mul(posX, boneData.gMatrix);
    
        boneData =  gPmxBoneData[vin.BoneIndices.y];
        float4 posY = mul(posW, boneData.gOriginMatrix);
        posY = mul(posY, boneData.gMatrix);
    
        posW = 
            (posX * vin.BoneWeights.x) + 
            (posY * vin.BoneWeights.y);
    }
    else {
        pmxBoneData boneData =  gPmxBoneData[vin.BoneIndices.x];
        float4 posX = mul(posW, (boneData.gOriginMatrix));
        posX = mul(posX, boneData.gMatrix);
    
        boneData =  gPmxBoneData[vin.BoneIndices.y];
        float4 posY = mul(posW, boneData.gOriginMatrix);
        posY = mul(posY, boneData.gMatrix);
    
        boneData =  gPmxBoneData[vin.BoneIndices.z];
        float4 posZ = mul(posW, boneData.gOriginMatrix);
        posZ = mul(posZ, boneData.gMatrix);
    
        boneData =  gPmxBoneData[vin.BoneIndices.w];
        float4 posU = mul(posW, boneData.gOriginMatrix);
        posU = mul(posU, boneData.gMatrix);
    
        posW = 
            (posX * vin.BoneWeights.x) + 
            (posY * vin.BoneWeights.y) + 
            (posZ * vin.BoneWeights.z) + 
            (posU * vin.BoneWeights.w);
    }
    
    posW = mul(posW, world);
    
#else
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    
#endif
    vout.PosW = posW.xyz;
        
    vout.NormalW = mul(vin.NormalL, (float3x3) world);

    vout.PosH = mul(posW, gViewProj);
	vout.ShadowPosH = mul(posW, gShadowViewProjNDC);

    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), texTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy;
    
    return vout;
    
}

float4 PS(VertexOut pin) : SV_Target
{ 
    MaterialData matData = gMaterialData[pin.MatIndex];

    float4 diffuseAlbedo = gDiffuseMap.Sample(gsamAnisotropicWrap, pin.TexC);
	float3 bumpedNormalW = NormalSampleToWorldSpace(diffuseAlbedo.rgb, pin.NormalW, pin.TangentW);
    
    clip(diffuseAlbedo.a - 0.2f);

	diffuseAlbedo = diffuseAlbedo * matData.DiffuseAlbedo;
    
    pin.NormalW = normalize(pin.NormalW);
    
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    
    float4 ambient = gAmbientLight * diffuseAlbedo;
    
    const float shininess = 1.0f - matData.Roughness;
	float3 shadowFactor = float3(1.0f, 1.0f, 1.0f);
	shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);

    float4 directLight = ComputeLighting(
		matData, 
		pin.PosW, 
		pin.NormalW,
		toEyeW, 
		shadowFactor
	);

	float4 litColor = directLight;

	//// Add in specular reflection
	//float3 r = reflect(-toEyeW, pin.NormalW);
	//float4 reflectionColor = gCubeMap.Sample(gsamLinearWrap, r);

	//litColor.rgb += reflectionColor.rgb * shininess * 0.1f;

	//litColor.a = diffuseAlbedo.a;

    return litColor;

}

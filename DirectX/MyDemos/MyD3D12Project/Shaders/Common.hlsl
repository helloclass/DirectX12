//***************************************************************************************
// Common.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//***************************************************************************************

// Include structures and functions for lighting.
#include "LightingUtil.hlsl"

struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint MaterialIndex;
};

struct pmxBoneData
{
    float4x4 gOriginMatrix;
    float4x4 gMatrix;
};

// Constant data that varies per material.
cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
	float4x4 gShadowViewProj;
	float4x4 gShadowViewProjNDC;

    float3 gEyePosW;
    //float2 gRenderTargetSize;
    //float2 gInvRenderTargetSize;

    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;

    float4 gAmbientLight;

    //LightData gLights[3];
};

// Rate of Anim
cbuffer cbRateOfAnimTime : register(b2)
{
    float rateOfAnimTime;
}

StructuredBuffer<InstanceData>	gInstanceData           : register(t0, space0);
StructuredBuffer<MaterialData>	gMaterialData           : register(t0, space1);
#ifdef _PMX_FORMAT
StructuredBuffer<pmxBoneData> gPmxBoneData				: register(t0, space2);
#endif

Texture2D gDiffuseMap									: register(t1, space0);
Texture2D gMaskMap										: register(t1, space1);
Texture2D gNoiseMap										: register(t1, space2);
TextureCube gCubeMap									: register(t2, space0);
Texture2D gShadowMap									: register(t2, space1);
Texture2D gDrawTexMap									: register(t2, space2);

SamplerState gsamPointWrap								: register(s0);
SamplerState gsamPointClamp								: register(s1);
SamplerState gsamLinearWrap								: register(s2);
SamplerState gsamLinearClamp							: register(s3);
SamplerState gsamAnisotropicWrap						: register(s4);
SamplerState gsamAnisotropicClamp						: register(s5);
SamplerComparisonState gsamShadow						: register(s6);


//Texture2D gSsaoMap   : register(t2);

//// An array of textures, which is only supported in shader model 5.1+.  Unlike Texture2DArray, the textures
//// in this array can be different sizes and formats, making it more flexible than texture arrays.
//Texture2D gTextureMaps[48] : register(t3);

//// Put in space1, so the texture array does not overlap with these resources.  
//// The texture array will occupy registers t0, t1, ..., t3 in space0. 
//StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

//---------------------------------------------------------------------------------------
// Transforms a normal map sample to world space.
//---------------------------------------------------------------------------------------
float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 unitNormalW, float3 tangentW)
{
	// Uncompress each component from [0,1] to [-1,1].
	float3 normalT = 2.0f*normalMapSample - 1.0f;

	// Build orthonormal basis.
	float3 N = unitNormalW;
	float3 T = normalize(tangentW - dot(tangentW, N)*N);
	float3 B = cross(N, T);

	float3x3 TBN = float3x3(T, B, N);

	// Transform from tangent space to world space.
	float3 bumpedNormalW = mul(normalT, TBN);

	return bumpedNormalW;
}

//---------------------------------------------------------------------------------------
// PCF for shadow mapping.
//---------------------------------------------------------------------------------------

float CalcShadowFactor(float4 shadowPosH)
{
	// Complete projection by doing division by w.
	shadowPosH.xyz /= shadowPosH.w;

	// Depth in NDC space.
	float depth = shadowPosH.z;

	if (shadowPosH.x < 0.01 || 0.99 < shadowPosH.x ||
		shadowPosH.y < 0.01 || 0.99 < shadowPosH.y )
		return 1.0;

	uint width, height, numMips;
	gShadowMap.GetDimensions(0, width, height, numMips);

	// Texel size.
	float dx = 1.0f / (float)width;

	float percentLit = 0.0f;
	const float2 offsets[9] =
	{
		float2(-dx,  -dx), float2(0.0f,  -dx), float2(dx,  -dx),
		float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f),
		float2(-dx,  +dx), float2(0.0f,  +dx), float2(dx,  +dx)
	};

	// depth�� �þ�� ������
	// ��� ���� �������� �Ǿ�� ��.

	[unroll]
	for (int i = 0; i < 9; ++i)
	{
		percentLit +=
			gShadowMap.SampleCmpLevelZero(
				gsamShadow,
				shadowPosH.xy + offsets[i],
				depth
			).r;
	}

	// �׸��� ��� ���� �ִ� ���� �ƴ�, 
	// �׸��ڰ� ����� ��� ���� �ִ� ��.
	return percentLit / 9.0f;
}


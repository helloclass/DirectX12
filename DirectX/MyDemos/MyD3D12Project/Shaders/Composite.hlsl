//***************************************************************************************
// Composite.hlsl by Frank Luna (C) 2015 All Rights Reserved.
//
// Combines two images.
//***************************************************************************************

Texture2D gBaseMap : register(t0);
Texture2D gEdgeMap : register(t1);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

static const float2 gTexCoords[6] = 
{
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;
	
	vout.TexC = gTexCoords[vid];
	
	// Map [0,1]^2 to NDC space.
	vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);

    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	// 그림자 요소를 여기에다가 더할것이다.
	// 이유1. 굳이 Shadow.hlsl을 만들어서 배경 + ShadowMap을 할 필요 없이 여기에 c + ShadowMap을 하여 결과를 얻을 수 있다. 최적화에 알맞음.
	// 이유2. 그림자에 Sobel이 먹히는 현상을 방지할 수 있음. 

    float4 c = gBaseMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
	float4 e = gEdgeMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f);
	
	return c*e;
}



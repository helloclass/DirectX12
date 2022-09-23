//=============================================================================
// Performs edge detection using Sobel operator.
//=============================================================================

Texture2D gInput            : register(t0);
RWTexture2D<float4> gOutput : register(u0);


// 이후 여기에도 가중치를 넣어주면 좋을 것 같다.
// Constant Buffer로


// Approximates luminance ("brightness") from an RGB value.  These weights are derived from
// experiment based on eye sensitivity to different wavelengths of light.
float CalcLuminance(float3 color)
{
    return dot(color, float3(0.299f, 0.587f, 0.114f));
}

[numthreads(16, 16, 1)]
void SobelCS(int3 dispatchThreadID : SV_DispatchThreadID)
{
    // sample the pixels in the neighborhood of this pixel.
	float4 c[3][3];
	for(int i = 0; i < 3; ++i)
	{
		for(int j = 0; j < 3; ++j)
		{
			int2 xy = dispatchThreadID.xy + int2(-1 + j, -1 + i);
			c[i][j] = gInput[xy];
		}
	}

	// for each color channel, estimate partial x derivative using sobel scheme.
	float4 gx = -1.0f*c[0][0] - 2.0f*c[1][0] - 1.0f*c[2][0] + 1.0f*c[0][2] + 2.0f*c[1][2] + 1.0f*c[2][2];

	// for each color channel, estimate partial y derivative using sobel scheme.
	float4 gy = -1.0f*c[2][0] - 2.0f*c[2][1] - 1.0f*c[2][1] + 1.0f*c[0][0] + 2.0f*c[0][1] + 1.0f*c[0][2];

	// gradient is (gx, gy).  for each color channel, compute magnitude to get maximum rate of change.
	float4 mag = sqrt(gx*gx + gy*gy);
    
    // 거리 파라미터를 받아서 멀어질 수록 magnitude가 선형적으로 작아져야 할 듯
    mag = mag * 0.20f;

	// make edges black, and nonedges white.
	mag = 1.0f - saturate(CalcLuminance(mag.rgb));
	mag.r += 0.6f;
	mag.g += 0.4f;
	mag.b += 0.4f;
    
	gOutput[dispatchThreadID.xy] = mag;
}

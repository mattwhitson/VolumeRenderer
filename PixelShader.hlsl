#define anisoClampSampler  0

struct PixelInput {
	float4 position : SV_POSITION;
	float3 vertPos : VERT_POS;
};

struct PerFrameConstants {
	matrix modelMatrix;
	float2 cameraDimensions;
	uint frontBufferIndex;
	uint backBufferIndex;
	uint cubeBufferIndex;
	uint volumeDataBufferIndex;
};

ConstantBuffer<PerFrameConstants> PerFrameConstantBuffer : register(b0, space1);

float4 PSMain(PixelInput input) : SV_TARGET
{
	float2 coords = input.position.xy / PerFrameConstantBuffer.cameraDimensions;

	Texture2D<float4> frontTexture = ResourceDescriptorHeap[PerFrameConstantBuffer.frontBufferIndex];
	Texture2D<float4> backTexture = ResourceDescriptorHeap[PerFrameConstantBuffer.backBufferIndex];
	Texture3D<float> volumeData = ResourceDescriptorHeap[PerFrameConstantBuffer.volumeDataBufferIndex];
	SamplerState anisoSampler = SamplerDescriptorHeap[anisoClampSampler];

	float3 front = frontTexture.Sample(anisoSampler, coords);
	float3 back = backTexture.Sample(anisoSampler, coords);

	float3 direction = normalize(back - front);
	float dist = distance(back, front);

	float3 step = direction * 0.05f;
	float stepDistance = length(step);

	uint iterations = ceil(dist / stepDistance);

	float3 pos = float4(front, 0);
	float2 result = float2(0, 0);

	for (uint i = 0; i < iterations; i++)
	{
       float2 src = volumeData.Sample(anisoSampler, pos).rr;

	   result += (1 - result.y)*src.y * src;

	   pos += step;
	}

	return float4(result.rrr, result.y);
	//float3 color = (input.vertPos + 1.0f) * 0.5f;
	//return float4(color, 1.0);
}
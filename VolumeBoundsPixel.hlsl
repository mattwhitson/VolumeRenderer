struct PixelInput {
	float4 position : SV_POSITION;
	float3 vertexPosition: VERTEX_POSITION;
};

float4 PSMain(PixelInput input) : SV_TARGET
{
	return float4(input.vertexPosition, 1.0);
}
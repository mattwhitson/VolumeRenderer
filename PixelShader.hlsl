struct PixelInput {
	float4 position : SV_POSITION;
	float3 vertPos : VERT_POS;
};

float4 PSMain(PixelInput input) : SV_TARGET
{
	float3 color = (input.vertPos + 1.0f) * 0.5f;
	return float4(color, 1.0);
}
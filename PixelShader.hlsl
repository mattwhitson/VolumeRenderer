struct PixelInput {
	float4 position : SV_POSITION;
};

float4 PSMain() : SV_TARGET
{
	return float4(1.0, 1.0, 1.0, 1.0);
}
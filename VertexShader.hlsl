struct VertexOutput {
	float4 position : SV_POSITION;
	float3 vertPos: VERT_POS;
};

struct CameraConstants {
	matrix projMatrix;
	matrix cameraMatrix;
};

struct PerFrameConstants {
	matrix modelMatrix;
	float2 cameraDimensions;
	uint frontBufferIndex;
	uint backBufferIndex;
	uint cubeBufferIndex;
	uint volumeDataBufferIndex;
};


ConstantBuffer<CameraConstants> CameraConstantBuffer : register(b0, space0);
ConstantBuffer<PerFrameConstants> PerFrameConstantBuffer : register(b0, space1);

VertexOutput VSMain(uint vertexId : SV_VERTEXID)
{
	ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[PerFrameConstantBuffer.cubeBufferIndex];
	float3 pos = vertexBuffer.Load<float3>(vertexId * sizeof(float3));

	VertexOutput output;
	output.position = mul(float4(pos, 1.0f), PerFrameConstantBuffer.modelMatrix);
	output.position = mul(output.position, CameraConstantBuffer.cameraMatrix);
	output.position = mul(output.position, CameraConstantBuffer.projMatrix);
	output.vertPos = pos;
	return output;
}
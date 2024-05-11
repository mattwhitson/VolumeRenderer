struct VertexOutput {
	float4 position : SV_POSITION;
};

struct TriangleConstants {
	float4x4 projMatrix;
	uint vertexBufferIndex;
};

ConstantBuffer<TriangleConstants> ObjectConstantBuffer : register(b0, space0);

VertexOutput VSMain(uint vertexId : SV_VERTEXID, uint instanceId : SV_INSTANCEID)
{
	ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.vertexBufferIndex];
	float3 pos = vertexBuffer.Load<float3>(vertexId * sizeof(float3));

	VertexOutput output;
	output.position = mul(float4(pos, 1.0), ObjectConstantBuffer.projMatrix);

	return output;
}
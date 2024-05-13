struct VertexOutput {
	float4 position : SV_POSITION;
	float3 vertexPosition: VERTEX_POSITION;
};

struct MeshConstants {
	matrix projMatrix;
	matrix cameraMatrix;
	matrix modelMatrix;
	uint vertexBufferIndex;
};

ConstantBuffer<MeshConstants> ObjectConstantBuffer : register(b0, space0);

VertexOutput VSMain(uint vertexId : SV_VERTEXID)
{
	ByteAddressBuffer vertexBuffer = ResourceDescriptorHeap[ObjectConstantBuffer.vertexBufferIndex];
	float3 pos = vertexBuffer.Load<float3>(vertexId * sizeof(float3));

	VertexOutput output;
	output.position = mul(float4(pos, 1.0f), ObjectConstantBuffer.modelMatrix);
	output.position = mul(output.position, ObjectConstantBuffer.cameraMatrix);
	output.position = mul(output.position, ObjectConstantBuffer.projMatrix);
	output.vertexPosition = (pos + 1) * 0.5;
	return output;
}
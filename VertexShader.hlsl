struct VertexOutput {
	float4 position : SV_POSITION;
	float3 vertPos: VERT_POS;
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

	matrix translation = matrix(
		1.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 1.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f, 
		0.0f, 0.0f, 10.0f, 1.0f
	);

	VertexOutput output;
	output.position = mul(float4(pos, 1.0f), ObjectConstantBuffer.modelMatrix);
	output.position = mul(output.position, ObjectConstantBuffer.cameraMatrix);
	output.position = mul(output.position, ObjectConstantBuffer.projMatrix);
	output.vertPos = pos;
	return output;
}
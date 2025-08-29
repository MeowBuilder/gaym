#include "stdafx.h"
#include "Mesh.h"

Mesh::Mesh()
{
}

Mesh::~Mesh()
{
}

void Mesh::BuildCube(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList)
{
	Vertex cubeVertices[] =
	{
		{ XMFLOAT3(-0.5f, 0.5f, -0.5f), RANDOM_COLOR },
		{ XMFLOAT3(0.5f, 0.5f, -0.5f), RANDOM_COLOR },
		{ XMFLOAT3(0.5f, 0.5f, 0.5f), RANDOM_COLOR },
		{ XMFLOAT3(-0.5f, 0.5f, 0.5f), RANDOM_COLOR },
		{ XMFLOAT3(-0.5f, -0.5f, -0.5f), RANDOM_COLOR },
		{ XMFLOAT3(0.5f, -0.5f, -0.5f), RANDOM_COLOR },
		{ XMFLOAT3(0.5f, -0.5f, 0.5f), RANDOM_COLOR },
		{ XMFLOAT3(-0.5f, -0.5f, 0.5f), RANDOM_COLOR }
	};

	DWORD cubeIndices[] =
	{
		// Front Face
		0, 1, 5,
		0, 5, 4,

		// Back Face
		2, 3, 7,
		2, 7, 6,

		// Top Face
		3, 2, 1,
		3, 1, 0,

		// Bottom Face
		4, 5, 6,
		4, 6, 7,

		// Left Face
		3, 0, 4,
		3, 4, 7,

		// Right Face
		1, 2, 6,
		1, 6, 5
	};

	m_nIndexCount = _countof(cubeIndices);

	m_pVertexBuffer = CreateBufferResource(pDevice, pCommandList, cubeVertices, sizeof(cubeVertices), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &m_pVertexUploadBuffer);
	m_pIndexBuffer = CreateBufferResource(pDevice, pCommandList, cubeIndices, sizeof(cubeIndices), D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &m_pIndexUploadBuffer);

	m_VertexBufferView.BufferLocation = m_pVertexBuffer->GetGPUVirtualAddress();
	m_VertexBufferView.StrideInBytes = sizeof(Vertex);
	m_VertexBufferView.SizeInBytes = sizeof(cubeVertices);

	m_IndexBufferView.BufferLocation = m_pIndexBuffer->GetGPUVirtualAddress();
	m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_IndexBufferView.SizeInBytes = sizeof(cubeIndices);
}

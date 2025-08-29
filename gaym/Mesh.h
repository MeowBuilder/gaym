#pragma once

#include "stdafx.h"

struct Vertex
{
	XMFLOAT3 pos;
	XMFLOAT4 color;
};

class Mesh
{
public:
	Mesh();
	~Mesh();

	void BuildCube(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList);

	ID3D12Resource* GetVertexBuffer() const { return m_pVertexBuffer.Get(); }
	ID3D12Resource* GetIndexBuffer() const { return m_pIndexBuffer.Get(); }

	const D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() const { return m_VertexBufferView; }
	const D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() const { return m_IndexBufferView; }

	UINT GetIndexCount() const { return m_nIndexCount; }

private:
	ComPtr<ID3D12Resource> m_pVertexBuffer;
	ComPtr<ID3D12Resource> m_pIndexBuffer;
	ComPtr<ID3D12Resource> m_pVertexUploadBuffer;
	ComPtr<ID3D12Resource> m_pIndexUploadBuffer;

	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

	UINT m_nIndexCount = 0;
};

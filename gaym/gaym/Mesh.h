//------------------------------------------------------- ----------------------
// File: Mesh.h
//-----------------------------------------------------------------------------

#pragma once

#include <vector>
#include <string>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class Mesh
{
public:
	Mesh() { }
    virtual ~Mesh();

private:
	int								m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }

	virtual void ReleaseUploadBuffers() { }

protected:
	D3D12_PRIMITIVE_TOPOLOGY		m_d3dPrimitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	UINT							m_nSlot = 0;
	UINT							m_nVertices = 0;
	UINT							m_nOffset = 0;

	UINT							m_nType = 0;

public:
	UINT GetType() { return(m_nType); }
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList) { }
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nSubSet) { }
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
#define VERTEXT_POSITION			0x01
#define VERTEXT_COLOR				0x02
#define VERTEXT_NORMAL				0x04
#define VERTEXT_TEXTURE_COORD0		0x08
#define VERTEXT_BONE_INDEX_WEIGHT   0x10

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class MeshFromFile : public Mesh
{
public:
	MeshFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, class MeshLoadInfo *pMeshInfo);
	virtual ~MeshFromFile();

public:
	virtual void ReleaseUploadBuffers();

protected:
	ComPtr<ID3D12Resource>				m_pd3dPositionBuffer;
	ComPtr<ID3D12Resource>				m_pd3dPositionUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dPositionBufferView;

	int								m_nSubMeshes = 0;
	int								*m_pnSubSetIndices = NULL;

	ComPtr<ID3D12Resource>					*m_ppd3dSubSetIndexBuffers = NULL;
	ComPtr<ID3D12Resource>					*m_ppd3dSubSetIndexUploadBuffers = NULL;
	D3D12_INDEX_BUFFER_VIEW			*m_pd3dSubSetIndexBufferViews = NULL;

public:
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nSubSet);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class MeshIlluminatedFromFile : public MeshFromFile
{
public:
	MeshIlluminatedFromFile(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, class MeshLoadInfo *pMeshInfo);
	virtual ~MeshIlluminatedFromFile();

	virtual void ReleaseUploadBuffers();

protected:
	ComPtr<ID3D12Resource>				m_pd3dNormalBuffer;
	ComPtr<ID3D12Resource>				m_pd3dNormalUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dNormalBufferView;

	ComPtr<ID3D12Resource>				m_pd3dTextureCoord0Buffer;
	ComPtr<ID3D12Resource>				m_pd3dTextureCoord0UploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW		m_d3dTextureCoord0BufferView;

public:
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nSubSet);
};

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
class SkinnedMesh : public MeshIlluminatedFromFile
{
public:
	SkinnedMesh(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList *pd3dCommandList, class MeshLoadInfo *pMeshInfo);
	virtual ~SkinnedMesh();

	virtual void ReleaseUploadBuffers();
	virtual void Render(ID3D12GraphicsCommandList *pd3dCommandList, int nSubSet);

protected:
	ComPtr<ID3D12Resource>				m_pd3dBoneIndexBuffer;
	ComPtr<ID3D12Resource>				m_pd3dBoneIndexUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_d3dBoneIndexBufferView;

	ComPtr<ID3D12Resource>				m_pd3dBoneWeightBuffer;
	ComPtr<ID3D12Resource>				m_pd3dBoneWeightUploadBuffer;
	D3D12_VERTEX_BUFFER_VIEW			m_d3dBoneWeightBufferView;

public:
	std::vector<std::string>		m_vBoneNames;
	std::vector<XMFLOAT4X4>			m_vBindPoses; // Inverse Bind Pose Matrices
};

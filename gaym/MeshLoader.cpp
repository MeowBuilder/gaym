#include "stdafx.h"
#include "MeshLoader.h"
#include "GameObject.h"
#include "Mesh.h"
#include "Scene.h" // Include Scene.h

MeshLoadInfo::~MeshLoadInfo()
{
	if (m_pxmf3Positions) delete[] m_pxmf3Positions;
	if (m_pxmf4Colors) delete[] m_pxmf4Colors;
	if (m_pxmf3Normals) delete[] m_pxmf3Normals;

	if (m_pnIndices) delete[] m_pnIndices;
	
	if (m_pnSubSetIndices) delete[] m_pnSubSetIndices;

	for (int i = 0; i < m_nSubMeshes; i++) if (m_ppnSubSetIndices[i]) delete[] m_ppnSubSetIndices[i];
	if (m_ppnSubSetIndices) delete[] m_ppnSubSetIndices;
}

int ReadIntegerFromFile(FILE* pInFile)
{
	int nValue = 0;
	UINT nReads = (UINT)::fread(&nValue, sizeof(int), 1, pInFile);

	wchar_t buffer[128];
	swprintf_s(buffer, 128, L"  Read Integer: %d\n", nValue);
	OutputDebugString(buffer);

	return(nValue);
}

float ReadFloatFromFile(FILE* pInFile)
{
	float fValue = 0;
	UINT nReads = (UINT)::fread(&fValue, sizeof(float), 1, pInFile);
	return(fValue);
}

BYTE ReadStringFromFile(FILE* pInFile, char* pstrToken, int nBufferSize)
{
	if (!pInFile) return 0;
	BYTE nStrLength = 0;
	UINT nReads = 0;
	nReads = (UINT)::fread(&nStrLength, sizeof(BYTE), 1, pInFile);

	if (nStrLength >= nBufferSize)
	{
		fseek(pInFile, nStrLength, SEEK_CUR);
		return 0;
	}

	nReads = (UINT)::fread(pstrToken, sizeof(char), nStrLength, pInFile);
	pstrToken[nStrLength] = '\0';

	return(nStrLength);
}

void MeshLoader::LoadMaterialsInfoFromFile(FILE* pInFile)
{
    char pstrToken[64] = { '\0' };
    int nMaterials = ReadIntegerFromFile(pInFile);

    for ( ; ; )
    {
        if (ReadStringFromFile(pInFile, pstrToken, 64) == 0) break;
        if (!strcmp(pstrToken, "</Materials>")) break;
        // This is a dummy implementation. We just need to parse it.
        // In a real implementation, we would read the material properties.
        if (!strcmp(pstrToken, "<Material>:"))
        {
            int nMaterial = ReadIntegerFromFile(pInFile);
        }
        else if (!strcmp(pstrToken, "<AlbedoColor>:"))
        {
            XMFLOAT4 xmf4AlbedoColor;
            fread(&xmf4AlbedoColor, sizeof(float), 4, pInFile);
        }
        else if (!strcmp(pstrToken, "<EmissiveColor>:"))
        {
            XMFLOAT4 xmf4EmissiveColor;
            fread(&xmf4EmissiveColor, sizeof(float), 4, pInFile);
        }
        else if (!strcmp(pstrToken, "<SpecularColor>:"))
        {
            XMFLOAT4 xmf4SpecularColor;
            fread(&xmf4SpecularColor, sizeof(float), 4, pInFile);
        }
        else if (!strcmp(pstrToken, "<Glossiness>:"))
        {
            float fGlossiness;
            fread(&fGlossiness, sizeof(float), 1, pInFile);
        }
        else if (!strcmp(pstrToken, "<Smoothness>:"))
        {
            float fSmoothness;
            fread(&fSmoothness, sizeof(float), 1, pInFile);
        }
        else if (!strcmp(pstrToken, "<Metallic>:"))
        {
            float fMetallic;
            fread(&fMetallic, sizeof(float), 1, pInFile);
        }
        else if (!strcmp(pstrToken, "<SpecularHighlight>:"))
        {
            float fSpecularHighlight;
            fread(&fSpecularHighlight, sizeof(float), 1, pInFile);
        }
        else if (!strcmp(pstrToken, "<GlossyReflection>:"))
        {
            float fGlossyReflection;
            fread(&fGlossyReflection, sizeof(float), 1, pInFile);
        }
    }
}


GameObject* MeshLoader::LoadGeometryFromFile(Scene* pScene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, const char* pstrFileName)
{
	FILE* pInFile = NULL;
	if (::fopen_s(&pInFile, pstrFileName, "rb") != 0)
	{
		return nullptr;
	}
	::rewind(pInFile);

	GameObject* pGameObject = NULL;
	char pstrToken[64] = { '\0' };

	for ( ; ; )
	{
		if (::ReadStringFromFile(pInFile, pstrToken, 64) == 0) break;

		if (!strcmp(pstrToken, "<Hierarchy>:"))
		{
			pGameObject = MeshLoader::LoadFrameHierarchyFromFile(pScene, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pInFile);
		}
		else if (!strcmp(pstrToken, "</Hierarchy>"))
		{
			break;
		}
	}
    fclose(pInFile);
	return(pGameObject);
}

GameObject* MeshLoader::LoadFrameHierarchyFromFile(Scene* pScene, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList, ID3D12RootSignature* pd3dGraphicsRootSignature, FILE* pInFile)
{
    char pstrToken[64] = { '\0' };
    UINT nReads = 0;
    int nFrame = 0;
    GameObject* pGameObject = nullptr;

    for ( ; ; )
    {
        if (::ReadStringFromFile(pInFile, pstrToken, 64) == 0) break;

        wchar_t buffer[128];
        swprintf_s(buffer, 128, L"  Read Token: %hs\n", pstrToken);
        OutputDebugString(buffer);

        if (!strcmp(pstrToken, "<Frame>:"))
        {
            pGameObject = pScene->CreateGameObject(pd3dDevice, pd3dCommandList);
            nFrame = ::ReadIntegerFromFile(pInFile);
            ::ReadStringFromFile(pInFile, pGameObject->m_pstrFrameName, 64);
        }
        else if (!strcmp(pstrToken, "<Transform>:"))
        {
            XMFLOAT3 xmf3Position, xmf3Rotation, xmf3Scale;
            XMFLOAT4 xmf4Rotation;
            nReads = (UINT)::fread(&xmf3Position, sizeof(float), 3, pInFile);
            nReads = (UINT)::fread(&xmf3Rotation, sizeof(float), 3, pInFile);
            nReads = (UINT)::fread(&xmf3Scale, sizeof(float), 3, pInFile);
            nReads = (UINT)::fread(&xmf4Rotation, sizeof(float), 4, pInFile);
        }
        else if (!strcmp(pstrToken, "<TransformMatrix>:"))
        {
            XMFLOAT4X4 xmf4x4Transform;
            nReads = (UINT)::fread(&xmf4x4Transform, sizeof(float), 16, pInFile);
            if (pGameObject) pGameObject->SetTransform(xmf4x4Transform);
        }
        else if (!strcmp(pstrToken, "<Mesh>:"))
        {
            MeshLoadInfo *pMeshInfo = MeshLoader::LoadMeshInfoFromFile(pInFile);
            if (pGameObject && pMeshInfo)
            {
                Mesh *pMesh = NULL;
                if (pMeshInfo->m_nType & VERTEXT_NORMAL)
                {
                    pMesh = new MeshIlluminatedFromFile(pd3dDevice, pd3dCommandList, pMeshInfo);
                }
                else
                {
                    pMesh = new MeshFromFile(pd3dDevice, pd3dCommandList, pMeshInfo);
                }
                if (pMesh) pGameObject->SetMesh(pMesh);
                delete pMeshInfo;
            }
        }
        else if (!strcmp(pstrToken, "<Materials>:"))
        {
            LoadMaterialsInfoFromFile(pInFile);
        }
        else if (!strcmp(pstrToken, "<Children>:"))
        {
            int nChilds = ::ReadIntegerFromFile(pInFile);
            if (nChilds > 0)
            {
                for (int i = 0; i < nChilds; i++)
                {
                    GameObject *pChild = MeshLoader::LoadFrameHierarchyFromFile(pScene, pd3dDevice, pd3dCommandList, pd3dGraphicsRootSignature, pInFile);
                    if (pGameObject && pChild) pGameObject->SetChild(pChild);
                }
            }
        }
        else if (!strcmp(pstrToken, "</Frame>"))
        {
            break;
        }
    }
    return(pGameObject);
}

MeshLoadInfo* MeshLoader::LoadMeshInfoFromFile(FILE* pInFile)
{
	char pstrToken[64] = { '\0' };
	UINT nReads = 0;
	int nPositions = 0, nColors = 0, nNormals = 0, nIndices = 0, nSubMeshes = 0, nSubIndices = 0;
	MeshLoadInfo *pMeshInfo = new MeshLoadInfo;

	pMeshInfo->m_nVertices = ::ReadIntegerFromFile(pInFile);
	::ReadStringFromFile(pInFile, pMeshInfo->m_pstrMeshName, 64);

	for ( ; ; )
	{
		if (::ReadStringFromFile(pInFile, pstrToken, 64) == 0) break;

		if (!strcmp(pstrToken, "<Bounds>:"))
		{
			nReads = (UINT)::fread(&(pMeshInfo->m_xmf3AABBCenter), sizeof(XMFLOAT3), 1, pInFile);
			nReads = (UINT)::fread(&(pMeshInfo->m_xmf3AABBExtents), sizeof(XMFLOAT3), 1, pInFile);
		}
		else if (!strcmp(pstrToken, "<Positions>:"))
		{
			nPositions = ::ReadIntegerFromFile(pInFile);
			if (nPositions > 0)
			{
				pMeshInfo->m_nType |= VERTEXT_POSITION;
				pMeshInfo->m_pxmf3Positions = new XMFLOAT3[nPositions];
				nReads = (UINT)::fread(pMeshInfo->m_pxmf3Positions, sizeof(XMFLOAT3), nPositions, pInFile);
			}
		}
		else if (!strcmp(pstrToken, "<Colors>:"))
		{
			nColors = ::ReadIntegerFromFile(pInFile);
			if (nColors > 0)
			{
				pMeshInfo->m_nType |= VERTEXT_COLOR;
				pMeshInfo->m_pxmf4Colors = new XMFLOAT4[nColors];
				nReads = (UINT)::fread(pMeshInfo->m_pxmf4Colors, sizeof(XMFLOAT4), nColors, pInFile);
			}
		}
		else if (!strcmp(pstrToken, "<Normals>:"))
		{
			nNormals = ::ReadIntegerFromFile(pInFile);
			if (nNormals > 0)
			{
				pMeshInfo->m_nType |= VERTEXT_NORMAL;
				pMeshInfo->m_pxmf3Normals = new XMFLOAT3[nNormals];
				nReads = (UINT)::fread(pMeshInfo->m_pxmf3Normals, sizeof(XMFLOAT3), nNormals, pInFile);
			}
		}
		else if (!strcmp(pstrToken, "<Indices>:"))
		{
			nIndices = ::ReadIntegerFromFile(pInFile);
			if (nIndices > 0)
			{
				pMeshInfo->m_pnIndices = new UINT[nIndices];
				nReads = (UINT)::fread(pMeshInfo->m_pnIndices, sizeof(int), nIndices, pInFile);
			}
		}
		else if (!strcmp(pstrToken, "<SubMeshes>:"))
		{
			pMeshInfo->m_nSubMeshes = ::ReadIntegerFromFile(pInFile);
			if (pMeshInfo->m_nSubMeshes > 0)
			{
				pMeshInfo->m_pnSubSetIndices = new int[pMeshInfo->m_nSubMeshes];
				pMeshInfo->m_ppnSubSetIndices = new UINT*[pMeshInfo->m_nSubMeshes];
				for (int i = 0; i < pMeshInfo->m_nSubMeshes; i++)
				{
					pMeshInfo->m_ppnSubSetIndices[i] = NULL;
					::ReadStringFromFile(pInFile, pstrToken, 64);
					if (!strcmp(pstrToken, "<SubMesh>:"))
					{
						int nIndex = ::ReadIntegerFromFile(pInFile);
						pMeshInfo->m_pnSubSetIndices[i] = ::ReadIntegerFromFile(pInFile);
						if (pMeshInfo->m_pnSubSetIndices[i] > 0)
						{
							pMeshInfo->m_ppnSubSetIndices[i] = new UINT[pMeshInfo->m_pnSubSetIndices[i]];
							nReads = (UINT)::fread(pMeshInfo->m_ppnSubSetIndices[i], sizeof(UINT), pMeshInfo->m_pnSubSetIndices[i], pInFile);
						}

					}
				}
			}
		}
		else if (!strcmp(pstrToken, "</Mesh>"))
		{
			break;
		}
	}
	return(pMeshInfo);
}
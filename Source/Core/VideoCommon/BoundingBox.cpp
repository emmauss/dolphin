// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.



#include "VideoBackends/Software/Clipper.h"
#include "VideoBackends/Software/Rasterizer.h"
#include "VideoBackends/Software/SetupUnit.h"
#include "VideoBackends/Software/TransformUnit.h"
#include "VideoCommon/BoundingBox.h"
#include "VideoCommon/PixelShaderManager.h"


namespace BoundingBox
{

// External vars
bool active = false;
u16 coords[4] = { 0x80, 0xA0, 0x80, 0xA0 };
u8 posMtxIdx;
u8 texMtxIdx[8];


// Internal vars
static SetupUnit vtxUnit;
static VAT myVat;
static u8 * bufferPos;
static TVtxDesc vertexDesc;
static PortableVertexDeclaration vertexDecl;

// Gets the pointer to the current buffer position
void LOADERDECL SetVertexBufferPosition(VertexLoader* loader)
{
	bufferPos = g_vertex_manager_write_ptr;
}

// Prepares the bounding box for new primitive data
void Prepare(const VAT & vat, int primitive, const TVtxDesc & vtxDesc, const PortableVertexDeclaration & vtxDecl)
{
	if (!active)
		return;

	myVat = vat;
	vertexDesc = vtxDesc;
	vertexDecl = vtxDecl;

	vtxUnit.Init(primitive);

	// Initialize the SW renderer
	static bool SWinit = false;

	if (!SWinit)
	{
		Clipper::Init();
		Rasterizer::Init();
		SWinit = true;
	}

	// Update SW renderer values
	Clipper::SetViewOffset();
	Rasterizer::SetScissor();

	for (u8 i = 0; i < 4; ++i)
	{
		Rasterizer::SetTevReg(i, 0, true, (s16)PixelShaderManager::constants.kcolors[i][0]);
		Rasterizer::SetTevReg(i, 1, true, (s16)PixelShaderManager::constants.kcolors[i][1]);
		Rasterizer::SetTevReg(i, 2, true, (s16)PixelShaderManager::constants.kcolors[i][2]);
		Rasterizer::SetTevReg(i, 3, true, (s16)PixelShaderManager::constants.kcolors[i][3]);

		Rasterizer::SetTevReg(i, 0, false, (s16)PixelShaderManager::constants.colors[i][0]);
		Rasterizer::SetTevReg(i, 1, false, (s16)PixelShaderManager::constants.colors[i][1]);
		Rasterizer::SetTevReg(i, 2, false, (s16)PixelShaderManager::constants.colors[i][2]);
		Rasterizer::SetTevReg(i, 3, false, (s16)PixelShaderManager::constants.colors[i][3]);
	}
}

// Updates the bounding box
void LOADERDECL Update(VertexLoader* loader)
{
	if (!active)
		return;

	// Grab vertex input data and transform to output vertex
	InputVertexData myVertex;
	OutputVertexData * outVertex = vtxUnit.GetVertex();

	// Feed vertex position and matrix
	myVertex.position = Vec3((const float *)bufferPos);
	myVertex.posMtx = vertexDesc.PosMatIdx ? posMtxIdx : g_main_cp_state.matrix_index_a.PosNormalMtxIdx;

	// Transform position
	TransformUnit::TransformPosition(&myVertex, outVertex);

	if (g_main_cp_state.vtx_desc.Normal != NOT_PRESENT)
	{
		// Feed normal input data and transform
		memcpy((u8 *)myVertex.normal, bufferPos + vertexDecl.normals[0].offset, sizeof(float) * 3 * ((myVat.g0.NormalElements) ? 3 : 1));

		TransformUnit::TransformNormal(&myVertex, myVat.g0.NormalElements, outVertex);
	}

	// Feed color input data
	for (int i = 0; i < 2; ++i)
	{
		if (vertexDecl.colors[i].enable)
		{
			u32 color;
			memcpy((u8 *)&color, bufferPos + vertexDecl.colors[i].offset, sizeof(u32));
			*(u32*)myVertex.color[i] = Common::swap32(color);
		}
	}

	// Transform color
	TransformUnit::TransformColor(&myVertex, outVertex);

	// Feed texture matrices
	int idx = 0;

	myVertex.texMtx[0] = (vertexDesc.Tex0MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_a.Tex0MtxIdx;
	myVertex.texMtx[1] = (vertexDesc.Tex1MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_a.Tex1MtxIdx;
	myVertex.texMtx[2] = (vertexDesc.Tex2MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_a.Tex2MtxIdx;
	myVertex.texMtx[3] = (vertexDesc.Tex3MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_a.Tex3MtxIdx;
	myVertex.texMtx[4] = (vertexDesc.Tex4MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_b.Tex4MtxIdx;
	myVertex.texMtx[5] = (vertexDesc.Tex5MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_b.Tex5MtxIdx;
	myVertex.texMtx[6] = (vertexDesc.Tex6MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_b.Tex6MtxIdx;
	myVertex.texMtx[7] = (vertexDesc.Tex7MatIdx) ? texMtxIdx[idx++] : g_main_cp_state.matrix_index_b.Tex7MtxIdx;

	// Feed texture coordinate data
	for (int i = 0; i < 8; ++i)
	{
		if (vertexDecl.texcoords[i].enable)
			memcpy((u8 *)&myVertex.texCoords[i], bufferPos + vertexDecl.texcoords[i].offset, sizeof(float) * 2);
	}

	// Transform texture coordinate
	TransformUnit::TransformTexCoord(&myVertex, outVertex, false);

	// Render the vertex in SW to calculate bbox
	vtxUnit.SetupVertex();
}

// Save state
void DoState(PointerWrap &p)
{
	p.Do(active);
	p.Do(coords);
}

} // namespace BoundingBox

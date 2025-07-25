#include "TextureRenderAtlas.h"

#include <algorithm>

#include "LegacyAtlasAlloc.h"
#include "QuadtreeAtlasAlloc.h"
#include "RowAtlasAlloc.h"
#include "MultiPageAtlasAlloc.hpp"

#include "Rendering/GlobalRendering.h"
#include "Rendering/GL/myGL.h"
#include "Rendering/GL/FBO.h"
#include "Rendering/GL/TexBind.h"
#include "Rendering/GL/SubState.h"
#include "Rendering/GL/RenderBuffers.h"
#include "Rendering/Textures/Bitmap.h"
#include "Rendering/Shaders/Shader.h"
#include "Rendering/Shaders/ShaderHandler.h"
#include "System/Config/ConfigHandler.h"
#include "System/StringUtil.h"
#include "fmt/format.h"

#include "System/Misc/TracyDefs.h"

namespace {
static constexpr const char* vsTRA = R"(
#version 130

in vec2 pos;
in vec2 uv;

out vec2 vUV;

void main() {
	vUV  = uv;
	gl_Position = vec4(pos, 0.0, 1.0);
}
)";

static constexpr const char* fsTRA = R"(
#version 130

uniform sampler2D tex;
uniform float lod;

in vec2 vUV;
out vec4 outColor;

void main() {
	outColor = textureLod(tex, vUV, lod);
}
)";
};

CTextureRenderAtlas::CTextureRenderAtlas(
	CTextureAtlas::AllocatorType allocType_,
	int atlasSizeX_,
	int atlasSizeY_,
	uint32_t glInternalType_,
	const std::string& atlasName_
	)
	: atlasSizeX(atlasSizeX_)
	, atlasSizeY(atlasSizeY_)
	, allocType(allocType_)
	, glInternalType(glInternalType_)
	, atlasName(atlasName_)
	, finalized(false)
{
	RECOIL_DETAILED_TRACY_ZONE;

	using MPLegacyAtlasAlloc = MultiPageAtlasAlloc<CLegacyAtlasAlloc>;
	using MPQuadtreeAtlasAlloc = MultiPageAtlasAlloc<CQuadtreeAtlasAlloc>;
	using MPRowAtlasAlloc = MultiPageAtlasAlloc<CRowAtlasAlloc>;

	static constexpr uint32_t MAX_TEXTURE_PAGES = 16;

	switch (allocType) {
		case CTextureAtlas::ATLAS_ALLOC_LEGACY:      { atlasAllocator = std::make_unique<   CLegacyAtlasAlloc>(                 ); } break;
		case CTextureAtlas::ATLAS_ALLOC_QUADTREE:    { atlasAllocator = std::make_unique< CQuadtreeAtlasAlloc>(                 ); } break;
		case CTextureAtlas::ATLAS_ALLOC_ROW:         { atlasAllocator = std::make_unique<      CRowAtlasAlloc>(                 ); } break;
		case CTextureAtlas::ATLAS_ALLOC_MP_LEGACY:   { atlasAllocator = std::make_unique<  MPLegacyAtlasAlloc>(MAX_TEXTURE_PAGES); } break;
		case CTextureAtlas::ATLAS_ALLOC_MP_QUADTREE: { atlasAllocator = std::make_unique<MPQuadtreeAtlasAlloc>(MAX_TEXTURE_PAGES); } break;
		case CTextureAtlas::ATLAS_ALLOC_MP_ROW:      { atlasAllocator = std::make_unique<     MPRowAtlasAlloc>(MAX_TEXTURE_PAGES); } break;
		default:                                     {                                                              assert(false); } break;
	}

	atlasSizeX = std::min(globalRendering->maxTextureSize, (atlasSizeX > 0) ? atlasSizeX : configHandler->GetInt("MaxTextureAtlasSizeX"));
	atlasSizeY = std::min(globalRendering->maxTextureSize, (atlasSizeY > 0) ? atlasSizeY : configHandler->GetInt("MaxTextureAtlasSizeY"));

	atlasAllocator->SetMaxSize(atlasSizeX, atlasSizeY);

	if (shaderRef == 0) {
		shader = shaderHandler->CreateProgramObject("[TextureRenderAtlas]", "TextureRenderAtlas");
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(vsTRA, "", GL_VERTEX_SHADER));
		shader->AttachShaderObject(shaderHandler->CreateShaderObject(fsTRA, "", GL_FRAGMENT_SHADER));
		shader->BindAttribLocation("pos", 0);
		shader->BindAttribLocation("uv", 1);
		shader->Link();

		shader->Enable();
		shader->SetUniform("tex", 0);
		shader->SetUniform("lod", 0.0f);
		shader->Disable();
		shader->Validate();
	}

	shaderRef++;
}

CTextureRenderAtlas::~CTextureRenderAtlas()
{
	RECOIL_DETAILED_TRACY_ZONE;
	shaderRef--;

	if (shaderRef == 0)
		shaderHandler->ReleaseProgramObjects("[TextureRenderAtlas]");

	for (auto& [_, tID] : nameToTexID) {
		if (tID) {
			glDeleteTextures(1, &tID);
			tID = 0;
		}
	}

	atlasTex = nullptr;
}

bool CTextureRenderAtlas::TextureExists(const std::string& texName)
{
	RECOIL_DETAILED_TRACY_ZONE;
	return finalized && nameToTexID.contains(texName);
}

bool CTextureRenderAtlas::TextureExists(const std::string& texName, const std::string& texBackupName)
{
	RECOIL_DETAILED_TRACY_ZONE;
	return finalized && (nameToTexID.contains(texName) || nameToTexID.contains(texBackupName));
}

bool CTextureRenderAtlas::AddTexFromFile(const std::string& name, const std::string& file)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (finalized)
		return false;

	if (nameToTexID.contains(name))
		return false;

	if (!CFileHandler::FileExists(name, SPRING_VFS_ALL))
		return false;

	CBitmap bm;
	if (!bm.Load(file))
		return false;

	return AddTexFromBitmapRaw(name, bm);
}

bool CTextureRenderAtlas::AddTexFromBitmap(const std::string& name, const CBitmap& bm)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (finalized)
		return false;

	if (nameToTexID.contains(name))
		return false;

	return AddTexFromBitmapRaw(name, bm);
}


bool CTextureRenderAtlas::AddTexFromBitmapRaw(const std::string& name, const CBitmap& bm)
{
	RECOIL_DETAILED_TRACY_ZONE;
	atlasAllocator->AddEntry(name, int2{ bm.xsize, bm.ysize });
	nameToTexID[name] = bm.CreateMipMapTexture();

	return true;
}


bool CTextureRenderAtlas::AddTex(const std::string& name, int xsize, int ysize, const SColor& color)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (finalized)
		return false;

	if (nameToTexID.contains(name))
		return false;

	CBitmap bm;
	bm.AllocDummy(color);
	bm = bm.CreateRescaled(xsize, ysize);

	return AddTexFromBitmapRaw(name, bm);
}

AtlasedTexture CTextureRenderAtlas::GetTexture(const std::string& texName)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!finalized)
		return AtlasedTexture::DefaultAtlasTexture;

	if (!nameToTexID.contains(texName))
		return AtlasedTexture::DefaultAtlasTexture;

	return AtlasedTexture(atlasAllocator->GetTexCoords(texName));
}

AtlasedTexture CTextureRenderAtlas::GetTexture(const std::string& texName, const std::string& texBackupName)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!finalized)
		return AtlasedTexture::DefaultAtlasTexture;

	if (nameToTexID.contains(texName))
		return AtlasedTexture(atlasAllocator->GetTexCoords(texName));

	if (texBackupName.empty())
		return AtlasedTexture::DefaultAtlasTexture;

	return GetTexture(texBackupName);
}

uint32_t CTextureRenderAtlas::GetTexTarget() const
{
	return (atlasAllocator->GetNumPages() > 1) ?
		GL_TEXTURE_2D_ARRAY :
		GL_TEXTURE_2D;
}

uint32_t CTextureRenderAtlas::GetTexID() const
{
	if (!finalized)
		return 0;

	return atlasTex->GetId();
}

int CTextureRenderAtlas::GetMinDim() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return atlasAllocator->GetMinDim();
}

int CTextureRenderAtlas::GetNumTexLevels() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return atlasAllocator->GetNumTexLevels();
}

void CTextureRenderAtlas::SetMaxTexLevel(int maxLevels)
{
	RECOIL_DETAILED_TRACY_ZONE;
	atlasAllocator->SetMaxTexLevel(maxLevels);
}

bool CTextureRenderAtlas::Finalize()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (finalized)
		return false;

	if (!FBO::IsSupported())
		return false;

	if (!atlasAllocator->Allocate())
		return false;

	const auto numLevels = atlasAllocator->GetNumTexLevels();
	const auto numPages = atlasAllocator->GetNumPages();

	const auto atlasSize = atlasAllocator->GetAtlasSize();
	{
		GL::TextureCreationParams tcp{
			//make function re-entrant
			.texID = atlasTex ? atlasTex->GetId() : 0,
			.reqNumLevels = numLevels,
			.linearMipMapFilter = true,
			.linearTextureFilter = true,
			.wrapMirror = false
		};

		if (numPages > 1) {
			atlasTex = std::make_unique<GL::Texture2DArray>(atlasSize, numPages, glInternalType, tcp, true);
		}
		else {
			atlasTex = std::make_unique<GL::Texture2D     >(atlasSize, glInternalType, tcp, true);
		}
	}
	{
		using namespace GL::State;
		auto state = GL::SubState(
			DepthTest(GL_FALSE),
			Blending(GL_FALSE),
			DepthMask(GL_FALSE)
		);

		auto& rb = RenderBuffer::GetTypedRenderBuffer<VA_TYPE_2DT>();

		FBO fbo;
		fbo.Init(false);
		fbo.Bind();
		if (numPages > 1)
			fbo.AttachTextureLayer(atlasTex->GetId(), GL_COLOR_ATTACHMENT0, 0, 0);
		else
			fbo.AttachTexture(atlasTex->GetId(), GL_TEXTURE_2D, GL_COLOR_ATTACHMENT0, 0);
		fbo.CheckStatus("TEXTURE-RENDER-ATLAS");
		finalized = fbo.IsValid();

		const auto Norm2SNorm = [](float value) { return (value * 2.0f - 1.0f); };

		for (uint32_t page = 0; page < numPages; ++page) {
			for (uint32_t level = 0; finalized && (level < numLevels); ++level) {
				glViewport(0, 0, std::max(atlasSize.x >> level, 1), std::max(atlasSize.y >> level, 1));

				if (numPages > 1)
					fbo.AttachTextureLayer(atlasTex->GetId(), GL_COLOR_ATTACHMENT0, level, page);
				else
					fbo.AttachTexture(atlasTex->GetId(), GL_TEXTURE_2D, GL_COLOR_ATTACHMENT0, level);

				glDrawBuffer(GL_COLOR_ATTACHMENT0);
				glReadBuffer(GL_COLOR_ATTACHMENT0);

				auto shEnToken = shader->EnableScoped();
				shader->SetUniform("lod", static_cast<float>(level));
				// draw
				for (auto& [name, entry] : atlasAllocator->GetEntries()) {
					if (entry.texCoords.pageNum != page)
						continue;

					const auto texID = nameToTexID[name];
					if (texID == 0)
						continue;

					const auto tc = atlasAllocator->GetTexCoords(name);

					VA_TYPE_2DT posTL = { .x = Norm2SNorm(tc.x1), .y = Norm2SNorm(tc.y1), .s = 0.0f, .t = 0.0f };
					VA_TYPE_2DT posTR = { .x = Norm2SNorm(tc.x2), .y = Norm2SNorm(tc.y1), .s = 1.0f, .t = 0.0f };
					VA_TYPE_2DT posBL = { .x = Norm2SNorm(tc.x1), .y = Norm2SNorm(tc.y2), .s = 0.0f, .t = 1.0f };
					VA_TYPE_2DT posBR = { .x = Norm2SNorm(tc.x2), .y = Norm2SNorm(tc.y2), .s = 1.0f, .t = 1.0f };

					auto texBind = GL::TexBind(GL_TEXTURE_2D, texID);

					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
					glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

					rb.AddQuadTriangles(
						std::move(posTL),
						std::move(posTR),
						std::move(posBR),
						std::move(posBL)
					);

					rb.DrawElements(GL_TRIANGLES);
				}
			}
		}

		fbo.DetachAll();
		FBO::Unbind();
		globalRendering->LoadViewport();
	}

	if (!finalized)
		return false;

	for (auto& [_, texID] : nameToTexID) {
		if (texID) {
			glDeleteTextures(1, &texID);
			texID = 0;
		}
	}

	//DumpTexture();

	return true;
}

bool CTextureRenderAtlas::IsValid() const
{
	return finalized && (atlasTex->GetId() > 0);
}

bool CTextureRenderAtlas::DumpTexture() const
{
	RECOIL_DETAILED_TRACY_ZONE;

	if (!IsValid())
		return false;

	const auto numLevels = atlasAllocator->GetNumTexLevels();
	const auto numPages = atlasAllocator->GetNumPages();

	if (numPages > 1) {
		for (uint32_t page = 0; page < numPages; ++page) {
			for (uint32_t level = 0; level < numLevels; ++level) {
				glSaveTextureArray(atlasTex->GetId(), fmt::format("{}_{}_{}.png", atlasName, page, level).c_str(), level, page);
			}
		}
	}
	else {
		for (uint32_t level = 0; level < numLevels; ++level) {
			glSaveTexture(atlasTex->GetId(), fmt::format("{}_{}.png", atlasName, level).c_str(), level);
		}
	}

	return true;
}

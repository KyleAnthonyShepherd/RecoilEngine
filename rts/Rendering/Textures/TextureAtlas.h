/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#pragma once

#include <string>
#include <vector>
#include <memory>

#include "IAtlasAllocator.h"
#include "AtlasedTexture.hpp"
#include "Texture.hpp"
#include "System/float4.h"
#include "System/type2.h"
#include "System/UnorderedMap.hpp"

/** @brief Class for combining multiple bitmaps into one large single bitmap. */
class CTextureAtlas
{
public:
	enum TextureType {
		RGBA32
	};
	enum AllocatorType {
		ATLAS_ALLOC_LEGACY      = 0,
		ATLAS_ALLOC_QUADTREE    = 1,
		ATLAS_ALLOC_ROW         = 2,
		ATLAS_ALLOC_MP_LEGACY   = 3,
		ATLAS_ALLOC_MP_QUADTREE = 4,
		ATLAS_ALLOC_MP_ROW      = 5
	};

public:
	CTextureAtlas(uint32_t allocType = ATLAS_ALLOC_LEGACY, int32_t atlasSizeX_ = 0, int32_t atlasSizeY_ = 0, const std::string& name_ = "", bool reloadable_ = false);
	CTextureAtlas(CTextureAtlas&& ta) noexcept { *this = std::move(ta); };
	CTextureAtlas(const CTextureAtlas&) = delete;

	~CTextureAtlas();

	void ReinitAllocator();

	CTextureAtlas& operator= (CTextureAtlas&& ta) noexcept {
		if (this != &ta) {
			delete atlasAllocator; // Free the existing atlasAllocator.

			atlasAllocator = ta.atlasAllocator;
			name = std::move(ta.name);
			memTextures = std::move(ta.memTextures);
			files = std::move(ta.files);
			textures = std::move(ta.textures);
			texToName = std::move(ta.texToName);
			atlasTex = std::move(ta.atlasTex);
			initialized = ta.initialized;

			allocType = ta.allocType;
			atlasSizeX = ta.atlasSizeX;
			atlasSizeY = ta.atlasSizeY;

			reloadable = ta.reloadable;

			// Trick to not call destructor on atlasAllocator multiple times
			ta.atlasAllocator = nullptr;
		}
		return *this;
	};
	CTextureAtlas& operator= (const CTextureAtlas&) = delete;

	// add a texture from a memory pointer
	size_t AddTexFromMem(std::string name, int xsize, int ysize, TextureType texType, const void* data);
	// add a texture from a file
	size_t AddTexFromFile(std::string name, const std::string& file);
	// add a blank texture
	size_t AddTex(std::string name, int xsize, int ysize, TextureType texType = RGBA32);

	void* AddGetTex(std::string name, int xsize, int ysize, TextureType texType = RGBA32) {
		const size_t idx = AddTex(std::move(name), xsize, ysize, texType);

		MemTex& tex = memTextures[idx];
		auto& mem = tex.mem;
		return (mem.data());
	}


	/**
	 * Creates the atlas containing all the specified textures.
	 * @return true if succeeded, false if not all textures did fit
	 *         into the specified maxsize.
	 */
	bool Finalize();

	/**
	 * @return a boolean true if the texture exists within
	 *         the "textures" map and false if it does not.
	 */
	bool TextureExists(const std::string& name);

	const spring::unordered_map<std::string, IAtlasAllocator::SAtlasEntry>& GetTextures() const;

	void ReloadTextures();
	void DumpTexture(const char* newFileName = nullptr) const;


	//! @return reference to the Texture struct of the specified texture
	AtlasedTexture& GetTexture(const std::string& name);

	// NOTE: safe with unordered_map after atlas has been finalized
	AtlasedTexture* GetTexturePtr(const std::string& name) { return &GetTexture(name); }

	/**
	 * @return a Texture struct of the specified texture if it exists,
	 *         otherwise return a backup texture.
	 */
	AtlasedTexture& GetTextureWithBackup(const std::string& name, const std::string& backupName);

	std::string GetTextureName(AtlasedTexture* tex);


	IAtlasAllocator* GetAllocator() { return atlasAllocator; }

	int2 GetSize() const;
	std::string GetName() const { return name; }

	uint32_t GetTexID() const { return atlasTex->GetId(); }
	uint32_t GetTexTarget() const;
	uint32_t GetNumPages() const;

	int GetNumTexLevels() const;
	void SetMaxTexLevel(int maxLevels);

	void BindTexture();
	void UnbindTexture();
	void DisOwnTexture() { atlasTex->DisOwn(); }
	void SetName(const std::string& s) { name = s; }

	static void SetDebug(bool b) { debug = b; }
	static bool GetDebug() { return debug; }

protected:
	int GetBPP(TextureType texType) const {
		switch (texType) {
			case RGBA32: return 32;
			default: return 32;
		}
	}
	bool CreateTexture();

protected:
	uint32_t allocType;
	int32_t atlasSizeX;
	int32_t atlasSizeY;

	bool reloadable;

	IAtlasAllocator* atlasAllocator = nullptr;

	struct MemTex {
	public:
		MemTex(): xsize(0), ysize(0), texType(RGBA32) {}
		MemTex(const MemTex&) = delete;
		MemTex(MemTex&& t) noexcept { *this = std::move(t); }

		MemTex& operator=(const MemTex&) = delete;
		MemTex& operator=(MemTex&& t) noexcept {
			xsize = t.xsize;
			ysize = t.ysize;
			texType = t.texType;

			names = std::move(t.names);
			mem = std::move(t.mem);

			return *this;
		}

	public:
		int xsize;
		int ysize;

		TextureType texType;

		std::vector<std::string> names;
		std::vector<uint8_t> mem;
	};

	std::string name;

	// temporary storage of all textures
	std::vector<MemTex> memTextures;

	spring::unordered_map<std::string, size_t> files;
	spring::unordered_map<std::string, AtlasedTexture> textures;
	spring::unordered_map<AtlasedTexture*, std::string> texToName;  // non-creg serialization

	std::unique_ptr<GL::TextureBase> atlasTex;

	bool initialized = false;

	// set to true to write finalized texture atlas to disk
	static inline bool debug = false;
};
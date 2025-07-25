/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

#ifndef FBO_H
#define FBO_H

#include <vector>
#include <array>

#include "myGL.h"
#include "System/UnorderedMap.hpp"

// TODO: add multisample buffers

/**
 * @brief FBO
 *
 * Framebuffer Object class (EXT_framebuffer_object).
 */
class FBO
{
public:
	/**
	 * @brief IsSupported
	 *
	 * if FrameBuffers are supported by the current platform
	 */
	static bool IsSupported();
	static GLint GetCurrentBoundFBO();

	FBO(         ) { Init(false); }
	explicit FBO(bool noop) { Init( noop); }
	~FBO() { Kill(); }

	void Init(bool noop);
	void Kill();

	uint32_t GetId() const { return fboId; }

	/**
	 * @brief fboId
	 *
	 * GLuint pointing to the current framebuffer
	 */
	GLuint fboId = 0;

	/**
	 * @brief reloadOnAltTab
	 *
	 * bool save all attachments in system RAM and reloaded them on OpenGL-Context lost (alt-tab) (default: false)
	 */
	bool reloadOnAltTab = false;

	/**
	 * @brief check FBO status
	 */
	bool CheckStatus(const char* name);

	/**
	 * @brief get FBO status
	 */
	GLenum GetStatus();

	/**
	 * @return GL_MAX_SAMPLES or 0 if multi-sampling not supported
	 */
	static GLsizei GetMaxSamples();

	/**
	 * @brief IsValid
	 * @return whether a valid framebuffer exists
	 */
	bool IsValid() const;


	void AttachTextures(const GLuint* ids, const GLenum* attachments, const GLenum texTarget, const unsigned int texCount, const int mipLevel = 0, const int zSlice = 0) {
		for (unsigned int i = 0; i < texCount; i++) {
			AttachTexture(ids[i], texTarget, attachments[i], mipLevel, zSlice);
		}
	}

	/**
	 * @brief AttachTexture
	 * @param texTarget texture target (GL_TEXTURE_2D etc.)
	 * @param texId texture to attach
	 * @param attachment (GL_COLOR_ATTACHMENT0_EXT etc.)
	 * @param mipLevel miplevel to attach
	 * @param zSlice z offset (3d textures only)
	 */
	void AttachTexture(const GLuint texId, const GLenum texTarget = GL_TEXTURE_2D, const GLenum attachment = GL_COLOR_ATTACHMENT0_EXT, const int mipLevel = 0, const int zSlice = 0);

	void AttachTextureLayer(const GLuint texId, const GLenum attachment = GL_COLOR_ATTACHMENT0_EXT, const int mipLevel = 0, const int layer = 0);

	/**
	 * @brief AttachRenderBuffer
	 * @param rboId RenderBuffer to attach
	 * @param attachment
	 */
	void AttachRenderBuffer(const GLuint rboId, const GLenum attachment = GL_COLOR_ATTACHMENT0_EXT);

	/**
	 * @brief Creates a RenderBufferObject and attaches it to the FBO (it is also auto destructed)
	 * @param attachment
	 * @param format
	 * @param width
	 * @param height
	 */
	void CreateRenderBuffer(const GLenum attachment, const GLenum format, const GLsizei width, const GLsizei height);

	/**
	 * @brief Creates a multisampled RenderBufferObject and attaches it to the FBO (it is also auto destructed)
	 * @param attachment
	 * @param format
	 * @param width
	 * @param height
	 * @param samples
	 */
	void CreateRenderBufferMultisample(const GLenum attachment, const GLenum format, const GLsizei width, const GLsizei height, GLsizei samples);

	/**
	 * @brief Detach
	 * @param attachment
	 */
	void Detach(const GLenum attachment);

	/**
	 * @brief DetachAll
	 */
	void DetachAll();

	/**
	 * @brief Bind
	 */
	void Bind();

	/**
	 * @brief Unbind
	 */
	static void Unbind();

	static bool Blit(
		int32_t fromID,
		int32_t toID,
		const std::array<int, 4>& srcRect,
		const std::array<int, 4>& dstRect,
		uint32_t mask = GL_DEPTH_BUFFER_BIT,
		uint32_t filter = GL_NEAREST
	);

	/**
	 * @brief GLContextLost (post atl-tab)
	 */
	static void GLContextLost();

	/**
	 * @brief GLContextReinit (pre atl-tab)
	 */
	static void GLContextReinit();


private:
	bool valid = false;

	/**
	 * @brief rbos
	 *
	 * List with all Renderbuffer Objects that should be destructed with the FBO
	 */
	std::vector<GLuint> rboIDs;


	struct TexData {
	public:
		TexData() { id = 0; }
		TexData(const TexData& td) { assert(td.id == 0); } // = delete;
		TexData(TexData&& td) {
			id = td.id;

			xsize = td.xsize;
			ysize = td.ysize;
			zsize = td.zsize;

			target = td.target;
			format = td.format;
			type   = td.type;

			pixels = std::move(td.pixels);
		}

	public:
		GLuint id;
		GLsizei xsize, ysize, zsize;
		GLenum target, format, type;
		std::vector<unsigned char> pixels;
	};

	static std::vector<FBO*> activeFBOs;
	static spring::unordered_map<GLuint, TexData> fboTexData;

	static GLint maxAttachments;
	static GLsizei maxSamples;

	/**
	 * @brief DownloadAttachment
	 *
	 * copies the attachment content to sysram
	 */
	static void DownloadAttachment(const GLenum attachment);

	/**
	 * @brief GetTextureTargetByID
	 *
	 * detects the textureTarget just by the textureName/ID
	 */
	static GLenum GetTextureTargetByID(const GLuint id, const unsigned int i = 0);
};

#endif /* FBO_H */

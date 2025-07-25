/* This file is part of the Spring engine (GPL v2 or later), see LICENSE.html */

/**
 * @brief EXT_framebuffer_object implementation
 * EXT_framebuffer_object class implementation
 */

#include <cassert>
#include <vector>

#include "FBO.h"
#include "System/ContainerUtil.h"
#include "System/Log/ILog.h"
#include "System/Config/ConfigHandler.h"

#include "System/Misc/TracyDefs.h"

CONFIG(bool, AtiSwapRBFix).defaultValue(false);

std::vector<FBO*> FBO::activeFBOs;
spring::unordered_map<GLuint, FBO::TexData> FBO::fboTexData;

GLint FBO::maxAttachments = 0;
GLsizei FBO::maxSamples = -1;


/**
 * Returns if the current gpu supports Framebuffer Objects
 */
bool FBO::IsSupported()
{
	return (GLAD_GL_EXT_framebuffer_object);
}


GLint FBO::GetCurrentBoundFBO()
{
	RECOIL_DETAILED_TRACY_ZONE;
	GLint curFBO;
	glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &curFBO);
	return curFBO;
}



/**
 * Detects the textureTarget just by the textureName/ID
 */
GLenum FBO::GetTextureTargetByID(const GLuint id, const unsigned int i)
{
	static constexpr std::array _targets = { GL_TEXTURE_2D, GL_TEXTURE_RECTANGLE_ARB, GL_TEXTURE_1D, GL_TEXTURE_3D, GL_TEXTURE_2D_ARRAY };
	GLint format;
	glBindTexture(_targets[i], id);
	glGetTexLevelParameteriv(_targets[i], 0, GL_TEXTURE_INTERNAL_FORMAT, &format);

	if (format != 1)
		return _targets[i];
	if (i < _targets.size() - 1)
		return GetTextureTargetByID(id, i + 1);
	return GL_INVALID_ENUM;
}


/**
 * Makes a copy of a texture/RBO in the system ram
 */
void FBO::DownloadAttachment(const GLenum attachment)
{
	RECOIL_DETAILED_TRACY_ZONE;
	GLuint target;
	GLuint id;

	glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, (GLint*) &target);
	glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, (GLint*) &id);

	if (target == GL_NONE || id == 0)
		return;

	if (fboTexData.find(id) != fboTexData.end())
		return;

	if (target == GL_TEXTURE) {
		target = GetTextureTargetByID(id);

		if (target == GL_INVALID_ENUM)
			return;
	}

	fboTexData.emplace(id, FBO::TexData{});
	FBO::TexData& tex = fboTexData[id];

	tex.id = id;
	tex.target = target;

	int bits = 0;

	if (target == GL_RENDERBUFFER_EXT) {
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, id);
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_WIDTH_EXT,  &tex.xsize);
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_HEIGHT_EXT, &tex.ysize);
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_INTERNAL_FORMAT_EXT, (GLint*)&tex.format);

		GLint _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_RED_SIZE_EXT, &_cbits); bits += _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_GREEN_SIZE_EXT, &_cbits); bits += _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_BLUE_SIZE_EXT, &_cbits); bits += _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_ALPHA_SIZE_EXT, &_cbits); bits += _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_DEPTH_SIZE_EXT, &_cbits); bits += _cbits;
		glGetRenderbufferParameterivEXT(GL_RENDERBUFFER_EXT, GL_RENDERBUFFER_STENCIL_SIZE_EXT, &_cbits); bits += _cbits;
	} else {
		glBindTexture(target, id);

		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_WIDTH, &tex.xsize);
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_HEIGHT, &tex.ysize);
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH, &tex.zsize);
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_INTERNAL_FORMAT, (GLint*)&tex.format);

		GLint _cbits;
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_RED_SIZE, &_cbits); bits += _cbits;
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_GREEN_SIZE, &_cbits); bits += _cbits;
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_BLUE_SIZE, &_cbits); bits += _cbits;
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_ALPHA_SIZE, &_cbits); bits += _cbits;
		glGetTexLevelParameteriv(target, 0, GL_TEXTURE_DEPTH_SIZE, &_cbits); bits += _cbits;
	}

	if (configHandler->GetBool("AtiSwapRBFix")) {
		if (tex.format == GL_RGBA) {
			tex.format = GL_BGRA;
		} else if (tex.format == GL_RGB) {
			tex.format = GL_BGR;
		}
	}

	if (bits < 32) /*FIXME*/
		bits = 32;

	switch (target) {
		case GL_TEXTURE_2D_ARRAY: [[fallthrough]];
		case GL_TEXTURE_3D:
			tex.pixels.resize(tex.xsize * tex.ysize * tex.zsize * (bits / 8));
			glGetTexImage(tex.target, 0, /*FIXME*/GL_RGBA, /*FIXME*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
			break;
		case GL_TEXTURE_1D:
			tex.pixels.resize(tex.xsize * (bits / 8));
			glGetTexImage(tex.target, 0, /*FIXME*/GL_RGBA, /*FIXME*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
			break;
		case GL_RENDERBUFFER_EXT:
			tex.pixels.resize(tex.xsize * tex.ysize * (bits / 8));
			glReadBuffer(attachment);
			glReadPixels(0, 0, tex.xsize, tex.ysize, /*FIXME*/GL_RGBA, /*FIXME*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
			break;
		default: //GL_TEXTURE_2D & GL_TEXTURE_RECTANGLE
			tex.pixels.resize(tex.xsize * tex.ysize * (bits / 8));
			glGetTexImage(tex.target, 0, /*FIXME*/GL_RGBA, /*FIXME*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
	}
}

/**
 * @brief GLContextLost
 */
void FBO::GLContextLost()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!IsSupported())
		return;

	GLint oldReadBuffer;

	for (FBO* fbo: activeFBOs) {
		if (!fbo->reloadOnAltTab)
			continue;

		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fbo->fboId);
		glGetIntegerv(GL_READ_BUFFER, &oldReadBuffer);

		for (int i = 0; i < maxAttachments; ++i) {
			DownloadAttachment(GL_COLOR_ATTACHMENT0_EXT + i);
		}
		DownloadAttachment(GL_DEPTH_ATTACHMENT_EXT);
		DownloadAttachment(GL_STENCIL_ATTACHMENT_EXT);

		glReadBuffer(oldReadBuffer);
	}

	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}


/**
 * @brief GLContextReinit
 */
void FBO::GLContextReinit()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!IsSupported())
		return;

	for (auto ti = fboTexData.begin(); ti != fboTexData.end(); ++ti) {
		const FBO::TexData& tex = ti->second;

		if (glIsTexture(tex.id)) {
			glBindTexture(tex.target, tex.id);

			// TODO regen mipmaps?
			switch (tex.target) {
				case GL_TEXTURE_2D_ARRAY: [[fallthrough]];
				case GL_TEXTURE_3D:
					// glTexSubImage3D(tex.target, 0, 0,0,0, tex.xsize, tex.ysize, tex.zsize, /*FIXME?*/GL_RGBA, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
					glTexImage3D(tex.target, 0, tex.format, tex.xsize, tex.ysize, tex.zsize, 0, /*FIXME?*/GL_RGBA, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
					break;
				case GL_TEXTURE_1D:
					// glTexSubImage1D(tex.target, 0, 0, tex.xsize, /*FIXME?*/GL_RGBA, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
					glTexImage1D(tex.target, 0, tex.format, tex.xsize, /*FIXME?*/GL_RGBA, 0, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
					break;
				default: // case GL_TEXTURE_2D & GL_TEXTURE_RECTANGLE
					// glTexSubImage2D(tex.target, 0, 0,0, tex.xsize, tex.ysize, /*FIXME?*/GL_RGBA, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
					glTexImage2D(tex.target, 0, tex.format, tex.xsize, tex.ysize, 0, /*FIXME?*/GL_RGBA, /*FIXME?*/GL_UNSIGNED_BYTE, &tex.pixels[0]);
			}
		} else if (glIsRenderbufferEXT(tex.id)) {
			// FIXME implement rendering buffer context init
		}
	}

	fboTexData.clear();
}


/**
 * Tests for support of the EXT_framebuffer_object
 * extension, and generates a framebuffer if supported
 */
void FBO::Init(bool noop)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (noop)
		return;
	if (!IsSupported())
		return;

	glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS_EXT, &maxAttachments);

	GetMaxSamples();

	glGenFramebuffersEXT(1, &fboId);

	// we need to bind it once, else it isn't valid
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);

	activeFBOs.push_back(this);

	valid = true;
}


/**
 * Unbinds the framebuffer and deletes it
 */
void FBO::Kill()
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (fboId == 0)
		return;
	if (!IsSupported())
		return;

	{
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, 0);

		for (auto ri = rboIDs.begin(); ri != rboIDs.end(); ++ri) {
			glDeleteRenderbuffersEXT(1, &(*ri));
		}

		rboIDs.clear();
	}
	{
		glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		glDeleteFramebuffersEXT(1, &fboId);

		fboId = 0;
	}

	spring::VectorErase(activeFBOs, this);

	if (!activeFBOs.empty())
		return;

	// we are the last fbo left, delete remaining alloc'ed stuff
	fboTexData.clear();
}


/**
 * Tests if we have a valid (generated and complete) framebuffer
 */
bool FBO::IsValid() const
{
	RECOIL_DETAILED_TRACY_ZONE;
	return (fboId != 0 && valid);
}


/**
 * Makes the framebuffer the active framebuffer context
 */
void FBO::Bind()
{
	RECOIL_DETAILED_TRACY_ZONE;
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboId);
}


/**
 * Unbinds the framebuffer from the current context
 */
void FBO::Unbind()
{
	RECOIL_DETAILED_TRACY_ZONE;
	// Bind is instance whereas Unbind is static (!),
	// this is cause Binding FBOs is a very expensive function
	// and so you want to save redundant FBO bindings when ever possible. e.g:
	// fbo1.Bind();
	//   do stuff
	// FBO::Unbind(); <- redundant!
	// fbo2.Bind();
	//   do stuff
	// FBO::Unbind(); <- not redundant!
	//   continue with screen FBO
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

bool FBO::Blit(int32_t fromID, int32_t toID, const std::array<int, 4>& srcRect, const std::array<int, 4>& dstRect, uint32_t mask, uint32_t filter)
{
	RECOIL_DETAILED_TRACY_ZONE;
	if (!GLAD_GL_EXT_framebuffer_blit)
		return false;

	if (srcRect[2] - srcRect[0] <= 0 || srcRect[3] - srcRect[1] <= 0)
		return false;

	if (dstRect[2] - dstRect[0] <= 0 || dstRect[3] - dstRect[1] <= 0)
		return false;

	GLint currentFBO;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT, &currentFBO);
	if (fromID < 0)
		fromID = currentFBO;

	glBindFramebufferEXT(GL_READ_FRAMEBUFFER_EXT, fromID);
	glBindFramebufferEXT(GL_DRAW_FRAMEBUFFER_EXT,   toID);

	int samples; int sampleBuffers;
	glGetFramebufferParameteriv(GL_READ_FRAMEBUFFER_EXT, GL_SAMPLES, &samples);
	glGetFramebufferParameteriv(GL_READ_FRAMEBUFFER_EXT, GL_SAMPLE_BUFFERS, &sampleBuffers);

	glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER_EXT, GL_SAMPLES, &samples);
	glGetFramebufferParameteriv(GL_DRAW_FRAMEBUFFER_EXT, GL_SAMPLE_BUFFERS, &sampleBuffers);

	glBlitFramebufferEXT(srcRect[0], srcRect[1], srcRect[2], srcRect[3], dstRect[0], dstRect[1], dstRect[2], dstRect[3], mask, filter);

	// required call
	// Calling glBindFramebuffer with target set to GL_FRAMEBUFFER binds framebuffer to both the read and draw framebuffer targets.
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, currentFBO);

	return true;
}


/**
 * Tests if the framebuffer is a complete and
 * legitimate framebuffer
 */
bool FBO::CheckStatus(const char* name)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);

	switch (status) {
		case GL_FRAMEBUFFER_COMPLETE_EXT:
			return (valid = true);
		case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT_EXT:
			LOG_L(L_WARNING, "FBO-%s: None/Unsupported textures/buffers attached!", name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT_EXT:
			LOG_L(L_WARNING, "FBO-%s: Missing a required texture/buffer attachment!", name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS_EXT:
			LOG_L(L_WARNING, "FBO-%s: Has mismatched texture/buffer dimensions!", name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_FORMATS_EXT:
			LOG_L(L_WARNING, "FBO-%s: Incomplete buffer formats!", name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER_EXT:
			LOG_L(L_WARNING, "FBO-%s: Incomplete draw buffers!", name);
			break;
		case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER_EXT:
			LOG_L(L_WARNING, "FBO-%s: Incomplete read buffer!", name);
			break;
		case GL_FRAMEBUFFER_UNSUPPORTED_EXT:
			LOG_L(L_WARNING, "FBO-%s: GL_FRAMEBUFFER_UNSUPPORTED_EXT", name);
			break;
		default:
			LOG_L(L_WARNING, "FBO-%s: error code 0x%X", name, status);
			break;
	}

	return (valid = false);
}


/**
 * Returns the current framebuffer status
 */
GLenum FBO::GetStatus()
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	return glCheckFramebufferStatus(GL_FRAMEBUFFER_EXT);
}


/**
 * Attaches a GL texture to the framebuffer
 */
void FBO::AttachTexture(const GLuint texId, const GLenum texTarget, const GLenum attachment, const int mipLevel, const int zSlice )
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	if (texTarget == GL_TEXTURE_1D) {
		glFramebufferTexture1DEXT(GL_FRAMEBUFFER_EXT, attachment, GL_TEXTURE_1D, texId, mipLevel);
	} else if (texTarget == GL_TEXTURE_3D) {
		glFramebufferTexture3DEXT(GL_FRAMEBUFFER_EXT, attachment, GL_TEXTURE_3D, texId, mipLevel, zSlice);
	} else if (texTarget == GL_TEXTURE_CUBE_MAP || texTarget == GL_TEXTURE_2D_ARRAY) {
		if (GLAD_GL_VERSION_3_2)
			glFramebufferTexture(GL_FRAMEBUFFER_EXT, attachment, texId, mipLevel); //attach the whole texture
	} else {
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment, texTarget, texId, mipLevel);
	}
}

void FBO::AttachTextureLayer(const GLuint texId, const GLenum attachment, const int mipLevel, const int layer)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif

	glFramebufferTextureLayerEXT(GL_FRAMEBUFFER_EXT, attachment, texId, mipLevel, layer);
}


/**
 * Attaches a GL RenderBuffer to the framebuffer
 */
void FBO::AttachRenderBuffer(const GLuint rboId, const GLenum attachment)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, attachment, GL_RENDERBUFFER_EXT, rboId);
}


/**
 * Detaches an attachment from the framebuffer
 */
void FBO::Detach(const GLenum attachment)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	GLuint target = 0;
	glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE_EXT, (GLint*) &target);

	if (target != GL_RENDERBUFFER_EXT) {
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, attachment, GL_TEXTURE_2D, 0, 0);
		return;
	}

	//! check if the RBO was created via FBO::CreateRenderBuffer()
	GLuint attID;
	glGetFramebufferAttachmentParameterivEXT(GL_FRAMEBUFFER_EXT, attachment, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME_EXT, (GLint*) &attID);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, attachment, GL_RENDERBUFFER_EXT, 0);

	spring::VectorEraseIf(rboIDs, [&](GLuint& rboID) {
		if (rboID != attID) return false;
		glDeleteRenderbuffersEXT(1, &rboID); return true;
	});
}


/**
 * Detaches any attachments from the framebuffer
 */
void FBO::DetachAll()
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	for (int i = 0; i < maxAttachments; ++i) {
		Detach(GL_COLOR_ATTACHMENT0_EXT + i);
	}
	Detach(GL_DEPTH_ATTACHMENT_EXT);
	Detach(GL_STENCIL_ATTACHMENT_EXT);
}


/**
 * Creates and attaches a RBO
 */
void FBO::CreateRenderBuffer(const GLenum attachment, const GLenum format, const GLsizei width, const GLsizei height)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	GLuint rbo;
	glGenRenderbuffersEXT(1, &rbo);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
	glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, format, width, height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, attachment, GL_RENDERBUFFER_EXT, rbo);
	rboIDs.push_back(rbo);
}


/**
 * Creates and attaches a multisampled RBO
 */
void FBO::CreateRenderBufferMultisample(const GLenum attachment, const GLenum format, const GLsizei width, const GLsizei height, GLsizei samples)
{
	RECOIL_DETAILED_TRACY_ZONE;
#ifndef HEADLESS
	assert(GetCurrentBoundFBO() == fboId);
#endif
	assert(maxSamples > 0);
	samples = std::min(samples, maxSamples);

	GLuint rbo;
	glGenRenderbuffersEXT(1, &rbo);
	glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, rbo);
	glRenderbufferStorageMultisampleEXT(GL_RENDERBUFFER_EXT, samples, format, width, height);
	glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, attachment, GL_RENDERBUFFER_EXT, rbo);
	rboIDs.push_back(rbo);
}

GLsizei FBO::GetMaxSamples()
{
	if (maxSamples >= 0)
		return maxSamples;
#ifdef HEADLESS
	maxSamples = 1;
#else
	// set maxSamples once
	if (maxSamples == -1) {
		glGetIntegerv(GL_MAX_SAMPLES_EXT, &maxSamples);
		maxSamples = std::max(0, maxSamples);
	}
#endif // HEADLESS
	return maxSamples;
}

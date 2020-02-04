#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "converter.hpp"

extern const GLchar *glsl;

YUYVtoYUV420P::YUYVtoYUV420P(
	X264Encoder& encoder,
	int width,
	int height,
	const char *device
) :
	mEncoder(encoder),
	mHeight(height),
	mWidth(width)
{
	// Compute shader operates on 2x16 pixel blocks
	if ((0 != mWidth % 16) || (0 != mHeight % 2)) {
		throw std::range_error(
			"width and height must be multiples of 16 and 2 respectively."
		);
	}

	mDRIHandle = open(device, O_RDWR);

	mGBM = gbm_create_device(mDRIHandle);

	// Initialize virtual display
	mEGLDisplay = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, mGBM, NULL);
	if (!eglInitialize(mEGLDisplay, NULL, NULL)) {
		throw std::runtime_error("Failed to initialize EGL display");
	}

	// Ensure specific extensions are supported by graphics device
	{
		const char *exts = eglQueryString(mEGLDisplay, EGL_EXTENSIONS);
		if (!strstr(exts, "EGL_KHR_create_context")) {
			throw std::runtime_error("Missing create context extension");
		}
		if (!strstr(exts, "EGL_KHR_surfaceless_context")) {
			throw std::runtime_error("Missing surfaceless context extension");
		}
	}

	// Configure EGL display
	EGLConfig cfg;
	{
		const EGLint attrs[] = {
			EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
			EGL_NONE
		};
		EGLint count;
		if (!eglChooseConfig(mEGLDisplay, attrs, &cfg, 1, &count)) {
			throw std::runtime_error("Failed to set EGL display");
		}
	}
	if (!eglBindAPI(EGL_OPENGL_ES_API)) {
		throw std::runtime_error("Failed to bind EGL API");
	}

	// Create OpenGL ES 3 context
	{
		const EGLint attrs[] = {
			EGL_CONTEXT_CLIENT_VERSION, 3,
			EGL_NONE
		};
		if (mEGLContext = eglCreateContext(
			mEGLDisplay,
			cfg,
			EGL_NO_CONTEXT,
			attrs
		), EGL_NO_CONTEXT == mEGLContext) {
			throw std::runtime_error("Failed to create EGL context");
		}
	}
	if (!eglMakeCurrent(
		mEGLDisplay,
		EGL_NO_SURFACE,
		EGL_NO_SURFACE,
		mEGLContext
	)) {
		throw std::runtime_error("Failed to set current EGL context");
	}

	// Setup compute shader
	GLuint shader = glCreateShader(GL_COMPUTE_SHADER);

	// Compute Shader (packed-to-planar conversion)
    glShaderSource(shader, 1, &glsl, NULL);
    assert(glGetError () == GL_NO_ERROR);

	glCompileShader(shader);
	assert(glGetError() == GL_NO_ERROR);

	mProgram = glCreateProgram();

	glAttachShader(mProgram, shader);
	assert(glGetError() == GL_NO_ERROR);

	glLinkProgram(mProgram);
	assert(glGetError() == GL_NO_ERROR);

	glDeleteShader(shader);
	assert(glGetError() == GL_NO_ERROR);

	// YUYV input buffer
	glGenBuffers(1, &mSSBOi);
	assert(glGetError() == GL_NO_ERROR);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mSSBOi);
	assert(glGetError() == GL_NO_ERROR);
	glBufferData(
		GL_SHADER_STORAGE_BUFFER,
		2 * mWidth*mHeight,
		NULL,
		GL_DYNAMIC_COPY
	);
	assert(glGetError() == GL_NO_ERROR);

	// YUV420P output buffer
	glGenBuffers(1, &mSSBOo);
	assert(glGetError() == GL_NO_ERROR);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, mSSBOo);
	assert(glGetError() == GL_NO_ERROR);
	glBufferData(
		GL_SHADER_STORAGE_BUFFER,
		mWidth*mHeight + (2 * (mWidth/2)*(mHeight/2)),
		NULL,
		GL_DYNAMIC_COPY
	);
	assert(glGetError() == GL_NO_ERROR);

	// Initialize input picture. Instead of using x264_picture_alloc(), use
	// x264_picture_init(), and set plane pointers to shader shared buffer
	// object for zero-copy access.
	x264_picture_init(&mPic);
	mPic.img.i_csp = X264_CSP_I420;
	mPic.img.i_stride[0] = width;
	mPic.img.i_stride[1] = width / 2;
	mPic.img.i_stride[2] = width / 2;
	mPic.img.i_plane = 3;
}

//////////////////////////////////////////////////////////////////////////////
//
// Convert YUYV to YUV420P
//
// YUYV is a packed format, where luma and chroma are interleaved, 8-bits per
// pixel:
//
//     YUYVYUYV...
//     YUYVYUYV...
//     ...
//
// Color is subsampled horizontally.
//
//
// YUV420 is a planar format, and the most common H.264 colorspace. For each
// 2x2 square of pixels, there are 4 luma values and 2 chroma values. Each
// value is 8-bits; however, there are 4*8 + 8 + 8 = 48 bits total for 4
// pixels, so on average there are effectively 12-bits per pixel:
//
// YYYY...	U.U..	V.V..
// YYYY...	.....	.....
// YYYY...	U.U..	V.V..
// YYYY...	.....	.....
// .......	.....	.....
//
// Arguments:
// y:      Pointer to planar destination buffer for luma.
// yuyv:   Pointer to packed source buffer.
// stride: Stride (in bytes) of source buffer.
//
//////////////////////////////////////////////////////////////////////////////

void YUYVtoYUV420P::convert(const uint8_t *yuyv) {
	// Copy input data into graphics device
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSSBOi);
	assert(glGetError() == GL_NO_ERROR);
	uint8_t *in = (uint8_t *) glMapBufferRange(
		GL_SHADER_STORAGE_BUFFER,
		0,
		2 * mWidth * mHeight,
		GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT
	);
	assert(in);
	memcpy(in, yuyv, 2 * mWidth * mHeight);
	assert(glUnmapBuffer(GL_SHADER_STORAGE_BUFFER) == GL_TRUE);

	// Select program
	glUseProgram(mProgram);
	assert(glGetError() == GL_NO_ERROR);

	// Run computation on graphics device
	glDispatchCompute(mWidth / 16, mHeight / 2, 1);
	assert(glGetError() == GL_NO_ERROR);

	// Wait until memory is valid
	glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
	assert(glGetError() == GL_NO_ERROR);

	// Copy output data from graphics device
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, mSSBOo);
	assert(glGetError() == GL_NO_ERROR);
	uint8_t *out = (uint8_t *) glMapBufferRange(
		GL_SHADER_STORAGE_BUFFER,
		0,
		mWidth * mHeight + (2 * (mWidth/2) * (mHeight/2)),
		GL_MAP_READ_BIT
	);
	assert(out);

	mPic.img.plane[0] = out;
	mPic.img.plane[1] = out + mWidth * mHeight;
	mPic.img.plane[2] = out + mWidth * mHeight + (mWidth/2)*(mHeight/2);

	mEncoder.encode(&mPic);

	assert(glUnmapBuffer(GL_SHADER_STORAGE_BUFFER) == GL_TRUE);
}

YUYVtoYUV420P::~YUYVtoYUV420P() {
	glDeleteProgram(mProgram);
	eglDestroyContext(mEGLDisplay, mEGLContext);
	eglTerminate(mEGLDisplay);
	gbm_device_destroy(mGBM);
	close(mDRIHandle);
}

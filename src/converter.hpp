#ifndef CORALVID_CONVERTER_H_
#define CORALVID_CONVERTER_H_

#include <EGL/egl.h>					// EGL
#include <EGL/eglext.h>
#include <GLES3/gl31.h>					// OpenGL ES 3.1
#include <gbm.h>						// libgbm

#include <inttypes.h>

#include "x264.hpp"

class YUYVtoYUV420P {
	X264Encoder&       mEncoder;

	const unsigned int mHeight, mWidth;

	int                mDRIHandle;
	struct gbm_device *mGBM;
	EGLDisplay         mEGLDisplay;
	EGLContext         mEGLContext;
	GLuint             mProgram;
	GLuint             mSSBOi;
	GLuint             mSSBOo;

	x264_picture_t     mPic;
	x264_picture_t     mPicOut;

	public:
		YUYVtoYUV420P(
			X264Encoder& encoder,
			int width,
			int height,
			const char *device = "/dev/dri/card0"
		);
		~YUYVtoYUV420P();
		void convert(const uint8_t *yuyv);
};

#endif // CORALVID_CONVERTER_H_

#ifndef CORALVID_X264_H_
#define CORALVID_X264_H_

#include <stdexcept>

#include <x264.h>

// Wrapper for x264_picture_t
class X264Picture {
	x264_picture_t mPicture;

	public:
		X264Picture(int width, int height, int csp = X264_CSP_I420) {
			if (!x264_picture_alloc(&mPicture, csp, width, height)) {
				throw std::runtime_error("failed to allocate x264 picture");
			}
		}

		~X264Picture() {
			x264_picture_clean(&mPicture);
		}
};

class X264Encoder {
	FILE *mFilePointer;
	x264_t *mEncoder;
	x264_param_t mParam;
	int mPts;

	public:
		X264Encoder(
			int width,
			int height,
			int kbps,
			int fps,
			const char *profile,
			FILE *fp
		) : mFilePointer(fp), mPts(0) {
			// Load default parameters for ultrafast encoding
			if (0 != x264_param_default_preset(
				&mParam,
				"ultrafast",
				NULL
			)) {
				throw std::runtime_error("failed to load ultrafast presets");
			}

			// Configure non-default parameters
			mParam.i_csp = X264_CSP_I420;
			mParam.i_width = width;
			mParam.i_height = height;
			mParam.i_threads = 0;
			mParam.b_vfr_input = 0;
			mParam.b_repeat_headers = 1;
			mParam.b_annexb = 1;
			mParam.i_keyint_max = fps;
			mParam.rc.i_rc_method = X264_RC_CRF;
			mParam.rc.i_bitrate = kbps;
			mParam.rc.i_vbv_max_bitrate = kbps;
			mParam.rc.i_vbv_buffer_size = 2 * kbps;
			mParam.rc.f_rf_constant = 2.;
			mParam.i_fps_num = fps;
			mParam.i_fps_den = 1;
		
			// Apply profile restriction
			if (0 != x264_param_apply_profile(&mParam, profile)) {
				throw std::runtime_error("failed to set profile");
			}

			// Open encoder
			if (mEncoder = x264_encoder_open(&mParam), !mEncoder) {
				throw std::runtime_error("failed to open encoder");
			}
		}

		void encode(x264_picture_t *pic) {
			x264_nal_t *nal = NULL;
			int i_nal = 0;

			x264_picture_t pic_out;

			pic->i_pts = mPts;
			mPts++;

			int n = x264_encoder_encode(
				mEncoder,
				&nal,
				&i_nal,
				pic,
				&pic_out
			);

			if (nal) {
				fwrite(nal->p_payload, n, 1, mFilePointer);
			}
		}

		~X264Encoder() {
			x264_encoder_close(mEncoder);
		}
};

#endif // CORALVID_X264_H_

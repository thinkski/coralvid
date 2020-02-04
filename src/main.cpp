#include <cassert>  // assert
#include <iostream> // cout

#include <cstring>  // memset

#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/videodev2.h>		// video4linux2

#include "config.hpp"
#include "converter.hpp"

void help(void) {
	std::cout << "Capture H264 from CSI/MIPI camera at the requested bitrate"
	          << std::endl << std::endl
	          << "usage: coralvid [options]" << std::endl
	          << std::endl
	          << "Options:" << std::endl
	          << "  -b, --bitrate=<int>   Bitrate in Kbps (default: 1000)"
	          << std::endl
	          << "  -f, --fps=<int>       Frame rate (default: 30)"
	          << std::endl
	          << "  -h, --height=<int>    Frame height (default: 720)"
	          << std::endl
	          << "  -w, --width=<int>     Frame width (default: 1280)"
	          << std::endl
	          << "  -i, --input=<device>  Input device (default: /dev/video0)"
	          << std::endl
	          << "  -n, --num-buffers=<n> Number of video buffers (default: 4)"
	          << std::endl
	          << "  -o, --output=<file>   Output file (default: stdout)"
	          << std::endl
	          << "  -p, --profile=<str>   H.264 profile (default: baseline)"
	          << std::endl
	          << "  -t, --timeout=<int>   Seconds to capture (default: 10)"
	          << std::endl
	          << "      --autofocus       Enable autofocus"
	          << std::endl
	          << "      --help            Print this message"
	          << std::endl
	          << "  -v, --version         Print version"
	          << std::endl
	          << "      --verbose         Print frames/sec information"
	          << std::endl;
	exit(0);
}

void version(void) {
	std::cout << "coralvid v" << CORALVID_VERSION_STRING << std::endl
	          << "Copyright (c) 2019 Chris Hiszpanski. All rights reserved."
	          << std::endl;
	exit(0);
}

int main(int argc, char **argv) {
	int retval = 0;

	const char * const kDefaultProfile = "baseline";
	const char * const kDefaultDeviceName = "/dev/video0";

	FILE *output = stdout;
	std::string oProfile(kDefaultProfile);
	char *devname = (char *)kDefaultDeviceName;
	int num_buffers = 4;
	int oWidth = 1280;
	int oHeight = 720;
	int oKbps = 1000;
	int oFps = 30;
	bool verbose = false;
	bool oAutofocus = false;
	int oSeconds = 10;

	// Parse arguments
	while (1) {
		static struct option long_options[] = {
			{ "bitrate", required_argument,     0, 'b'  },
			{ "fps", required_argument,         0, 'f'  },
			{ "num-buffers", required_argument, 0, 'n'  },
			{ "height", required_argument,      0, 'h'  },
			{ "help",  no_argument,             0, 1001 },
			{ "input", required_argument,       0, 'i'  },
			{ "output", required_argument,      0, 'o'  },
			{ "profile", required_argument,     0, 'p'  },
			{ "autofocus", no_argument,         0, 1003 },
			{ "timeout", required_argument,     0, 't'  },
			{ "width", required_argument,       0, 'w'  },
			{ "version", no_argument,           0, 'v'  },
			{ "verbose", no_argument,           0, 1002 },
			{  NULL,   0,                       0,  0   }
		};

		int option_index = 0;

		int c = getopt_long(
			argc,
			argv,
			"b:f:h:i:n:o:p:t:vw:",
			long_options,
			&option_index
		);

		if (c == -1) break;

		switch (c) {
			case 'h':
				oHeight = atoi(optarg);
				break;
			case 1001:
				help();
				break;
			case 1002:
				verbose = true;
				break;
			case 1003:
				oAutofocus = true;
				break;
			case 'b':
				oKbps = atoi(optarg);
				break;
			case 'f':
				oFps = atoi(optarg);
				break;
			case 'i':
				devname = optarg;
				break;
			case 'o':
				if (output = fopen(optarg, "w"), !output) {
					perror("fopen");
					exit(1);
				}
				break;
			case 'n':
				num_buffers = atoi(optarg);
				break;
			case 'p':
				oProfile = std::string(optarg);
				break;
			case 't':
				oSeconds = atoi(optarg);
				break;
			case 'v':
				version();
				break;
			case 'w':
				oWidth = atoi(optarg);
				break;
			case '?':
				exit(1);
				break;
		}
	}

	// Create encoder
	X264Encoder encoder(oWidth, oHeight, oKbps, oFps, oProfile.c_str(), output);

	// Open device
	int fd = open(devname, O_RDWR);
	if (fd == -1) {
		perror("open");
		return fd;
	}

	// Query capabilities
	struct v4l2_capability cap;
	if (retval = ioctl(fd, VIDIOC_QUERYCAP, &cap), -1 == retval) {
		perror("VIDIOC_QUERYCAP");
		return retval;
	}

	if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
		fprintf(stderr, "V4L2_CAP_VIDEO_CAPTURE not supported\n");
		exit(1);
	}

	if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
		fprintf(stderr, "V4L2_CAP_STREAMING not supported\n");
		exit(1);
	}

	// Set format
	struct v4l2_format fmt = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE
	};
	fmt.fmt.pix.width = oWidth;
	fmt.fmt.pix.height = oHeight;
	fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
	fmt.fmt.pix.field = V4L2_FIELD_NONE;
	retval = ioctl(fd, VIDIOC_S_FMT, &fmt);
	if (retval == -1) {
		perror("VIDIOC_S_FMT");
		return retval;
	}

	// Get param
	struct v4l2_streamparm parm = {
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE
	};
	retval = ioctl(fd, VIDIOC_G_PARM, &parm);
	if (retval == -1) {
		perror("VIDIOC_G_PARM");
		return retval;
	}

	// Set parm
	parm.parm.capture.timeperframe.numerator = 1;
	parm.parm.capture.timeperframe.denominator = oFps;
	retval = ioctl(fd, VIDIOC_S_PARM, &parm);
	if (retval == -1) {
		perror("VIDIOC_S_PARM");
		return retval;
	}

	// Request buffer
	struct v4l2_requestbuffers rb = {
		.count = static_cast<unsigned int>(num_buffers),
		.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
		.memory = V4L2_MEMORY_MMAP,
	};
	retval = ioctl(fd, VIDIOC_REQBUFS, &rb);
	if (retval == -1) {
		perror("VIDIOC_REQBUFS");
		return retval;
	}

	void * buf[rb.count];
	for (int i = 0; i < rb.count; i++) {
		// Query buffer
		struct v4l2_buffer qb;
		memset(&qb, 0, sizeof(qb));
		qb.index = i;
		qb.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		qb.memory = V4L2_MEMORY_MMAP;

		retval = ioctl(fd, VIDIOC_QUERYBUF, &qb);
		if (retval == -1) {
			perror("VIDIOC_QUERYBUF");
			return retval;
		}
	
		// Memory map
		buf[i] = mmap(NULL, qb.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, qb.m.offset);
		if (buf == MAP_FAILED) {
			perror("mmap");
			return 1;
		}

		memset(buf[i], 0, qb.length);
	
		// Enqueue buffer
		struct v4l2_buffer qbuf = {};
		qbuf.index = i;
		qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
		qbuf.memory = V4L2_MEMORY_MMAP;
		retval = ioctl(fd, VIDIOC_QBUF, &qbuf);
		if (retval == -1) {
			perror("VIDIOC_QBUF");
			return retval;
		}
	}

	// Start
	retval = ioctl(fd, VIDIOC_STREAMON, &rb.type);
	if (retval == -1) {
		perror("VIDIOC_STREAMON");
		return retval;
	}

	// Disable autofocus
	if (!oAutofocus) {
		int fd = open(
			"/sys/module/ov5645_camera_mipi_v2/parameters/ov5645_af", O_WRONLY
		);
		assert(fd >= 0);
		assert(2 == write(fd, "1", 2));
		assert(0 == close(fd));
	}

	YUYVtoYUV420P converter(encoder, oWidth, oHeight);

	int ii = 0;
	int i = 0;
	struct timeval start, stop;
	gettimeofday(&start, NULL);
	while (1) {
		{
			struct v4l2_buffer dqbuf;

			// Dequeue buffer
			if (verbose) {
				fprintf(stderr, "dequeuing frame...");
			}
			memset(&dqbuf, 0, sizeof(dqbuf));

			dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
			dqbuf.memory = V4L2_MEMORY_MMAP;
			dqbuf.index = i;

			retval = ioctl(fd, VIDIOC_DQBUF, &dqbuf);
			if (retval == -1) {
				perror("VIDIOC_DQBUF");
				return retval;
			}

			// Convert from YUYV to YUV420P pixel format
			converter.convert(static_cast<uint8_t *>(buf[i]));

			// Re-enqueue buffer
			retval = ioctl(fd, VIDIOC_QBUF, &dqbuf);
			if (retval == -1) {
				perror("VIDIOC_QBUF");
				return retval;
			}

			if (verbose) {
				gettimeofday(&stop, NULL);
				fprintf(stderr, "%0.2f frames/sec\n",
					1e6 / ((stop.tv_sec - start.tv_sec) * 1e6 + (stop.tv_usec - start.tv_usec))
				);
				gettimeofday(&start, NULL);
			}
		}

		i = (i + 1) % num_buffers;

		if (oSeconds && ii++ >= oSeconds * oFps) {
			break;
		}
	}

	retval = ioctl(fd, VIDIOC_STREAMOFF, &rb.type);
	if (retval == -1) {
		perror("VIDIOC_STREAMOFF");
		return retval;
	}

	return retval;
}

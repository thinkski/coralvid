coralvid
========

Command line tool for capturing video with the Google Coral EdgeTPU camera
module.

## Quickstart

Build:
```
mendel@edgetpu:~/coralvid$ mkdir build
mendel@edgetpu:~/coralvid$ cd build
mendel@edgetpu:~/coralvid/build$ cmake -DCMAKE_BUILD_TYPE=Release ..
mendel@edgetpu:~/coralvid/build$ make
mendel@edgetpu:~/coralvid/build$ sudo make install
```
The above is for on-target builds. Cross-compilation is left as an exercise to the reader.

Print usage:
```
mendel@edgetpu:~$ coralvid --help
Capture H264 from CSI/MIPI camera at the requested bitrate

usage: coralvid [options]

Options:
  -b, --bitrate=<int>   Bitrate in Kbps (default: 1000)
  -f, --fps=<int>       Frame rate (default: 30)
  -h, --height=<int>    Frame height (default: 720)
  -w, --width=<int>     Frame width (default: 1280)
  -i, --input=<device>  Input device (default: /dev/video0)
  -n, --num-buffers=<n> Number of video buffers (default: 4)
  -o, --output=<file>   Output file (default: stdout)
  -p, --profile=<str>   H.264 profile (default: baseline)
  -t, --timeout=<int>   Seconds to capture (default: 10)
      --autofocus       Enable autofocus
      --help            Print this message
  -v, --version         Print version
      --verbose         Print frames/sec information
```

Capture sample:
```
mendel@edgetpu:~$ coralvid -o sample.264
```

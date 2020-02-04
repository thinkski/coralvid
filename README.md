coralvid
========

Command line tool for capturing video with the Google Coral EdgeTPU camera
module.

## Quickstart

Build, then capture a 10 second H.264 video clip:

```
mkdir build && cd build
cmake .. && make
src/coralvid --output=sample.264
```

Play with `ffplay sample.264`

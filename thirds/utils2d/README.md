# Utils2d dependencies

Utils2d uses the installed OpenCV 4 package and requires the `core`, `imgproc`
and `imgcodecs` components. OpenCV is not downloaded automatically because it is
normally supplied by the application, operating system or an imaging SDK.

Configure with `OpenCV_DIR` or `CMAKE_PREFIX_PATH` when CMake cannot discover it.

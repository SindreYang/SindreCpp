# Utils2d module guidance

Utils2d is a thin C++17/OpenCV adapter. Include `utils2d/index.hpp`
and link `SindreCpp::Utils2d`. It requires OpenCV 4 components `core`,
`imgproc`, and `imgcodecs`.

The current API is exception-based for invalid image operations; new APIs
should converge on General Result without changing the existing module
contract implicitly. Keep path conversion and image validation in this module.

#pragma once

// VTK is exposed through one vedo-style core entry point. The individual
// headers remain available for projects that need a smaller include surface.
#include "mesh.hpp"

#if defined(SINDRECPP_UTILS3D_VTK_DATA)
#include "data.hpp"
#include "image.hpp"
#endif

#if defined(SINDRECPP_UTILS3D_SHOW)
#include "show.hpp"
#if defined(SINDRECPP_UTILS3D_VTK_DATA)
#include "plot.hpp"
#endif
#endif

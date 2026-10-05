#pragma once

// VTK is exposed through one vedo-style core entry point. The individual
// headers remain available for projects that need a smaller include surface.
#include "sindremesh.hpp"

#if defined(SINDRECPP_UTILS3D_VTK_DATA)
#include "sindredata.hpp"
#include "sindreimage.hpp"
#endif

#if defined(SINDRECPP_UTILS3D_SHOW)
#include "show_mesh.hpp"
#if defined(SINDRECPP_UTILS3D_VTK_DATA)
#include "show_plot.hpp"
#endif
#endif


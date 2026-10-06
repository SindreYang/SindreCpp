# Utils3d module

Enable with `SINDRECPP_WITH_UTILS3D=ON`. The module discovers VTK 9 and Eigen3;
it can fetch Eigen when no package is available. `SINDRECPP_UTILS3D_SHOW` adds
viewer components, while `SINDRECPP_UTILS3D_VTK_DATA` adds data/image helpers.

Tests are `sindrecpp.utils3d` and `sindrecpp.mesh`, with additional targets for
show/data features. Keep heavyweight backend checks opt-in and document any
new SDK-specific include ordering in this file and `AGENTS.md`.

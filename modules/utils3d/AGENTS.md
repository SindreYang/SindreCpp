# Utils3d module guidance

Utils3d owns Eigen/VTK mesh and visualization helpers. Include
`utils3d/index.hpp` for the aggregate surface; use the `core/` and
algorithm headers only when a narrower include is intentional.

The base module requires Eigen3 and VTK 9. Show, VTK-data, BLAS, CGAL, Open3D,
MeshLib, libigl, and VCG support are independently controlled by CMake options.
Do not expose SDK dependency roots broadly; attach only the include paths and
targets required by the selected feature.

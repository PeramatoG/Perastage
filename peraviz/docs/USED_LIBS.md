# Peraviz: reused Perastage modules

## Modules used in the MVR proxy milestone

- `models/types.h` and `models/matrixutils.h`  
  Used to parse MVR matrices (`MatrixUtils::ParseMatrix`), compose hierarchical transforms (`MatrixUtils::Multiply`), and extract Euler angles for proxy placement.

- MVR schema compatibility from `mvr/`  
  The native Peraviz loader follows the same node structure used by Perastage (`GeneralSceneDescription -> Scene -> Layers -> ChildList`) for fixtures, trusses, supports, and scene objects.

## Notes

- This milestone still does not reuse the full geometry/material pipeline. It reuses the transformation and spatial parsing foundation to validate axis and unit conversion first.

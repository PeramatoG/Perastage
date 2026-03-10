# Peraviz gobo vectorization assessment and C++ module proposal

## Goal
Assess how gobo vectorization is currently implemented in Peraviz (Godot), compare it against Perastage symbol vectorization (including the SVG symbol generation workflow), and evaluate the feasibility of introducing a dedicated C++ gobo vectorization module.

## 1) Current implementation in Peraviz (Godot)

Peraviz currently vectorizes gobos in `gobo_prism_mesh_builder.gd` through a **bitmap-to-polygon** pipeline:

1. Convert gobo texture to `RGBA8` and downscale to a max size (`VECTORIZATION_MAX_SIZE = 192`).
2. Optional mask cleanup (`_prepare_binary_mask_image`) applies border clipping and binary thresholding.
3. Use `BitMap.create_from_image_alpha(..., VECTORIZATION_ALPHA_THRESHOLD)`.
4. Extract polygons with `BitMap.opaque_to_polygons(..., VECTORIZATION_EPSILON)`.
5. Normalize, rotate, scale, and Y-flip points.
6. Apply iterative RDP-style simplification and hard cap total points.
7. Extrude polygons to a 3D beam prism mesh.

### Practical consequences

- This is **fast and simple**, but it is fundamentally raster contour extraction.
- Smooth curves are represented as dense piecewise-linear boundaries and later simplified; this can visibly degrade circles into jagged polygons.
- There is no explicit representation of:
  - curve primitives (arc/cubic/quadratic);
  - topological vertex identity (shared vertices across contours);
  - canonical contour direction (CW/CCW policy beyond implicit area sign);
  - edge semantic orientation metadata.

## 2) Perastage symbol vectorization workflow (reference)

Perastage has a richer symbol workflow:

- `Symbol2DImageBuilder` builds vector symbols from rendered RGBA images using:
  - fill/background separation;
  - contour extraction for fills with hole ownership inference;
  - stroke extraction via skeletonization (Zhang-Suen) and graph traversal.
- `SymbolGeometrySimplifier` applies RDP simplification to strokes and closed polygon rings.
- The symbol preview/export pipeline writes SVG using polygon/polyline primitives.

### Strengths vs Peraviz current gobo vectorization

- Better separation of **fill geometry** and **stroke geometry**.
- Hole handling and ownership are explicit in polygon-with-holes structures.
- The process already supports production usage in a symbol generation tool and export flow.

### Shared limitation with Peraviz

- Geometry is still polygon/polyline based after raster analysis.
- There is no native curve-fitting stage that emits arc/bezier primitives.
- Therefore, both pipelines can approximate circles as segmented paths unless source data is already analytic SVG.

## 3) Why circles become jagged in Peraviz gobos

The observed issue (circle turning into serrated polygon) is consistent with the current stack:

- Downscaling to max 192 px reduces high-frequency boundary fidelity.
- Binary thresholding introduces staircase boundaries on anti-aliased edges.
- `opaque_to_polygons` emits linear contours.
- Point-reduction then removes vertices based on epsilon, which can break smoothness if tuned for budget.
- Final mesh directly follows the resulting piecewise-linear contour.

So the artifact is not a rendering bug by itself; it is an expected geometric consequence of the chosen vectorization method.

## 4) Feasibility of a dedicated C++ gobo vectorization module

## Recommendation

**Yes, it is feasible and technically justified** to create a dedicated C++ module for gobo vectorization, especially if the objective is preserving smooth geometry and topological semantics.

A native module can provide:

- deterministic topology extraction;
- curve detection/fitting (arc/bezier) before polygonization;
- stable vertex/edge graph with shared-vertex identity;
- consistent winding and directional metadata;
- configurable tessellation quality for GPU mesh generation.

### Proposed module scope

Suggested module name and ownership:

- `core/gobo_vectorization/` (engine-agnostic geometry logic)
- optional Peraviz adapter in `peraviz/native/` to expose API to Godot.

Suggested C++ API output model:

- `GoboVectorShape`
  - `std::vector<Contour>`
- `Contour`
  - ordered `Segments` (Line/Arc/Bezier)
  - `winding` (CW/CCW)
  - `is_hole`
- `TopologyGraph`
  - shared vertices (id + position)
  - half-edges with direction/sense

This model directly supports your request for curves, shared vertices, line direction, and orientation.

## 5) Integration strategy (incremental, low risk)

1. **Phase 1 (parity mode)**
   - Move current bitmap-to-polygon logic to C++ with equivalent output.
   - Keep Godot-side behavior visually identical.
2. **Phase 2 (curve-aware mode, opt-in)**
   - Add contour smoothing + analytic fitting (arcs/beziers).
   - Keep quality/performance knobs exposed.
3. **Phase 3 (topology-aware mesh generation)**
   - Build extrusion from contour graph with explicit winding and edge direction.
   - Add robust handling of holes and nested contours.
4. **Phase 4 (regression harness)**
   - Golden images and geometry snapshots for representative production gobos.
   - Numeric checks: area error, Hausdorff-like contour deviation, vertex count budget.

## 6) Risks and mitigations

- **Performance risk**: curve fitting/topology can be heavier.
  - Mitigate with multi-tier quality presets and cache keying.
- **Behavior drift**: existing fixtures may change look.
  - Mitigate with compatibility mode and A/B toggles per fixture/profile.
- **Complexity risk**: adding geometry kernel increases maintenance.
  - Mitigate by keeping module boundary strict and documenting contracts.

## 7) Decision summary

- The current Peraviz vectorization path is fit for speed and broad compatibility but not for high-fidelity smooth contours.
- Perastage symbol tooling demonstrates stronger 2D extraction architecture, but still primarily polygonal.
- A dedicated C++ gobo-vectorization module is a solid next step if quality and geometric semantics are priorities.
- The best path is incremental: parity first, then curve/topology features behind guarded rollout flags.

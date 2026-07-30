# Viewer coordinate and placement contract

The viewer interaction code uses the following authoritative units and state.

- Mouse events provide top-origin **logical window pixels**. They are converted
  once to top-origin **physical framebuffer pixels** before projection or
  picking. The framebuffer size, rather than the logical client size, defines
  the OpenGL viewport.
- Viewer2D world coordinates are metres. `zoom` is a dimensionless multiplier
  of the base scale of 25 framebuffer pixels per metre. The stored pan offsets
  are pixels at zoom 1; therefore their visible framebuffer translation is the
  stored offset multiplied by zoom.
- `viewer2d_coordinate_math` owns the inverse world/framebuffer formulas for
  Top, Bottom, Front, and Side. The hidden coordinate is zero when unprojecting.
  Rendering uses the equivalent orthographic bounds, and overlays and pointer
  interaction call this utility rather than maintaining another formula.
- Viewer3D world coordinates are metres. Pointer placement intersects the ray
  from the current camera matrices with the view plane through the active
  selection-drag anchor. Logical pointer coordinates are scaled to framebuffer
  coordinates before unprojection.
- A continuous-placement anchor is the raw, unsnapped element origin in world
  metres. A selection-drag anchor is the effective selection centre in world
  metres. A Magnet preview is restored before either anchor is updated.
- Both viewers maintain a viewport revision. Zoom, pan, orbit, view changes,
  fit/reset operations, camera interpolation, resize, and framebuffer changes
  invalidate pointer alignment. The next safe update restores any preview and
  aligns from the absolute current pointer under the new mapping; it never
  applies a pixel delta calculated with the old revision.

The view-dependent hidden-axis Magnet weights are separate from projection and
remain intentional: 2D ignores the hidden axis, while 3D progressively reduces
the camera-aligned axis weight near an orthogonal view.

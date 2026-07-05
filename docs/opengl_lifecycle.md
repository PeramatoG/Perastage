# OpenGL lifecycle ownership

Perastage centralizes wxGLCanvas configuration and OpenGL context lifecycle helpers in `viewer_common`:

- `viewer_common/gl_canvas_config.*` owns the standard canvas attribute lists used by wxGLCanvas-based panels, including the MSAA-capable selection used by the 3D viewer.
- `viewer_common/gl_context_utils.*` owns safe context binding diagnostics and the shared GLEW initialization wrapper.

Viewer3DPanel, Viewer2DPanel, LayoutViewerPanel, and FixturePreviewPanel should use these helpers instead of defining local canvas attribute arrays or ad-hoc context binding diagnostics. Renderer code remains owned by the individual viewer modules.

## Linux diagnostics

The shared GLEW initialization path logs Linux backend details once per process for diagnostics:

- `GDK_BACKEND`
- `WAYLAND_DISPLAY`
- `DISPLAY`
- `XDG_SESSION_TYPE`
- `QT_QPA_PLATFORM`

After a valid OpenGL context exists, the same path records OpenGL vendor, renderer, and version information. These diagnostics help distinguish native Wayland, XWayland, GLX, and driver-related failures without changing rendering behavior.

Native Wayland OpenGL support still depends on wxGTK being built with EGL support. This refactor only makes context creation and diagnostics consistent; it does not rewrite renderers, change visual output, or solve layout offscreen rasterization.

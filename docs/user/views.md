# Views (2D and 3D)

Perastage combines 3D scene inspection with 2D layout-oriented review.

## 3D view

Use 3D when you need spatial understanding.

Typical checks:

- Position and orientation.
- Relationships between fixtures, trusses, hoists, and objects.
- Group membership while hovering scene items: the hovered item uses the primary hover highlight, and other members of the same group use a paler related-green highlight in the 3D view and related tables, with table rows styled like selected rows.
- Group selection while clicking scene items: clicking a grouped member selects its full group across related tables.
- General scene structure before export.
- Dragging feedback: while moving scene elements in 2D or with the 3D gizmo, the bottom X/Y/Z status readout shows the dragged insertion-point position with a highlighted font color until the mouse is released.
- Drag Move: the **Drag Move** toolbar toggle, shown with the Move icon, enables moving scene selections by holding the left mouse button over an element and dragging. It is disabled by default so left-dragging pans the viewport in dense scenes unless the user enables selection dragging, and the setting is stored in the project.
- Axis lock: the **Axis Lock** toolbar toggle, shown with the Move 3D icon, is enabled by default and stores its state in the project. When disabled, 3D selection dragging follows a plane parallel to the current camera view through the selection origin, similar to Blender free move.
- Cross-table actions: the **Cross-table Actions** toolbar toggle, shown with the Layers icon, is disabled by default and stores its state in the project. When enabled, viewport hover, selection, measuring, and compatible interaction tools can target fixtures, trusses, hoists, and scene objects without being limited to the active Data Views table.

## 2D view and layouts

Use 2D when you need plan-oriented validation and output.

Typical checks:

- Top/plan readability.
- Free 2D selection movement when **Axis Lock** is disabled; when enabled, dragged selections remain constrained to the dominant horizontal or vertical movement axis.
- Layout alignment and coverage.
- Print-ready preparation.

You can export layout-oriented output as PDF for sharing or printing.

## Tables and views together

Best practice is to combine visual review with table review:

- Validate elements in Fixtures, Trusses, Hoists, and Objects tables.
- Cross-check selected elements in 2D and 3D.
- Confirm updates before saving or exporting MVR.

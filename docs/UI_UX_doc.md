# UI/UX Documentation — nativeview (nv-ui)

## Principles
- Vue owns the DOM: nativeview C code does not directly construct or mutate arbitrary HTML after the initial scaffold is loaded.
- UI is virtual: widgets are data + events; there is no “native control per widget”.
- Patches are minimal: state changes should result in a small diff/patch payload rather than a full re-render.
- Deterministic geometry: widgets use explicit `x, y, w, h` fields and are positioned with absolute layout in the scaffold.
- Predictable events: DOM events are mapped to a small, consistent event set surfaced to C.

## UI Model
### Component Tree
nv-ui models UI as a tree of components (owner/parent semantics):
- Owner controls memory lifetime (arena-based).
- Parent controls layout and containment.

### Reactive State
Widgets have state structs that are serialized into the JS runtime and drive Vue reactivity. A state update triggers:
1. native side computes diffs
2. native side sends patch JS
3. Vue updates DOM bindings

## Widget Inventory (Current)
The widget layer is intended to cover common form-style UI needs while keeping the runtime minimal:
- Label
- Button
- Input
- Checkbox
- Select (dropdown)
- Slider
- Textarea
- Vue Slot (`NV_COMP_VUE`): mounts a JS/Vue component into a rectangular region owned by C

## Layout and Styling
### Geometry
- Each widget includes `x, y, w, h` in cfg/state.
- Scaffold emits absolute positioning (`position: absolute`) derived from those fields.
- Layout is currently explicit; responsive behavior is achieved by updating geometry from C when the window size changes.

### Styling Expectations
nativeview does not impose a design system by default. Apps can:
- ship their own CSS and fonts as part of the loaded web content
- provide global CSS variables and theme classes in the host page
- implement custom visual components via the Vue Slot

## Interaction Patterns
### Events
The bridge should surface a minimal set of UI events to C:
- click (buttons, checkbox label/input)
- input (text inputs, sliders, textarea)
- change (select, checkbox, textarea, inputs where appropriate)

Event payload rules:
- Include widget identifier/name
- Include a small, type-appropriate value (e.g., `checked` boolean, `selectedIndex` number, `text` string, `value` float)
- Avoid sending large DOM snapshots

### Focus and Keyboard
Expected behaviors for form-style UIs:
- Input-like widgets should be focusable and preserve cursor position on patch updates.
- Patches should not recreate DOM nodes unnecessarily (avoid focus loss).
- Keyboard interaction should follow platform/web defaults unless explicitly overridden.

## Vue Slot (`NV_COMP_VUE`)
### Purpose
Provide an escape hatch for richer UI while keeping C in control of:
- rectangle (geometry)
- lifecycle and ownership
- data flow (props from C to JS)

### Contract
- C defines a slot ID/name and geometry.
- JS mounts a Vue component into that slot.
- C can update props via a JSON string.
- Slot DOM content is owned entirely by JS/Vue once mounted.

### Lifecycle
Recommended lifecycle signals:
- mounted: JS notifies C when the component is mounted and interactive
- unmounted: JS notifies C when it is destroyed/removed

## Accessibility Requirements
Minimum expectations for widgets emitted by nv-ui scaffolding:
- Associate labels with inputs where applicable
- Maintain sufficient contrast in default styles if any are provided
- Keep interactive targets reasonably sized (apps can enforce in CSS)
- Prefer semantic HTML elements (button/input/select/textarea/label)

## Demo UI
The `demo/` app acts as a UX reference for:
- consistent presentation of ops and their inputs/outputs
- a built-in “test runner” experience for manual verification
- showcasing the JS API surface for each capability

# UI Demo - Lua UI API Tests

These scripts are meant to test the UI Lua API added/updated in issue `#631`.

## Quick usage

1. In `UI_Demo`, create one empty actor in your test scene.
2. Add one or more behaviours with these script paths:
   - `Scripts/UI_API_SmokeTest.lua`
   - `Scripts/UI_API_LayoutStress.lua`
   - `Scripts/UI_API_AnchorCycle.lua`
   - `Scripts/UI_API_TextImagePulse.lua`
3. Press Play and watch both Game View and logs.

## What each script validates

- `UI_API_SmokeTest.lua`
  - Canvas setup and scaling API
  - Runtime creation of UI hierarchy
  - Anchor presets cycle (including stretch presets)
  - Text/Image runtime updates

- `UI_API_LayoutStress.lua`
  - `controlChildrenWidth` / `controlChildrenHeight` behavior
  - Runtime switch between `VerticalLayout` and `HorizontalLayout`
  - Spacing, size, padding, and alignment updates over time

- `UI_API_AnchorCycle.lua`
  - Full anchor preset cycle (all 16 values)
  - Runtime behavior when position is changed while using stretch anchors

- `UI_API_TextImagePulse.lua`
  - Text color/font/extents/alignment animation
  - Image tint/size animation
  - Texture loading from `:Textures\Overload.png`

## Notes

- These scripts intentionally favor runtime API coverage over production gameplay logic.
- They are local UI demo assets and can be adjusted freely for visual tuning.

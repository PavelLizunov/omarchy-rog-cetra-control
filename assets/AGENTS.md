# Assets Directory Instructions

## Purpose

Contains static graphic assets for `io.github.pavellizunov.rog-cetra-control`.

## Asset Rules & Design Standards

1. **Symbolic SVG Only:**
   - Icons must follow the FreeDesktop / GNOME Symbolic icon specification.
   - Standard viewBox: `0 0 64 64`.
   - Stroke / Fill colors must be white (`#fff` or `#ffffff`).
   - Hardcoded UI hex colors (e.g. `#1a1a1a`, `#ff0000`) are strictly forbidden in SVG assets (enforced by `./tests/run.sh`).
   - Dynamic coloring is handled at runtime by `CetraIcon.qml` using `MultiEffect.colorizationColor` or CSS styling.

2. **Hardware Fidelity:**
   - The symbolic headset icon (`cetra-symbolic.svg`) must reflect the physical industrial design of ASUS ROG Cetra True Wireless SpeedNova:
     - In-ear silicone tips and sound chambers facing inward toward each other.
     - Faceted, angled stems pointing downward and slightly outward with characteristic diagonal cuts at the tips.
     - Clean, minimalist outline without decorative artifacts or stray paths.

3. **No Symlinks:**
   - Symlinks are strictly forbidden by Omarchy Plugin Marketplace rules.
   - All files must be regular files.

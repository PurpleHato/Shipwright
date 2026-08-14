#pragma once

// Style selector for the Link cape/scarf cosmetic.
// Stored in CVAR_COSMETIC("DefaultCapeType") (default CAPE_NONE = 0).
typedef enum {
    CAPE_NONE,  // 0 - no cape (cape actor despawned)
    CAPE_CAPE,  // 1 - right shoulder / left shoulder
    CAPE_SCARF, // 2 - right shoulder / head
    CAPE_HIPS,  // 3 - left shin / torso
} DefaultCapeStyle;

#ifdef __cplusplus
// Draws the "Cape and Scarf" controls (style combo + tuning sliders) inside the
// cosmetics editor. Called from CosmeticsEditor.cpp after the Link group.
void DrawCapeAndScarfOptions();
#endif

#pragma once

// Frontend-level enums that upstream defines in Cocoa/AppleCommon rather than Core.

// Cocoa/Document.h `enum model`; values are persisted in GBEmulatedModel.
enum class EmulatedModel : int {
    None = 0,
    DMG = 1,
    CGB = 2,
    AGB = 3,
    SGB = 4,
    MGB = 5,
    Auto = 6,
    QuickReset = -1,
};

// AppleCommon/GBViewBase.h GB_frame_blending_mode_t; persisted in GBFrameBlendingMode.
enum GB_frame_blending_mode_t {
    GB_FRAME_BLENDING_MODE_DISABLED,
    GB_FRAME_BLENDING_MODE_SIMPLE,
    GB_FRAME_BLENDING_MODE_ACCURATE,
    GB_FRAME_BLENDING_MODE_ACCURATE_EVEN = GB_FRAME_BLENDING_MODE_ACCURATE,
    GB_FRAME_BLENDING_MODE_ACCURATE_ODD,
};

// Old Cocoa builds stored SGB PAL with this bit (Document.m GB_MODEL_PAL_BIT_OLD).
constexpr int GB_MODEL_PAL_BIT_OLD = 0x1000;

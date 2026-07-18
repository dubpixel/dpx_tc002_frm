// ================================================================================
// dpx_matrix.cpp — Usermod Entry Point
// ================================================================================
// Original work — dubpixel / dpx_tc002 (MIT)
// ================================================================================
// PROJECT: dpx_tc002_frm
// ================================================================================
//
// File: dpx_matrix.cpp
// Purpose: Includes wled.h (gives access to strip, server, bri, etc.) and then
//          the full dpx_matrix usermod implementation.
//          WLED's build system compiles this as a separate library and links it.
//
// ================================================================================

#include "wled.h"
#include "dpx_matrix.h"

// ── Global definitions ────────────────────────────────────────────────────────
// dpxIndicator is declared extern in dpx_osc.h; one definition here.
uint32_t dpxIndicator[3] = {0, 0, 0};

// ── Usermod registration ──────────────────────────────────────────────────────
static DpxMatrix dpxMatrixMod;
REGISTER_USERMOD(dpxMatrixMod);

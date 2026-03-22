#pragma once
#include "math/ast.h"
#include <stdbool.h>
#include <stdint.h>

// ============================================================================
// Box Drawing Character Analysis and Junction Synthesis
// ============================================================================
//
// This module provides:
// 1. Detection of "arms" (line segments) in Unicode box-drawing characters
// 2. Synthesis of junction characters from arm combinations
// 3. Post-processing to fix intersections in rendered boxes
//
// Box-drawing characters have arms pointing in cardinal directions.
// For example:
//   ─  has arms: East, West
//   │  has arms: North, South
//   ┼  has arms: North, South, East, West
//   ╔  has arms: South, East (double style)
//
// ============================================================================

// Arm direction flags
#define ARM_NORTH 0x01
#define ARM_SOUTH 0x02
#define ARM_EAST 0x04
#define ARM_WEST 0x08

// Style flags (for the arm in that direction)
#define STYLE_DOUBLE_N 0x10
#define STYLE_DOUBLE_S 0x20
#define STYLE_DOUBLE_E 0x40
#define STYLE_DOUBLE_W 0x80

// Combined style helpers
#define STYLE_DOUBLE_V (STYLE_DOUBLE_N | STYLE_DOUBLE_S)
#define STYLE_DOUBLE_H (STYLE_DOUBLE_E | STYLE_DOUBLE_W)
#define STYLE_DOUBLE_ALL (STYLE_DOUBLE_V | STYLE_DOUBLE_H)

// Arm mask (direction only, no style)
#define ARM_MASK (ARM_NORTH | ARM_SOUTH | ARM_EAST | ARM_WEST)

// ============================================================================
// Core API
// ============================================================================

// Get the arm flags for a box-drawing character.
// Returns 0 if the character is not a box-drawing character.
uint8_t box_char_get_arms(uint32_t cp);

// Check if a character has a specific arm
bool box_char_has_arm(uint32_t cp, uint8_t direction);

// Check if a character is a box-drawing character
bool is_box_drawing_char(uint32_t cp);

// Given a set of arm flags (with style), return the appropriate box-drawing character.
// Returns 0 if no matching character exists (e.g., no arms).
uint32_t arms_to_box_char(uint8_t arms);

// ============================================================================
// Box Post-Processing
// ============================================================================

// Scan a rendered box and fix junction characters where box-drawing
// lines intersect. This examines 3x3 neighborhoods and replaces
// characters at intersection points with the appropriate junction glyph.
//
// This modifies the box in place.
void box_fixup_junctions(Box *b);
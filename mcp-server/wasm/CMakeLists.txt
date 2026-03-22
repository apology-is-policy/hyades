# CMakeLists.txt for Hyades MCP render-only WASM build
# Build with: emcmake cmake .. -DCMAKE_BUILD_TYPE=Release && emmake make
#
# This is a minimal build that excludes:
# - Subnivean VM (bytecode compiler, 10 files)
# - Cassilda document processor (2 files)
# - Standard library macros/lambdas (2 generated files)
# - LSP/parse API (2 files)
#
# Defines HYADES_RENDER_ONLY to gate out stdlib and Cassilda references.

cmake_minimum_required(VERSION 3.22)
project(hyades_mcp_wasm C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Root of the Hyades source tree (two levels up from mcp-server/wasm/)
set(HYADES_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# ============================================================================
# Source files - render pipeline only
# ============================================================================

set(HYADES_UTILS_SOURCES
    ${HYADES_ROOT}/src/utils/strbuf.c
    ${HYADES_ROOT}/src/utils/utf8.c
    ${HYADES_ROOT}/src/utils/error.c
    ${HYADES_ROOT}/src/utils/warnings.c
    ${HYADES_ROOT}/src/utils/parser_recovery.c
)

set(HYADES_LEXER_SOURCES
    ${HYADES_ROOT}/src/lexer/lexer.c
)

set(HYADES_LAYOUT_SOURCES
    ${HYADES_ROOT}/src/layout/box.c
    ${HYADES_ROOT}/src/layout/box_align.c
    ${HYADES_ROOT}/src/layout/box_merge.c
    ${HYADES_ROOT}/src/layout/box_layout_parser.c
    ${HYADES_ROOT}/src/layout/box_layout_helpers.c
    ${HYADES_ROOT}/src/layout/box_layout_rules_parser.c
    ${HYADES_ROOT}/src/layout/layout_new.c
    ${HYADES_ROOT}/src/layout/layout_render.c
    ${HYADES_ROOT}/src/layout/rules.c
    ${HYADES_ROOT}/src/layout/width_resolve.c
)

set(HYADES_DOCUMENT_SOURCES
    ${HYADES_ROOT}/src/document/doc_commands.c
    ${HYADES_ROOT}/src/document/document_parser.c
    ${HYADES_ROOT}/src/document/macro_expand.c
    ${HYADES_ROOT}/src/document/calc.c
    ${HYADES_ROOT}/src/document/source_map.c
    ${HYADES_ROOT}/src/document/delimiter_stack.c
    ${HYADES_ROOT}/src/document/symbol_table.c
)

set(HYADES_MATH_SOURCES
    ${HYADES_ROOT}/src/math/ast.c
    # Parser
    ${HYADES_ROOT}/src/math/parser/parser_core.c
    ${HYADES_ROOT}/src/math/parser/parser_tokens.c
    ${HYADES_ROOT}/src/math/parser/parser_delimiters.c
    ${HYADES_ROOT}/src/math/parser/parser_operators.c
    ${HYADES_ROOT}/src/math/parser/parser_constructs.c
    ${HYADES_ROOT}/src/math/parser/parser_grammar.c
    ${HYADES_ROOT}/src/math/parser/parser_entry.c
    # Renderer
    ${HYADES_ROOT}/src/math/renderer/render_opts.c
    ${HYADES_ROOT}/src/math/renderer/symbols.c
    ${HYADES_ROOT}/src/math/renderer/math_styles.c
    ${HYADES_ROOT}/src/math/renderer/math_boxes.c
    ${HYADES_ROOT}/src/math/renderer/math_symbols.c
    ${HYADES_ROOT}/src/math/renderer/math_delimiters.c
    ${HYADES_ROOT}/src/math/renderer/math_accents.c
    ${HYADES_ROOT}/src/math/renderer/math_scripts.c
    ${HYADES_ROOT}/src/math/renderer/math_constructs.c
    ${HYADES_ROOT}/src/math/renderer/math_render.c
)

set(HYADES_COMPOSITOR_SOURCES
    ${HYADES_ROOT}/src/compositor/strbuf.c
    ${HYADES_ROOT}/src/compositor/textstyle.c
    ${HYADES_ROOT}/src/compositor/hyphenate.c
    ${HYADES_ROOT}/src/compositor/mapping.c
    ${HYADES_ROOT}/src/compositor/segments.c
    ${HYADES_ROOT}/src/compositor/linebreak.c
    ${HYADES_ROOT}/src/compositor/typography.c
    ${HYADES_ROOT}/src/compositor/commands.c
    ${HYADES_ROOT}/src/compositor/paragraph.c
    ${HYADES_ROOT}/src/compositor/compose.c
)

set(HYADES_RENDER_SOURCES
    ${HYADES_ROOT}/src/render/box_drawing.c
    ${HYADES_ROOT}/src/render/figlet.c
)

# System macros only (no stdlib)
set(HYADES_MACRO_SOURCES
    ${HYADES_ROOT}/src/macro/user/macro.c
    ${HYADES_ROOT}/src/macro/system/table_macro.c
    ${HYADES_ROOT}/src/macro/system/aligned_macro.c
    ${HYADES_ROOT}/src/macro/system/list_macro.c
)

set(HYADES_DIAGNOSTICS_SOURCES
    ${HYADES_ROOT}/src/diagnostics/diagnostics.c
)

# WASM stubs (shared with full WASM build)
set(HYADES_INTEROP_SOURCES
    ${HYADES_ROOT}/wasm/julia_bridge_wasm.c
    ${HYADES_ROOT}/wasm/terminal_input_wasm.c
)

# Public API (no parse API, no Cassilda)
set(HYADES_API_SOURCES
    ${HYADES_ROOT}/src/public_api/hyades_api.c
    ${HYADES_ROOT}/src/public_api/hyades_public_api.c
)

# MCP bindings
set(HYADES_MCP_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp_bindings.c
)

# All sources combined
set(HYADES_SOURCES
    ${HYADES_UTILS_SOURCES}
    ${HYADES_LEXER_SOURCES}
    ${HYADES_LAYOUT_SOURCES}
    ${HYADES_DOCUMENT_SOURCES}
    ${HYADES_MATH_SOURCES}
    ${HYADES_COMPOSITOR_SOURCES}
    ${HYADES_RENDER_SOURCES}
    ${HYADES_MACRO_SOURCES}
    ${HYADES_DIAGNOSTICS_SOURCES}
    ${HYADES_INTEROP_SOURCES}
    ${HYADES_API_SOURCES}
)

# Include directories
include_directories(
    ${HYADES_ROOT}/include
    ${HYADES_ROOT}/src
)

# Build static library
add_library(hyades_render_core STATIC ${HYADES_SOURCES})

# Gate out stdlib and Cassilda; do NOT enable Subnivean VM
target_compile_definitions(hyades_render_core PRIVATE HYADES_RENDER_ONLY)

# Main WASM module
add_executable(hyades_render ${HYADES_MCP_SOURCES})
target_link_libraries(hyades_render hyades_render_core)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set_target_properties(hyades_render PROPERTIES
        OUTPUT_NAME "hyades-render"
        SUFFIX ".js"
    )

    set(EXPORTED_FUNCTIONS
        "_hyades_mcp_init"
        "_hyades_mcp_render"
        "_hyades_mcp_free"
        "_malloc"
        "_free"
    )

    string(JOIN "," EXPORTED_FUNCS_STR ${EXPORTED_FUNCTIONS})

    target_link_options(hyades_render PRIVATE
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createHyadesModule"
        "-sEXPORTED_FUNCTIONS=${EXPORTED_FUNCS_STR}"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToUTF8,UTF8ToString,lengthBytesUTF8"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=4MB"
        "-sMAXIMUM_MEMORY=64MB"
        "-sSTACK_SIZE=512KB"
        "-sWASM=1"
        "-sENVIRONMENT=web,worker"
        "-sFILESYSTEM=0"
        "-O3"
    )
endif()

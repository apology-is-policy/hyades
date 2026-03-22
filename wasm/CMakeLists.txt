# CMakeLists.txt for Hyades WASM build
# Build with: emcmake cmake .. && emmake make

cmake_minimum_required(VERSION 3.22)
project(hyades_wasm C)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================================
# Source files organized by module (matching native build)
# ============================================================================

set(HYADES_UTILS_SOURCES
    ../src/utils/strbuf.c
    ../src/utils/utf8.c
    ../src/utils/error.c
    ../src/utils/warnings.c
    ../src/utils/parser_recovery.c
)

set(HYADES_LEXER_SOURCES
    ../src/lexer/lexer.c
)

set(HYADES_LAYOUT_SOURCES
    ../src/layout/box.c
    ../src/layout/box_align.c
    ../src/layout/box_merge.c
    ../src/layout/box_layout_parser.c
    ../src/layout/box_layout_helpers.c
    ../src/layout/box_layout_rules_parser.c
    ../src/layout/layout_new.c
    ../src/layout/layout_render.c
    ../src/layout/rules.c
    ../src/layout/width_resolve.c
)

set(HYADES_DOCUMENT_SOURCES
    ../src/document/doc_commands.c
    ../src/document/document_parser.c
    ../src/document/macro_expand.c
    ../src/document/calc.c
    ../src/document/source_map.c
    ../src/document/delimiter_stack.c
    ../src/document/symbol_table.c
)

set(HYADES_MATH_SOURCES
    ../src/math/ast.c
    # Parser
    ../src/math/parser/parser_core.c
    ../src/math/parser/parser_tokens.c
    ../src/math/parser/parser_delimiters.c
    ../src/math/parser/parser_operators.c
    ../src/math/parser/parser_constructs.c
    ../src/math/parser/parser_grammar.c
    ../src/math/parser/parser_entry.c
    # Renderer
    ../src/math/renderer/render_opts.c
    ../src/math/renderer/symbols.c
    ../src/math/renderer/math_styles.c
    ../src/math/renderer/math_boxes.c
    ../src/math/renderer/math_symbols.c
    ../src/math/renderer/math_delimiters.c
    ../src/math/renderer/math_accents.c
    ../src/math/renderer/math_scripts.c
    ../src/math/renderer/math_constructs.c
    ../src/math/renderer/math_render.c
)

set(HYADES_COMPOSITOR_SOURCES
    ../src/compositor/strbuf.c
    ../src/compositor/textstyle.c
    ../src/compositor/hyphenate.c
    ../src/compositor/mapping.c
    ../src/compositor/segments.c
    ../src/compositor/linebreak.c
    ../src/compositor/typography.c
    ../src/compositor/commands.c
    ../src/compositor/paragraph.c
    ../src/compositor/compose.c
)

set(HYADES_RENDER_SOURCES
    ../src/render/box_drawing.c
    ../src/render/figlet.c
)

set(HYADES_MACRO_SOURCES
    ../src/macro/user/macro.c
    ../src/macro/system/table_macro.c
    ../src/macro/system/aligned_macro.c
    ../src/macro/system/list_macro.c
    ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
)

set(HYADES_DIAGNOSTICS_SOURCES
    ../src/diagnostics/diagnostics.c
)

set(HYADES_INTEROP_SOURCES
    # Use WASM stub for Julia (no dlopen/popen in browser)
    julia_bridge_wasm.c
    # Use WASM stub for terminal input (no TTY in browser/LSP)
    terminal_input_wasm.c
)

set(HYADES_SUBNIVEAN_SOURCES
    ../src/subnivean/v2/value.c
    ../src/subnivean/v2/scope.c
    ../src/subnivean/v2/function.c
    ../src/subnivean/v2/vm.c
    ../src/subnivean/v2/calc_parser.c
    ../src/subnivean/v2/calc_compiler.c
    ../src/subnivean/v2/subnivean_api.c
    ../src/subnivean/v2/sn_assembler.c
    ../src/subnivean/v2/persistent_store.c
    ../src/subnivean/v2/hashmap.c
)

set(HYADES_API_SOURCES
    ../src/public_api/hyades_api.c
    ../src/public_api/hyades_public_api.c
    ../src/public_api/hyades_parse_api.c
    ../src/cassilda/cassilda.c
    ../src/cassilda/label_library.c
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
    ${HYADES_INTEROP_SOURCES}
    ${HYADES_DIAGNOSTICS_SOURCES}
    ${HYADES_SUBNIVEAN_SOURCES}
    ${HYADES_API_SOURCES}
)

# ============================================================================
# Standard Library Macro Generation
# ============================================================================

# Build the generator tool using the host compiler (not emscripten)
# We need to use a native build of gen_stdlib to generate the C file
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/gen_stdlib
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_stdlib.c
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gen_stdlib
        ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/stdlib.cld
        ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_stdlib.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/stdlib.cld
    COMMENT "Generating stdlib_macros.c from stdlib/stdlib.cld (WASM build)"
    VERBATIM
)

# Generate stdlib_lambdas.c from stdlib_lambdas.cld
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
    COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/gen_stdlib_lambdas
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_stdlib_lambdas.c
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gen_stdlib_lambdas
        ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/stdlib_lambdas.cld
        ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_stdlib_lambdas.c
            ${CMAKE_CURRENT_SOURCE_DIR}/../stdlib/stdlib_lambdas.cld
    COMMENT "Generating stdlib_lambdas.c from stdlib/stdlib_lambdas.cld (WASM build)"
    VERBATIM
)

# ============================================================================
# Examples Library Generation
# ============================================================================

# Build gen_examples with host compiler and generate examples-library.js
add_custom_target(examples ALL
    COMMAND cc -o ${CMAKE_CURRENT_BINARY_DIR}/gen_examples
        ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_examples.c
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/gen_examples
        ${CMAKE_CURRENT_SOURCE_DIR}/examples
        ${CMAKE_CURRENT_SOURCE_DIR}/examples-library.js
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../src/tools/gen_examples.c
    COMMENT "Generating examples-library.js from wasm/examples/*.cld files"
    VERBATIM
)

# Include directories
include_directories(
    ../include
    ../src
    ../src/subnivean/v2
)

# Build as a library first (for potential modular use)
add_library(hyades_core STATIC ${HYADES_SOURCES})

# Enable Subnivean VM for compiled computational lambdas
target_compile_definitions(hyades_core PRIVATE SUBNIVEAN_ENABLED)

# Main WASM module
add_executable(hyades_wasm wasm_bindings.c hyades_lsp.c)
target_link_libraries(hyades_wasm hyades_core)

# Emscripten-specific settings
if(EMSCRIPTEN)
    set_target_properties(hyades_wasm PROPERTIES
        OUTPUT_NAME "hyades"
        SUFFIX ".js"
    )
    
    # Exported functions for JavaScript
    set(EXPORTED_FUNCTIONS
        "_hyades_wasm_render"
        "_hyades_wasm_render_math"
        "_hyades_wasm_cassilda_process"
        "_hyades_wasm_free"
        "_hyades_wasm_init"
        "_hyades_lsp_parse"
        "_hyades_lsp_get_diagnostics"
        "_hyades_lsp_get_symbols"
        "_hyades_lsp_get_completions"
        "_hyades_lsp_get_definition"
        "_hyades_lsp_get_hover"
        "_hyades_lsp_get_references"
        "_hyades_lsp_get_semantic_tokens"
        "_hyades_lsp_compute_semantic_tokens"
        "_hyades_lsp_semantic_tokens_ptr"
        "_hyades_lsp_validate"
        "_hyades_lsp_version"
        "_hyades_lsp_clear"
        "_hyades_lsp_free"
        "_malloc"
        "_free"
    )
    
    # Join with commas for linker flag
    string(JOIN "," EXPORTED_FUNCS_STR ${EXPORTED_FUNCTIONS})
    
    # Common flags for both debug and release
    set(COMMON_LINK_FLAGS
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createHyadesModule"
        "-sEXPORTED_FUNCTIONS=${EXPORTED_FUNCS_STR}"
        "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap,stringToUTF8,UTF8ToString,lengthBytesUTF8"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=64MB"
        "-sMAXIMUM_MEMORY=512MB"
        "-sSTACK_SIZE=1MB"
        "-sWASM=1"
        "-sWASM_ASYNC_COMPILATION=1"
        "-sENVIRONMENT=web,worker,node"
        "-sFILESYSTEM=0"
    )
    
    # Debug vs Release configuration
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Building WASM in DEBUG mode with stack traces")
        
        # Debug compile flags
        target_compile_options(hyades_core PRIVATE -g3 -O0)
        target_compile_options(hyades_wasm PRIVATE -g3 -O0)
        
        # Debug link flags
        target_link_options(hyades_wasm PRIVATE
            ${COMMON_LINK_FLAGS}
            # ES6 module
            "-sEXPORT_ES6=1"
            # Debug flags
            "-g3"
            "-O0"
            "-sASSERTIONS=2"
            "-sSAFE_HEAP=1"
            "-sSTACK_OVERFLOW_CHECK=2"
            # This is the key for readable stack traces!
            "-sERROR_ON_UNDEFINED_SYMBOLS=0"
            "--emit-symbol-map"
            # Keep function names
            "-sEXPORT_KEEPALIVE=1"
        )
    else()
        message(STATUS "Building WASM in RELEASE mode")
        
        # Release link flags
        target_link_options(hyades_wasm PRIVATE
            ${COMMON_LINK_FLAGS}
            "-sEXPORT_ES6=1"
            "-O3"
        )
    endif()
    
    # Also build a simpler non-ES6 version for broader compatibility
    add_executable(hyades_wasm_compat wasm_bindings.c hyades_lsp.c)
    target_link_libraries(hyades_wasm_compat hyades_core)
    
    set_target_properties(hyades_wasm_compat PROPERTIES
        OUTPUT_NAME "hyades-compat"
        SUFFIX ".js"
    )
    
    target_link_options(hyades_wasm_compat PRIVATE
        ${COMMON_LINK_FLAGS}
        "-O3"
    )
endif()
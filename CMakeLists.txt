cmake_minimum_required(VERSION 3.22)

project(hyades C)

option(HYADES_BUILD_SHARED "Build hyades as a shared library" OFF)
option(HYADES_JULIA_SUPPORT "Enable Julia integration via dlopen" ON)

if(MSVC)
    set(CMAKE_C_STANDARD 17)
else()
    set(CMAKE_C_STANDARD 23)
endif()
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(HYADES_PUBLIC_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# ============================================================================
# Source files organized by module
# ============================================================================

if(WIN32)
    set(HYADES_TERMINAL_INPUT src/utils/terminal_input_win.c)
else()
    set(HYADES_TERMINAL_INPUT src/utils/terminal_input.c)
endif()

set(HYADES_UTILS_SOURCES
    src/utils/strbuf.c
    src/utils/utf8.c
    src/utils/error.c
    src/utils/warnings.c
    src/utils/parser_recovery.c
    ${HYADES_TERMINAL_INPUT}
)

set(HYADES_LEXER_SOURCES
    src/lexer/lexer.c
)

set(HYADES_LAYOUT_SOURCES
    src/layout/box.c
    src/layout/box_align.c
    src/layout/box_merge.c
    src/layout/box_layout_parser.c
    src/layout/box_layout_helpers.c
    src/layout/box_layout_rules_parser.c
    src/layout/layout_new.c
    src/layout/layout_render.c
    src/layout/rules.c
    src/layout/width_resolve.c
)

set(HYADES_DOCUMENT_SOURCES
    src/document/doc_commands.c
    src/document/document_parser.c
    src/document/macro_expand.c
    src/document/calc.c
    src/document/source_map.c
    src/document/delimiter_stack.c
    src/document/symbol_table.c
)

# Subnivean VM sources (bytecode compiler for computational lambdas)
set(HYADES_SUBNIVEAN_SOURCES
    src/subnivean/v2/value.c
    src/subnivean/v2/scope.c
    src/subnivean/v2/function.c
    src/subnivean/v2/vm.c
    src/subnivean/v2/calc_parser.c
    src/subnivean/v2/calc_compiler.c
    src/subnivean/v2/subnivean_api.c
    src/subnivean/v2/sn_assembler.c
    src/subnivean/v2/persistent_store.c
    src/subnivean/v2/hashmap.c
)

set(HYADES_MATH_SOURCES
    src/math/ast.c
    # Parser
    src/math/parser/parser_core.c
    src/math/parser/parser_tokens.c
    src/math/parser/parser_delimiters.c
    src/math/parser/parser_operators.c
    src/math/parser/parser_constructs.c
    src/math/parser/parser_grammar.c
    src/math/parser/parser_entry.c
    # Renderer
    src/math/renderer/render_opts.c
    src/math/renderer/symbols.c
    src/math/renderer/math_styles.c
    src/math/renderer/math_boxes.c
    src/math/renderer/math_symbols.c
    src/math/renderer/math_delimiters.c
    src/math/renderer/math_accents.c
    src/math/renderer/math_scripts.c
    src/math/renderer/math_constructs.c
    src/math/renderer/math_render.c
)

set(HYADES_COMPOSITOR_SOURCES
    src/compositor/strbuf.c
    src/compositor/textstyle.c
    src/compositor/hyphenate.c
    src/compositor/mapping.c
    src/compositor/segments.c
    src/compositor/linebreak.c
    src/compositor/typography.c
    src/compositor/commands.c
    src/compositor/paragraph.c
    src/compositor/compose.c
)

set(HYADES_RENDER_SOURCES
    src/render/box_drawing.c
    src/render/figlet.c
)

set(HYADES_MACRO_SOURCES
    src/macro/user/macro.c
    src/macro/system/table_macro.c
    src/macro/system/aligned_macro.c
    src/macro/system/list_macro.c
    ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
)

set(HYADES_INTEROP_SOURCES
    src/interop/julia_bridge.c
)

set(HYADES_DIAGNOSTICS_SOURCES
    src/diagnostics/diagnostics.c
)

set(HYADES_API_SOURCES
    src/public_api/hyades_api.c
    src/public_api/hyades_public_api.c
    src/public_api/hyades_parse_api.c
)

# Note: hyades_kernel.c has been replaced by modular sources
# All functions now live in layout/ and document/ directories
set(HYADES_LEGACY_SOURCES
    # src/hyades_kernel.c  # REMOVED - replaced by modular code
)

# All sources combined
set(HYADES_ALL_SOURCES
    ${HYADES_UTILS_SOURCES}
    ${HYADES_LEXER_SOURCES}
    ${HYADES_LAYOUT_SOURCES}
    ${HYADES_DOCUMENT_SOURCES}
    ${HYADES_SUBNIVEAN_SOURCES}
    ${HYADES_MATH_SOURCES}
    ${HYADES_COMPOSITOR_SOURCES}
    ${HYADES_RENDER_SOURCES}
    ${HYADES_MACRO_SOURCES}
    ${HYADES_INTEROP_SOURCES}
    ${HYADES_DIAGNOSTICS_SOURCES}
    ${HYADES_API_SOURCES}
    ${HYADES_LEGACY_SOURCES}
)

# ============================================================================
# Standard Library Macro Generation
# ============================================================================

# Build the generator tool first (it doesn't depend on hyades library)
add_executable(gen_stdlib src/tools/gen_stdlib.c)

# Generate stdlib_macros.c from stdlib.cld
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    COMMAND $<TARGET_FILE:gen_stdlib>
            ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/stdlib.cld
            ${CMAKE_CURRENT_BINARY_DIR}/stdlib_macros.c
    DEPENDS gen_stdlib ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/stdlib.cld
    COMMENT "Generating stdlib_macros.c from stdlib/stdlib.cld"
    VERBATIM
)

# ============================================================================
# Standard Library Lambda (Function) Generation
# ============================================================================

# Build the lambda generator tool
add_executable(gen_stdlib_lambdas src/tools/gen_stdlib_lambdas.c)

# Generate stdlib_lambdas.c from stdlib_lambdas.cld
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
    COMMAND $<TARGET_FILE:gen_stdlib_lambdas>
            ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/stdlib_lambdas.cld
            ${CMAKE_CURRENT_BINARY_DIR}/stdlib_lambdas.c
    DEPENDS gen_stdlib_lambdas ${CMAKE_CURRENT_SOURCE_DIR}/stdlib/stdlib_lambdas.cld
    COMMENT "Generating stdlib_lambdas.c from stdlib/stdlib_lambdas.cld"
    VERBATIM
)

# ============================================================================
# Library target
# ============================================================================

if (HYADES_BUILD_SHARED)
    add_library(hyades SHARED ${HYADES_ALL_SOURCES})
else()
    add_library(hyades STATIC ${HYADES_ALL_SOURCES})
endif()

# On Unix-like platforms, link libdl when needed
if(NOT WIN32)
    target_link_libraries(hyades PRIVATE ${CMAKE_DL_LIBS})
endif()

target_include_directories(hyades
    PUBLIC
        ${HYADES_PUBLIC_INCLUDE}
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/subnivean/v2
)

if(MSVC)
    # CMake/MSVC doesn't advertise c_std_23; build in the best-supported C mode.
    set_property(TARGET hyades PROPERTY C_STANDARD 17)
else()
    target_compile_features(hyades PUBLIC c_std_23)
endif()

if(HYADES_JULIA_SUPPORT)
    target_compile_definitions(hyades PRIVATE HYADES_JULIA_SUPPORT)
endif()

# Enable Subnivean VM for compiled computational lambdas (~65x faster)
target_compile_definitions(hyades PRIVATE SUBNIVEAN_ENABLED)

# Helpful (and portable) warnings; doesn't change structure
if(MSVC)
    target_compile_options(hyades PRIVATE /W4)
else()
    target_compile_options(hyades PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ============================================================================
# AddressSanitizer for Debug builds (helps catch memory bugs)
# ============================================================================
option(HYADES_ENABLE_ASAN "Enable AddressSanitizer for debug builds" ON)

if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    message(STATUS "AddressSanitizer enabled for Debug build")
    target_compile_options(hyades PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(hyades PRIVATE -fsanitize=address)
endif()

# If building a DLL on Windows, export symbols automatically (no header changes needed)
if(WIN32 AND HYADES_BUILD_SHARED)
    set_target_properties(hyades PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

set_target_properties(hyades PROPERTIES
    VERSION 1.0.0
    SOVERSION 1
)

# ============================================================================
# Install rules
# ============================================================================

include(GNUInstallDirs)

install(TARGETS hyades
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES ${HYADES_PUBLIC_INCLUDE}/hyades.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY defaults/
    DESTINATION share/hyades/defaults
    FILES_MATCHING PATTERN "*.hy"
)

# ============================================================================
# Applications (commented out until main files are available)
# ============================================================================

# project(hyades_app C)
#
# add_executable(hyades_app src/main.c)
# target_link_libraries(hyades_app PRIVATE hyades)
# target_include_directories(hyades_app PRIVATE ${HYADES_PUBLIC_INCLUDE})
#
# add_executable(hyades_repl src/main_repl.c)
# target_link_libraries(hyades_repl PRIVATE hyades)
# target_link_libraries(hyades_repl PRIVATE -lreadline)
# target_include_directories(hyades_repl PRIVATE ${HYADES_PUBLIC_INCLUDE})
#
# add_executable(test_hyades tests/test_docparser.c)
# target_link_libraries(test_hyades PRIVATE hyades)
# target_include_directories(test_hyades PRIVATE ${HYADES_PUBLIC_INCLUDE})
#
# add_executable(test_debug src/test_vbox_debug.c)
# target_link_libraries(test_debug PRIVATE hyades)
# target_include_directories(test_debug PRIVATE ${HYADES_PUBLIC_INCLUDE})

add_executable(export_symbol_templates src/tools/export_symbol_templates.c)
target_link_libraries(export_symbol_templates PRIVATE hyades)
target_include_directories(export_symbol_templates PRIVATE ${HYADES_PUBLIC_INCLUDE})

# Enable ASan for build tools in Debug builds (needed when hyades library has ASan)
if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(export_symbol_templates PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(export_symbol_templates PRIVATE -fsanitize=address)
endif()

add_executable(cassilda
    src/cassilda/cassilda.c
    src/cassilda/cassilda_main.c
    src/cassilda/label_library.c
)
target_link_libraries(cassilda PRIVATE hyades)
target_include_directories(cassilda PRIVATE ${HYADES_PUBLIC_INCLUDE})

# Enable ASan for cassilda in Debug builds
if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(cassilda PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(cassilda PRIVATE -fsanitize=address)
endif()

install(TARGETS cassilda
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Hyades CLI (simple typesetting without Cassilda extensions)
add_executable(hyades_cli
    src/hyades/hyades_main.c
)
set_target_properties(hyades_cli PROPERTIES OUTPUT_NAME "hyades")
target_link_libraries(hyades_cli PRIVATE hyades)
target_include_directories(hyades_cli PRIVATE ${HYADES_PUBLIC_INCLUDE})

# Enable ASan for hyades_cli in Debug builds
if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(hyades_cli PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(hyades_cli PRIVATE -fsanitize=address)
endif()

install(TARGETS hyades_cli
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Hyades MCP server (native stdio transport for Claude Desktop / Claude Code)
add_executable(hyades_mcp
    src/mcp/mcp_server.c
    src/mcp/mcp_json.c
    src/cassilda/cassilda.c
    src/cassilda/label_library.c
)
set_target_properties(hyades_mcp PROPERTIES OUTPUT_NAME "hyades-mcp")
target_link_libraries(hyades_mcp PRIVATE hyades)
target_include_directories(hyades_mcp PRIVATE ${HYADES_PUBLIC_INCLUDE})

if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(hyades_mcp PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(hyades_mcp PRIVATE -fsanitize=address)
endif()

install(TARGETS hyades_mcp
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# LSP parse test binary (for debugging with ASan)
add_executable(test_lsp_parse src/test_lsp_parse.c)
target_link_libraries(test_lsp_parse PRIVATE hyades)
target_include_directories(test_lsp_parse PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Enable ASan for test_lsp_parse in Debug builds
if(HYADES_ENABLE_ASAN AND CMAKE_BUILD_TYPE STREQUAL "Debug" AND NOT MSVC AND NOT EMSCRIPTEN)
    target_compile_options(test_lsp_parse PRIVATE -fsanitize=address -fno-omit-frame-pointer -g)
    target_link_options(test_lsp_parse PRIVATE -fsanitize=address)
endif()

# ============================================================================
# Symbol template generation
# ============================================================================

add_custom_target(templates
    COMMAND $<TARGET_FILE:export_symbol_templates>
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    DEPENDS export_symbol_templates
    COMMENT "Generating symbol template files"
    VERBATIM
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/symbols_ascii.hy
    ${CMAKE_CURRENT_BINARY_DIR}/symbols_unicode.hy
    DESTINATION share/hyades/templates
    OPTIONAL
)

# ============================================================================
# Release Package Configuration
# ============================================================================

set(HYADES_ARTIFACTS_DIR ${CMAKE_BINARY_DIR}/artifacts)

# Determine platform-specific names
if(WIN32)
    set(HYADES_EXE_SUFFIX ".exe")
    set(HYADES_LIB_NAME "hyades.lib")
    set(HYADES_TS_LIB "cassilda.dll")
else()
    set(HYADES_EXE_SUFFIX "")
    set(HYADES_LIB_NAME "libhyades.a")
    set(HYADES_TS_LIB "cassilda.so")
endif()

# ============================================================================
# Tree-sitter Grammar Build Target
# ============================================================================
# Builds the tree-sitter grammar for Cassilda/Hyades syntax highlighting.
# This generates the parser from grammar.js and compiles it to a shared library.

add_custom_target(tree-sitter-grammar
    COMMAND ${CMAKE_COMMAND} -E echo "Building tree-sitter grammar..."
    # Generate parser from grammar.js
    COMMAND npx tree-sitter generate
    # Compile to shared library (parser.c + scanner.c if present)
    COMMAND ${CMAKE_C_COMPILER} -shared -fPIC -I./src src/parser.c src/scanner.c -o ${HYADES_TS_LIB}
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda
    COMMENT "Building tree-sitter-cassilda grammar"
    VERBATIM
)

# ============================================================================
# LSP Server Bundle Target
# ============================================================================
# Builds the LSP server bundle (requires WASM to be built first).

add_custom_target(lsp-bundle
    COMMAND ${CMAKE_COMMAND} -E echo "Building LSP server bundle..."
    COMMAND npm run bundle
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/lsp-server
    COMMENT "Building LSP server bundle"
    VERBATIM
)

# ============================================================================
# MCP Server WASM Build Target
# ============================================================================
# Builds the render-only WASM binary for the MCP server.
# Requires Emscripten SDK to be installed and activated.
#
# Usage: cmake --build build --target mcp-wasm

add_custom_target(mcp-wasm
    COMMAND ${CMAKE_COMMAND} -E echo "Building MCP render-only WASM..."
    COMMAND ${CMAKE_SOURCE_DIR}/mcp-server/wasm/build.sh
    COMMENT "Building MCP server WASM (render-only)"
    VERBATIM
)

# ============================================================================
# MCP Worker Setup Target
# ============================================================================
# Installs worker dependencies and copies WASM artifacts.
# Run after mcp-wasm.
#
# Usage: cmake --build build --target mcp-worker-setup

add_custom_target(mcp-worker-setup
    COMMAND ${CMAKE_COMMAND} -E echo "Setting up MCP worker..."
    COMMAND npm install
    COMMAND npm run copy-wasm
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/mcp-server/worker
    COMMENT "Installing MCP worker dependencies and copying WASM"
    VERBATIM
)

# ============================================================================
# Release Package Target
# ============================================================================
# Creates a complete release package with binaries, libraries, LSP server,
# tree-sitter grammar, and editor configuration files.
#
# Prerequisites (run these first if not already done):
#   1. Build WASM:        cd wasm/build && emcmake cmake .. && make
#   2. Build LSP bundle:  make lsp-bundle (or: cd lsp-server && npm run bundle)
#   3. Build tree-sitter: make tree-sitter-grammar

add_custom_target(package-release
    COMMAND ${CMAKE_COMMAND} -E echo "Creating release package in ${HYADES_ARTIFACTS_DIR}..."

    # Create directory structure
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/lib
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/include
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/lsp-server
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/queries
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/editor-configs
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/docs

    # Copy binaries
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/cassilda${HYADES_EXE_SUFFIX}
        ${HYADES_ARTIFACTS_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/hyades${HYADES_EXE_SUFFIX}
        ${HYADES_ARTIFACTS_DIR}/bin/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/hyades-mcp${HYADES_EXE_SUFFIX}
        ${HYADES_ARTIFACTS_DIR}/bin/

    # Copy library
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_BINARY_DIR}/${HYADES_LIB_NAME}
        ${HYADES_ARTIFACTS_DIR}/lib/

    # Copy headers
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/include/hyades.h
        ${HYADES_ARTIFACTS_DIR}/include/

    # Copy LSP server bundle (requires: npm run bundle in lsp-server/)
    # Bundle includes server.js + hyades-compat.js + hyades-compat.wasm
    # No node_modules needed - everything is bundled into server.js
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/lsp-server/bundle
        ${HYADES_ARTIFACTS_DIR}/lsp-server

    # Copy tree-sitter grammar source
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda/grammar.js
        ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda/package.json
        ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda/queries
        ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/queries
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda/src
        ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/src

    # Copy pre-built tree-sitter library (build with: make tree-sitter-grammar)
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/tree-sitter-cassilda/${HYADES_TS_LIB}
        ${HYADES_ARTIFACTS_DIR}/tree-sitter-cassilda/

    # Copy editor configs
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/nvim-cassilda.lua
        ${HYADES_ARTIFACTS_DIR}/editor-configs/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/helix-languages.toml
        ${HYADES_ARTIFACTS_DIR}/editor-configs/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/helix-config.toml
        ${HYADES_ARTIFACTS_DIR}/editor-configs/
    # Copy install and render-docs scripts to package root
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/install.sh
        ${HYADES_ARTIFACTS_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/install.ps1
        ${HYADES_ARTIFACTS_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/render-docs.sh
        ${HYADES_ARTIFACTS_DIR}/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/editor-configs/render-docs.ps1
        ${HYADES_ARTIFACTS_DIR}/

    # Copy documentation (.cld notebook files)
    COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/wasm/examples/*.cld ${HYADES_ARTIFACTS_DIR}/docs/"

    # Copy WASM binaries (optional — only available after a WASM build)
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/wasm
    COMMAND bash -c "cp ${CMAKE_SOURCE_DIR}/wasm/dist/hyades.wasm ${CMAKE_SOURCE_DIR}/wasm/dist/hyades.js ${HYADES_ARTIFACTS_DIR}/wasm/ 2>/dev/null || echo 'WASM binaries not found (skipping — run WASM build first)'"

    # Package MCP Cloudflare Worker (zip of deployable worker sources + WASM)
    COMMAND ${CMAKE_COMMAND} -E make_directory ${HYADES_ARTIFACTS_DIR}/mcp-worker
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/worker/src/index.ts
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/worker/src/tool-description.ts
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/worker/wrangler.toml
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/worker/package.json
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/wasm/build/hyades-render.js
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/mcp-server/wasm/build/hyades-render.wasm
        ${HYADES_ARTIFACTS_DIR}/mcp-worker/
    COMMAND ${CMAKE_COMMAND} -E chdir ${HYADES_ARTIFACTS_DIR}
        ${CMAKE_COMMAND} -E tar czf mcp-worker.tar.gz -- mcp-worker
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${HYADES_ARTIFACTS_DIR}/mcp-worker

    COMMAND ${CMAKE_COMMAND} -E echo "Release package created in ${HYADES_ARTIFACTS_DIR}"

    DEPENDS cassilda hyades_cli hyades_mcp tree-sitter-grammar lsp-bundle
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Creating release package with all artifacts"
    VERBATIM
)

# ============================================================================
# Golden tests
# ============================================================================
# Run golden tests: compares cassilda output against checked-in goldens.
# Usage: make test

add_custom_target(test
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/run_tests.sh $<TARGET_FILE:cassilda>
    DEPENDS cassilda
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running golden tests..."
)

# Update golden files from wasm/examples/*.cld.
# Usage: make update-goldens

add_custom_target(update-goldens
    COMMAND bash ${CMAKE_SOURCE_DIR}/tests/update_goldens.sh $<TARGET_FILE:cassilda>
    DEPENDS cassilda
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Updating golden files..."
)
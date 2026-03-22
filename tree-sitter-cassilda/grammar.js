/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

/**
 * Tree-sitter grammar for Cassilda/Hyades documents
 * Matches LSP semantic tokens as closely as possible
 *
 * Updated: 2026-02-06
 * - Added new CL syntax: ${}, \invoke, \at, \set, \addressof, etc.
 * - Added type annotations for \let and \lambda
 * - Added dereference patterns (*name)
 * - Added nested variable access ${item${i}}
 * - Added map/array literals
 */

// Known built-in commands for different highlighting
// Note: keywords that overlap with CONTROL_KEYWORDS or CALC_COMMANDS are excluded
// from MATH_FUNCTIONS to avoid tree-sitter token conflicts.
// Excluded from CONTROL overlap: max, min, div, leq, geq, neq, neg, mod
// Excluded from CALC overlap: lambda, split
// These are still recognized as control/calc commands and highlighted appropriately.
const MATH_FUNCTIONS = [
  'frac', 'sqrt', 'sum', 'prod', 'int', 'iint', 'iiint', 'oint',
  'lim', 'sin', 'cos', 'tan', 'cot', 'sec', 'csc',
  'arcsin', 'arccos', 'arctan', 'sinh', 'cosh', 'tanh',
  'log', 'ln', 'exp', 'sup', 'inf', 'arg', 'gcd', 'det',
  'partial', 'nabla', 'infty', 'pm', 'mp', 'times', 'cdot',
  'approx', 'equiv', 'subset', 'supset',
  'in', 'notin', 'forall', 'exists', 'given', 'cond',
  'vee', 'wedge', 'oplus', 'otimes',
  'rightarrow', 'leftarrow', 'Rightarrow', 'Leftarrow', 'leftrightarrow',
  'uparrow', 'downarrow', 'mapsto', 'to', 'gets',
  'alpha', 'beta', 'gamma', 'delta', 'epsilon', 'varepsilon',
  'zeta', 'eta', 'theta', 'vartheta', 'iota', 'kappa',
  'mu', 'nu', 'xi', 'pi', 'varpi', 'rho', 'varrho',
  'sigma', 'varsigma', 'tau', 'upsilon', 'phi', 'varphi',
  'chi', 'psi', 'omega',
  'Gamma', 'Delta', 'Theta', 'Lambda', 'Xi', 'Pi',
  'Sigma', 'Upsilon', 'Phi', 'Psi', 'Omega',
  'binom', 'tbinom', 'dbinom', 'cases', 'matrix', 'pmatrix', 'bmatrix',
  'hat', 'bar', 'vec', 'dot', 'ddot', 'tilde', 'overline', 'underline', 'fn',
  'overbrace', 'underbrace', 'text', 'mathbf', 'mathbb', 'mathcal', 'mathrm',
  'operatorname',
  'left', 'right', 'big', 'Big', 'bigg', 'Bigg',
  'langle', 'rangle', 'lfloor', 'rfloor', 'lceil', 'rceil',
  // Spacing commands
  ',', ';', '!', 'quad', 'qquad',
  // Additional common math
  'setminus', 'lbrace', 'rbrace', 'lvert', 'rvert', 'Vert',
  'll', 'gg', 'sim', 'simeq', 'cong', 'propto', 'ni', 'subseteq', 'supseteq',
  'cup', 'cap', 'emptyset', 'varnothing', 'nexists', 'land', 'lor', 'implies', 'iff',
  'Leftrightarrow', 'top', 'bot', 'perp', 'longmapsto', 'longrightarrow', 'longleftarrow',
  'hookrightarrow', 'hookleftarrow', 'updownarrow', 'Uparrow', 'Downarrow', 'nearrow',
  'searrow', 'nwarrow', 'swarrow', 'bigcup', 'bigcap', 'Iint', 'IINT', 'Iiint', 'IIInt',
  'Oint', 'OINT', 'oiint', 'Oiint', 'OIINT', 'coprod', 'Coprod', 'COPROD', 'Sum', 'SUM',
  'Prod', 'PROD', 'limsup', 'liminf', 'lcm', 'Pr', 'acute', 'grave', 'breve', 'check',
  'widehat', 'widetilde', 'overrightarrow', 'overleftarrow', 'Bmatrix', 'Vmatrix', 'vmatrix',
  'cdots', 'vdots', 'ddots', 'ldots', 'hbar', 'ell', 'mid', 'Var', 'Cov', 'pmod', 'bmod',
  'nmid', 'ominus', 'odot', 'circ', 'bullet', 'star', 'asc', 'dagger', 'ddagger',
  'textbackslash', 'textit', 'figlet', 'boxed', 'aligned', 'gather',
];

// Calc/Lambda commands - highlighted as @type (computational subsystem)
// Split into categories for better semantic highlighting
const CALC_COMMANDS_DECLARATION = [
  'lambda', 'let', 'assign',
];

const CALC_COMMANDS_ACCESS = [
  'valueof', 'recall', 'ref', 'addressof',
  'at',       // New CL: unified collection read
  'invoke',   // New CL: function invocation
];

const CALC_COMMANDS_MUTATION = [
  'inc', 'dec',
  'set',        // New CL: unified collection write
  'setelement', // Old syntax (deprecated in CL)
  'push', 'pop', 'peek', 'enqueue', 'dequeue',
];

const CALC_COMMANDS_MAP = [
  'map_has', 'map_del', 'map_keys',   // New CL syntax
];

const CALC_COMMANDS_MEMORY = [
  'mem_alloc', 'mem_load', 'mem_store',  // Heap operations
];

const CALC_COMMANDS_ARRAY = [
  'len', 'split',
];

const CALC_COMMANDS_OUTPUT = [
  'emit', 'cursor', 'ansi',  // Terminal output
];

const CALC_COMMANDS_CONTROL = [
  'return', 'exit_when',
];

// All calc commands combined
const CALC_COMMANDS = [
  ...CALC_COMMANDS_DECLARATION,
  ...CALC_COMMANDS_ACCESS,
  ...CALC_COMMANDS_MUTATION,
  ...CALC_COMMANDS_MAP,
  ...CALC_COMMANDS_MEMORY,
  ...CALC_COMMANDS_ARRAY,
  ...CALC_COMMANDS_OUTPUT,
  ...CALC_COMMANDS_CONTROL,
];

// Control flow and operations - highlighted as @keyword
const CONTROL_KEYWORDS = [
  // Conditionals
  'if', 'else',
  // Arithmetic
  'add', 'sub', 'mul', 'div', 'mod', 'neg', 'abs', 'floor', 'ceil', 'round',
  'max', 'min', 'rand',
  // Comparison
  'eq', 'neq', 'ne', 'lt', 'gt', 'leq', 'le', 'geq', 'ge', 'and', 'or', 'not',
  // String operations
  'streq', 'strneq', 'strlen', 'substr', 'strcat', 'startswith', 'endswith', 'contains',
  // Measurement/misc
  'measure', 'measureref', 'lineroutine',
  'unicode', 'ascii', 'width', 'parskip',
  'setunicode', 'setwidth', 'setmathitalic', 'setparskip', 'setmathabove', 'setmathbelow',
  'linebreaker', 'setlinepenalty', 'sethyphenpenalty', 'setconsechyphenpenalty', 'settolerance',
];

const LAYOUT_COMMANDS = [
  'table', 'row', 'col', 'frame', 'hbox', 'vbox', 'child',
  'hrule', 'vrule', 'hspace', 'vspace', 'hfill', 'vfill',
  'center', 'hcenter', 'indent', 'columns', 'vskip', 'hskip'
];

// CL type names for syntax highlighting
const CL_TYPES = ['int', 'string', 'int\\[\\]', 'string\\[\\]', 'map', 'address'];

module.exports = grammar({
  name: 'cassilda',

  extras: $ => [/[ \t]/],  // Don't skip newlines - we need them for structure

  // No conflicts needed - identifier used in nested_dollar_var prefix
  // resolves naturally with the simple identifier in _dollar_var_content

  externals: $ => [
    $.verbatim_delimited,  // External scanner handles \verb|...|
  ],

  rules: {
    document: $ => repeat($._item),

    _item: $ => choice(
      $.output_line,  // Rendered output (lines with NBSP) - must be first
      $.comment,
      $.directive,
      $.label_definition,
      $.label_end,
      $.cassilda_reference,
      $.display_math,
      $.inline_math,
      $.dollar_variable,   // ${name} - variable access (new CL + interpreter)
      $.parameter_ref,     // ${name} inside macro bodies (same syntax, different context)
      $.macro_definition,
      $.environment,
      $.keyword_command,
      $.short_command,       // \! \, \: \; \| - math spacing
      $.linebreak_command,  // \\ - explicit line break
      $.generic_command,    // All \name commands (differentiated by highlights.scm)
      $.brace_group,       // Standalone {scoping} groups
      $.escaped_char,      // \{ \} \< \> \$ \% \# \@ \&
      $.number,
      $.angle_bracket,
      $.square_bracket,    // Standalone [ ] that aren't part of optional args
      $.pipe_char,         // Standalone | characters
      $.newline,
      $.text,
    ),

    square_bracket: $ => choice('[', ']'),
    pipe_char: $ => '|',  // Standalone pipe character

    // Escaped special characters - these produce literal characters
    // Note: \\ is NOT here - it's a line break command, not an escape
    escaped_char: $ => token(seq('\\', choice(
      '{', '}', '[', ']', '<', '>', '$', '%', '#', '@', '&', '_', '^'
    ))),

    // Rendered output lines (contain NBSP \u00A0)
    // These are output from @cassilda: references and should be dimmed
    output_line: $ => token(prec(10, /[^\n]*\u00A0[^\n]*/)),

    // ========================================================================
    // Comments
    // ========================================================================
    comment: $ => token(seq('%', /.*/)),

    // ========================================================================
    // Directives
    // ========================================================================
    directive: $ => choice(
      $.hash_directive,
      $.before_each,
      $.after_each,
      $.hash_end,
    ),

    hash_directive: $ => seq(
      field('name', choice(
        '#source_prefix',
        '#target_prefix',
        '#comment_char',
        '#output_prefix',
        '#output_suffix',
      )),
      optional(field('value', /[^\n]*/)),
    ),

    before_each: $ => '#before_each',
    after_each: $ => '#after_each',
    hash_end: $ => '#end',

    // ========================================================================
    // Labels
    // ========================================================================
    label_definition: $ => seq(
      '@label',
      field('name', $.identifier),
    ),

    label_end: $ => '@end',

    cassilda_reference: $ => seq(
      '@cassilda:',
      field('names', $.reference_list),
    ),

    reference_list: $ => seq(
      $.identifier,
      repeat(seq(',', optional(/\s*/), $.identifier)),
    ),

    // ========================================================================
    // Math modes
    // ========================================================================
    display_math: $ => seq(
      $.display_math_start,
      repeat($._math_item),
      $.display_math_end,
    ),

    display_math_start: $ => '$$',
    display_math_end: $ => '$$',

    inline_math: $ => prec(-1, seq(
      $.inline_math_start,
      repeat($._math_item),
      $.inline_math_end,
    )),

    inline_math_start: $ => '$',
    inline_math_end: $ => '$',

    _math_item: $ => choice(
      $.generic_command,  // All commands (math and user-defined)
      $.short_command,    // \! \, \: \; \| - math spacing
      $.linebreak_command,
      $.escaped_char,
      $.newline,
      $.subscript,
      $.superscript,
      $.math_brace_group,
      $.number,
      $.math_text,
    ),

    subscript: $ => seq($.subscript_op, choice($.math_brace_group, $.generic_command, /[a-zA-Z0-9*+\-']/)),
    superscript: $ => seq($.superscript_op, choice($.math_brace_group, $.generic_command, /[a-zA-Z0-9*+\-']/)),
    subscript_op: $ => '_',
    superscript_op: $ => '^',
    math_brace_group: $ => seq('{', repeat($._math_item), '}'),
    math_text: $ => /[^${}\\^_\s]+/,

    // ========================================================================
    // Variable Access: ${name} and ${item${i}}
    // ========================================================================

    // ${name} - unified variable access (CL syntax, also works in interpreter)
    // Supports: ${name}, ${*name} (dereference), ${item${i}} (nested)
    dollar_variable: $ => prec(3, seq(
      '${',
      optional(field('deref', '*')),  // Optional dereference marker
      field('name', $._dollar_var_content),
      '}',
    )),

    // Content inside ${...} - can be identifier or nested ${...}
    _dollar_var_content: $ => choice(
      $.nested_dollar_var,  // ${item${i}} pattern
      $.identifier,         // Simple ${name}
    ),

    // Nested variable: prefix${inner}suffix
    nested_dollar_var: $ => seq(
      optional(field('prefix', $.identifier)),
      $.dollar_variable,  // The nested ${...}
      optional(field('suffix', /[a-zA-Z0-9_]*/)),
    ),

    // Parameter reference in macro bodies (same syntax, highlighted differently)
    parameter_ref: $ => prec(2, seq(
      '${',
      field('name', $.identifier),
      optional(seq('.', field('field', $.identifier))),  // Field access: ${param.field}
      '}',
    )),

    // ========================================================================
    // Commands - categorized for different highlighting
    // ========================================================================

    // Keywords: \begin, \end, \verb, \sn
    keyword_command: $ => choice(
      $.begin_command,
      $.end_command,
      $.verb_command,
      $.sn_command,
    ),

    begin_command: $ => seq('\\', $.begin_keyword, optional($.angle_group), optional($.optional_arg), $.brace_group),
    begin_keyword: $ => 'begin',
    end_command: $ => seq('\\', $.end_keyword, $.brace_group),
    end_keyword: $ => 'end',

    verb_command: $ => seq(
      '\\verb',
      field('content', $.verbatim_delimited),  // External scanner handles delimiter + content
    ),

    // \sn{...} - Subnivean inline assembly
    // sn_keyword as literal 'sn' beats command_name (regex) for exact match,
    // but loses for longer input like 'snort', so \snort still parses as generic_command
    sn_command: $ => seq('\\', $.sn_keyword, '{', repeat($._asm_item), '}'),
    sn_keyword: $ => 'sn',

    // Assembly sub-grammar inside \sn{...}
    _asm_item: $ => choice(
      $.asm_comment,
      $.asm_label_def,
      $.asm_directive,
      $.asm_instruction,
      $.newline,
    ),

    asm_comment: $ => token(prec(1, seq(';', /[^\n}]*/))),

    asm_label_def: $ => seq(field('name', $.identifier), ':'),

    asm_directive: $ => prec.right(seq(
      field('kind', choice('.const', '.sym')),
      optional(field('value', choice($.asm_string, $.identifier))),
    )),

    asm_instruction: $ => prec.right(seq(
      field('opcode', $.identifier),
      optional(field('operand', choice($.number, $.identifier))),
    )),

    asm_string: $ => token(seq('"', /([^"\\]|\\.)*/, '"')),

    // Dereference marker: * prefix in <*name>
    deref_marker: $ => '*',

    // Type annotation: :int, :string, :int[], :string[], :map, :address
    type_annotation: $ => seq(
      ':',
      field('type', $.type_name),
    ),

    type_name: $ => choice(
      'int',
      'string',
      'int[]',
      'string[]',
      'map',
      'address',
    ),

    // Compact body for computational lambdas: #{...}
    compact_body: $ => seq('#', $.brace_group),

    // Math spacing commands: \! \, \: \; \|
    // These are single non-alpha characters that don't match command_name regex
    short_command: $ => token(seq('\\', choice('!', ',', ':', ';', '|'))),

    // Line break command: \\
    linebreak_command: $ => seq('\\', $.linebreak_name),
    linebreak_name: $ => '\\',

    // Generic command: handles ALL \name commands uniformly
    // Differentiation between calc, control, layout, math is done in highlights.scm
    // This avoids token conflicts between separate keyword token types
    generic_command: $ => prec.right(seq(
      '\\',
      field('name', $.command_name),
      optional($.angle_group),
      optional($.optional_arg),
      optional($.compact_body),
      repeat($.brace_group),
    )),

    command_name: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    // ========================================================================
    // Macro definitions
    // ========================================================================
    macro_definition: $ => seq(
      '\\macro',
      '<',
      field('signature', $.macro_signature),
      '>',
      field('body', $.brace_group),
    ),

    macro_signature: $ => seq(
      '\\',
      field('name', $.command_name),
      optional($.macro_params),
      repeat($.brace_group),
    ),

    macro_params: $ => seq('[', optional($.param_list), ']'),

    param_list: $ => seq(
      $.param,
      repeat(seq(',', $.param)),
    ),

    param: $ => seq(
      field('name', $.identifier),
      optional(seq('=', field('default', $._param_value))),
    ),

    _param_value: $ => choice(
      $.identifier,
      $.number,
      $.brace_group,
      $.param_text,  // Allow any text as default value (e.g., unicode symbols)
    ),

    param_text: $ => /[^\[\],{}=\s]+/,

    // ========================================================================
    // Environments
    // ========================================================================
    // Supports both standard: \begin{name}...\end{name}
    // and iteration: \begin<array>[vars]{enumerate}...\end{enumerate}
    environment: $ => seq(
      '\\begin',
      optional($.angle_group),  // Optional array name for iteration
      optional($.optional_arg),       // Optional loop variables
      '{',
      field('name', $.identifier),
      '}',
      repeat($._item),
      '\\end',
      '{',
      field('end_name', $.identifier),
      '}',
    ),

    // ========================================================================
    // Groups and arguments
    // ========================================================================
    angle_group: $ => seq(
      $.angle_open,
      repeat($._angle_content),
      $.angle_close,
    ),

    angle_open: $ => '<',
    angle_close: $ => '>',

    _angle_content: $ => choice(
      $.comment,
      $.newline,
      $.escaped_char,
      $.deref_marker,        // * for dereference in <*name>
      $.type_annotation,     // :type in <name:type>
      $.dollar_variable,     // ${...} in angle groups
      $.parameter_ref,
      $.inline_math,
      $.keyword_command,
      $.short_command,       // \! \, \: \; \| - math spacing
      $.linebreak_command,
      $.generic_command,
      $.brace_group,
      $.number,
      $.text_in_angle,
    ),

    text_in_angle: $ => /[^<>{}$\\%\n:*]+/,  // Excludes : and * for type_annotation and deref_marker

    optional_arg: $ => seq('[', repeat($._optional_content), ']'),

    // Content inside [...] optional arguments
    // Note: type annotations (:int, :string) are only valid in specific contexts
    // like \lambda<f>[a:int] - they're handled by calc_angle content, not here.
    // General optional args like [align:c] or [width:20] use colon as key:value separator.
    _optional_content: $ => choice(
      /[^\[\]{}]+/,  // Allow colons - they're just part of key:value syntax
      $.brace_group,
      seq('[', repeat($._optional_content), ']'),
    ),

    brace_group: $ => seq('{', repeat($._brace_content), '}'),

    _brace_content: $ => choice(
      $.comment,
      $.newline,
      $.escaped_char,
      $.dollar_variable,     // ${...} variable access
      $.parameter_ref,
      $.inline_math,
      $.keyword_command,
      $.short_command,       // \! \, \: \; \| - math spacing
      $.linebreak_command,
      $.generic_command,
      $.brace_group,
      $.number,
      $.angle_bracket,       // Standalone < > that aren't part of commands
      $.text_in_braces,
    ),

    // Note: map_literal (|...|) and array_literal ([...]) are only valid in
    // calc/CL contexts like \let<x:map>{|1->10|} - they're handled by
    // calc_brace_group, not general _brace_content. This prevents parse errors
    // when | or [ appear in regular text.
    text_in_braces: $ => /[^{}<>$\\%\n]+/,

    // ========================================================================
    // Basic tokens
    // ========================================================================
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_-]*/,
    number: $ => /-?[0-9]+(\.[0-9]+)?/,
    angle_bracket: $ => choice('<', '>'),
    newline: $ => /\n/,
    text: $ => /[^\s%#@$\\{}<>\[\]|0-9]+/,
  },
});

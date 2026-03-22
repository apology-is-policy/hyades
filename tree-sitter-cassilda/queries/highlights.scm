; Cassilda/Hyades Tree-sitter Highlight Queries
; All commands parse as generic_command; differentiation by command_name matching
;
; Updated: 2026-02-06
; - Unified all command types under generic_command
; - Added ${name} variable access (dollar_variable / parameter_ref)
; - Added type annotations (:int, :string[], etc.)
; - Added dereference marker (*name)
; - Added nested variable access ${item${i}}

; ============================================================================
; Rendered output (lines containing NBSP - dimmed/comment style)
; ============================================================================
;(output_line) @variable

; ============================================================================
; Comments (LSP: SEM_TOK_COMMENT)
; ============================================================================
(comment) @comment

; ============================================================================
; Directives (LSP: SEM_TOK_KEYWORD)
; ============================================================================
(hash_directive) @module
(before_each) @module
(after_each) @module
(hash_end) @module

; ============================================================================
; Labels (LSP: SEM_TOK_KEYWORD for @label/@end, SEM_TOK_VARIABLE for name)
; ============================================================================
(label_definition
  "@label" @module
  name: (identifier) @variable)

(label_end) @module

(cassilda_reference
  "@cassilda:" @module
  names: (reference_list
    (identifier) @variable))

; ============================================================================
; Math delimiters (LSP: SEM_TOK_KEYWORD)
; ============================================================================
(display_math_start) @keyword.directive
(display_math_end) @keyword.directive
(inline_math_start) @keyword.directive
(inline_math_end) @keyword.directive

; ============================================================================
; Variable access ${name}, ${*name}, ${item${i}} (LSP: SEM_TOK_VARIABLE)
; ============================================================================
(dollar_variable
  "${" @punctuation.special
  deref: "*"? @keyword.operator
  name: (_) @variable
  "}" @punctuation.special)

; Parameter references in macro bodies (same syntax, different context)
(parameter_ref
  "${" @punctuation.special
  name: (identifier) @variable.parameter
  field: (identifier)? @property
  "}" @punctuation.special)

; Nested variable access ${item${i}}
; The whole nested_dollar_var is captured, inner dollar_variable handled by its own rule
(nested_dollar_var) @variable

; ============================================================================
; Type annotations :int, :string[], :map, :address (LSP: SEM_TOK_TYPE)
; ============================================================================
(type_annotation
  ":" @punctuation
  type: (type_name) @type)

; Dereference marker * in angle brackets
(deref_marker) @keyword.operator

; ============================================================================
; Keyword commands
; \begin, \end, \verb
; ============================================================================
(begin_command
  "\\" @keyword
  (begin_keyword) @keyword)

(end_command
  "\\" @keyword
  (end_keyword) @keyword)

(verb_command
  "\\verb" @keyword
  content: (verbatim_delimited) @string)

; \sn{} inline assembly
(sn_command
  "\\" @type
  (sn_keyword) @type
  "{" @punctuation.bracket
  "}" @punctuation.bracket)
(asm_comment) @comment
(asm_label_def
  name: (identifier) @label
  ":" @punctuation)
(asm_directive
  kind: _ @module)
(asm_directive
  value: (asm_string) @string)
(asm_directive
  value: (identifier) @variable)
(asm_instruction
  opcode: (identifier) @keyword)
(asm_instruction
  operand: (number) @constant.numeric)
(asm_instruction
  operand: (identifier) @variable)

; Math spacing commands (\! \, \: \; \|)
(short_command) @keyword.directive

; Compact body marker #
(compact_body
  "#" @type)

; ============================================================================
; Line break command (\\)
; ============================================================================
(linebreak_command
  "\\" @function.builtin
  (linebreak_name) @function.builtin)

; ============================================================================
; Generic/user commands - fallback (must come BEFORE specific patterns)
; In tree-sitter, later patterns override earlier ones for the same node,
; so specific #match? patterns below will take priority over this fallback.
; ============================================================================
(generic_command
  "\\" @function
  name: (command_name) @function)

; ============================================================================
; Calc/Lambda commands (computational subsystem) - highlighted as @type
; \lambda, \recall, \invoke, \at, \set, \let, \valueof, \ref, \return,
; \exit_when, \push, \pop, \len, \split, \setelement, \addressof,
; \map_has, \map_del, \map_keys, \mem_alloc, \mem_load, \mem_store,
; \assign, \inc, \dec, \emit, \cursor, \ansi, \peek, \enqueue, \dequeue
; ============================================================================
((generic_command
  "\\" @type
  name: (command_name) @type)
  (#match? @type "^(lambda|let|assign|valueof|recall|ref|addressof|at|invoke|inc|dec|set|setelement|push|pop|peek|enqueue|dequeue|map_has|map_del|map_keys|mem_alloc|mem_load|mem_store|len|split|emit|cursor|ansi|return|exit_when)$"))

; ============================================================================
; Control commands (LSP: SEM_TOK_KEYWORD)
; \if, \else, \add, \sub, \mul, \eq, \gt, \rand, etc.
; ============================================================================
((generic_command
  "\\" @keyword
  name: (command_name) @keyword)
  (#match? @keyword "^(if|else|add|sub|mul|div|mod|neg|abs|floor|ceil|round|max|min|rand|eq|neq|ne|lt|gt|leq|le|geq|ge|and|or|not|streq|strneq|strlen|substr|strcat|startswith|endswith|contains|measure|measureref|lineroutine|unicode|ascii|width|parskip|setunicode|setwidth|setmathitalic|setparskip|setmathabove|setmathbelow|linebreaker|setlinepenalty|sethyphenpenalty|setconsechyphenpenalty|settolerance)$"))

; ============================================================================
; Layout commands (LSP: SEM_TOK_FUNCTION with DEFAULT_LIBRARY)
; \table, \row, \col, \frame, \hbox, \vbox, etc.
; ============================================================================
((generic_command
  "\\" @keyword
  name: (command_name) @keyword)
  (#match? @keyword "^(table|row|col|frame|hbox|vbox|child|hrule|vrule|hspace|vspace|hfill|vfill|center|hcenter|indent|columns|vskip|hskip)$"))

; ============================================================================
; Math commands (LSP: SEM_TOK_FUNCTION with DEFAULT_LIBRARY)
; \frac, \sum, \int, Greek letters, etc.
; ============================================================================
((generic_command
  "\\" @keyword.directive
  name: (command_name) @keyword.directive)
  (#match? @keyword.directive "^(frac|sqrt|sum|prod|int|iint|iiint|oint|lim|sin|cos|tan|cot|sec|csc|arcsin|arccos|arctan|sinh|cosh|tanh|log|ln|exp|sup|inf|arg|gcd|det|partial|nabla|infty|pm|mp|times|cdot|approx|equiv|subset|supset|in|notin|forall|exists|given|cond|vee|wedge|oplus|otimes|rightarrow|leftarrow|Rightarrow|Leftarrow|leftrightarrow|uparrow|downarrow|mapsto|to|gets|alpha|beta|gamma|delta|epsilon|varepsilon|zeta|eta|theta|vartheta|iota|kappa|mu|nu|xi|pi|varpi|rho|varrho|sigma|varsigma|tau|upsilon|phi|varphi|chi|psi|omega|Gamma|Delta|Theta|Lambda|Xi|Pi|Sigma|Upsilon|Phi|Psi|Omega|binom|tbinom|dbinom|cases|matrix|pmatrix|bmatrix|hat|bar|vec|dot|ddot|tilde|overline|underline|fn|overbrace|underbrace|text|mathbf|mathbb|mathcal|mathrm|operatorname|left|right|big|Big|bigg|Bigg|langle|rangle|lfloor|rfloor|lceil|rceil|quad|qquad|setminus|lbrace|rbrace|lvert|rvert|Vert|ll|gg|sim|simeq|cong|propto|ni|subseteq|supseteq|cup|cap|emptyset|varnothing|nexists|land|lor|implies|iff|Leftrightarrow|top|bot|perp|longmapsto|longrightarrow|longleftarrow|hookrightarrow|hookleftarrow|updownarrow|Uparrow|Downarrow|nearrow|searrow|nwarrow|swarrow|bigcup|bigcap|Iint|IINT|Iiint|IIInt|Oint|OINT|oiint|Oiint|OIINT|coprod|Coprod|COPROD|Sum|SUM|Prod|PROD|limsup|liminf|lcm|Pr|acute|grave|breve|check|widehat|widetilde|overrightarrow|overleftarrow|Bmatrix|Vmatrix|vmatrix|cdots|vdots|ddots|ldots|hbar|ell|mid|Var|Cov|pmod|bmod|nmid|ominus|odot|circ|bullet|star|asc|dagger|ddagger|textbackslash|textit|figlet|boxed|aligned|gather)$"))

; ============================================================================
; Macro definitions (LSP: SEM_TOK_KEYWORD for \macro)
; ============================================================================
(macro_definition
  "\\macro" @type
  "<" @type
  signature: (macro_signature
    name: (command_name) @function.definition)
  ">" @type)

(param
  name: (identifier) @variable.parameter)

; ============================================================================
; Environments - name captured in main rule above with @label
; ============================================================================

; ============================================================================
; Angle brackets (LSP: SEM_TOK_CLASS - light blue)
; ============================================================================
(angle_open) @type
(angle_close) @type
(angle_bracket) @type

; ============================================================================
; Numbers (LSP: SEM_TOK_NUMBER)
; ============================================================================
(number) @constant.numeric

; ============================================================================
; Math content
; ============================================================================
(math_text) @variable

; Subscript/superscript operators (LSP: SEM_TOK_OPERATOR)
(subscript_op) @keyword.operator
(superscript_op) @keyword.operator

; ============================================================================
; Brackets (LSP: SEM_TOK_CLASS)
; ============================================================================
"{" @type
"}" @type
"[" @type
"]" @type
(square_bracket) @type
; Note: () not in grammar as explicit tokens, so covered by LSP semantic tokens only
; Note: [..] inside brace_group text (e.g. \vrule[auto] in {..}) is consumed
;       by the text_in_braces regex and can't be individually highlighted

; ============================================================================
; Escaped characters (\{ \} \< \> etc.)
; ============================================================================
(escaped_char) @string.escape

; ============================================================================
; Text and identifiers
; ============================================================================
(identifier) @variable

; ============================================================================
; Environment names (must be after generic identifier to take precedence)
; ============================================================================
(environment
  "\\begin" @keyword
  name: (identifier) @keyword
  "\\end" @keyword
  end_name: (identifier) @keyword)

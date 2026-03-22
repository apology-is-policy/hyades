;;; cassilda-mode.el --- Major mode for Cassilda/Hyades documents -*- lexical-binding: t; -*-

;; Copyright (C) 2025
;; Author: Hyades Project
;; Version: 1.0.0
;; Keywords: languages, tex, math
;; Package-Requires: ((emacs "29.1"))

;;; Commentary:

;; Major mode for editing Cassilda (.cld) documents with LSP support via eglot.
;;
;; Features:
;; - Syntax highlighting for Cassilda/Hyades markup
;; - LSP integration (diagnostics, completion, hover, go-to-definition)
;; - Semantic token highlighting
;;
;; Installation:
;;   (load "/path/to/cassilda-mode.el")
;;
;; Or add to your init.el:
;;   (add-to-list 'load-path "/path/to/lsp-server")
;;   (require 'cassilda-mode)

;;; Code:

(require 'eglot)

;; ============================================================================
;; Customization
;; ============================================================================

(defgroup cassilda nil
  "Major mode for Cassilda/Hyades documents."
  :group 'languages
  :prefix "cassilda-")

(defcustom cassilda-lsp-server-path nil
  "Path to the Hyades LSP server.
If nil, uses the server relative to this file's location."
  :type '(choice (const nil) string)
  :group 'cassilda)

;; ============================================================================
;; Syntax Highlighting (Font Lock)
;; ============================================================================

(defvar cassilda-font-lock-keywords
  `(
    ;; Comments
    ("%.*$" . font-lock-comment-face)

    ;; Directives
    ("#\\(?:source_prefix\\|target_prefix\\|comment_char\\|output_prefix\\|output_suffix\\|before_each\\|after_each\\|end\\)\\b"
     . font-lock-preprocessor-face)

    ;; Label definition and end
    ("@label\\s-+\\(\\w+\\)"
     (0 font-lock-keyword-face)
     (1 font-lock-type-face))
    ("@end\\b" . font-lock-keyword-face)

    ;; Cassilda reference
    ("@cassilda:\\s-*\\([\\w-]+\\)"
     (0 font-lock-keyword-face)
     (1 font-lock-variable-name-face))

    ;; Display math delimiters
    ("\\$\\$" . font-lock-string-face)

    ;; Inline math (but not ${...} parameter refs)
    ("\\$\\(?:[^${}]\\|{[^}]*}\\)*\\$" . font-lock-string-face)

    ;; Macro parameter reference ${name}
    ("\\${[^}]*}" . font-lock-variable-name-face)

    ;; TeX commands
    ("\\\\\\(?:frac\\|sqrt\\|sum\\|prod\\|int\\|lim\\|sin\\|cos\\|tan\\|log\\|ln\\|exp\\|begin\\|end\\|left\\|right\\|text\\|mathbf\\|mathbb\\|mathcal\\)\\b"
     . font-lock-builtin-face)

    ;; User-defined macros (backslash followed by word)
    ("\\\\[a-zA-Z]+" . font-lock-function-name-face)

    ;; Numbers
    ("\\b[0-9]+\\(?:\\.[0-9]+\\)?\\b" . font-lock-constant-face)
    )
  "Font lock keywords for Cassilda mode.")

;; ============================================================================
;; Syntax Table
;; ============================================================================

(defvar cassilda-mode-syntax-table
  (let ((st (make-syntax-table)))
    ;; % starts a comment to end of line
    (modify-syntax-entry ?% "<" st)
    (modify-syntax-entry ?\n ">" st)

    ;; Backslash is an escape/prefix
    (modify-syntax-entry ?\\ "/" st)

    ;; $ is a string delimiter (for math mode visual)
    (modify-syntax-entry ?$ "\"" st)

    ;; Braces are paired delimiters
    (modify-syntax-entry ?{ "(}" st)
    (modify-syntax-entry ?} "){" st)

    ;; Brackets
    (modify-syntax-entry ?[ "(]" st)
    (modify-syntax-entry ?] ")[" st)

    ;; @ and # are symbol constituents
    (modify-syntax-entry ?@ "_" st)
    (modify-syntax-entry ?# "_" st)

    st)
  "Syntax table for Cassilda mode.")

;; ============================================================================
;; LSP (Eglot) Integration
;; ============================================================================

(defun cassilda--lsp-server-command ()
  "Return the command to start the Hyades LSP server."
  (let* ((this-file (or load-file-name buffer-file-name))
         (lsp-dir (if this-file
                      (file-name-directory this-file)
                    default-directory))
         (server-path (or cassilda-lsp-server-path
                          (expand-file-name "dist/server.js" lsp-dir))))
    (if (file-exists-p server-path)
        `("node" ,server-path "--stdio")
      ;; Fallback: try to find it via npm
      '("npx" "hyades-lsp" "--stdio"))))

;; Register with eglot
(add-to-list 'eglot-server-programs
             '(cassilda-mode . cassilda--lsp-server-command))

;; ============================================================================
;; Semantic Token Support
;; ============================================================================

;; Map LSP semantic token types to Emacs faces
;; These match the token types from hyades_parse_api.c
(defvar cassilda-semantic-token-faces
  '(("comment" . font-lock-comment-face)
    ("keyword" . font-lock-keyword-face)
    ("string" . font-lock-string-face)
    ("number" . font-lock-constant-face)
    ("operator" . font-lock-builtin-face)
    ("function" . font-lock-function-name-face)
    ("macro" . font-lock-preprocessor-face)
    ("variable" . font-lock-variable-name-face)
    ("parameter" . font-lock-variable-name-face)
    ("type" . font-lock-type-face)
    ("class" . font-lock-type-face)
    ("property" . font-lock-constant-face)
    ("label" . font-lock-constant-face)
    ("enumMember" . font-lock-constant-face)
    ("event" . font-lock-warning-face)
    ("modifier" . font-lock-keyword-face)
    ("regexp" . font-lock-string-face)
    ("decorator" . font-lock-preprocessor-face)
    ("namespace" . font-lock-constant-face)
    ("typeParameter" . font-lock-type-face))
  "Mapping of LSP semantic token types to Emacs faces.")

;; ============================================================================
;; Mode Definition
;; ============================================================================

;;;###autoload
(define-derived-mode cassilda-mode prog-mode "Cassilda"
  "Major mode for editing Cassilda/Hyades documents.

Cassilda is a TeX-like markup language for Unicode mathematical typesetting.

\\{cassilda-mode-map}"
  :syntax-table cassilda-mode-syntax-table

  ;; Font lock
  (setq-local font-lock-defaults '(cassilda-font-lock-keywords))

  ;; Comments
  (setq-local comment-start "% ")
  (setq-local comment-end "")
  (setq-local comment-start-skip "%+\\s-*")

  ;; Indentation (simple for now)
  (setq-local indent-tabs-mode nil)
  (setq-local tab-width 2)

  ;; Paragraph handling
  (setq-local paragraph-start "\\|@\\|#\\|$")
  (setq-local paragraph-separate paragraph-start))

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.cld\\'" . cassilda-mode))

;; ============================================================================
;; Interactive Commands
;; ============================================================================

(defun cassilda-start-lsp ()
  "Start the Hyades LSP server for the current buffer."
  (interactive)
  (eglot-ensure))

(defun cassilda-restart-lsp ()
  "Restart the Hyades LSP server."
  (interactive)
  (eglot-shutdown (eglot-current-server))
  (eglot-ensure))

;; ============================================================================
;; Keybindings
;; ============================================================================

(defvar cassilda-mode-map
  (let ((map (make-sparse-keymap)))
    ;; LSP commands
    (define-key map (kbd "C-c C-l") #'cassilda-start-lsp)
    (define-key map (kbd "C-c C-r") #'cassilda-restart-lsp)
    map)
  "Keymap for Cassilda mode.")

;; ============================================================================
;; Auto-start LSP
;; ============================================================================

(defun cassilda--maybe-start-lsp ()
  "Start LSP if eglot is available."
  (when (and (featurep 'eglot)
             (not (eglot-current-server)))
    (eglot-ensure)))

;; Uncomment to auto-start LSP when opening .cld files:
;; (add-hook 'cassilda-mode-hook #'cassilda--maybe-start-lsp)

(provide 'cassilda-mode)

;;; cassilda-mode.el ends here

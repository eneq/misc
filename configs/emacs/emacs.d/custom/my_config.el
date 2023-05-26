;; -*- mode: emacs-lisp -*-
(provide 'my_config)

; First, avoid the evil:
(when (featurep 'xemacs)
  (error "This machine runs XEmacs this config is for Emacs"))

; Set custom variables
(custom-set-variables
 '(text-mode-hook (quote (text-mode-hook-identify)))
 '(tool-bar-mode nil)		        ; Remove toolbar
 '(tooltip-mode nil)		        ; No tooltipes
 '(blink-cursor-mode nil)		; Solid cursor
 '(display-time-day-and-date t)		; Date and time
 '(font-lock-maximum-decoration t)      ; As much font decoration as possible
 '(font-lock-mode t nil (font-lock))	; Font lock mode enabled
 '(global-font-lock-mode t)		; Global font lock on
 '(indent-tabs-mode nil)		; Dont use tabs, use spaces
 '(kill-whole-line t)			; Kill whole line in one 
 '(line-number-mode t)			; Line numbering
 '(column-number-mode t)		; Column numbering
 '(next-line-add-newlines nil)		; Disable automatic line generation
 '(time-stamp-active t)			; Enable time stamp support
 '(delete-selection-mode t)		; Replace selected text
 '(inhibit-startup-screen t)		; Skip splash screen
 '(visible-bell t)                      ; Skip the beep use visual flash
 '(scroll-step t)                       ; Scroll step 1
 '(fill-column 78)			; Def. fill column 78
 '(auto-fill-mode t)                    ; Auto fill mode on
 '(mouse-wheel-progressive-speed nil)   ; Fixed mouse scrolling
 '(enable-recursive-minibuffers t)      ; Enable minibuffer play
 '(max-mini-window-height 1)            ; Stop miniwindow resizing
 '(server-kill-new-buffers t)           ; Kills the buffers when a server frame dies
)

; Save temp files to system temp instead of directory tree
(setq backup-directory-alist
      `((".*" . ,temporary-file-directory)))
(setq auto-save-file-name-transforms
      `((".*" ,temporary-file-directory t)))

; ws-trim
(add-to-list 'load-path "~/.emacs.d/ws-trim")
(require 'ws-trim)

; Display tome
(display-time)

; Doxymacs
(add-to-list 'load-path "~/.emacs.d/doxymacs")
(require 'doxymacs)


; Setup color-theme
(add-to-list 'load-path "~/.emacs.d/color-theme")
(require 'color-theme)
(color-theme-initialize)
(color-theme-deep-blue)

; Start server
;(require 'server)
;(if (not (server-running-p))
;  (progn 
(server-mode t)
(add-hook 'server-switch-hook
          (lambda nil
            (let ((server-buf (current-buffer)))
              (bury-buffer)
              (switch-to-buffer-other-frame server-buf))))

;))

; Treat y and n as Yes and No
(fset 'yes-or-no-p 'y-or-n-p)

; Update string in the first 8 lines looking like Timestamp: <> or " "
(add-hook 'write-file-hooks 'time-stamp)

; Allow for killring navigation using M-y
(defadvice yank-pop (around kill-ring-browse-maybe (arg))
  "If last action was not a yank, run `browse-kill-ring' instead."
  (if (not (eq last-command 'yank))
      (browse-kill-ring)
    ad-do-it))
(ad-activate 'yank-pop)

; Setup C-mode as I want it
(defun my-c-stuff ()
  "My C Things."
  (interactive)
  (c-set-style "stroustrup")
  (setq c-tab-always-indent t)  
  (setq c-auto-align-backslashes t)
  (setq comment-column 48)
  (define-key c-mode-base-map [(return)]           'newline-and-indent)
  (define-key c-mode-base-map [(control return)]   'newline)
  (turn-on-auto-fill)
                                        ; Want backtab to be forced tab
  (local-set-key [S-tab] 'tab-to-tab-stop)
  (turn-on-ws-trim)
  ;; Automatic revert of files edited in other IDE (studio)
  (auto-revert-mode t)
  (add-hook 'c-mode-common-hook 'doxymacs-mode)
  )

; Add mode hooks
(add-hook 'c-mode-hook     '(lambda () (my-c-stuff)))
(add-hook 'c++-mode-hook   '(lambda () (my-c-stuff)))

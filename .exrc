if &cp | set nocp | endif
let s:cpo_save=&cpo
set cpo&vim
imap <S-Tab> <Plug>SuperTabBackward
inoremap <C-Tab> 	
inoremap <Right> <Nop>
inoremap <Left> <Nop>
inoremap <Down> <Nop>
inoremap <Up> <Nop>
nnoremap <silent>  :nohl
nnoremap <silent> ,c :call g:ClangUpdateQuickFix()
vnoremap < <gv
vnoremap > >gv
nmap gx <Plug>NetrwBrowseX
nnoremap gj j
nnoremap gk k
nnoremap j gj
nnoremap k gk
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#NetrwBrowseX(expand("<cfile>"),0)
imap 	 <Plug>SuperTabForward
cabbr rename =getcmdpos() == 1 && getcmdtype() == ":" ? "Rename" : "rename"
cnoreabbr <expr> help getcmdtype() == ":" && getcmdline() == "help" ? "vertical help" : "help"
cnoreabbr <expr> h getcmdtype() == ":" && getcmdline() == "h" ? "vertical h" : "h"
let &cpo=s:cpo_save
unlet s:cpo_save
set autoindent
set background=dark
set backspace=indent,eol,start
set completeopt=menu,menuone
set fileencodings=ucs-bom,utf-8,default,latin1
set formatoptions=tcqor2
set helplang=en
set hidden
set incsearch
set pumheight=20
set ruler
set runtimepath=~/.vim/bundle/vundle,~/.vim/bundle/tabular,~/.vim/bundle/smartfile,~/.vim/bundle/rename.vim,~/.vim/bundle/vim-colorschemes,~/.vim/bundle/vim-colors-solarized,~/.vim/bundle/clang_complete,~/.vim/bundle/supertab,~/.vim/bundle/ultisnips,~/.vim/bundle/vim-snippets,~/.vim/bundle/vim-markdown,~/.vim,/usr/share/vim/vimfiles,/usr/share/vim/vim74,/usr/share/vim/vimfiles/after,~/.vim/after,~/.vim/bundle/vundle/after,~/.vim/bundle/tabular/after,~/.vim/bundle/smartfile/after,~/.vim/bundle/rename.vim/after,~/.vim/bundle/vim-colorschemes/after,~/.vim/bundle/vim-colors-solarized/after,~/.vim/bundle/clang_complete/after,~/.vim/bundle/supertab/after,~/.vim/bundle/ultisnips/after,~/.vim/bundle/vim-snippets/after,~/.vim/bundle/vim-markdown/after
set sessionoptions=blank,buffers,folds,help,options,tabpages,winsize,sesdir
set showcmd
set showmatch
set splitright
set suffixes=.bak,~,.swp,.o,.info,.aux,.log,.dvi,.bbl,.blg,.brf,.cb,.ind,.idx,.ilg,.inx,.out,.toc
set textwidth=80
set ttimeoutlen=100
set visualbell
" vim: set ft=vim :

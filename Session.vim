let SessionLoad = 1
if &cp | set nocp | endif
let s:cpo_save=&cpo
set cpo&vim
imap <S-Tab> <Plug>SuperTabBackward
inoremap <silent> <C-Tab> =UltiSnips#ListSnippets()
inoremap <Right> <Nop>
inoremap <Left> <Nop>
inoremap <Down> <Nop>
inoremap <Up> <Nop>
map! <D-v> *
snoremap <silent>  c
xnoremap <silent> 	 :call UltiSnips#SaveLastVisualSelection()gvs
snoremap <silent> 	 :call UltiSnips#ExpandSnippetOrJump()
nnoremap <silent>  :nohl
vnoremap < <gv
vnoremap > >gv
nmap gx <Plug>NetrwBrowseX
nnoremap gj j
nnoremap gk k
nnoremap j gj
nnoremap k gk
nnoremap <silent> <Plug>NetrwBrowseX :call netrw#NetrwBrowseX(expand("<cWORD>"),0)
snoremap <silent> <Del> c
snoremap <silent> <BS> c
snoremap <silent> <C-Tab> :call UltiSnips#ListSnippets()
xmap <BS> "-d
vmap <D-x> "*d
vmap <D-c> "*y
vmap <D-v> "-d"*P
nmap <D-v> "*P
inoremap <silent> 	 =UltiSnips#ExpandSnippetOrJump()
cabbr rename =getcmdpos() == 1 && getcmdtype() == ":" ? "Rename" : "rename"
cnoreabbr <expr> help getcmdtype() == ":" && getcmdline() == "help" ? "vertical help" : "help"
cnoreabbr <expr> h getcmdtype() == ":" && getcmdline() == "h" ? "vertical h" : "h"
let &cpo=s:cpo_save
unlet s:cpo_save
set autoindent
set backspace=indent,eol,start
set completeopt=menuone
set fileencodings=ucs-bom,utf-8,default,latin1
set formatoptions=tcqor2
set helplang=en
set hidden
set incsearch
set pumheight=20
set ruler
set runtimepath=~/.vim,~/.vim/bundle/vundle,~/.vim/bundle/tabular,~/.vim/bundle/smartfile,~/.vim/bundle/rename.vim,~/.vim/bundle/vim-colorschemes,~/.vim/bundle/vim-colors-solarized,~/.vim/bundle/clang_complete,~/.vim/bundle/supertab,~/.vim/bundle/ultisnips,~/.vim/bundle/vim-snippets,~/.vim/bundle/vim-markdown,/opt/local/share/vim/vimfiles,/opt/local/share/vim/vim74,/opt/local/share/vim/vimfiles/after,~/.vim/after,~/.vim/bundle/vundle/after,~/.vim/bundle/tabular/after,~/.vim/bundle/smartfile/after,~/.vim/bundle/rename.vim/after,~/.vim/bundle/vim-colorschemes/after,~/.vim/bundle/vim-colors-solarized/after,~/.vim/bundle/clang_complete/after,~/.vim/bundle/supertab/after,~/.vim/bundle/ultisnips/after,~/.vim/bundle/vim-snippets/after,~/.vim/bundle/vim-markdown/after
set sessionoptions=blank,buffers,folds,help,options,tabpages,winsize,sesdir
set showcmd
set showmatch
set splitright
set textwidth=80
set ttimeoutlen=100
set visualbell
let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0
let v:this_session=expand("<sfile>:p")
silent only
exe "cd " . escape(expand("<sfile>:p:h"), ' ')
if expand('%') == '' && !&modified && line('$') <= 1 && getline(1) == ''
  let s:wipebuf = bufnr('%')
endif
set shortmess=aoO
badd +0 src/os_x/copy_benchmark.cpp
badd +0 src/os_x/read_benchmark.cpp
badd +0 src/os_x/write_benchmark.cpp
badd +0 include/configuration.hpp
badd +0 include/copy_common.hpp
badd +0 include/io_common.hpp
badd +0 include/read_common.hpp
badd +0 include/test.hpp
badd +0 include/write_common.hpp
badd +0 tools/make_data.rb
badd +0 tools/test_read.sh
badd +0 tools/test_write.sh
badd +0 Rakefile
argglobal
silent! argdel *
argadd ~/projects/c++/io_benchmark/src/os_x/copy_benchmark.cpp
argadd ~/projects/c++/io_benchmark/src/os_x/read_benchmark.cpp
argadd ~/projects/c++/io_benchmark/src/os_x/write_benchmark.cpp
argadd ~/projects/c++/io_benchmark/include/configuration.hpp
argadd ~/projects/c++/io_benchmark/include/copy_common.hpp
argadd ~/projects/c++/io_benchmark/include/io_common.hpp
argadd ~/projects/c++/io_benchmark/include/read_common.hpp
argadd ~/projects/c++/io_benchmark/include/test.hpp
argadd ~/projects/c++/io_benchmark/include/write_common.hpp
argadd ~/projects/c++/io_benchmark/tools/make_data.rb
argadd ~/projects/c++/io_benchmark/tools/test_read.sh
argadd ~/projects/c++/io_benchmark/tools/test_write.sh
argadd ~/projects/c++/io_benchmark/Rakefile
edit src/os_x/copy_benchmark.cpp
set splitbelow splitright
set nosplitbelow
wincmd t
set winheight=1 winwidth=1
argglobal
snoremap <buffer> <silent> 	 :python updateSnips()
nnoremap <buffer> <silent> 	 :python updateSnips()
xnoremap <buffer> <silent> 	 :python updateSnips()
onoremap <buffer> <silent> 	 :python updateSnips()
nnoremap <buffer> <silent>  
setlocal keymap=
setlocal noarabic
setlocal autoindent
setlocal nobinary
setlocal bufhidden=
setlocal buflisted
setlocal buftype=
setlocal cindent
setlocal cinkeys=0{,0},0),:,0#,!^F,o,O,e
setlocal cinoptions=
setlocal cinwords=if,else,while,do,for,switch
setlocal colorcolumn=
setlocal comments=s0:/*,mb:**,ex:*/,b://
setlocal commentstring=/*%s*/
setlocal complete=.,w,b,u,t,i
set concealcursor=vin
setlocal concealcursor=vin
set conceallevel=2
setlocal conceallevel=2
setlocal completefunc=ClangComplete
setlocal nocopyindent
setlocal cryptmethod=
setlocal nocursorbind
setlocal nocursorcolumn
set cursorline
setlocal cursorline
setlocal define=
setlocal dictionary=
setlocal nodiff
setlocal equalprg=
setlocal errorformat=
setlocal noexpandtab
if &filetype != 'cpp'
setlocal filetype=cpp
endif
setlocal foldcolumn=0
setlocal foldenable
setlocal foldexpr=0
setlocal foldignore=#
setlocal foldlevel=0
setlocal foldmarker={{{,}}}
setlocal foldmethod=manual
setlocal foldminlines=1
setlocal foldnestmax=20
setlocal foldtext=foldtext()
setlocal formatexpr=
setlocal formatoptions=2croql
setlocal formatlistpat=^\\s*\\d\\+[\\]:.)}\\t\ ]\\s*
setlocal grepprg=
setlocal iminsert=0
setlocal imsearch=0
setlocal include=
setlocal includeexpr=
setlocal indentexpr=
setlocal indentkeys=0{,0},:,0#,!^F,o,O,e
setlocal noinfercase
setlocal iskeyword=@,48-57,_,192-255
setlocal keywordprg=
setlocal nolinebreak
setlocal nolisp
setlocal lispwords=
setlocal nolist
setlocal makeprg=
setlocal matchpairs=(:),{:},[:]
setlocal modeline
setlocal modifiable
setlocal nrformats=octal,hex
setlocal nonumber
setlocal numberwidth=4
setlocal omnifunc=ClangComplete
setlocal path=
setlocal nopreserveindent
setlocal nopreviewwindow
setlocal quoteescape=\\
setlocal noreadonly
setlocal norelativenumber
setlocal norightleft
setlocal rightleftcmd=search
setlocal noscrollbind
setlocal shiftwidth=8
setlocal noshortname
setlocal smartindent
setlocal softtabstop=0
setlocal nospell
setlocal spellcapcheck=[.?!]\\_[\\])'\"\	\ ]\\+
setlocal spellfile=
setlocal spelllang=en
setlocal statusline=
setlocal suffixesadd=
setlocal swapfile
setlocal synmaxcol=3000
if &syntax != 'cpp'
setlocal syntax=cpp
endif
setlocal tabstop=8
setlocal tags=
setlocal textwidth=80
setlocal thesaurus=
setlocal noundofile
setlocal undolevels=-123456
setlocal nowinfixheight
setlocal nowinfixwidth
setlocal wrap
setlocal wrapmargin=0
silent! normal! zE
let s:l = 1 - ((0 * winheight(0) + 27) / 55)
if s:l < 1 | let s:l = 1 | endif
exe s:l
normal! zt
1
normal! 0
tabnext 1
if exists('s:wipebuf')
  silent exe 'bwipe ' . s:wipebuf
endif
unlet! s:wipebuf
set winheight=1 winwidth=20 shortmess=filnxtToO
let s:sx = expand("<sfile>:p:r")."x.vim"
if file_readable(s:sx)
  exe "source " . fnameescape(s:sx)
endif
let &so = s:so_save | let &siso = s:siso_save
doautoall SessionLoadPost
unlet SessionLoad
" vim: set ft=vim :

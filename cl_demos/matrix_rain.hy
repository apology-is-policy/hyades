\setunicode{true}

%%% ═══════════════════════════════════════════════════════════════════════════
%%%                         MATRIX DIGITAL RAIN
%%%
%%%                 Run with "hyades -c matrix_rain.hy"
%%%
%%%                          Press 'q' to quit
%%% ═══════════════════════════════════════════════════════════════════════════

%%% Seed RNG from time for variety
\srand{\time}

%%% Terminal size (reserve last row for status bar)
%%% Each fullwidth katakana is 2 terminal columns wide
\let<COLS>{\div{\term_cols,2}}
\let<ROWS>{\sub{\term_rows,1}}

%%% Katakana character array (persistent store string array)
\let<K>#{[ア,イ,ウ,エ,オ,カ,キ,ク,ケ,コ,サ,シ,ス,セ,ソ,タ,チ,ツ,テ,ト,ナ,ニ,ヌ,ネ,ノ,ハ,ヒ,フ,ヘ,ホ,マ,ミ,ム,メ,モ,ヤ,ユ,ヨ,ラ,リ,ル,レ,ロ,ワ,ン,ヲ,゛,゜]}

%%% Maps for drop state (using Subnivean persistent store)
\let<pos_map>#{||}
\let<spd_map>#{||}
\let<trail_map>#{||}
\let<chr_map>#{||}

%%% Frame counter
\let<frame>{0}

%%% Allocate persistent heap buffer for virtual screen
%%% Each cell stores packed value: char_idx * 4 + color_tier
%%% mem_alloc only works inside #{} lambdas (Subnivean bytecode)
\lambda<ALLOC_BUF>[size:int]#{\return{\mem_alloc{${size}}}}
\let<buf>{\invoke<ALLOC_BUF>[\mul{${COLS},${ROWS}}]}

%%% Initialize columns with random values
\let<c>{0}
\begin{loop}
    \exit_when{\ge{${c},${COLS}}}
    \map_set<pos_map>{${c},\sub{0,\rand{${ROWS}}}}
    \map_set<spd_map>{${c},\add{1,\rand{3}}}
    \map_set<trail_map>{${c},\add{8,\rand{24}}}
    \map_set<chr_map>{${c},\rand{100}}
    \inc<c>
\end{loop}

%%% COMPUTE lambda - pure computation, compiled to Subnivean bytecode
%%% Computes all cell values into a persistent heap buffer
\lambda<COMPUTE>[cols:int, rows:int, frm:int, buf_addr:int, pos_addr:int, spd_addr:int, trail_addr:int, chr_addr:int]#{
    %%% Clear buffer each frame (prevents stale trail cells with speed > 1)
    \let<total:int>{\mul{${cols}, ${rows}}}
    \let<idx:int>{0}
    \begin{loop}
        \exit_when{\ge{${idx}, ${total}}}
        \mem_store{${buf_addr}, ${idx}, 0}
        \inc<idx>
    \end{loop}
    \let<c:int>{0}
    \begin{loop}
        \exit_when{\ge{${c}, ${cols}}}
        \let<p:int>{\at<*pos_addr>[${c}]}
        \let<t:int>{\at<*trail_addr>[${c}]}
        \let<base:int>{\at<*chr_addr>[${c}]}
        \let<th1:int>{\max{\div{${t}, 4}, 2}}
        \let<th2:int>{\div{\mul{${t}, 3}, 4}}
        %% Write visible trail cells into buffer
        \let<i:int>{0}
        \begin{loop}
            \exit_when{\ge{${i}, ${t}}}
            \let<r:int>{\sub{${p}, ${i}}}
            \if{\and{\ge{${r}, 1}, \le{${r}, ${rows}}}}{
                %% Determine color tier
                \let<color:int>{3}
                \if{\eq{${i}, 0}}{\let<color>{0}}
                \else{\if{\lt{${i}, ${th1}}}{\let<color>{1}}
                \else{\if{\lt{${i}, ${th2}}}{\let<color>{2}}}}
                %% Determine character index
                \let<ch:int>{\mod{\add{${base}, \add{\mul{${i}, 13},
                    \div{${frm}, \add{3, \mod{${i}, 5}}}}}, 48}}
                \if{\eq{${i}, 0}}{\let<ch>{\rand{48}}}
                %% Pack and store: value = ch * 4 + color
                \mem_store{${buf_addr},
                    \add{\mul{\sub{${r}, 1}, ${cols}}, ${c}},
                    \add{\mul{${ch}, 4}, ${color}}}
            }
            \inc<i>
        \end{loop}
        %% Update position
        \let<s:int>{\at<*spd_addr>[${c}]}
        \let<np:int>{\add{${p}, ${s}}}
        \if{\gt{\sub{${np}, ${t}}, ${rows}}}{
            \set<*pos_addr>[${c}]{\sub{0, \rand{${rows}}}}
            \set<*spd_addr>[${c}]{\add{1, \rand{3}}}
            \set<*trail_addr>[${c}]{\add{8, \rand{100}}}
            \set<*chr_addr>[${c}]{\rand{48}}
        }\else{
            \set<*pos_addr>[${c}]{${np}}
        }
        \inc<c>
    \end{loop}
}

%%% RENDER lambda - compiled to bytecode, reads from buffer
%%% Only writes non-zero cells with per-cell cursor positioning
%%% (empty cells left to screen clear via \setclearbg{40})
\lambda<RENDER>[cols:int, rows:int, buf_addr:int, k_addr:int]#{
    \let<r:int>{1}
    \begin{loop}
        \exit_when{\gt{${r}, ${rows}}}
        \let<c:int>{0}
        \begin{loop}
            \exit_when{\ge{${c}, ${cols}}}
            \let<packed:int>{\mem_load{${buf_addr},
                \add{\mul{\sub{${r}, 1}, ${cols}}, ${c}}}}
            \if{\gt{${packed}, 0}}{
                \cursor{${r}, \add{\mul{${c}, 2}, 1}}
                \let<ch:int>{\div{${packed}, 4}}
                \let<clr:int>{\mod{${packed}, 4}}
                \if{\eq{${clr}, 0}}{\ansi{97;40}}
                \else{\if{\eq{${clr}, 1}}{\ansi{92;40}}
                \else{\if{\eq{${clr}, 2}}{\ansi{32;40}}
                \else{\ansi{90;40}}}}
                \emit{\at<*k_addr>[${ch}]}
                \ansi{0}
            }
            \inc<c>
        \end{loop}
        \inc<r>
    \end{loop}
}

%%% Set black background for screen clears
\setclearbg{40}

\main{%
%%% Input
\if{\haskey}{\let<k>{\getkey}\if{\streq{${k},q}}{\exit}\if{\streq{${k},Q}}{\exit}}%
%%% Compute
\invoke<COMPUTE>[${COLS},${ROWS},${frame},${buf},${pos_map},${spd_map},${trail_map},${chr_map}]%
%%% Render
\invoke<RENDER>[${COLS},${ROWS},${buf},${K}]%
\inc<frame>%
\wait{60}%
}

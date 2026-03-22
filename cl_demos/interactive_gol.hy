\setunicode{true}
\setwidth{80}

%%% Interactive Game of Life (Sparse Version)
%%%
%%% Run with "hyades -c interactive_gol.hy"
%%%
%%% - Space: Pause/Play
%%% - Mouse hover: Highlight cell
%%% - Left click: Make cell alive
%%% - Right click: Make cell dead
%%% - q: Quit

%%% Encode (row,col) into a single key for map storage
%%% Key = row * 10000 + col (supports grids up to 10000 wide)
\lambda<ENCODE_KEY>[row:int, col:int]#{
    \return{\add{\mul{${row}, 10000}, ${col}}}
}

%%% Decode key back to row
\lambda<DECODE_ROW>[key:int]#{
    \return{\div{${key}, 10000}}
}

%%% Decode key back to col
\lambda<DECODE_COL>[key:int]#{
    \return{\mod{${key}, 10000}}
}

%%% Check if cell is alive in sparse map
\lambda<IS_ALIVE>[grid_map:int, row:int, col:int, width:int, height:int]#{
    %%% Apply toroidal wrapping
    \let<wr:int>{\mod{\add{${row}, ${height}}, ${height}}}
    \let<wc:int>{\mod{\add{${col}, ${width}}, ${width}}}
    \let<key:int>{\invoke<ENCODE_KEY>[${wr}, ${wc}]}
    \return{\map_has<grid_map>[${key}]}
}

%%% Count neighbors of a cell (sparse version)
\lambda<COUNT_NEIGHBORS_SPARSE>[grid_map:int, width:int, height:int, row:int, col:int]#{
    \let<count:int>{0}
    \let<dr:int>{-1}
    \begin{loop}
        \exit_when{\gt{${dr}, 1}}
        \let<dc:int>{-1}
        \begin{loop}
            \exit_when{\gt{${dc}, 1}}
            \if{\and{\eq{${dr}, 0}, \eq{${dc}, 0}}}{
                %%% Skip self
            }\else{
                \let<nr:int>{\mod{\add{\add{${row}, ${dr}}, ${height}}, ${height}}}
                \let<nc:int>{\mod{\add{\add{${col}, ${dc}}, ${width}}, ${width}}}
                \let<key:int>{\invoke<ENCODE_KEY>[${nr}, ${nc}]}
                \if{\map_has<grid_map>[${key}]}{
                    \inc<count>
                }
            }
            \inc<dc>
        \end{loop}
        \inc<dr>
    \end{loop}
    \return{${count}}
}

%%% Determine next state based on current state and neighbor count
\lambda<NEXT_STATE>[current:int, neighbors:int]#{
    \if{${current}}{
        \if{\or{\eq{${neighbors}, 2}, \eq{${neighbors}, 3}}}{\return{1}}\else{\return{0}}
    }\else{
        \if{\eq{${neighbors}, 3}}{\return{1}}\else{\return{0}}
    }
}

%%% Step simulation (sparse version) - inlined for performance
%%% Modifies grid_map in place (clears and rebuilds)
\lambda<STEP_SPARSE>[grid_map:int, temp_map:int, width:int, height:int]#{
    %%% Build candidate set: all live cells and their neighbors
    \let<candidates:int>{\map_new{}}
    \let<keys:int>{\map_keys<grid_map>}
    \let<num_live:int>{\mem_len{${keys}}}

    \let<i:int>{0}
    \begin{loop}
        \exit_when{\ge{${i}, ${num_live}}}
        \let<key:int>{\mem_load{${keys}, ${i}}}
        \let<row:int>{\div{${key}, 10000}}
        \let<col:int>{\mod{${key}, 10000}}
        %%% Add 3x3 neighborhood to candidates (inlined)
        \let<dr:int>{-1}
        \begin{loop}
            \exit_when{\gt{${dr}, 1}}
            \let<dc:int>{-1}
            \begin{loop}
                \exit_when{\gt{${dc}, 1}}
                \let<nr:int>{\mod{\add{\add{${row}, ${dr}}, ${height}}, ${height}}}
                \let<nc:int>{\mod{\add{\add{${col}, ${dc}}, ${width}}, ${width}}}
                \let<ckey:int>{\add{\mul{${nr}, 10000}, ${nc}}}
                \set<*candidates>[${ckey}]{1}
                \inc<dc>
            \end{loop}
            \inc<dr>
        \end{loop}
        \inc<i>
    \end{loop}

    %%% Clear temp_map
    \let<temp_keys:int>{\map_keys<temp_map>}
    \let<num_temp:int>{\mem_len{${temp_keys}}}
    \let<t:int>{0}
    \begin{loop}
        \exit_when{\ge{${t}, ${num_temp}}}
        \map_del<temp_map>[\mem_load{${temp_keys}, ${t}}]
        \inc<t>
    \end{loop}

    %%% Compute next generation into temp_map
    \let<cand_keys:int>{\map_keys<candidates>}
    \let<num_cands:int>{\mem_len{${cand_keys}}}

    \let<j:int>{0}
    \begin{loop}
        \exit_when{\ge{${j}, ${num_cands}}}
        \let<key:int>{\mem_load{${cand_keys}, ${j}}}
        \let<row:int>{\div{${key}, 10000}}
        \let<col:int>{\mod{${key}, 10000}}
        \let<current:int>{\map_has<grid_map>[${key}]}
        %%% Count neighbors (inlined)
        \let<count:int>{0}
        \let<dr:int>{-1}
        \begin{loop}
            \exit_when{\gt{${dr}, 1}}
            \let<dc:int>{-1}
            \begin{loop}
                \exit_when{\gt{${dc}, 1}}
                \if{\and{\eq{${dr}, 0}, \eq{${dc}, 0}}}{}\else{
                    \let<nr:int>{\mod{\add{\add{${row}, ${dr}}, ${height}}, ${height}}}
                    \let<nc:int>{\mod{\add{\add{${col}, ${dc}}, ${width}}, ${width}}}
                    \let<nkey:int>{\add{\mul{${nr}, 10000}, ${nc}}}
                    \if{\map_has<grid_map>[${nkey}]}{\inc<count>}
                }
                \inc<dc>
            \end{loop}
            \inc<dr>
        \end{loop}
        %%% Next state (inlined)
        \let<next:int>{\if{${current}}{\if{\or{\eq{${count}, 2}, \eq{${count}, 3}}}{1}\else{0}}\else{\if{\eq{${count}, 3}}{1}\else{0}}}
        \if{${next}}{
            \set<*temp_map>[${key}]{1}
        }
        \inc<j>
    \end{loop}

    %%% Clear grid_map and copy from temp_map
    \let<old_keys:int>{\map_keys<grid_map>}
    \let<num_old:int>{\mem_len{${old_keys}}}
    \let<o:int>{0}
    \begin{loop}
        \exit_when{\ge{${o}, ${num_old}}}
        \map_del<grid_map>[\mem_load{${old_keys}, ${o}}]
        \inc<o>
    \end{loop}

    \let<new_keys:int>{\map_keys<temp_map>}
    \let<num_new:int>{\mem_len{${new_keys}}}
    \let<n:int>{0}
    \begin{loop}
        \exit_when{\ge{${n}, ${num_new}}}
        \let<k:int>{\mem_load{${new_keys}, ${n}}}
        \set<*grid_map>[${k}]{1}
        \inc<n>
    \end{loop}

    %%% Clear temp for next iteration
    \let<tt:int>{0}
    \begin{loop}
        \exit_when{\ge{${tt}, ${num_new}}}
        \map_del<temp_map>[\mem_load{${new_keys}, ${tt}}]
        \inc<tt>
    \end{loop}
}

%%% Display grid with hover highlight (compiled to bytecode)
%%% Checks sparse map for each visible cell
%%% Uses ANSI colors: green for alive, dim for dead, yellow for hover
\lambda<DISPLAY_GRID_SPARSE>[grid_map:int, width:int, height:int, hover_row:int, hover_col:int, row_offset:int]#{
    \let<r:int>{0}
    \begin{loop}
        \exit_when{\ge{${r}, ${height}}}
        \cursor{\add{${r}, \add{${row_offset}, 1}}, 1}
        \let<c:int>{0}
        \begin{loop}
            \exit_when{\ge{${c}, ${width}}}
            \let<key:int>{\add{\mul{${r}, 10000}, ${c}}}
            \let<alive:int>{\map_has<grid_map>[${key}]}
            \let<is_hover:int>{\and{\eq{${r}, ${hover_row}}, \eq{${c}, ${hover_col}}}}
            \if{${is_hover}}{
                \if{${alive}}{\ansi{93}\emit{██}\ansi{0}}
                \else{\ansi{43;30}\emit{▒▒}\ansi{0}}
            }\else{
                \if{${alive}}{\ansi{92}\emit{▓▓}\ansi{0}}
                \else{\ansi{90}\emit{░░}\ansi{0}}
            }
            \inc<c>
        \end{loop}
        \inc<r>
    \end{loop}
}

\let<W>{40}
\let<H>{20}

%%% Initialize sparse grid with a glider
%%% Only store live cells in the map
%%% Glider at top-left:
%%%   .X.   (row 1, col 2)
%%%   ..X   (row 2, col 3)
%%%   XXX   (row 3, col 1,2,3)
%%% Key = row * 10000 + col
\let<grid>#{|10002:1, 20003:1, 30001:1, 30002:1, 30003:1|}
\let<temp>#{||}

\let<gen>{0}
\let<paused>{0}
\let<hover_row>{-1}
\let<hover_col>{-1}

%%% Header row offset (title + instructions + blank line)
\let<HEADER_ROWS>{4}

\let<last_time>{\gettime}

\main{
\let<now>{\gettime}
\let<frame_ms>{\sub{\valueof<now>,\valueof<last_time>}}
\let<last_time>{\valueof<now>}
Frame time: \valueof<frame_ms>ms | Live cells: \map_len<grid>
%%% Process keyboard input
\if{\haskey}{
    \assign<k>{\getkey}
    \if{\streq{\recall<k>,SPACE}}{\if{\valueof<paused>}{\let<paused>{0}}\else{\let<paused>{1}}}
    \if{\streq{\recall<k>,q}}{\exit}
    \if{\streq{\recall<k>,Q}}{\exit}
}

%%% Process mouse input
\let<mx>{\getmouseX}
\let<my>{\getmouseY}
\let<mb>{\getmousebutton}

%%% Calculate grid position from mouse (cells are 2 chars wide)
\let<gcol>{\div{\valueof<mx>, 2}}
\let<grow>{\sub{\valueof<my>,\valueof<HEADER_ROWS>}}

%%% Check if mouse is within grid bounds
\if{\and{\ge{\valueof<grow>,0},\and{\lt{\valueof<grow>,\valueof<H>},\and{\ge{\valueof<gcol>,0},\lt{\valueof<gcol>,\valueof<W>}}}}}{
    \let<hover_row>{\valueof<grow>}
    \let<hover_col>{\valueof<gcol>}

    %%% Handle clicks - modify sparse map
    \let<cell_key>{\add{\mul{\valueof<grow>,10000},\valueof<gcol>}}
    \if{\eq{\valueof<mb>,1}}{\map_set<grid>{\valueof<cell_key>,1}}
    \if{\eq{\valueof<mb>, 3}}{\map_del<grid>{\valueof<cell_key>}}
}\else{
    \let<hover_row>{-1}
    \let<hover_col>{-1}
}

%%% Display
Interactive Game of Life (SPARSE) - Gen \valueof<gen> \if{\valueof<paused>}{[PAUSED]}\else{[RUNNING]}\\
Space: pause/play | Left click: birth | Right click: death | q: quit\\
Mouse: (\valueof<mx>,\valueof<my>) -> Grid: (\valueof<hover_col>,\valueof<hover_row>)\\
\recall<DISPLAY_GRID_SPARSE>[\valueof<grid>,\valueof<W>,\valueof<H>,\valueof<hover_row>,\valueof<hover_col>,\valueof<HEADER_ROWS>]
%%% Step simulation if not paused
\if{\valueof<paused>}{}
\else{
    \recall<STEP_SPARSE>[\valueof<grid>,\valueof<temp>,\valueof<W>,\valueof<H>]
    \inc<gen>
}

\wait{30}
}

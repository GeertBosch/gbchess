#!/bin/bash
SVG_CHESS_DEFS="${SVG_CHESS_DEFS:-${BASH_SOURCE%/*}/img/chessboard.svg}" # piece & board definitions
SVG_CHESS_GRID=45 # Size of each square on the chess board. Depends on the SVG definitions.
SVG_CHESS_SIZE=180 # Size of the output SVG chessboard. Change freely.
SVG_CHESS_STYLE=brown # One of brown, green, paper. Style of the squares on the chess board.
SVG_CHESS_OUT=
SVG_CHESS_TMP="${TMPDIR:-/tmp}/fen2svg-$$.svg"

usage() {
    echo "Usage: $0 [-d defs] [-o outfile] [-s style] [fen]"
    echo "  -d defs     SVG file with the chess piece and board definitions"
    echo "  -o outfile  Output file (default: standard output)"
    echo "  -s style    Style of the squares on the chess board (brown, green, paper)"
    echo "  fen         FEN piece placement (default: standard starting position)"
    exit 1
}

# The chkfen function checks the FEN piece placement string for validity, including each rank having
# 8 squares, there being 8 ranks, and the pieces being valid. It does not check the number of pieces
# or the presence of pawns in rank 1 and 8. The function an exit status of 0 if valid, or an exit
# status of 1 - 8 indicating the invalid rank or 9 if the FEN doesn't pass basic validity.
chkfen() {
    local rank=0 file=0 fen="$1/" # A trailing slash makes the FEN placement easier to parse
    [[ "$fen" =~ ^([pnbrqkPNBRQK1-8]{1,8}/){8}$ ]] || return 9 # basic placement validity
    for ((i = 0; i < ${#fen}; ++i)); do
        local n=${fen:i:1}
        case $n in
            [1-8]) ((rank += n)) ;;
            /) ((file++)) ; ((rank == 8)) || return $file ; rank=0 ;;
            *) ((rank++)) ;;
        esac
    done
}

# Process the command line options
while getopts "d:o:s:" opt ; do
    case $opt in
        d) SVG_CHESS_DEFS="$OPTARG" ;;
        h) usage ;;
        o) SVG_CHESS_OUT=$OPTARG ;;
        s) if [[ ! $OPTARG =~ brown|green|paper ]] ; then
                echo "Invalid style: \"$OPTARG\"" >&2
                usage >&2
           fi
           SVG_CHESS_STYLE=$OPTARG ;;
        *) usage ;;
    esac
done
shift $((OPTIND - 1))

# This script takes a chess FEN piece placement and outputs an SVG file that shows
# the chessboard with that setup.
if [ $# -lt 1 -o "$1" = "startpos" ] ; then
    fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"
else
    fen=$1
fi
fen="${fen//_/1}" # allow _ to stand for no piece
if ! chkfen "$fen" ; then
    echo "Invalid FEN piece placement: \"$fen\"" >&2
    exit 2
fi

if [ ! -f "$SVG_CHESS_DEFS" ] ; then
    echo "Missing input SVG chessboard definition file: $SVG_CHESS_DEFS" >&2
    exit 2
fi

# Copy styles and definitions from the SVG definitions file
copy_styles_and_defs() {
    awk '/(desc|style|defs)[>]/ {
        show = !show
        print
        next
    }
    show {
        print
    }
    ' "$SVG_CHESS_DEFS"
}

# Create the board including labels
create_board() {
    echo "  <use id="\"board\"" href=\"#chessboard\" class=\"${SVG_CHESS_STYLE} board\"/>"
    echo "  <use id="\"darklabel\"" href=\"#darklabel\" class=\"dark ${SVG_CHESS_STYLE}\"/>"
    echo "  <use id="\"lightlabel\"" href=\"#lightlabel\" class=\"light ${SVG_CHESS_STYLE}\"/>"
}

# Place the pieces
place_pieces() {
    xpos=0
    ypos=0
    files="abcdefgh"
    for ((i = 0; i < ${#fen}; ++i)) ; do
        char=${fen:i:1}
        case $char in
            1 | 2 | 3 | 4 | 5 | 6 | 7 | 8) xpos=$((xpos + char)) ;;
            /) xpos=0; ypos=$((ypos + 1)) ;;
            k | K | q | Q | r | R | b | B | n | N | p | P)
                square=${files:xpos:1}$((ypos + 1))
                echo -n "  <use id=\"$square\" href=\"#$char\""
                echo " x=\"$((xpos * SVG_CHESS_GRID))\" y=\"$((ypos * SVG_CHESS_GRID))\" />"
                xpos=$((xpos + 1)) ;;
        esac
    done
}

output_svg() {
    local native=$((8 * $SVG_CHESS_GRID))

    echo -n "<svg xmlns=\"http://www.w3.org/2000/svg\""
    echo " width=\"$SVG_CHESS_SIZE\" height=\"$SVG_CHESS_SIZE\" viewBox=\"0 0 $native $native\">"

    copy_styles_and_defs
    create_board
    place_pieces
    echo "</svg>"
}

# Output the SVG to the standard output or to a file
if [ -n "$SVG_CHESS_OUT" ] ; then
    output_svg > "$SVG_CHESS_OUT"
else
    output_svg
fi

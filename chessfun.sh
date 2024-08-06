# chess utility bash shell functions for testing and debugging

# applies some move to a position in FEN format and shows the result using stockfish
echo "apply <fen> <move> ..."
apply() {
	if (($# < 2)) ; then
		echo "$0 <fen> <move> ..."
		return 1
	fi
	fen=$1
	shift
	if [ "$fen" == "startpos" ] ; then
		fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
	fi
	if [ "$1" == "moves" ] ; then
		shift
	fi
	moves="moves"
	while (($#)) ; do
		moves="$moves $1"
		shift
	done
	echo "\
position fen \"$fen\" $moves
d" | stockfish
}

export esc="\\033["
echo "setfg [color256] - sets the terminal foreground color to the given index, or reset"
setfg() {
	if (($# < 1)) ; then
		printf "\\E[0m"
	else
		printf "\\E[38;5;$1m"
	fi
}

echo "setbg [color256] - sets the terminal background color to the given index, or reset"
setbg() {
	if (($# < 1)) ; then
		printf "\\E[49m"
	else
		printf "\\E[48;5;$1m"
	fi
}
export lg="\\E[48;5;187m" # light green background
export dg="\\E[48;5;64m" # dark green background
export lh="▌" # left half block
export rh="▐" # right half block

echo "expfen <fen> - expands a FEN placement so empty squares become dots, / replaced by newline"
expfen() {
	if (($# != 1)) ; then
		echo "$0 <fen>"
		return 1
	fi
	dots=""
	exp=$1
	for i in $(seq 8) ; do
		dots=".$dots"
		exp=${exp//$i/$dots}
	done
	echo "$exp" | tr '/' '\n'
}

echo "chessrow <bg1> <bg2> <fenrow> - prints a chess row with alternating background colors"
chessrow() {
	if (($# < 3)) ; then
		echo "$0 <bg1> <bg2> <fenrow>"
		return 1
	fi
	bg1=$1
	bg2=$2
	row=$(echo "$3" | tr "." " ")
	for (( i=0; i<${#row}; i++ )) ; do
		piece=${row:$i:1}
		case $piece in
		"p")
			piece="♟"
			;;
		"n")
			piece="♞"
			;;
		"b")
			piece="♝"
			;;
		"r")
			piece="♜"
			;;
		"q")
			piece="♛"
			;;
		"k")
			piece="♚"
			;;
		"P")
			piece="♙"
			;;
		"N")
			piece="♘"
			;;
		"B")
			piece="♗"
			;;
		"R")
			piece="♖"
			;;
		"Q")
			piece="♕"
			;;
		"K")
			piece="♔"
			;;
		".")
			piece=" "
			;;
		esac
		if [ $((i % 2)) == 0 ] ; then
			printf "\\E[38;5;${bg1}m$rh\\E[48;5;${bg1}m\\E[38;5;0m$piece"
		else
			printf "\\E[38;5;${bg1}m\\E[48;5;${bg2}m$lh\\E[38;5;0m$piece"
		fi
	done
	printf "\\E[49m\\E[38;5;${bg2}m$lh\\E[39m\\n"
}

echo "chessboard <fen> - prints a chess board for the given FEN piece placement"
chessboard() {
	if (($# < 1)) ; then
		echo "$0 <fen>"
		return 1
	fi
	bg1=187
	bg2=64
	set - $(expfen $*)
	while [ $# -gt 0 ] ; do
		chessrow $bg1 $bg2 "$1"
		shift
		t=$bg1
		bg1=$bg2
		bg2=$t
	done
}

setfg 187
for i in 0 1 2 3 ; do
	p=" "
	P=" "
	if [ $i == 0 ] ; then
		p="♟"
	fi
	if [ $i == 3 ] ; then
		P="♙"
	fi
	chessrow 187 64 "$P$P$P$P$P$P$P$P"
	chessrow 64 187 "$p$p$p$p$p$p$p$p"
done
setbg
setfg

echo "flip <fen>"
flip() {
    if (($# < 1)) ; then
        echo "$0 <fen>"
        return 1
    fi
    echo -n $(echo "$1" | tr '/' '\n' | tail -r | rev) | tr ' ' '/'
    shift
    if (($# >= 1)) ; then
        case $1 in
        b|B)
            side=w
            ;;
        w|W)
            side=b
            ;;
        *)
            side=w
            ;;
        esac
        echo -n " $side"
        shift
    fi
    while (($# != 0)) ; do
        echo -n " $1"
        shift
    done
    echo ""
}

echo "fish <fen>"
fish() {
	if (($# < 1)) ; then
		echo "$0 fish <fen>"
		return 1
	fi
	fen=$1
	shift
	echo "\
position fen \"$fen\"
go infinite
d" | stockfish
}

# Compare a perft run on a  base FEN position followed by some moves at the given depth
perftdiff() {
	if [ $# == 0 ] ; then
		echo "$0 fen depth"
		return 1
	elif [ $# == 1 ] ; then
		echo "set fen to startpos"
		fen="startpos"
	else
		echo "set fen to $1"
		fen="$1"
		shift
	fi
	if [ "$fen" == "startpos" ] ; then
		fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
	fi
	moves=""
	if [ "$1" == "moves" ] ; then
		echo "skipping $1."
		shift
	fi
	if (($# > 1)) ; then
		moves="moves"
		while (($# > 1)) ; do
			moves="$moves $1"
			shift
		done
	fi
	depth=$1
	echo "perft \"$fen\" $moves $depth"

	diff -u <(
echo "\
position fen \"$fen\" $moves
d
perft $depth" | stockfish 2>&1 | egrep "^....(.)?:|Fen" | sort) <(
./perft "$fen" $moves $depth | egrep "^....(.)?:|Fen" | sort)
}

perftnext() {
	fen="$1"
	depth="$2"
	next=$(perftdiff "$fen" $2 | egrep "^[-+][a-h][1-8][a-h][1-8]([qrbn])?:|^[+-]Fen" | head -1 | cut -c2-)
	echo "next: $next, result $?"
	if [ $? != 0 ] ; then
		echo "error $?, result $next"
		return 1
	fi
	move=$(echo "$next" | cut -d: -f1)
	if [ "$move" == "Fen" ] ; then
		echo "Incorrect Fen: $next"
		return 2
	fi
	echo "Move: $move"
	nextfen=$(apply "$fen" "$move"| egrep "Fen:" | cut -d" " -f2-)
	echo "perftnext \"$nextfen\"" $((depth - 1))

}

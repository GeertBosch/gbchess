# applies some move to a position in FEN format and shows the result using stockfish
apply() {
	if (($# < 2)) ; then
		echo "$0 <fen> <move> ..."
		exit 1
	fi
	fen=$1
	shift
	if [ "$fen" == "startpos" ] ; then
		fen="rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"
	fi
	if [ "$1" == "moves" ] ; then
		shift;
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

# Compare a perft run on a  base FEN position followed by some moves at the given depth
perftdiff() {
	if (($# == 0)) ; then
		echo "$0 fen depth"
		exit 1
	elif (($# == 1)) ; then
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
perft $depth" | stockfish 2>&1 | egrep "^....:|Fen" | sort) <(
./perft "$fen" $moves $depth | egrep "^....:|Fen" | sort)
}

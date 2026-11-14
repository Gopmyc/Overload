#!/bin/bash
# Runs the in-game checks of Tests/GameChecks with a built OvGame. Each scene of the project is a check, run through the
# game's command line: it passes when the game ends with exit code 0 and logs "CHECK PASSED <name>".
# Usage: Tests/run_game_checks.sh <OvGame binary> [check name...]
# The game needs a display: a desktop session, or xvfb-run on a headless Linux machine. The saves of the checks go to a
# temporary folder, never to the user's own, and the system's language is set aside.
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
GAME="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
shift

if [ $# -gt 0 ]; then
	CHECKS=("$@")
else
	CHECKS=()
	for SCENE in "$ROOT/Tests/GameChecks/Assets/Scenes/"*.ovscene; do
		CHECKS+=("$(basename "$SCENE" .ovscene)")
	done
fi

DATA="$(mktemp -d)"
trap 'rm -rf "$DATA"' EXIT
export XDG_DATA_HOME="$DATA"

# The checks start in the project's default language, whatever the machine's
export LANG=C.UTF-8
unset LC_ALL LC_MESSAGES

FAILED=0
for NAME in "${CHECKS[@]}"; do
	LOG="$DATA/$NAME.log"
	# A check ends the game itself: the frame cap only stops one that never does. The physics steps in real time,
	# so a check that waits for it needs as many frames as the game draws in that time
	timeout 300 "$GAME" --project "$ROOT/Tests/GameChecks" --engine "$ROOT/Resources/Engine" \
		--scene "Scenes/$NAME.ovscene" --frames 20000 --hidden-window --fail-on-error > "$LOG" 2>&1
	CODE=$?
	VERDICT="$(sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -o "CHECK PASSED $NAME .*" | head -1)"

	if [ $CODE -eq 0 ] && [ -n "$VERDICT" ]; then
		echo "PASS $NAME: ${VERDICT#CHECK PASSED $NAME }"
	else
		echo "FAIL $NAME: exit code $CODE${VERDICT:+, though it logged its pass}"
		sed 's/\x1b\[[0-9;]*m//g' "$LOG" | grep -E "\[ERROR\]|CHECK" | head -20 | sed 's/^/    /'
		FAILED=1
	fi
done

exit $FAILED

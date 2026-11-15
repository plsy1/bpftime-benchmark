#!/usr/bin/env bash
# C5 (step 0b): checks of the guarantee audit. No GPU. Usage:
#   timeout -k 5 600 ./run_all.sh            # about 10 s, no network
#   timeout -k 5 600 ./run_all.sh --refetch  # also re-downloads all URLs (about 25 s)
# Prints one PASS/FAIL line per step and a final verdict; exit 0 only if all pass.
set -u
cd "$(dirname "$0")"
PY="python3 -I"
fail=0
step() { if [ "$2" -eq 0 ]; then echo "PASS  $1"; else echo "FAIL  $1"; fail=1; fi; }

tmp=$(mktemp -d "$PWD/.tmp_run.XXXXXX")   # temporary files stay inside this directory
trap 'rm -rf "$tmp"' EXIT
rc=0
$PY tools/manifest.py verify docs > "$tmp/m1" 2>&1 || rc=1; tail -1 "$tmp/m1"
$PY tools/manifest.py verify docs/v13.2.2 > "$tmp/m2" 2>&1 || rc=1; tail -1 "$tmp/m2"
step "1 saved pages match MANIFEST.tsv (sha256), current and 13.2.2" $rc
rc=0
for d in docs docs/v13.2.2; do
	for f in "$d"/raw/*.html; do
		b=$(basename "$f" .html)
		$PY tools/html2text.py "$f" "$tmp/$b.regen.txt" || rc=1
		cmp -s "$tmp/$b.regen.txt" "$d/text/$b.txt" || { echo "  text differs: $d/$b"; rc=1; }
	done
done
step "2 text regenerates from raw unchanged (current and 13.2.2)" $rc

$PY tools/check_quotes.py README.md docs/text; step "3 every quote in README.md is verbatim" $?
# step 4: exactly 2 rejections, and exactly the expected ones (line 7 NOT FOUND, line 13 WRONG SECTION)
$PY tools/check_quotes.py tests/altered.md docs/text --expect-bad 2 > "$tmp/c4" 2>&1; rc=$?
cat "$tmp/c4"
[ "$(grep -c ' NOT FOUND: \| WRONG SECTION ' "$tmp/c4")" -eq 2 ] || rc=1
grep -q '^tests/altered.md:7 \[ptx-isa §10.4\] NOT FOUND: ' "$tmp/c4" || rc=1
grep -q '^tests/altered.md:13 \[ptx-isa §10.5\] WRONG SECTION ' "$tmp/c4" || rc=1
step "4 control: the altered quote and the wrong section are rejected (exactly those 2)" $rc

# the same quotes against the archived CUDA 13.2.2 pages (the board's toolkit; PTX ISA 9.2).
# Section numbers differ between versions, so only the text is checked. Expected: exactly 1
# known wording difference, "::cudaMalloc" (13.5) against "cudaMalloc" (13.2.2) in
# cuThreadExchangeStreamCaptureMode. Pages without a 13.2.2 version (the v12.9 guide, the C++
# draft) are taken from docs/text.
mkdir -p "$tmp/t1322"
cp docs/v13.2.2/text/*.txt "$tmp/t1322/"
cp docs/text/cpg-legacy-12.9.txt docs/text/cxx-draft-*.txt "$tmp/t1322/"
$PY tools/check_quotes.py README.md "$tmp/t1322" --no-sections --expect-bad 1 > "$tmp/c5" 2>&1; rc=$?
cat "$tmp/c5"
[ "$(grep -c ' NOT FOUND: \| unknown document ' "$tmp/c5")" -eq 1 ] || rc=1
grep -q '\[drv-stream §6.37 cuThreadExchangeStreamCaptureMode\] NOT FOUND: During stream capture (see cuStreamBeginCapture), some actions, such as a call to ::cudaMalloc, may be unsafe.$' "$tmp/c5" || rc=1
step "5 quotes hold in the CUDA 13.2.2 pages (exactly the 1 known wording difference)" $rc

if [ "${1:-}" = "--refetch" ]; then
	mkdir -p "$tmp/new"
	changed=0; failed=0
	while IFS=$'\t' read -r name url; do
		if timeout -k 2 30 curl -sS -L -o "$tmp/new/$name.html" "$url"; then
			a=$(sha256sum < "$tmp/new/$name.html" | cut -d' ' -f1)
			b=$(sha256sum < "docs/raw/$name.html" | cut -d' ' -f1)
			if [ "$a" != "$b" ]; then
				$PY tools/html2text.py "$tmp/new/$name.html" "$tmp/new/$name.txt"
				echo "  changed since 2026-11-15: $name"; changed=$((changed + 1))
			fi
		else
			echo "  fetch failed: $name"; failed=$((failed + 1))
		fi
	done < docs/SOURCES.tsv
	echo "  refetch: $changed changed, $failed failed (saved copies untouched)"
	if [ "$changed" -gt 0 ]; then
		# recheck the quotes against the new text of the changed pages (others: saved text)
		cp -n docs/text/*.txt "$tmp/new/" 2>/dev/null
		$PY tools/check_quotes.py README.md "$tmp/new"; step "6 quotes still verbatim in the re-fetched pages" $?
	else
		step "6 re-fetched pages unchanged" "$failed"
	fi
fi

if [ $fail -eq 0 ]; then echo "C5 RESULT: PASS"; else echo "C5 RESULT: FAIL"; fi
exit $fail

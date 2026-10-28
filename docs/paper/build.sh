#!/usr/bin/env bash
# Builds docs/paper/main.pdf (pdflatex + bibtex). Run from anywhere.
set -e
cd "$(dirname "$0")"
OUT=../../build/paper
mkdir -p "$OUT"
pdflatex -interaction=nonstopmode -halt-on-error -output-directory="$OUT" main.tex >/dev/null
cp references.bib "$OUT"/
(cd "$OUT" && bibtex main >/dev/null)
pdflatex -interaction=nonstopmode -halt-on-error -output-directory="$OUT" main.tex >/dev/null
pdflatex -interaction=nonstopmode -halt-on-error -output-directory="$OUT" main.tex >/dev/null
cp "$OUT/main.pdf" main.pdf
echo "built docs/paper/main.pdf"

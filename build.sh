#!/usr/bin/env sh
set -eu
project=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
configuration="${CONFIGURATION:-Release}"
cmake -S "$project" -B "$project/build-$configuration" -DCMAKE_BUILD_TYPE="$configuration" -DJULRETSU_BUILD_BENCHMARKS=ON
cmake --build "$project/build-$configuration" --config "$configuration" --parallel 2
ctest --test-dir "$project/build-$configuration" -C "$configuration" --output-on-failure
"$project/build-$configuration/julretsu_headless"
"$project/build-$configuration/julretsu_benchmarks"

#!/usr/bin/env bash

if [ ! -f "$1" ]; then
    echo "Must supply a sanitizer log file to parse as command line arg"
    exit 1
fi

logfile="$1"

#
# Check for AddressSanitizer errors
#
asan_errors=$(grep -c "ERROR: AddressSanitizer:" "$logfile" 2>/dev/null) || asan_errors=0

#
# Check for LeakSanitizer errors
#
lsan_errors=$(grep -c "ERROR: LeakSanitizer:" "$logfile" 2>/dev/null) || lsan_errors=0

#
# Check for UndefinedBehaviorSanitizer errors (runtime error lines)
#
ubsan_errors=$(grep -c "runtime error:" "$logfile" 2>/dev/null) || ubsan_errors=0

total_errors=$((asan_errors + lsan_errors + ubsan_errors))

if ((total_errors == 0)); then
    echo ":== Congratulations: no errors found by sanitizers"
    exit 0
fi

echo ":== Error: Sanitizers found errors..."
echo

if ((asan_errors > 0)); then
    echo ":== AddressSanitizer errors ($asan_errors):"
    grep -A 20 "ERROR: AddressSanitizer:" "$logfile"
    echo
fi

if ((lsan_errors > 0)); then
    echo ":== LeakSanitizer errors ($lsan_errors):"
    grep -A 20 "ERROR: LeakSanitizer:" "$logfile"
    echo
fi

if ((ubsan_errors > 0)); then
    echo ":== UndefinedBehaviorSanitizer errors ($ubsan_errors):"
    grep "runtime error:" "$logfile"
    echo
fi

exit $total_errors

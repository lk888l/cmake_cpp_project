#!/bin/sh
set -eu

DURATION=${1:-28800}
CTL=${CAMERA_DISPLAY_CTL:-/userdata/camera-display/camera-displayctl}
PID_FILE=/var/run/camera-display.pid
REPORT=${SOAK_REPORT:-/userdata/camera-display/soak.csv}

"$CTL" start
START=$(date +%s)
echo "elapsed_s,rss_kib,vm_kib,threads" > "$REPORT"
trap '"$CTL" stop' EXIT INT TERM

while :; do
    NOW=$(date +%s)
    ELAPSED=$((NOW - START))
    [ "$ELAPSED" -lt "$DURATION" ] || break
    PID=$(cat "$PID_FILE")
    STATUS=/proc/$PID/status
    [ -r "$STATUS" ] || {
        echo "application exited during soak" >&2
        exit 1
    }
    RSS=$(awk '/^VmRSS:/ {print $2}' "$STATUS")
    VM=$(awk '/^VmSize:/ {print $2}' "$STATUS")
    THREADS=$(awk '/^Threads:/ {print $2}' "$STATUS")
    echo "$ELAPSED,$RSS,$VM,$THREADS" >> "$REPORT"
    if [ "${RSS:-0}" -gt 10240 ]; then
        echo "RSS limit exceeded: $RSS KiB" >&2
        exit 1
    fi
    sleep 10
done

echo "soak completed: $REPORT"

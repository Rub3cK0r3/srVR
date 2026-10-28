#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
SERVER_BIN="$ROOT_DIR/srVR"

if ! command -v curl >/dev/null 2>&1; then
	echo "test_server.sh requires curl" >&2
	exit 1
fi
if ! command -v python3 >/dev/null 2>&1; then
	echo "test_server.sh requires python3 to select an ephemeral port" >&2
	exit 1
fi

make -B -C "$ROOT_DIR" debug

TEST_DIR=$(mktemp -d "${TMPDIR:-/tmp}/srvr-smoke.XXXXXX")
SERVER_PID=
cleanup() {
	if [[ -n "$SERVER_PID" ]] && kill -0 "$SERVER_PID" 2>/dev/null; then
		kill -INT "$SERVER_PID" 2>/dev/null || true
		wait "$SERVER_PID" 2>/dev/null || true
	fi
	rm -rf -- "$TEST_DIR"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

mkdir -p "$TEST_DIR/config" "$TEST_DIR/www"
cp "$ROOT_DIR/www/index.html" "$TEST_DIR/www/index.html"

SERVER_PORT=${TEST_PORT:-$(python3 -c 'import socket; s = socket.socket(); s.bind(("127.0.0.1", 0)); print(s.getsockname()[1]); s.close()')}
if [[ ! "$SERVER_PORT" =~ ^[0-9]+$ ]] ||
	 (( SERVER_PORT < 1 || SERVER_PORT > 65535 )); then
	echo "TEST_PORT must be between 1 and 65535" >&2
	exit 1
fi

cat > "$TEST_DIR/config/server.conf" <<EOF
port=$SERVER_PORT
root=./www
use_epoll=1
EOF

(
	cd "$TEST_DIR"
	exec "$SERVER_BIN"
) >"$TEST_DIR/server-output.log" 2>&1 &
SERVER_PID=$!

BASE_URL="http://127.0.0.1:$SERVER_PORT"
BODY_FILE="$TEST_DIR/response-body"

request_status() {
	curl --silent --show-error --connect-timeout 2 --max-time 3 \
		--output "$BODY_FILE" --write-out '%{http_code}' "$@"
}

assert_status_and_body() {
	local name=$1 expected_status=$2 expected_body=$3 actual_status
	shift 3

	if ! actual_status=$(request_status "$@"); then
		echo "FAIL: $name (request failed)" >&2
		cat "$TEST_DIR/server-output.log" >&2
		exit 1
	fi
	if [[ "$actual_status" != "$expected_status" ]]; then
		echo "FAIL: $name (expected HTTP $expected_status, got $actual_status)" >&2
		cat "$BODY_FILE" >&2
		exit 1
	fi
	if [[ -n "$expected_body" ]] && ! grep -Fq -- "$expected_body" "$BODY_FILE"; then
		echo "FAIL: $name (response body did not contain '$expected_body')" >&2
		cat "$BODY_FILE" >&2
		exit 1
	fi
	printf 'PASS: %s\n' "$name"
}

ready=0
for _ in {1..50}; do
	if ! kill -0 "$SERVER_PID" 2>/dev/null; then
		echo "Server exited before becoming ready" >&2
		cat "$TEST_DIR/server-output.log" >&2
		exit 1
	fi
	if curl --silent --output /dev/null --connect-timeout 1 --max-time 1 \
			"$BASE_URL/index.html"; then
		ready=1
		break
	fi
	sleep 0.1
done

if (( ! ready )); then
	echo "Server did not become ready on port $SERVER_PORT" >&2
	cat "$TEST_DIR/server-output.log" >&2
	exit 1
fi

assert_status_and_body "GET serves index" 200 "It works." \
	"$BASE_URL/index.html"
assert_status_and_body "HEAD returns success" 200 "" \
	--head "$BASE_URL/index.html"
assert_status_and_body "missing file returns 404" 404 "404 Not Found" \
	"$BASE_URL/missing.html"
assert_status_and_body "directory traversal is rejected" 403 "403 Forbidden" \
	--path-as-is "$BASE_URL/../LICENSE"
assert_status_and_body "POST returns acknowledgement" 200 "POST received." \
	--request POST --data "test data" "$BASE_URL/"
assert_status_and_body "unsupported method returns 405" 405 "405 Method Not Allowed" \
	--request DELETE "$BASE_URL/"

echo "All server smoke tests passed."

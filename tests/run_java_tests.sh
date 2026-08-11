#!/bin/bash

set -euo pipefail

this_dir=$(cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(cd -- "$this_dir/.." && pwd)
database_root="$this_dir/java"
server_host=${DATAMAN_TEST_HOST:-localhost}
build_dir=$(mktemp -d /tmp/dataman-java-tests.XXXXXX)

cleanup()
{
	rm -rf -- "$build_dir"
}
trap cleanup EXIT

"$this_dir/setup_test.sh" java

javac -Xlint:all -d "$build_dir" \
	"$repo_root"/clientlib/java/*.java \
	"$this_dir"/java/*.java

java -cp "$build_dir" BuildOneRecordIndex "$database_root" "$server_host"
java -cp "$build_dir" OneRecordIntegrationTest "$database_root" "$server_host"

echo "java integration tests: PASS"

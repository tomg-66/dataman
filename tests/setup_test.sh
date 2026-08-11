#!/bin/bash
test_type=""

usage () {
	echo "$0: usage: $0: [php|java]"
	exit 255
}

if [ $# -ne 1 ] ; then
	usage
fi

case $1 in
	'php')
		test_type="php"
		;;
	'java')
		test_type="java"
		;;
	*)
		usage
		;;
esac

this_dir=$(cd -- "$(dirname -- "$0")" && pwd)
root=$this_dir/$test_type

fixtures_dir=$this_dir/fixtures
if [ ! -e "$fixtures_dir/one_rec.i" ] ; then
	echo no starting initialization file
	exit 255
fi

if  [ ! -e "$fixtures_dir/one_rec.t" ]  ; then
	echo no template file
	exit 255
fi

mkdir -p "$root/files" "$root/index" "$root/blobs"
rm -f -- "$root/index/one_rec_idx"

exe=$(command -v mkdf)
if [ ! -x "$exe" ] ; then
	echo dataman not in your PATH
	exit 255
fi

ROOT="$root" "$exe" one_rec "$fixtures_dir/one_rec.i" < "$fixtures_dir/one_rec.t"

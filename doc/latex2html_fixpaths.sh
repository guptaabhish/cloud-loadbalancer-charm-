#!/bin/sh
#
# Shell script to fix Latex2HTML's broken absolute paths
# in image files, by replacing them with relative paths.
#
#  Orion Sky Lawlor, olawlor@acm.org, 2003/12/10

die() {
	echo $@
	exit 1
}

[ -x manual ] || die "Must run program with a manual/ directory"
[ -x fig ] || die "Must run program with a fig/ directory"

cp -r fig manual/

for f in `echo manual/*.html`
do
	echo "Converting $f"
	sed -e 's!'`pwd`'/!!g' $f > tmp || die "error running sed on $f"
	mv $f $f.bak || die "error backing up $f"
	mv tmp $f || die "error replacing $f"
done

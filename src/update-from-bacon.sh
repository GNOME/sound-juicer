#!/bin/sh

function die() {
  echo $*
  exit 1
}

if test -z "$BACONDIR"; then
   echo "Must set BACONDIR"
   exit 1
fi

if test -z "$BACONFILES"; then
   echo "Must set BACONFILES"
   exit 1
fi

for FILE in $BACONFILES; do
  if cmp -s $BACONDIR/$FILE $FILE; then
     echo "File $FILE is unchanged"
  else
     cp $BACONDIR/$FILE $FILE || die "Could not move $BACONDIR/$FILE to $FILE"
     echo "Updated $FILE"
  fi
done

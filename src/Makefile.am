BACONDIR=$(srcdir)/../../libbacon/src/
BACONFILES=cd-drive.c cd-drive.h bacon-cd-selection.c bacon-cd-selection.h

regenerate-built-sources:
	BACONFILES="$(BACONFILES)" BACONDIR="$(BACONDIR)" $(srcdir)/src/update-from-bacon.sh

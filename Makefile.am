AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4
SUBDIRS = src include

EXTRA_DIST = \
	README.md \
	git-version-gen

dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

indent:
	indent -kr -ut -ts4 -l120 src/*.c src/*.h


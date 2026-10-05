CLANG = clang
NPM = npm
NAME = horus 
SUDO = sudo

UNAME_S = $(shell uname -s)

CFLAGS  = -std=c23 -Wall
LDFLAGS = -lm -lpapago -ljansson -lrattler -lcurl -ltomlc17

TEST_CFLAGS = -std=c23 -g -Wall

INCDIR  = /usr/local/include
LIBDIR  = /usr/local/lib
LOCALBIN = /usr/local/bin

.PHONY: build
build: build-web build-binary

.PHONY: build-web
build-web:
	# install npm dependencies
	cd web && $(NPM) install
	cd web && $(NPM) run build

.PHONY: build-binary
build-binary:
	# build the horus binary
	$(CLANG) $(CFLAGS) -o horus main.c horus.c $(LDFLAGS)

.PHONY: install
install: 
	cp horus.h $(INCDIR)
	$(SUDO) install horus $(LOCALBIN)

uninstall:
	rm -f $(INCDIR)/horus.h

.PHONY: clean
clean:
	rm -f $(NAME)

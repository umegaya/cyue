##################
# Configurations #
##################

include config


############
# Defaults #
############

WARN ?= -O3 -Wall
KINCS ?=
CFLAGS ?= -c fPIC -O3 -Wall
LIB_OPTION ?= -shared -lm -lssl
CC ?= gcc
LD = ld
PREFIX ?= /usr
LUA_DIR ?= $(PREFIX)/share/lua/5.1
LUA_LIBDIR ?= $(PREFIX)/lib/lua/5.1
MD ?= mkdir -p
RM ?= rm -f
RMDIR ?= rm -rf
INSTALL ?= install --owner=root --group=root --mode=644
CHMOD ?= chmod -R
LIB_PERM ?= 00755


#########
# Do it #
#########

all: mime core link prepare_test

core:
	$(CC) $(CFLAGS) src/core.c

mime:
	$(CC) $(CFLAGS) src/bn_mime.c

link:
	$(LD) $(LIB_OPTION) core.o bn_mime.o -o core.so

prepare_test:
	cp src/bignum.lua ../../
	cp core.so ../../bignum.so

clean:
	$(RM) core.o bn_mime.o core.so bignum.lua ../../bignum.lua ../../bignum.so

install:
	$(MD) $(LUA_LIBDIR)/bignum
	$(INSTALL) src/bignum.lua $(LUA_DIR)/
	$(INSTALL) core.so $(LUA_LIBDIR)/bignum/
	$(CHMOD) $(LIB_PERM) $(LUA_LIBDIR)/bignum/

uninstall:
	$(RMDIR) $(LUA_LIBDIR)/bignum
	$(RM) $(LUA_DIR)/bignum.lua

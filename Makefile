TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src
DOCS_DIRS := ./docs
TEST_DIR := ./tests
INSTALL_DIR := ~/.local/bin
DB_FILE := _db.sqlite3
OS := $(shell uname -s)

# Builds the wren_to_c_string codegen tool that runs during the build itself.
# Deliberately NOT $(CC): the Windows cross-build invokes `CC=x86_64-w64-mingw32-gcc
# make static`, and a tool built with that compiler would produce a Windows PE
# binary that can't run on the Linux build host mid-build.
HOSTCC ?= cc

SPHINXBUILD ?= sphinx-build
SPHINXOPTS ?=

SRCS := $(shell find $(SRC_DIRS) -name '*.c')
OBJS := $(SRCS:%=$(BUILD_DIR)/%.o)
OBJ_DIRS := $(sort $(dir $(OBJS)))

WREN_FILES := $(shell find $(SRC_DIRS) -name '*.wren')

# -std= belongs in CFLAGS. It used to sit in LDFLAGS, where the compiler driver
# ignores it for compilation, so the project was never actually built as C17 --
# it used whatever the compiler defaulted to.
#
# gnu17 rather than c17: strict c17 defines __STRICT_ANSI__, which on glibc hides
# strtok_r, strncasecmp, realpath, openat, mkstemp, ftw.h and localtime_r, all of
# which this codebase uses. Switching to plain c17 would need
# -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE alongside it.
#
# -O2: there was no optimization flag at all, so releases shipped unoptimized.
# -Wvla catches VLAs (optional in C11/C17, absent on MSVC).
#
# EXTRA_CFLAGS/EXTRA_LDFLAGS exist for packagers. `debian/rules` passes the
# output of `dpkg-buildflags` plus -Wno-error through them: a distro build
# must not fail on a warning a newer compiler introduces, and the flags have
# to be appended here rather than passed as CFLAGS= on the make command line.
# A command-line CFLAGS would replace this assignment outright and silently
# drop the hardening and OpenSSL flags added below.
EXTRA_CFLAGS ?=
EXTRA_LDFLAGS ?=
CFLAGS := -std=gnu17 -Wall -Wextra -Werror -g -O2 -Wvla -Wpointer-arith \
          -fstack-protector-strong $(EXTRA_CFLAGS)
LDFLAGS := -lm -lpthread -lsqlite3 -lcurl $(EXTRA_LDFLAGS)

# Optional allocator: `make MIMALLOC=1` links mimalloc, which lowers the cost of
# the allocation-heavy per-request path (strings, query rows, rendered pages).
# It is opt-in on purpose: the default build depends on nothing beyond libc, and
# no allocator is vendored. Detection prefers pkg-config and falls back to the
# plain soname, since the Debian package ships the library without a .pc file.
# `make static` keeps its own curated link line and is not affected.
MIMALLOC_CFLAGS :=
MIMALLOC_LIBS :=
ifneq (,$(filter 1 yes on,$(MIMALLOC)))
MIMALLOC_CFLAGS := $(shell pkg-config --cflags libmimalloc 2>/dev/null)
MIMALLOC_LIBS := $(shell pkg-config --libs libmimalloc 2>/dev/null)
ifeq ($(strip $(MIMALLOC_LIBS)),)
MIMALLOC_LIBS := -lmimalloc
endif
CFLAGS += $(MIMALLOC_CFLAGS)
LDFLAGS += $(MIMALLOC_LIBS)
endif

# Hardening for the network daemon. Gated on the *target*, not on uname: the
# Windows cross-compile runs inside a Linux container, so `uname -s` says Linux
# while the target is mingw (whose CRT does not implement _FORTIFY_SOURCE and
# whose linker does not take -z relro). -U first so a toolchain that already
# predefines _FORTIFY_SOURCE (Ubuntu 24.04's GCC defines it as 3) does not
# error out on the redefinition under -Werror.
IS_MINGW := $(findstring mingw32,$(CC))
ifeq ($(OS)$(IS_MINGW),Linux)
CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2
LDFLAGS += -Wl,-z,relro,-z,now
endif

# Not enabled, because they cannot pass with -Werror while the vendored Wren
# sources are compiled in the same pass, but useful to run by hand:
#   -Wshadow -Wstrict-prototypes -Wwrite-strings -Wcast-qual -Wconversion

# Not checking against OS because I compile with Wine on Linux
ifneq (,$(findstring x86_64-w64-mingw32-gcc,$(CC)))
    # If it does, append -lws2_32 (winsock) and -lcrypt32 (Windows cert store)
    LDFLAGS += -lws2_32 -lcrypt32
endif

ifeq (,$(findstring Darwin,$(OS)))
# The probe used to leave the preprocessor's *output* in HAVE_SSL -- `$(CC) -E -`
# writes the expanded header to stdout, so HAVE_SSL became thousands of lines of
# openssl/ssl.h and never compared equal to "1". Non-Darwin builds therefore
# always compiled without -DHAVE_SSL, silently falling back to the DJB2 password
# hash even with libssl-dev installed. Send the output to /dev/null so only the
# exit status decides.
HAVE_SSL := $(shell echo "#include <openssl/ssl.h>" | $(CC) -E - >/dev/null 2>&1 && echo 1 || echo 0)
ifeq ($(HAVE_SSL),1)
	CFLAGS += -DHAVE_SSL
	SSL_LIBS := -lssl -lcrypto
	LDFLAGS += $(SSL_LIBS)
endif
else
	# HAVE_SSL=0 is an explicit opt-out (for a LibreSSL or an OpenSSL below
	# 1.1, which src/hash.h refuses). Without the override this branch always
	# defined HAVE_SSL, so the documented escape did not exist on macOS.
	ifneq ($(HAVE_SSL),0)
		CFLAGS += $(shell pkg-config --cflags openssl) -DHAVE_SSL
		SSL_LIBS := $(shell pkg-config --libs openssl)
		LDFLAGS += $(SSL_LIBS) -framework CoreServices
	endif
endif

all: $(BUILD_DIR)/$(TARGET_EXEC)

# Link flags are not prerequisites of the binary, so switching MIMALLOC on or
# off after a build would leave the previous binary in place -- `make` would
# find every object up to date and stop. Record the real link line in a stamp
# whose timestamp only moves when that line (or the compiler) changes, so the
# binary relinks exactly then and stays put otherwise. This is what makes
# `make MIMALLOC=1` work on a tree that was already built.
.PHONY: FORCE
FORCE:

LINK_STAMP := $(BUILD_DIR)/.link-flags

$(LINK_STAMP): FORCE
	@mkdir -p $(dir $@)
	@printf '%s\n' '$(CC) $(LDFLAGS)' > $@.tmp
	@cmp -s $@.tmp $@ || mv -f $@.tmp $@
	@rm -f $@.tmp

wren_files: $(WREN_INCS)

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS) $(LINK_STAMP)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# The Wren sources are embedded as C strings via the .inc files, so any object
# file must be rebuilt when one of them changes.
WREN_INCS := $(WREN_FILES:%.wren=%.wren.inc)

# Without .SECONDARY, GNU Make treats .wren.inc as a disposable intermediate
# (built by one pattern rule, consumed by another) and deletes it from the
# working tree right after linking, whenever regeneration actually ran --
# silently untracking a file that's meant to be committed to git.
.SECONDARY: $(WREN_INCS)

# Regenerate a .wren.inc whenever its .wren source is newer. Without this rule,
# editing a .wren file and running plain `make` linked the OLD embedded source
# into the binary -- only an explicit `make wren_files` picked the change up.
%.wren.inc: %.wren $(BUILD_DIR)/tools/wren_to_c_string
	$(BUILD_DIR)/tools/wren_to_c_string $@ $<

# A .inc newer than its .wren is assumed up to date, so `make` after a
# successful build compiles a hand-edited .inc without noticing. Regenerate
# each one to a temporary file and compare: releases are safe (a clean build
# always regenerates), but a tampered or stale committed copy is caught here.
check-wren-inc: $(BUILD_DIR)/tools/wren_to_c_string
	@tmp=$$(mktemp); status=0; \
	for src in $(WREN_FILES); do \
		src=$${src#./}; \
		$(BUILD_DIR)/tools/wren_to_c_string "$$tmp" "$$src" || status=1; \
		if ! cmp -s "$$tmp" "$$src.inc"; then \
			echo "check-wren-inc: $$src.inc does not match a fresh regeneration" >&2; \
			echo "  run: make wren_files" >&2; \
			status=1; \
		fi; \
	done; \
	rm -f "$$tmp"; \
	exit $$status

$(BUILD_DIR)/tools/wren_to_c_string: tools/wren_to_c_string.c | $(BUILD_DIR)/tools
	$(HOSTCC) -O2 -o $@ $<

$(BUILD_DIR)/tools:
	@mkdir -p $@

# Header dependencies. Without these, editing a header left every object file
# that includes it stale: a change to a constant such as HASH_AND_SALT_LENGTH
# would relink mismatched objects, where one translation unit sizes a buffer
# with the old value and another writes it with the new one.
DEPS := $(OBJS:.o=.d)

$(BUILD_DIR)/%.c.o: %.c $(WREN_INCS) | $(OBJ_DIRS)
	$(CC) $(CFLAGS) -MMD -MP -MF $(@:.o=.d) -c $< -o $@

-include $(DEPS)

$(OBJ_DIRS):
	@mkdir -p $@

installcheck: install
	$(TEST_DIR)/run.sh $(INSTALL_DIR)/$(TARGET_EXEC)

# The examples on the documentation landing page are checked with the same
# validator users have: a snippet that stopped compiling should fail the build,
# not ship. `literalinclude` pulls the real file, and this proves the slice it
# shows is still a valid script.
#
# `-T docs/examples` then runs the HTTP tests that live next to the examples,
# and check_docs_apps.sh boots the three examples that own their schema
# (guestbook, guestbook-styled, inventory): those cannot run as pages of a
# shared app root, so nothing else covered them.
check-examples: $(BUILD_DIR)/$(TARGET_EXEC)
	python3 tools/check_docs_examples.py \
		--binary $(BUILD_DIR)/$(TARGET_EXEC) --docs $(DOCS_DIRS)
	./$(BUILD_DIR)/$(TARGET_EXEC) -T $(DOCS_DIRS)/examples
	./tools/check_docs_apps.sh $(BUILD_DIR)/$(TARGET_EXEC)

# Optional fast Windows gate: compiles every source for _WIN32 with MinGW
# (-fsyntax-only). Needs mingw-w64 and the target headers; skips cleanly when
# they are not installed. The release build still crosscompiles and runs the
# suite under Wine -- see AGENTS.md.
check-mingw:
	./tools/check_mingw_syntax.sh

check: $(BUILD_DIR)/$(TARGET_EXEC)
	@$(MAKE) --no-print-directory check-wren-inc
	$(TEST_DIR)/run.sh
	./$(BUILD_DIR)/$(TARGET_EXEC) -T $(TEST_DIR)
	$(MAKE) check-examples

# Per-file line coverage of the C sources, built instrumented in build-cov/
# (see tools/coverage.sh). clang by default; override with
# `make coverage COVERAGE_CC=gcc`.
COVERAGE_CC ?= clang

coverage:
	./tools/coverage.sh $(COVERAGE_CC)

install: $(BUILD_DIR)/$(TARGET_EXEC)
	mkdir -p $(INSTALL_DIR)
	@# Overwriting a running binary in place corrupts its on-disk image and
	@# makes later execs hang: on macOS the code signature and page cache are
	@# tied to the inode, so `cp` over a live process breaks the file at that
	@# path. Write a sibling and rename it into place -- the swap is atomic and
	@# running processes keep the old inode.
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_DIR)/.$(TARGET_EXEC).tmp
	chmod 755 $(INSTALL_DIR)/.$(TARGET_EXEC).tmp
	mv -f $(INSTALL_DIR)/.$(TARGET_EXEC).tmp $(INSTALL_DIR)/$(TARGET_EXEC)

install-hooks:
	git config core.hooksPath .githooks
	@echo "Pre-commit hook installed (clang-format validation + make check)."

uninstall:
	rm -f $(INSTALL_DIR)/$(TARGET_EXEC)

clean:
	rm -rf $(BUILD_DIR)
	find . -name "$(DB_FILE)*" -type f -delete
	@echo "CFLAGS: $(CFLAGS)"
	@echo "LDFLAGS: $(LDFLAGS)"

html:
	@$(SPHINXBUILD) -M html "$(DOCS_DIRS)" "$(BUILD_DIR)" $(SPHINXOPTS) $(O)

# Network required: verifies the release download links inside the built site
# point at assets that actually exist. Run `make html` first.
check-release-links:
	@python3 tools/check_release_urls.py $(BUILD_DIR)/html

# Static build — self-contained binary with all deps linked in
# MinGW cross-compilation (Windows) — uses -static, no libcurl on Windows
ifneq (,$(findstring mingw32,$(CC)))
static: $(OBJS)
	$(CC) -static $(CFLAGS) $(OBJS) -o $(BUILD_DIR)/$(TARGET_EXEC) \
		-lm -lpthread -lsqlite3 -lssl -lcrypto -lws2_32 -lcrypt32
else ifeq ($(OS),Linux)
CURL_STATIC_LIBS := $(shell curl-config --static-libs 2>/dev/null || echo '-lcurl')
CURL_BFLAGS := -Wl,-Bstatic -Wl,-Bdynamic
# OpenLDAP sonames are versioned per release (liblber-2.5.so.0 on Ubuntu 22.04
# vs liblber-2.6.so.0 on Ubuntu 24.04), so linking them dynamically makes the
# binary distro-specific. Link them statically instead. Ubuntu's libldap.a is
# built against GnuTLS and Cyrus SASL, so resolve those symbols dynamically
# (libgnutls.so.30 and libsasl2.so.2 have stable sonames). All other curl deps
# have stable sonames and stay dynamic.
CURL_DEPS := $(filter-out -lcurl -llber -lldap $(CURL_BFLAGS),$(CURL_STATIC_LIBS))
static: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(BUILD_DIR)/$(TARGET_EXEC) \
		-Wl,-Bstatic -lsqlite3 -lcurl -Wl,-Bdynamic $(CURL_DEPS) \
		-Wl,-Bstatic -llber -lldap -llber -Wl,-Bdynamic -lgnutls -lsasl2 \
		$(SSL_LIBS) -lm -lpthread -ldl
else
static:
	@echo "Static build is not supported on $(OS). Use 'make' instead."
	@exit 1
endif

.PHONY: all clean wren_files check-wren-inc install uninstall check check-examples check-mingw check-release-links html static install-hooks coverage

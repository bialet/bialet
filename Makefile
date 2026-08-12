TARGET_EXEC := bialet
BUILD_DIR := ./build
SRC_DIRS := ./src
DOCS_DIRS := ./docs
TEST_DIR := ./tests
INSTALL_DIR := ~/.local/bin
DB_FILE := _db.sqlite3
OS := $(shell uname -s)

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
CFLAGS := -std=gnu17 -Wall -Wextra -Werror -g -O2 -Wvla -Wpointer-arith \
          -fstack-protector-strong
LDFLAGS := -lm -lpthread -lsqlite3 -lcurl

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
    # If it does, append -lws2_32 to LDFLAGS
    LDFLAGS += -lws2_32
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
		CFLAGS += $(shell pkg-config --cflags openssl) -DHAVE_SSL
		SSL_LIBS := $(shell pkg-config --libs openssl)
		LDFLAGS += $(SSL_LIBS) -framework CoreServices
endif

all: $(BUILD_DIR)/$(TARGET_EXEC)

wren_files:
	python3 tools/wren_to_c_string.py src/bialet.wren.inc src/bialet.wren
	python3 tools/wren_to_c_string.py src/bialet_test.wren.inc src/bialet_test.wren
	python3 tools/wren_to_c_string.py src/wren_core.wren.inc src/wren_core.wren

$(BUILD_DIR)/$(TARGET_EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

# The Wren sources are embedded as C strings via the .inc files, so any object
# file must be rebuilt when one of them changes.
WREN_INCS := $(WREN_FILES:%.wren=%.wren.inc)

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

check: $(BUILD_DIR)/$(TARGET_EXEC)
	$(TEST_DIR)/run.sh
	./$(BUILD_DIR)/$(TARGET_EXEC) -T $(TEST_DIR)

install: $(BUILD_DIR)/$(TARGET_EXEC)
	mkdir -p $(INSTALL_DIR)
	cp $(BUILD_DIR)/$(TARGET_EXEC) $(INSTALL_DIR)

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

.PHONY: all clean wren_files install uninstall check html static install-hooks

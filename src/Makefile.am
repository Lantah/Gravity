## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = serial-tests
bin_PROGRAMS = gravity

include $(top_srcdir)/common.mk
include $(srcdir)/src.mk

noinst_HEADERS = $(SRC_H_FILES)

if BUILD_TESTS
gravity_SOURCES = main/GravityVersion.cpp main/XDRFilesSha256.cpp $(SRC_CXX_FILES) $(SRC_TEST_CXX_FILES)
else # !BUILD_TESTS
gravity_SOURCES = main/GravityVersion.cpp main/XDRFilesSha256.cpp $(SRC_CXX_FILES)
endif # !BUILD_TESTS

if ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
main/XDRFilesSha256.cpp: $(SRC_X_FILES) Makefile $(top_srcdir)/hash-xdrs.sh
	$(top_srcdir)/hash-xdrs.sh protocol-next >$@
else
main/XDRFilesSha256.cpp: $(SRC_X_FILES) Makefile $(top_srcdir)/hash-xdrs.sh
	$(top_srcdir)/hash-xdrs.sh protocol-curr >$@
endif

if ENABLE_PROTOCOL_UPGRADE_VIA_SOROBAN_ENV_HOST_PREV
CARGO_FEATURES = --features soroban-env-host-prev
else
CARGO_FEATURES =
endif
if USE_TRACY
CARGO_FEATURES += --features tracy
endif

gravity_LDADD = $(soci_LIBS) $(libmedida_LIBS)		\
	$(top_builddir)/lib/lib3rdparty.a $(sqlite3_LIBS)	\
	$(libpq_LIBS) $(xdrpp_LIBS) $(libsodium_LIBS) $(libunwind_LIBS)

TESTDATA_DIR = testdata
TEST_FILES = $(TESTDATA_DIR)/gravity_example.cfg $(TESTDATA_DIR)/gravity_standalone.cfg \
             $(TESTDATA_DIR)/gravity_testnet.cfg $(TESTDATA_DIR)/gravity_testnet_legacy.cfg \
             $(TESTDATA_DIR)/stellar-history.testnet.6714239.json $(TESTDATA_DIR)/stellar-history.livenet.15686975.json \
             $(TESTDATA_DIR)/gravity_testnet_validator.cfg $(TESTDATA_DIR)/gravity_example_validators.cfg \
             $(TESTDATA_DIR)/stellar-history.testnet.6714239.networkPassphrase.json

BUILT_SOURCES = $(SRC_X_FILES:.x=.h) main/GravityVersion.cpp main/XDRFilesSha256.cpp $(TEST_FILES)

$(SRC_X_FILES:.x=.h): $(XDRC)
SUFFIXES = .x .h .rs
.x.h:
	$(XDRC) -hh -pedantic -o $@ $<

BISON=bison
FLEX=flex

util/xdrquery/XDRQueryScanner.cpp: util/xdrquery/XDRQueryScanner.ll
	$(FLEX) --outfile=$@ $<

util/xdrquery/XDRQueryParser.cpp: util/xdrquery/XDRQueryParser.yy
	$(BISON) --defines=util/xdrquery/XDRQueryParser.h  --output=$@ $<

util/xdrquery/XDRQueryParser.h: util/xdrquery/XDRQueryParser.cpp
	touch $@

BUILT_SOURCES += util/xdrquery/XDRQueryScanner.cpp util/xdrquery/XDRQueryParser.h util/xdrquery/XDRQueryParser.cpp
gravity_SOURCES += util/xdrquery/XDRQueryScanner.cpp util/xdrquery/XDRQueryParser.h util/xdrquery/XDRQueryParser.cpp

# Old automakes have buggy dependency tracking for conditional generated
# sources. We work around this here by making rust/RustBridge.{cpp,h} generated
# in all cases, and just empty in the non-rust case. Also because of the way old
# automake relies on horrible path-munging code in config.status to extract the
# names of depfiles from the Makefile itself, we can't use any variables in the
# SOURCES addition we're doing here, have to list unadorned paths.

BUILT_SOURCES += rust/RustBridge.h rust/RustBridge.cpp
gravity_SOURCES += rust/RustBridge.h rust/RustBridge.cpp

if ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION
RUST_BUILD_DIR=$(top_builddir)/src/rust
RUST_BIN_DIR=$(RUST_BUILD_DIR)/bin
RUST_TARGET_DIR=$(RUST_BUILD_DIR)/target
RUST_CXXBRIDGE=$(RUST_BIN_DIR)/cxxbridge
RUST_PROFILE=release
RUST_HOST_DEPFILES=rust/Cargo.toml $(top_srcdir)/Cargo.toml $(top_srcdir)/Cargo.lock rust/src/host-dep-tree-curr.txt rust/src/host-dep-tree-prev.txt
LIBRUST_GRAVITY=$(RUST_TARGET_DIR)/$(RUST_PROFILE)/librust_gravity.a
gravity_LDADD += $(LIBRUST_GRAVITY) -ldl

$(RUST_CXXBRIDGE):
	mkdir -p $(RUST_BIN_DIR)
	$(CARGO) install --root $(RUST_BUILD_DIR) cxxbridge-cmd --version 1.0.68

rust/RustBridge.h: rust/src/lib.rs $(SRC_RUST_FILES) Makefile $(RUST_CXXBRIDGE)
	$(RUST_CXXBRIDGE) $< --header --output $@.tmp
	if cmp -s $@.tmp $@; then rm -v $@.tmp; else mv -v $@.tmp $@; fi

rust/RustBridge.cpp: rust/src/lib.rs $(SRC_RUST_FILES) Makefile $(RUST_CXXBRIDGE)
	$(RUST_CXXBRIDGE) $< --output $@.tmp
	if cmp -s $@.tmp $@; then rm -v $@.tmp; else mv -v $@.tmp $@; fi

$(LIBRUST_GRAVITY): $(RUST_HOST_DEPFILES) $(SRC_RUST_FILES) Makefile
	cd $(RUST_BUILD_DIR) && cargo build --$(RUST_PROFILE) --target-dir $(abspath $(RUST_TARGET_DIR)) $(CARGO_FEATURES)
	ranlib $@
else

rust/RustBridge.cpp:
	touch $@

rust/RustBridge.h:
	touch $@

endif # ENABLE_NEXT_PROTOCOL_VERSION_UNSAFE_FOR_PRODUCTION


$(srcdir)/src.mk: $(top_srcdir)/make-mks
	cd $(top_srcdir) && ./make-mks

$(TESTDATA_DIR)/%.cfg : $(top_srcdir)/docs/%.cfg
	mkdir -p $(@D) && cp $< $@

$(TESTDATA_DIR)/%.json : $(top_srcdir)/src/history/serialize-tests/%.json
	mkdir -p $(@D) && cp $< $@

.PHONY: always
always:
	@:

# Always rebuild because .git/HEAD is a symbolic ref one can't depend on
main/GravityVersion.cpp: always
	@vers=$$(cd "$(srcdir)" \
		&& git describe --always --dirty --tags 2>/dev/null \
		|| echo "$(PACKAGE) $(VERSION)"); \
		sed -e "s@%%VERSION%%@$$vers@" \
			< "$(srcdir)/main/GravityVersion.cpp.in" > $@~
	@if cmp -s $@~ $@; then rm -f $@~; else \
	    mv -f $@~ $@ && printf "echo '%s' > $@\n" "$$(cat $@)"; fi

if USE_POSTGRES
TESTS=test/selftest-pg
else # !USE_POSTGRES
TESTS=test/selftest-nopg
endif # !USE_POSTGRES
TESTS += test/check-nondet

if USE_CLANG_FORMAT
format: always
	cd $(srcdir) && $(CLANG_FORMAT) -style=file -i $(SRC_CXX_FILES) $(SRC_H_FILES) $(SRC_TEST_CXX_FILES) $(SRC_TEST_H_FILES)
endif # USE_CLANG_FORMAT

if USE_AFL_FUZZ
FUZZER_MODE ?= overlay

fuzz-testcases: gravity
	mkdir -p fuzz-testcases
	for i in `seq 1 10000`; do \
	    ./gravity gen-fuzz --mode=${FUZZER_MODE} fuzz-testcases/fuzz$$i.xdr ; \
	done
	mkdir -p min-testcases
	afl-cmin -i fuzz-testcases -o min-testcases -m 500 -t 250 ./gravity fuzz --ll ERROR --mode=${FUZZER_MODE} @@
	rm -Rf fuzz-testcases

# when running in parallel,
# run the same command than below replacing `-M main` with `-S worker_N`
# and `--process-id 0` with `--process-id N`

fuzz: fuzz-testcases gravity
	mkdir -p fuzz-findings
	afl-fuzz -m 500 -M main -t 250 -i min-testcases -o fuzz-findings \
	    ./gravity fuzz --ll ERROR --process-id 0 --mode=${FUZZER_MODE} @@

fuzz-clean: always
	rm -Rf fuzz-testcases fuzz-findings

distclean-local: fuzz-clean
endif # USE_AFL_FUZZ

CLEANFILES = $(BUILT_SOURCES) *~ */*~ stellar*.log
MAINTAINERCLEANFILES = $(srcdir)/Makefile.in $(srcdir)/*~ $(srcdir)/*/*~

EXTRA_DIST = $(SRC_X_FILES) test/selftest-nopg test/selftest-pg

#
# Copyright 2017, Data61
# Commonwealth Scientific and Industrial Research Organisation (CSIRO)
# ABN 41 687 119 230.
#
# This software may be distributed and modified according to the terms of
# the BSD 2-Clause license. Note that NO WARRANTY is provided.
# See "LICENSE_BSD2.txt" for details.
#
# @TAG(DATA61_BSD)
#

# Import Kbuild helper functions.
include tools/kbuild/Kbuild.include

### Verbose building
########################################

# Set V=1 for verbose building, this can be passed in on the command line
# Set V=2 to have make echo out commands before executing them

ifeq ($V, 1)
	Q =
else
ifeq ($V, 2)
	Q =
else
ifeq ($V, 3)
	Q =
else
	Q = @
endif
endif
endif

CONFIG_USER_COMPILER:=$(patsubst %",%,$(patsubst "%,%,${CONFIG_USER_COMPILER}))
#")") Help syntax-highlighting editors.
ifeq (${CONFIG_USER_COMPILER},)
  CC := $(CCACHE) $(TOOLPREFIX)gcc
else
  CC := $(CCACHE) ${CONFIG_USER_COMPILER}
endif
CXX := $(CCACHE) $(TOOLPREFIX)g++
ASM := $(TOOLPREFIX)as
LD  := $(TOOLPREFIX)ld
AR  := $(TOOLPREFIX)ar
CPP := $(TOOLPREFIX)cpp
OBJCOPY := $(TOOLPREFIX)objcopy
FMT := $(shell which clang-format)
ifneq (${FMT},)
  FMT += --style=LLVM
else
  FMT := $(shell which cat)
endif
SPONGE := $(shell which sponge)
# If `sponge` is unavailable, emulate it.
ifeq (${SPONGE},)
  SPONGE := python -c 'import sys; data = sys.stdin.read(); f = open(sys.argv[1], "w"); f.write(data); f.close()'
endif

# Default path configuration (useful for local development)
SEL4_LIBDIR     ?= $(STAGE_DIR)/lib
SEL4_INCLUDEDIR ?= $(STAGE_DIR)/include
SEL4_BINDIR     ?= $(STAGE_DIR)/bin
SEL4_KERNEL     ?= $(STAGE_DIR)/kernel/kernel.elf

# Compile configuration
INCLUDE_PATH := $(SEL4_INCLUDEDIR) $(INCLUDE_DIRS)
WARNINGS     := all

CPPFLAGS += $(INCLUDE_PATH:%=-I%)

# Strip enclosing quotes.
CONFIG_USER_CFLAGS:=$(patsubst %",%,$(patsubst "%,%,${CONFIG_USER_CFLAGS}))
#")") Help syntax-highlighting editors.

# We use start and end groups to recursively find symbols when linking.
# However we cannot use these when performing link time optimisations.
# We define these as variables here such that they can be overridden
# if LTO is enabled
STARTGROUP := -Wl,--start-group
ENDGROUP := -Wl,--end-group

ifeq (${CONFIG_USER_CFLAGS},)
    CFLAGS += $(WARNINGS:%=-W%) -nostdinc -std=gnu11
    CXXFLAGS += $(WARNINGS:%=-W%) -nostdinc $(call cc-option,-std=gnu++14,-std=gnu++98)

    ifeq (${CONFIG_USER_OPTIMISATION_Os},y)
        CFLAGS += -Os
        CXXFLAGS += -Os
    endif
    ifeq (${CONFIG_USER_OPTIMISATION_O0},y)
        CFLAGS += -O0
        CXXFLAGS += -O0
    endif
    ifeq (${CONFIG_USER_OPTIMISATION_O1},y)
        CFLAGS += -O1
        CXXFLAGS += -O1
    endif
    ifeq (${CONFIG_USER_OPTIMISATION_O3},y)
        CFLAGS += -O3
        CXXFLAGS += -O3
    endif
    ifeq (${CONFIG_USER_OPTIMISATION_O2},y)
        CFLAGS += -O2
        CXXFLAGS += -O2
    endif

    ifeq (${CONFIG_LINK_TIME_OPTIMISATIONS},y)
        CFLAGS += -flto
        CXXFLAGS += -flto
        STARTGROUP :=
        ENDGROUP :=
    endif

    CFLAGS += $(NK_CFLAGS)
    CXXFLAGS += $(NK_CXXFLAGS)
    CFLAGS += -fno-stack-protector
    CXXFLAGS += -fno-stack-protector
else
    # Override the cflags with Kconfig-specified flags
	CFLAGS += ${CONFIG_USER_CFLAGS}
endif

ifeq (${CONFIG_WHOLE_PROGRAM_OPTIMISATIONS_USER},y)
    LDFLAGS += -fwhole-program
endif

LIBGCC := -lgcc
EH := $(shell $(CC) --print-file-name libgcc_eh.a)
ifneq "${EH}" "libgcc_eh.a"
  LIBGCC += -lgcc_eh
endif

LDFLAGS += $(NK_LDFLAGS) \
		   $(SEL4_LIBDIR:%=-L%) \
		   $(LIBDIRS:%=-L%) \
		   $(STARTGROUP) \
		   $(LIBS:%=-l%) \
		   $(LIBGCC) \
		   $(ENDGROUP) \
		   $(CFLAGS) \
		   -static -nostdlib
ifeq (${CONFIG_USE_RUST},y)
 LDFLAGS += -lcompiler-rt
endif

ARCHIVES += $(LIBS:%=lib%.a)

ENTRY_POINT ?= _start

# Force start symbol to be linked in if need be - the user may already have it in a
# .o file, otherwise this will pull it from a library such as
# libsel4platsupport.
LDFLAGS += -u ${ENTRY_POINT}

# Set the entry point
LDFLAGS += -e ${ENTRY_POINT}

PAGE_SIZE ?= 0x1000

# Prevent linker padding sections to align to a large page size.
# This is to prevent elf sections starting at low addresses being
# rounded down to address 0.
LDFLAGS += -z max-page-size=${PAGE_SIZE}

ASFLAGS += $(NK_ASFLAGS)

# Object files
OBJFILES = $(ASMFILES:%.S=%.o) $(CFILES:%.c=%.o) $(CXXFILES:%.cxx=%.o) $(OFILES) $(RUST_TARGET)

# Define standard crt files if are building against a C library that has them
ifeq (${CONFIG_HAVE_CRT},y)
	CRTOBJFILES ?= $(SEL4_LIBDIR)/crt1.o $(SEL4_LIBDIR)/crti.o $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtbegin.o)
	FINOBJFILES ?= $(shell $(CC) $(CFLAGS) $(CPPFLAGS) -print-file-name=crtend.o) $(SEL4_LIBDIR)/crtn.o
else
	CRTOBJFILES ?=
	FINOBJFILES ?=
endif

# Copy a file.
cp_file = \
	@echo " [STAGE] $(notdir $2)"; cp -f $(1) $(2)

# Where to look for header dependencies
vpath %.h $(INCLUDE_PATH)

# Where to look for library dependencies
vpath %.a $(SEL4_LIBDIR)

# Where to find the sources
vpath %.c $(SOURCE_DIR)
vpath %.cxx $(SOURCE_DIR)
vpath %.S $(SOURCE_DIR)

PRIORITY_TARGETS += $(RUST_TARGET)

# Default is to build/install all targets
default: $(PRIORITY_TARGETS) install-headers $(TARGETS)

#
# For each ".bin" or ".a" target, we also setup a rule to copy it into a final
# binaries output directory. For binary files we want to remove the .bin suffix
#
default: $(patsubst %.bin,$(SEL4_BINDIR)/%,$(filter %.bin,$(TARGETS)))
default: $(patsubst %.a,$(SEL4_LIBDIR)/%.a,$(filter %.a,$(TARGETS)))
#default: $(patsubst %.a,$(EXTLIBDIRS)/%.a,$(filter %.a,$(TARGETS)))

$(SEL4_BINDIR)/%: %.bin
	$(Q)mkdir -p $(dir $@)
	$(call cp_file,$<,$@)

$(SEL4_LIBDIR)/%.a: %.a
	$(Q)mkdir -p $(dir $@)
	$(call cp_file,$<,$@)

.PHONY: install-headers

HDRFILES += $(srctree)/include/generated/autoconf.h

install-headers:
	@if [ -n "$(HDRFILES)" ] ; then \
		mkdir -p $(SEL4_INCLUDEDIR) ; \
		echo " [HEADERS]"; \
		for file in $(HDRFILES); do \
			printf " [STAGE] "; printf `basename $$file`; \
			if [ -d $$file ]; then echo "/*"; else echo; fi; \
			cp -aL $$file $(SEL4_INCLUDEDIR) ; \
		done; \
	fi
	@if [ -n "$(RHDRFILES)" ] ; then \
		mkdir -p $(SEL4_INCLUDEDIR) ; \
		echo " [HEADERS] "; \
		for hdrfile in $(RHDRFILES) ; do \
			source=`echo "$$hdrfile" | sed 's/^\(.*\)[ \t][^ \t]*$$/\1/'` ; \
			dest=$(SEL4_INCLUDEDIR)/`echo "$$hdrfile" | sed 's/^.*[ \t]\([^ \t]*\)$$/\1/'` ; \
			mkdir -p $$dest; \
			cp -a $$source $$dest ; \
			printf " [STAGE]"; basename $$dest; \
		done ; \
	fi

.PHONY: $(RUST_TARGET)
$(RUST_TARGET):
	@echo " [RS] $(RUST_TARGET)"
	$(Q) RUST_TARGET_PATH=${STAGE_DIR}/common/  \
	$(XARGO) build --lib $(RUST_CARGO_FLAGS) --manifest-path $(SOURCE_DIR)/Cargo.toml --target=$(RUST_CUSTOM_TARGET)
	$(Q) cp $(SOURCE_DIR)/target/${RUST_CUSTOM_TARGET}/$(RUST_RELEASE_MODE)/$(RUST_TARGET) ${STAGE_DIR}/lib/$(RUST_TARGET)

ifeq (${CONFIG_BUILDSYS_CPP_SEPARATE},y)
%.o: %.c_pp
	@echo " [CC] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) -x c $(CFLAGS) $(CPPFLAGS) -c $< -o $@
else
%.o: %.c $(HFILES) | install-headers
	@echo " [CC] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(call make-depend,$<,$@,$(patsubst %.o,%.d,$@))
	$(Q)$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
endif
%.o: %.cxx $(HFILES) | install-headers
	@echo " [CXX] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(call make-cxx-depend,$<,$@,$(patsubst %.o,%.d,$@))
	$(Q)$(CXX) $(CXXFLAGS) $(CPPFLAGS) -c $< -o $@

.PRECIOUS: %.c_pp
%.c_pp: %.c $(HFILES) | install-headers
	@echo " [CPP] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(call make-depend,$<,$@,$(patsubst %.c_pp,%.d,$@))
	$(Q)$(CC) -E \
        $(if ${CONFIG_BUILDSYS_CPP_RETAIN_COMMENTS},-C -CC,) \
        $(if ${CONFIG_BUILDSYS_CPP_LINE_DIRECTIVES},,-P) \
        $(CFLAGS) $(CPPFLAGS) -c $< -o $@

%.o: %.S $(HFILES) | install-headers
	@echo " [ASM] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(call make-depend,$<,$@,$(patsubst %.o,%.d,$@))
	$(Q)$(CC) $(ASFLAGS) $(CPPFLAGS) -c $< -o $@

%.a: $(OBJFILES)
	@echo " [AR] $@ objs:$(OBJFILES)"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(AR) r $@ $(OBJFILES) > /dev/null 2>&1

%.bin: %.elf
	$(call cp_file,$<,$@)

# Note: Create a separate rule ARCHIVES to clearly indicate they
# the ARCHIVES must be built before doing any other steps. Previously
# when ARCHIVES was a prerequisite on the line below make thought
# it was local to this directory and would make the archives using
# src/main.obj.
%.elf: $(ARCHIVES)

# Note: below the CC line does not have ARCHIVES because
# LDFLAGS already includes the "-l" version of ARCHIVES
%.elf: $(CRTOBJFILES) $(FINOBJFILES) $(OBJFILES)
	@echo " [LINK] $@"
	$(Q)mkdir -p $(dir $@)
	$(Q)$(CC) $(CRTOBJFILES) $(OBJFILES) $(FINOBJFILES) $(LDFLAGS) -o $@

$(TARGETS): $(LIBS:%=-l%)

# Avoid inadvertently passing local shared libraries with the same names as ours to the
# linker. This is a hack and will need to be changed if we start depending on SOs.
# (Default is .LIBPATTERNS = lib%.so lib%.a)
.LIBPATTERNS = lib%.a

DEPS = $(patsubst %.c,%.d,$(CFILES)) $(patsubst %.cxx,%.d,$(CXXFILES)) $(patsubst %.S,%.d,$(ASMFILES))

ifneq "$(MAKECMDGOALS)" "clean"
  -include ${DEPS}
endif

# $(call make-depend,source-file,object-file,depend-file)
define make-depend
  ${CC} -MM            \
         -MF $3         \
         -MP            \
         -MT $2         \
         $(CFLAGS)      \
         $(CPPFLAGS)    \
         $1
endef
define make-cxx-depend
  ${CXX} -MM            \
         -MF $3         \
         -MP            \
         -MT $2         \
         $(CXXFLAGS)    \
         $(CPPFLAGS)    \
         $1
endef

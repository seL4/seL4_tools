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

# Some of the targets in here use bashisms, so force bash as the interpreter.
SHELL=/bin/bash
ifeq ($(wildcard ${SHELL}),)
$(error Prerequisite ${SHELL} not found)
endif

elfloader: export STAGE_DIR=$(STAGE_BASE)
elfloader: export BUILD_DIR=$(BUILD_BASE)/$@
elfloader: export SOURCE_DIR=${TOOLS_ROOT}/elfloader
elfloader: libcpio common FORCE
	@echo "[elfloader] building..."
	$(Q)mkdir -p $(BUILD_DIR)
	$(Q)CFLAGS= LDFLAGS= $(MAKE) $(MAKE_SILENT) \
		--directory=$(BUILD_DIR) --file=$(SOURCE_DIR)/Makefile \
		TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
	$(Q)mkdir -p $(SEL4_COMMON)/elfloader
	$(call cp_if_changed, "$(BUILD_DIR)/elfloader.o", \
		"$(SEL4_COMMON)/elfloader/elfloader.o")
	$(call cp_if_changed, "$(SOURCE_DIR)/src/archive.bin.lds", \
		"$(SEL4_COMMON)/elfloader/archive.bin.lds")
	$(call cp_if_changed, "$(SOURCE_DIR)/src/arch-$(ARCH)/linker.lds", \
		"$(SEL4_COMMON)/elfloader/linker.lds")
	$(call cp_if_changed, "$(SOURCE_DIR)/gen_boot_image.sh", \
		"$(SEL4_COMMON)/elfloader/gen_boot_image.sh")
	@echo "[elfloader] done."

%-image: export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
%-image: export STRIP=$(CONFIG_REMOVE_SYMBOLS)
%-image: export HASH=$(CONFIG_HASH_INSTRUCTIONS)
%-image: export HASH_SHA=$(CONFIG_HASH_SHA)
%-image: export V
%-image: % kernel_elf common elfloader FORCE
	@echo "[GEN_IMAGE] $@-$(ARCH)-$(PLAT)"
	$(Q)$(SEL4_COMMON)/elfloader/gen_boot_image.sh $(STAGE_BASE)/kernel.elf \
		$(STAGE_BASE)/bin/$< $(IMAGE_ROOT)/$@-$(ARCH)-$(PLAT) 2>&1 \
		| while read line; do echo " [GEN_IMAGE] $$line"; done; \
		exit $${PIPESTATUS[0]}

# make pre-image IMAGE_DIR=~/sel4-tutorials/stage/arm/zynq7000/bin/hello-3
pre-image: export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
pre-image: export STRIP=$(CONFIG_REMOVE_SYMBOLS)
pre-image: export HASH=$(CONFIG_HASH_INSTRUCTIONS)
pre-image: export HASH_SHA=$(CONFIG_HASH_SHA)
pre-image: export V
pre-image: elfloader
	@echo "[GEN_IMAGE] $(apps)-image-$(ARCH)-$(PLAT)"
	$(Q)$(SEL4_COMMON)/elfloader/gen_boot_image.sh $(STAGE_BASE)/kernel.elf \
		$(IMAGE_DIR) $(IMAGE_ROOT)/image 2>&1 \
		| while read line; do echo " [GEN_IMAGE] $$line"; done; \
		exit $${PIPESTATUS[0]}

capDL-$(ARCH)-$(PLAT): export TOOLPREFIX=$(CONFIG_CROSS_COMPILER_PREFIX:"%"=%)
capDL-$(ARCH)-$(PLAT): kernel_elf common elfloader
	@echo "[GEN_IMAGE] $@"
	$(Q)$(SEL4_COMMON)/elfloader/gen_boot_image.sh $(STAGE_BASE)/kernel.elf \
		$(STAGE_BASE)/bin/capDL-loader $(IMAGE_ROOT)/$@

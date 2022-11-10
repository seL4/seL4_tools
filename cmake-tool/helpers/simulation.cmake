#
# Copyright 2020, Data61, CSIRO (ABN 41 687 119 230)
#
# SPDX-License-Identifier: BSD-2-Clause
#

# This module provides a function GenerateSimulateScript which will place a `simulate`
# script in the build directory for running produced images in Qemu.
include_guard(GLOBAL)
include("${KERNEL_HELPERS_PATH}")
RequireFile(SIMULATE_SCRIPT simulate.py PATHS "${CMAKE_CURRENT_LIST_DIR}/../simulate_scripts/")
RequireFile(GDB_SCRIPT launch_gdb.py PATHS "${CMAKE_CURRENT_LIST_DIR}/../simulate_scripts/")
RequireFile(CONFIGURE_FILE_SCRIPT configure_file.cmake PATHS "${CMAKE_CURRENT_LIST_DIR}")

# Help macro for testing a config and appending to a list that is destined for a qemu -cpu line
macro(TestQemuCPUFeature config feature string)
    if(${config})
        set(${string} "${${string}},+${feature}")
    else()
        set(${string} "${${string}},-${feature}")
    endif()
endmacro(TestQemuCPUFeature)

# Function for the user for configuration the simulation script. Valid values for property are
#  'GRAPHIC' if set to TRUE disables the -nographic flag
#  'MEM_SIZE' if set will override the memory size given to qemu
function(SetSimulationScriptProperty property value)
    # define the target if it doesn't already exist
    if(NOT (TARGET simulation_script_prop_target))
        add_custom_target(simulation_script_prop_target)
    endif()
    set_property(TARGET simulation_script_prop_target PROPERTY "${property}" "${value}")
endfunction(SetSimulationScriptProperty)

macro(SetDefaultMemSize default)
    set(
        QemuMemSize
        "$<IF:$<BOOL:$<TARGET_PROPERTY:simulation_script_prop_target,MEM_SIZE>>,$<TARGET_PROPERTY:simulation_script_prop_target,MEM_SIZE>,${default}>"
    )
endmacro(SetDefaultMemSize)

# Helper function that generates targets that will attempt to generate a ./simulate style script
function(GenerateSimulateScript)
    set(error "")
    set(KERNEL_IMAGE_NAME "$<TARGET_PROPERTY:rootserver_image,KERNEL_IMAGE_NAME>")
    set(IMAGE_NAME "$<TARGET_PROPERTY:rootserver_image,IMAGE_NAME>")
    # Define simulation script target if it doesn't exist to simplify the generator expressions
    if(NOT (TARGET simulation_script_prop_target))
        add_custom_target(simulation_script_prop_target)
    endif()
    set(
        sim_graphic_opt
        "$<IF:$<BOOL:$<TARGET_PROPERTY:simulation_script_prop_target,GRAPHIC>>,,-nographic>"
    )
    set(sim_serial_opt "")
    set(sim_cpu "")
    set(sim_cpu_opt "")
    set(sim_machine "")
    set(qemu_sim_extra_args "")
    if(KernelArchX86)
        # Try and simulate the correct micro architecture and features
        if(KernelX86MicroArchNehalem)
            set(sim_cpu "Nehalem")
        elseif(KernelX86MicroArchGeneric)
            if(KernelSel4ArchIA32)
                set(sim_cpu "qemu32")
            else()
                set(sim_cpu "qemu64")
            endif()
        elseif(KernelX86MicroArchWestmere)
            set(sim_cpu "Westmere")
        elseif(KernelX86MicroArchSandy)
            set(sim_cpu "SandyBridge")
        elseif(KernelX86MicroArchIvy)
            set(sim_cpu "IvyBridge")
        elseif(KernelX86MicroArchHaswell)
            set(sim_cpu "Haswell")
        elseif(KernelX86MicroArchBroadwell)
            set(sim_cpu "Broadwell")
        else()
            set(error "Unknown x86 micro-architecture for simulation")
        endif()
        TestQemuCPUFeature(KernelVTX vme sim_cpu_opt)
        TestQemuCPUFeature(KernelHugePage pdpe1gb sim_cpu_opt)
        TestQemuCPUFeature(KernelFPUXSave xsave sim_cpu_opt)
        TestQemuCPUFeature(KernelXSaveXSaveOpt xsaveopt sim_cpu_opt)
        TestQemuCPUFeature(KernelXSaveXSaveC xsavec sim_cpu_opt)
        TestQemuCPUFeature(KernelFSGSBaseInst fsgsbase sim_cpu_opt)
        TestQemuCPUFeature(KernelSupportPCID invpcid sim_cpu_opt)
        TestQemuCPUFeature(KernelX86SyscallSyscall syscall sim_cpu_opt)
        TestQemuCPUFeature(KernelSel4ArchX86_64 lm sim_cpu_opt)
        set(sim_cpu "${sim_cpu}")
        set(sim_cpu_opt "${sim_cpu_opt},enforce")
        if(KernelSel4ArchIA32)
            set(QemuBinaryMachine "qemu-system-i386")
        else()
            set(QemuBinaryMachine "qemu-system-x86_64")
        endif()
        set(sim_serial_opt "-serial mon:stdio")
        SetDefaultMemSize("512M")
    elseif(KernelPlatformSabre)
        set(QemuBinaryMachine "qemu-system-arm")
        # '-serial null -serial mon:stdio' means connect second UART to
        # the terminal and ignore the first UART
        set(sim_serial_opt "-serial null -serial mon:stdio")
        set(sim_machine "sabrelite")
        SetDefaultMemSize("1024M")
    elseif(KernelPlatformZynq7000)
        set(QemuBinaryMachine "qemu-system-arm")
        set(sim_serial_opt "-serial null -serial mon:stdio")
        set(sim_machine "xilinx-zynq-a9")
        SetDefaultMemSize("1024M")
    elseif(KernelPlatformWandQ)
        set(QemuBinaryMachine "qemu-system-arm")
        set(sim_serial_opt "-serial mon:stdio")
        set(sim_machine "sabrelite")
        SetDefaultMemSize("2048M")
    elseif(KernelPlatformRpi3 AND KernelSel4ArchAarch64)
        set(QemuBinaryMachine "qemu-system-aarch64")
        set(sim_serial_opt "-serial null -serial mon:stdio")
        set(sim_machine "raspi3")
        SetDefaultMemSize("1024M")
    elseif(KernelPlatformSpike)
        if(KernelSel4ArchRiscV32)
            set(binary "qemu-system-riscv32")
            set(sim_cpu "rv32")
            SetDefaultMemSize("2000M")
            set(sim_machine "virt")
        elseif(KernelSel4ArchRiscV64)
            set(binary "qemu-system-riscv64")
            set(sim_cpu "rv64")
            SetDefaultMemSize("4095M")
            set(sim_machine "spike")
        endif()
        set(QemuBinaryMachine "${binary}")
        set(sim_serial_opt "-serial mon:stdio")
        set(qemu_sim_extra_args "-bios none")
    elseif(KernelPlatformRocketchip)
        set(binary "qemu-system-riscv64")
        set(sim_cpu "rv64")
        SetDefaultMemSize("256M")
        set(QemuBinaryMachine "${binary}")
        set(sim_serial_opt "-serial mon:stdio")
        set(qemu_sim_extra_args "-bios none")
    elseif(KernelPlatformQEMUArmVirt)
        set(QemuBinaryMachine "qemu-system-${QEMU_ARCH}")
        if(KernelArmHypervisorSupport)
            set(sim_machine "virt,virtualization=on,highmem=off,secure=off")
        else()
            set(sim_machine "virt")
        endif()
        set(sim_graphic_opt "-nographic")
        set(sim_cpu "${KernelArmCPU}")
        SetDefaultMemSize("${QEMU_MEMORY}")
    elseif(KernelPlatformQEMURiscVVirt)
        set(QemuBinaryMachine "qemu-system-${KernelSel4Arch}")
        set(sim_machine "virt")
        # defaults for the virt machine:
        #     aclint=off (on/off)
        #     aia=none (none/aplic/aplic-imsic)
        #     aia-guests=0 (VS-level AIA IMSIC pages per hart)
        set(sim_graphic_opt "-nographic")
        set(sim_cpu "rv${KernelWordSize}")
        SetDefaultMemSize("${QEMU_MEMORY}")
        set(qemu_sim_extra_args "-bios none")
    else()
        set(error "Unsupported platform or architecture for simulation")
    endif()
    set(sim_path "${CMAKE_BINARY_DIR}/simulate")
    set(gdb_path "${CMAKE_BINARY_DIR}/launch_gdb")
    if(NOT "${error}" STREQUAL "")
        set(script "#!/bin/sh\\necho ${error} && exit 1\\n")
        add_custom_command(
            OUTPUT "${sim_path}" "${gdb_path}"
            COMMAND
                printf "${script}" > "${sim_path}"
            COMMAND
                printf "${script}" > "${gdb_path}"
            COMMAND
                chmod u+x "${sim_path}" "${gdb_path}"
            VERBATIM
        )
    else()
        # We assume a x86 host, but will provide options to override the default gdb binary
        if(KernelArchX86)
            set(GdbBinary "gdb")
        else()
            set(GdbBinary "gdb-multiarch")
        endif()
        add_custom_command(
            OUTPUT "${sim_path}"
            COMMAND
                ${CMAKE_COMMAND} -DCONFIGURE_INPUT_FILE=${SIMULATE_SCRIPT}
                -DCONFIGURE_OUTPUT_FILE=${sim_path} -DQEMU_SIM_BINARY=${QemuBinaryMachine}
                -DQEMU_SIM_CPU=${sim_cpu} -DQEMU_SIM_MACHINE=${sim_machine}
                -DQEMU_SIM_CPU_OPT=${sim_cpu_opt} -DQEMU_SIM_GRAPHIC_OPT=${sim_graphic_opt}
                -DQEMU_SIM_SERIAL_OPT=${sim_serial_opt} -DQEMU_SIM_MEM_SIZE_OPT=${QemuMemSize}
                -DQEMU_SIM_KERNEL_FILE=${KERNEL_IMAGE_NAME} -DQEMU_SIM_INITRD_FILE=${IMAGE_NAME}
                -DQEMU_SIM_EXTRA_ARGS=${qemu_sim_extra_args} -P ${CONFIGURE_FILE_SCRIPT}
            COMMAND chmod u+x "${sim_path}"
            VERBATIM COMMAND_EXPAND_LISTS
        )
        add_custom_command(
            OUTPUT "${gdb_path}"
            COMMAND
                ${CMAKE_COMMAND} -DCONFIGURE_INPUT_FILE=${GDB_SCRIPT}
                -DCONFIGURE_OUTPUT_FILE=${gdb_path} -DGDB_BINARY=${GdbBinary}
                -DQEMU_SIM_KERNEL_FILE=${KERNEL_IMAGE_NAME} -DQEMU_SIM_INITRD_FILE=${IMAGE_NAME} -P
                ${CONFIGURE_FILE_SCRIPT}
            COMMAND chmod u+x "${gdb_path}"
            VERBATIM COMMAND_EXPAND_LISTS
        )
    endif()
    add_custom_target(simulate_gen ALL DEPENDS "${sim_path}")
    add_custom_target(gdb_gen ALL DEPENDS "${gdb_path}")
endfunction(GenerateSimulateScript)

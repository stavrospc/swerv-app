# toolchain
# use a toolchain that is compiled with -enable-multilib
# (on riscv-gnu-toolchain: ./configure /path/to/output -enable-multilib)
TOOLCHAIN = riscv64-unknown-elf
GCC = $(TOOLCHAIN)-gcc
CPP = $(TOOLCHAIN)-cpp
AS = $(TOOLCHAIN)-as
LD = $(TOOLCHAIN)-ld
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump

# program creation flags
ABI = -mabi=ilp32 -march=rv32imc
MAIN = mc_smc
OUT_DIR = out
SRC_DIR = src

# platform creation flags
snapshot = swerv-app
SNAPSHOT_DIR = snapshots/${snapshot}
TBDIR = ${RV_ROOT}/testbench
TBFILES = vfiles/tb_top.sv $(TBDIR)/ahb_sif.sv
# uncomment for gtk waveforms (.vcd files)
VERILATOR_DEBUG = #--autoflush --trace
defines = ${SNAPSHOT_DIR}/common_defines.vh ${RV_ROOT}/design/include/swerv_types.sv
includes = -I${RV_ROOT}/design/include -I${RV_ROOT}/design/lib  -I${SNAPSHOT_DIR}
# Optimization for better performance; alternative is nothing for slower runtime (faster compiles)
# -O2 for faster runtime (slower compiles), or -O for balance.
VERILATOR_MAKE_FLAGS = OPT_FAST="-Os" # CPPFLAGS=-DVL_DEBUG

# ============================================================================ #

all: clean program.hex

whisper: clean
	$(MAKE) program.hex DEFINITIONS=-DIS_WHISPER 

# ============================================================================ #

verilator: clean-verilator ${TBFILES} ${SNAPSHOT_DIR}/defines.h ${RV_ROOT}/testbench/test_tb_top.cpp
	echo '`undef ASSERT_ON' >> ${SNAPSHOT_DIR}/common_defines.vh
	verilator --cc -CFLAGS  "-std=c++11" $(defines) $(includes) \
                -Wno-UNOPTFLAT \
                -I${RV_ROOT}/testbench \
		-f vfiles/flist.questa \
                ${TBFILES} \
                --top-module tb_top -exe ${RV_ROOT}/testbench/test_tb_top.cpp $(VERILATOR_DEBUG)
	cp ${RV_ROOT}/testbench/test_tb_top.cpp obj_dir
	$(MAKE) -j -C obj_dir/ -f Vtb_top.mk $(VERILATOR_MAKE_FLAGS)

# ============================================================================ #

program.hex: $(OUT_DIR)/$(MAIN).o $(OUT_DIR)/crt0.o
	@echo Building $(MAIN)
	mkdir -p $(OUT_DIR)
	$(GCC) $(ABI) $(DEFINITIONS) -o $(OUT_DIR)/$(MAIN).exe \
		-nostartfiles \
		-Wl,-Map=$(OUT_DIR)/$(MAIN).map -T$(SNAPSHOT_DIR)/link.ld -lgcc  \
		$(OUT_DIR)/crt0.o $(OUT_DIR)/$(MAIN).o 
	$(OBJCOPY) -O verilog $(OUT_DIR)/$(MAIN).exe program.hex
	$(OBJDUMP) -S $(OUT_DIR)/$(MAIN).exe > $(OUT_DIR)/$(MAIN).dis
	@echo Completed building $(MAIN)

$(OUT_DIR)/%.o : $(SRC_DIR)/%.c $(SNAPSHOT_DIR)/defines.h
	mkdir -p $(OUT_DIR)
	$(GCC) $(ABI) $(DEFINITIONS) -nostartfiles -g -I$(SNAPSHOT_DIR) -c $< -o $@

$(OUT_DIR)/%.o : $(SRC_DIR)/%.s $(SNAPSHOT_DIR)/defines.h
	mkdir -p $(OUT_DIR)
	$(CPP) -I$(SNAPSHOT_DIR) $(DEFINITIONS) -g $< > $(OUT_DIR)/$*.cpp.s
	$(AS) $(ABI) $(OUT_DIR)/$*.cpp.s -o $@

# ============================================================================ #

clean-all:
	rm -rf $(OUT_DIR) program.hex obj_dir *.log *.vcd *.csv

clean:
	rm -rf $(OUT_DIR) program.hex

clean-verilator:
	rm -rf obj_dir

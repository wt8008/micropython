include ../../py/mkenv.mk

CROSS = 1

# qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# include py core make definitions
include $(TOP)/py/py.mk

ifeq ($(CROSS), 1)
CROSS_COMPILE = arm-none-eabi-
endif

INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -IMAX3263X/Libraries/CMSIS/Device/Maxim/MAX3263X/Include
INC += -IMAX3263X/Libraries/CMSIS/Include
INC += -IMAX3263X/Libraries/MAX3263XPeriphDriver/Include
INC += -IMAX3263X/Libraries/Boards/Include
INC += -IMAX3263X/Libraries/Boards/Pegasus_V1/Include

ifeq ($(CROSS), 1)
DFU = $(TOP)/tools/dfu.py
PYDFU = $(TOP)/tools/pydfu.py
CFLAGS_CORTEX_M4 = 	-mthumb \
				   	-mcpu=cortex-m4 \
				   	-mfloat-abi=softfp \
				   	-mfpu=fpv4-sp-d16 \
				   	-Wa,-mimplicit-it=thumb \
				   	-ffunction-sections \
					-fdata-sections \
					-MD \
					-Wno-format \
					-c \
					-DTARGET_REV=0x4132 -DTARGET=MAX3263x -DRO_FREQ=96000000 -DMXC_ASSERT_ENABLE -g3 -ggdb -DDEBUG -nostdlib
CFLAGS = $(INC) -Wall -Werror -std=c99 $(CFLAGS_CORTEX_M4) $(COPT)
LDFLAGS = 	-T MAX3263X/Libraries/CMSIS/Device/Maxim/MAX3263X/Source/GCC/max3263x.ld --entry Reset_Handler \
			-mthumb \
		  	-mcpu=cortex-m4 \
		  	-mfloat-abi=softfp \
		  	-mfpu=fpv4-sp-d16 \
		  	-Xlinker --gc-sections \
		  	-Xlinker -Map\
			-Xlinker build/$@.map \
		   	-Xlinker -Lbuild
LD = gcc
else
LD = gcc
CFLAGS = -m32 $(INC) -Wall -Werror -std=c99 $(COPT)
LDFLAGS = -m32 -Wl,-Map=$@.map,--cref -Wl,--gc-sections
endif

# Tune for Debugging or Optimization
ifeq ($(DEBUG), 1)
CFLAGS += -O0 -ggdb
else
CFLAGS += -Os -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
endif

LIBS = -Xlinker --start-group -l :PeriphDriver.a -l :max3263x.a -lc -lm -lnosys -Xlinker --end-group 

SRC_C = \
	main.c \
	uart_core.c \
	lib/utils/stdout_helpers.c \
	lib/utils/pyexec.c \
	lib/libc/string0.c \
	lib/mp-readline/readline.c \
	$(BUILD)/_frozen_mpy.c \

OBJ = $(PY_O) $(addprefix $(BUILD)/, $(SRC_C:.c=.o))

ifeq ($(CROSS), 1)
all: $(BUILD)/firmware.dfu
else
all: $(BUILD)/firmware.elf
endif

library:
	TARGET=MAX3263x COMPILER=GCC make -C MAX3263X/Libraries/MAX3263XPeriphDriver
	mkdir -p build
	cp MAX3263X/Libraries/MAX3263XPeriphDriver/Build/PeriphDriver.a ./build/

board:
	make -C MAX3263X/Applications/EvKitExamples/Hello_World_MAX32630FTHR lib
	mkdir -p build
	cp MAX3263X/Applications/EvKitExamples/Hello_World_MAX32630FTHR/build/max3263x.a ./build/

$(BUILD)/_frozen_mpy.c: frozentest.mpy $(BUILD)/genhdr/qstrdefs.generated.h
	$(ECHO) "MISC freezing bytecode"
	$(Q)$(TOP)/tools/mpy-tool.py -f -q $(BUILD)/genhdr/qstrdefs.preprocessed.h -mlongint-impl=none $< > $@

$(BUILD)/firmware.elf: $(OBJ) library board
	$(ECHO) "LINK $@"
	$(Q)$(CROSS_COMPILE)$(LD) $(LDFLAGS) -o $@ $(OBJ) $(LIBS)
	$(Q)$(SIZE) $@

$(BUILD)/firmware.bin: $(BUILD)/firmware.elf
	$(Q)$(OBJCOPY) -O binary -j .isr_vector -j .text -j .data $^ $(BUILD)/firmware.bin

$(BUILD)/firmware.dfu: $(BUILD)/firmware.bin
	$(ECHO) "Create $@"
	$(Q)$(PYTHON) $(DFU) -b 0x08000000:$(BUILD)/firmware.bin $@

deploy: $(BUILD)/firmware.dfu
	$(ECHO) "Writing $< to the board"
	$(Q)$(PYTHON) $(PYDFU) -u $<

# Run emulation build on a POSIX system with suitable terminal settings
run:
	stty raw opost -echo
	build/firmware.elf
	@echo Resetting terminal...
# This sleep is useful to spot segfaults
	sleep 1
	reset

test: $(BUILD)/firmware.elf
	$(Q)/bin/echo -e "print('hello world!', list(x+1 for x in range(10)), end='eol\\\\n')\\r\\n\\x04" | $(BUILD)/firmware.elf | tail -n2 | grep "^hello world! \\[1, 2, 3, 4, 5, 6, 7, 8, 9, 10\\]eol"

include $(TOP)/py/mkrules.mk

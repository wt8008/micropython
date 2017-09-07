LIB_BLE=libmaxusb.a

OBJS=	\
	src/core/arm/usb.o \
	src/core/usb_event.o \
	src/enumerate/enumerate.o \
	src/devclass/hid_kbd.o \
	src/devclass/cdc_acm.o \
	src/devclass/fifo.o

INCLUDE_PATHS= \
	-I. \
	-Iinclude/core/arm \
	-Iinclude/core \
	-Iinclude/devclass \
	-Iinclude/enumerate \
	-I../Libraries/CMSIS/Device/Maxim/MAX32600/Include \
	-I../Libraries/CMSIS/Core/Include \
	-I../Libraries/MAX32600PeriphDriver/Include

DEFINE=-D__Vendor_SysTickConfig=1

DEPS=$(OBJS:.o=.d)

CFLAGS= $(DEFINE)            \
	$(INCLUDE_PATHS)     \
	-mthumb              \
	-mcpu=cortex-m3      \
	-Os                  \
	-ffunction-sections  \
	-fshort-wchar        \
	-fdata-sections      \
	-MD                  \
	-Wno-unused-function \
	-g 

CC=arm-none-eabi-gcc
AR=arm-none-eabi-ar
LIBS=libstacklib.a

$(LIB_BLE): Makefile $(OBJS)
	$(AR) -rcs $(LIB_BLE) $(OBJS)

clean:
	rm -rf $(OBJS) $(DEPS) $(LIB_BLE)

-include *.d

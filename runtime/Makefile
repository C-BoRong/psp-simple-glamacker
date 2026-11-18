TARGET = reader
OBJS = main.o storage.o ui.o ui_motion.o ui_effects.o gallery.o layout.o cnfont.o tiles.o trace.o story.o gpu_budget.o
CFLAGS = -O2 -G0 -Wall -Wextra -Werror -std=c99
BUILD_PRX = 1
PSP_LARGE_MEMORY = 0
PSP_FW_VERSION = 550
LDFLAGS += -Wl,--wrap=sceGuGetMemory -Wl,--wrap=sceGuDrawBufferList
LIBS = -lpspgu -lpspge -lpspdisplay -lpspctrl -lpspdebug -lpspaudio -lz -lm
include $(PSPSDK)/lib/build.mak

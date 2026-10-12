# POCKET RECON - GBDK-2020 build
#
# Set GBDK_HOME to where you unzipped GBDK-2020 (contains bin/, include/...).
# Linux/macOS:  make GBDK_HOME=~/gbdk/
# Windows:      see build.bat (simpler if you don't have `make`)

GBDK_HOME ?= /opt/gbdk/
LCC        = $(GBDK_HOME)bin/lcc

ROM     = pocketrecon.gb
SRCS    = main.c scan_mock.c          # <-- Stage 2: swap scan_mock.c -> scan_cart.c
OBJS    = $(SRCS:.c=.o)

# -Wl-j : target handheld   -Wm-yC : GBC-compatible header
LCCFLAGS = -Wa-l -Wl-j -Wm-yC

all: $(ROM)

%.o: %.c scan.h
	$(LCC) $(LCCFLAGS) -c -o $@ $<

$(ROM): $(OBJS)
	$(LCC) $(LCCFLAGS) -o $@ $(OBJS)

clean:
	rm -f *.o *.lst *.map *.sym *.ihx *.cdb *.adb *.rst *.rel $(ROM)

.PHONY: all clean

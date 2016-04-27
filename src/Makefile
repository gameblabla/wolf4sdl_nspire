CC = gcc
CXX = g++
LD = gcc

DATADIR   = ./

WOLF_VERSION = SPEAR_DEMO

CFLAGS += -Ofast -fdata-sections -ffunction-sections -marm -march=armv5te -mtune=arm926ej-s

ifdef DATADIR
CFLAGS += -DDATADIR=\"$(DATADIR)\"
endif

ifeq ($(WOLF_VERSION),SHAREWARE)
BINARY    = wolf3d_shareware
CFLAGS += -DCARMACIZED -DUPLOAD
else ifeq ($(WOLF_VERSION),ACTIVISION)
BINARY    = wolf3d_activision
CFLAGS += -DGOODTIMES -DCARMACIZED
else ifeq ($(WOLF_VERSION),APOGEE)
BINARY    = wolf3d_apogee
CFLAGS += -DCARMACIZED
else ifeq ($(WOLF_VERSION),SPEAR_FULL)
BINARY    = wolf3d_spear
CFLAGS += -DCARMACIZED -DSPEAR
else ifeq ($(WOLF_VERSION),SPEAR_DEMO)
BINARY    = wolf3d_speardemo
CFLAGS += -DCARMACIZED -DSPEAR -DSPEARDEMO
endif


CXXFLAGS += $(CFLAGS)

LDFLAGS += -lSDL -lm

SRCS :=
SRCS += id_ca.cpp
SRCS += id_in.cpp
SRCS += id_pm.cpp
SRCS += id_us_1.cpp
SRCS += id_vh.cpp
SRCS += id_vl.cpp
SRCS += signon.cpp
SRCS += wl_act1.cpp
SRCS += wl_act2.cpp
SRCS += wl_agent.cpp
SRCS += wl_atmos.cpp
SRCS += wl_cloudsky.cpp
SRCS += wl_debug.cpp
SRCS += wl_draw.cpp
SRCS += wl_floorceiling.cpp
SRCS += wl_game.cpp
SRCS += wl_inter.cpp
SRCS += wl_main.cpp
SRCS += wl_menu.cpp
SRCS += wl_parallax.cpp
SRCS += wl_play.cpp
SRCS += wl_state.cpp
SRCS += wl_text.cpp
SRCS += wl_shade.cpp

OBJS = $(filter %.o, $(SRCS:.c=.o) $(SRCS:.cpp=.o))

all: $(BINARY)

$(BINARY): $(OBJS)
	$(CXX) $(CFLAGS) $(OBJS) $(LDFLAGS) -o $@

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

.cpp.o:
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean distclean:
	@echo '===> CLEAN'
	rm -fr $(DEPS) $(OBJS) $(BINARY)

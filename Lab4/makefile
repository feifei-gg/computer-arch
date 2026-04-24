# -------- Pin configuration --------
PIN_ROOT := ../base/pin
ARCH     := intel64
OBJDIR   := ./

CXX      := $(PIN_ROOT)/$(ARCH)/pinrt/bin/pin-g++
CXXFLAGS := -std=c++17 \
            -Wall -Werror -Wno-unknown-pragmas \
            -fno-stack-protector -fno-exceptions \
            -funwind-tables -fasynchronous-unwind-tables \
            -fno-rtti -fPIC \
            -Wno-dangling-pointer -faligned-new \
            -O3 -fomit-frame-pointer -fno-strict-aliasing \
            -I$(PIN_ROOT)/source/include/pin \
            -I$(PIN_ROOT)/source/include/pin/gen \
            -isystem $(PIN_ROOT)/$(ARCH)/pinrt/include/adaptor \
            -I$(PIN_ROOT)/extras/components/include \
            -I$(PIN_ROOT)/extras/xed-$(ARCH)/include/xed \
            -I$(PIN_ROOT)/source/tools/Utils

LDFLAGS  := -shared \
            -Wl,-Bsymbolic \
            -Wl,--version-script=$(PIN_ROOT)/source/include/pin/pintool.ver \
            -L$(PIN_ROOT)/$(ARCH)/lib \
            -L$(PIN_ROOT)/extras/xed-$(ARCH)/lib

LDLIBS   := -lpin -lpinrt-adaptor-static -lxed -lpindwarf -ldwarf -lunwind-dynamic

# -------- Targets --------
TOOL     := address_translation
SRC      := address_translation.cpp
OBJ      := $(OBJDIR)/$(TOOL).o
SO       := $(OBJDIR)/$(TOOL).so

all: $(SO)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJ): $(SRC) | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(SO): $(OBJ)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)
	
clean:
	rm -rf *.o *.so

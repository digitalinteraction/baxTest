## Compile project BAXTest 

TARGET = BAXTest 
OBJDIR = obj

CC = gcc
CFLAGS = -g -Wall -Wno-attributes -Wno-unused-function $(OPTFLAGS)
LIBS = -lm

# OS Specific entries

ifeq ($(OS),Windows_NT)
  UNAME := $(OS)
else
  UNAME := $(shell uname -s)
endif

ifeq ($(UNAME),Linux)
  LIBS := -lm -lncurses 
else ifeq ($(UNAME),Darwin) # OSX
  LIBS := -lm -lncurses
else ifeq ($(UNAME),Windows_NT)
  LIBS := -lwsock32 -lcfgmgr32
endif



PATHS = BaxReceiver Common 
INC = -I. -ICommon -IBaxReceiver
VPATH = $(foreach path,$(PATHS),$(path):):							# Vpath sets where make looks for files (include the other directories)
HEADERS := $(wildcard *.h $(foreach path,$(PATHS),$(path)/*.h))
SOURCES := $(wildcard *.c $(foreach path,$(PATHS),$(path)/*.c))		# Sources also finds .c files in the include directory and compiles them
OBJECTS := $(patsubst %.c, $(OBJDIR)/%.o, $(notdir $(SOURCES)))
DIRS    := $(dir $(SOURCES))

# $(info --------------------------------------)
# $(info $$HEADERS are [${HEADERS}])
# $(info --------------------------------------)
# $(info ) 

# Make targets
.PHONY: clean all default
.PRECIOUS: $(TARGET) $(OBJECTS)

default: all
all: mkdir $(TARGET)
clean:
	-rm -rf obj/
	-rm -f $(TARGET)
mkdir:
	-mkdir -p obj

# Compile
$(OBJDIR)/%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) $(INC) -c $< -o $@

# Link
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) $(LIBS) -o $@


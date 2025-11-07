.PHONY: all clean

OUT_DIR := out
UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
  LIB_EXT := .so
  OS_COMPONENT := linux
else ifeq ($(UNAME), Darwin)
  LIB_EXT := .dylib
  OS_COMPONENT := macos
else
  LIB_EXT := .dll
  OS_COMPONENT := win32
endif

all: lib/esl_lib$(LIB_EXT) lib/esl_key_lib$(LIB_EXT)

lib:
	mkdir -p lib

esl_lib/$(OUT_DIR)/esl_lib.pdx.Makefile:
	slc generate esl_lib/esl_lib.slcp -d esl_lib/$(OUT_DIR) --with $(OS_COMPONENT)

esl_lib/$(OUT_DIR)/build/debug/esl_lib.pdx: esl_lib/$(OUT_DIR)/esl_lib.pdx.Makefile
	$(MAKE) -f esl_lib.pdx.Makefile -C esl_lib/$(OUT_DIR)

lib/esl_lib$(LIB_EXT): esl_lib/$(OUT_DIR)/build/debug/esl_lib.pdx lib
	cp esl_lib/$(OUT_DIR)/build/debug/esl_lib.pdx lib/esl_lib$(LIB_EXT)

esl_key_lib/$(OUT_DIR)/esl_key_lib.pdx.Makefile:
	slc generate esl_key_lib/esl_key_lib.slcp -d esl_key_lib/$(OUT_DIR) --with $(OS_COMPONENT)

esl_key_lib/$(OUT_DIR)/build/debug/esl_key_lib.pdx: esl_key_lib/$(OUT_DIR)/esl_key_lib.pdx.Makefile
	$(MAKE) -f esl_key_lib.pdx.Makefile -C esl_key_lib/$(OUT_DIR)

lib/esl_key_lib$(LIB_EXT): esl_key_lib/$(OUT_DIR)/build/debug/esl_key_lib.pdx lib
	cp esl_key_lib/$(OUT_DIR)/build/debug/esl_key_lib.pdx lib/esl_key_lib$(LIB_EXT)

clean:
	rm -rf lib
	rm -rf esl_lib/$(OUT_DIR)
	rm -rf esl_key_lib/$(OUT_DIR)

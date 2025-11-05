################################################################################
# BT Mesh Decoder Component                                                    #
################################################################################

override INCLUDEPATHS += \
$(SDK_DIR)/app/btmesh/common_host/btmesh_decoder \
$(SDK_DIR)/app/btmesh/common_host/btmesh_decoder/config

override CFLAGS += -DSL_CATALOG_BTMESH_DECODER_PRESENT

# mbedTLS (mbedcrypto) includes and libraries
ifeq ($(UNAME), darwin)
  MBEDTLS_PREFIX := $(shell brew --prefix mbedtls)
  override INCLUDEPATHS += $(MBEDTLS_PREFIX)/include
  override LDFLAGS += -L$(MBEDTLS_PREFIX)/lib
else
  override CFLAGS += -I/usr/include/mbedtls
endif

override LDFLAGS += -lmbedcrypto

override C_SRC += \
$(wildcard $(SDK_DIR)/app/btmesh/common_host/btmesh_decoder/*.c)
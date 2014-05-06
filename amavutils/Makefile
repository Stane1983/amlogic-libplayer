NAME = libamavutils

TARGET = libamavutils.so

TARGET_OBJS = itemlist.o amconfigutils.o amaudioutils.o amdisplayutils.o amsysfsutils.o amutils_common.o Amvideoutils.o

TARGET=libamavutils.so

LDFLAGS += -shared -lm -lrt -fPIC

TARGET_DIR=$(shell pwd)/../../../target
STAGING_DIR=$(shell pwd)/../../../staging

CFLAGS+=-O2
CFLAGS+=-I$(shell pwd) -I$(shell pwd)/include -I$(shell pwd)/../amcodec/include -I${STAGING_DIR}/include -I${STAGING_DIR}/usr/include -L${TARGET_DIR}/lib -L${TARGET_DIR}/usr/lib

export CC CFLAGS

all: $(TARGET) 

$(TARGET): $(TARGET_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS:%.o=%.c) -o $@

clean:
	-rm -rf *.o $(TARGET)

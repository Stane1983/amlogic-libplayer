TARGET = libamvdec.so

TARGET_OBJS = amlv4l.o
TARGET_OBJS += amvideo.o

TARGET_DIR=$(shell pwd)/../../../target
STAGING_DIR=$(shell pwd)/../../../staging

CFLAGS+=-O2
CFLAGS+=-I$(shell pwd) -I$(shell pwd)/include -I${STAGING_DIR}/include -I${STAGING_DIR}/usr/include -L${TARGET_DIR}/lib -L${TARGET_DIR}/usr/lib

export CC CFLAGS

all: $(TARGET) 

$(TARGET): $(TARGET_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS:%.o=%.c) -o $@  

clean:
	-rm -rf *.o $(TARGET)

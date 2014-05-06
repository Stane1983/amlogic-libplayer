M_PREFIX=$(PREFIX)

TARGET=libamadec.so

TARGET_OBJS=adec-external-ctrl.o adec-internal-mgt.o adec-ffmpeg-mgt.o adec-message.o adec-pts-mgt.o feeder.o adec_write.o adec_read.o\
           dsp/audiodsp-ctl.o audio_out/alsa-out.o audio_out/aml_resample.o audiodsp_update_format.o spdif_api.o pcmenc_api.o dts_transenc_api.o dts_enc.o 
TARGET=libamadec.so

INSTALL_DIR=${M_PREFIX}/lib

LDFLAGS += -shared -lpthread -lm -lasound -lrt -fPIC

TARGET_DIR=$(shell pwd)/../../../target
STAGING_DIR=$(shell pwd)/../../../staging
CFLAGS+=-O2
CFLAGS+=-I$(shell pwd) -I$(shell pwd)/include -I$(shell pwd)/../amavutils/include -I${STAGING_DIR}/include -I${STAGING_DIR}/usr/include -L${TARGET_DIR}/lib -L${TARGET_DIR}/usr/lib
CFLAGS+=-DALSA_OUT -DENABLE_WAIT_FORMAT

export CC CFLAGS

all: $(TARGET) 

$(TARGET): $(TARGET_OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(TARGET_OBJS:%.o=%.c) -o $@  

install:$(TARGET)
	-install -m 555 ${TARGET} $(INSTALL_DIR)

force:

clean:
	-rm -rf *.o ${AMADEC_DIST} ${MP3LIB} $(PCMLIB) $(ADPCMLIB) $(AACLIB) $(A52LIB)

CC=$(CROSS)gcc
STRIP=$(CROSS)strip
DATE=$(shell date "+%Y%m%d-%H%M")

LDFLAGS=
CFLAGS= -Wno-deprecated -DBNO=\"$(DATE)\"

FFLIBS=-L../../lib/ffmpeg/lib -lm -lpthread -lrt -lc -lavcodec -lavdevice -lavfilter -lavformat -lavutil -lswresample -lswscale
RTSPLIB=#-leasyrtspclient -lsrsrtmp

INC+= -I../../lib -I../../lib/inc -I../../lib/ffmpeg/include

LIBS=-L../../lib -lcfgfile -llogfile -ljson -lsock -lvos -lm -lpthread -lrt  $(FFLIBS) $(RTSPLIB) -lsqlite3

EXEC = Transcoder
#OBJS = rtxpmain.o utils.o rtspclient.o
OBJS = transmain.o utils.o ffmpeg.o auth.o

all: $(EXEC)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LIBS)
	$(STRIP) $(EXEC)
$(OBJS):%.o: %.c
	$(CC) $(INC) $(CFLAGS) $< -c

clean:
	rm -rf $(OBJS) $(EXEC)

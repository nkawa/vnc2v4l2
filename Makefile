

#CC = clang++-18 
CC = gcc
CFLAGS  = -v
#CFLAGS  = -I../libvncserver/build/include -I../libvncserver/include
#LDFLAGS = -L../libvncserver/build
LIBS = -lvncclient -v -ljpeg
LDFLAGS = -L/lib/x86_64-linux-gnu
OBJS = vnc2v4l2.o
TARGET = vnc2v4l2

$(TARGET) : $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $(TARGET)


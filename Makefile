OBJECTS := print.o detour.o config.o lcfg_static.o gtv_logger.o adv.o

CC := gcc
CFLAGS := -Wall# -DDEBUG
LFLAGS := -lpthread

OUTPUT := gtv_logger.so

$(OUTPUT): $(OBJECTS)
	$(CC) $(CFLAGS) $(LFLAGS) $(OBJECTS) -fPIC -shared -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o *.so

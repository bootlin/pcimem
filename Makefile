CFLAGS := -Wall -Werror -Wextra

all: pcimem

pcimem_cmdline.c: pcimem.ggo 
	gengetopt -F pcimem_cmdline -f pcimem_cmdline -a pcimem_args_info  < $<

pcimem_cmdline.o: pcimem_cmdline.c pcimem_cmdline.h

pcimem: pcimem_cmdline.o pcimem.o
	$(CC) -o $@ $^ $(CFLAGS)

clean:
	rm -rf *.o pcimem pcimem_cmdline.c pcimem_cmdline.h

obj = main.o
target = hello-world

.PHONY: all clean

all: $(target)

$(target): $(obj)
	$(CC) $(CFLAGS) $(obj) -o $(target) $(LDFLAGS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(obj) $(target)

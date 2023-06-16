obj = main.o
target = hello-world

all: $(obj)
	${CC} $(obj) -o $(target)

%.o:%.c
	${CC} -c $^ -o $@

.PHONY: clean
clean:
	rm -rf $(obj) $(target)


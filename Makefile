UDEVFILE=bulbctrl.rules

obj-m+=bulbctrl.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	gcc -Wall -Wextra -Wpedantic togglebulb.c -o togglebulb
install:
	cp ${UDEVFILE} /etc/udev/rules.d && chown pi:pi /etc/udev/rules.d/${UDEVFILE}
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f togglebulb *~

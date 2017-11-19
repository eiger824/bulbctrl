UDEVFILE=bulbctrl.rules
BUILDCFLAGS=-Wall -Wextra -Wpedantic --std=c11

obj-m+=bulbctrl.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	gcc ${BUILDCFLAGS} togglebulb.c -o togglebulb
	gcc ${BUILDCFLAGS} togglebulb-server.c -o togglebulb-server
install:
	cp ${UDEVFILE} /etc/udev/rules.d && chown pi:pi /etc/udev/rules.d/${UDEVFILE}
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm -f togglebulb togglebulb-server *~

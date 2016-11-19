obj-m += openflowclienttest4.o
openflowclienttest4-objs := ofc_main.o ofc_util.o ofc_data.o ofc_cntrl.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

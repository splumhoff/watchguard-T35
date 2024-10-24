
obj-m	:= qat_mem.o


default: clean
	make -C $(KERNEL_SOURCE_ROOT) M=$(PWD) modules


clean:
	/bin/rm -rf *.ko *.o

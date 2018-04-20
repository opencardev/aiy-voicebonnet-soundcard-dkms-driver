obj-m := snd-aiy-voicebonnet.o snd-soc-bcm2835-i2s.o rt5645.o rl6231.o
KVERSION := $(shell uname -r)

all:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) modules

clean:
	$(MAKE) -C /lib/modules/$(KVERSION)/build M=$(PWD) clean


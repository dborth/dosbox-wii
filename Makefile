.PHONY = all wii wii-clean

all: wii

clean: wii-clean

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

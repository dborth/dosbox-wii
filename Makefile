.PHONY = all wii wii-clean wii-run wiiu wiiu-clean wiiu-run

all: wii wiiu

run: wii-run wiiu-run

clean: wii-clean wiiu-clean

wii:
	$(MAKE) -f Makefile.wii

wii-clean:
	$(MAKE) -f Makefile.wii clean

wii-run: wii
	$(MAKE) -f Makefile.wii run

wiiu:
	$(MAKE) -f Makefile.wiiu

wiiu-clean:
	$(MAKE) -f Makefile.wiiu clean

wiiu-run: wiiu
	$(MAKE) -f Makefile.wiiu run

.PHONY: all clean

all: cmu

clean:
	@rm -f *~ cmu

cmu: cmu.c
	gcc -O2 -fomit-frame-pointer $< -o $@

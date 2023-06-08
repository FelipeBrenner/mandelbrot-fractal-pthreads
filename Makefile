clean:
	rm main

build: src/main.c
	gcc src/main.c -o main -I/opt/X11/include -I/opt/X11/include/X11 -L/opt/X11/lib -L/opt/X11/lib/X11 -lX11

run: build
	./main

run-coordinates: build
	./main -2 0 -1 1

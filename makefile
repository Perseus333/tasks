main: main.c
	mkdir -p build
	gcc main.c -o build/tt
	chmod +x ./build/tt
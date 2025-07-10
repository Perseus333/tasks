main: main.c
	mkdir -p build
	gcc main.c -o build/tasks
	chmod +x ./build/tasks
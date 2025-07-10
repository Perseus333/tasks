all: tasks

tasks: 	main.c
	mkdir -p build
	gcc main.c -Wall -o build/tasks
	chmod +x ./build/tasks

install: tasks
	sudo cp ./build/tasks /usr/local/bin

clean:
	rm -r ./build
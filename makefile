all: tt

tt: 	main.c
	mkdir -p build
	gcc main.c -o build/tt
	chmod +x ./build/tt

install: tt

	sudo cp ./build/tt /usr/local/bin

clean:
	rm -r ./build
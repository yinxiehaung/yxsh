all:
	gcc build.c src/*.c -Iinclude -Wall -g -o build	
clean:
	rm -f build yxsh

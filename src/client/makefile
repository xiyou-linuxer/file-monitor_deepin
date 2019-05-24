inotify:inotify.cpp
	 g++ inotify.cpp -g -lpthread -o inotify -lrt
testp.so:testp.c
	gcc -shared -fPIC testp.c -o testp.so -ldl -lpthread -lrt 
clean:
	-rm -rf inotify.o

#this will be updated as we add other files ( if needed )

fs: main.c
	gcc -o fs main.c
	cp ../fat32-original.img fat32.img

clean:
	rm fs

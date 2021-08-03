# opsys-proj3: fat32 file system utility


### Contents

- main.c - driver program, parses input, shell utility

- fat.h - FAT32 structs and functions

- Makefile

### How to compile 
```
make
fs <fat image>
```

## How to clean
make clean


## Bugs/unfinished portions
  
+ parts 10 and 11: Read and Write commands handle error checking, 
  but need to implement reading from file and writing to file

+ part 7: mkdir creates a new directory and attempts to make the
	  parent and current directories ".." and "." in the 
	  created directory, but it is not implemented correctly 
	- If you cd into the newly created directory and use 
	  command ls, nothing lists and the program becomes 
	  unresponsive and it must be exited manually

+ Filename/directory name arguments will match directory contents if the first characters match. 
	- For example ``` size longfil ``` will match with longfile


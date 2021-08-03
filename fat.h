#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

typedef struct BPB {

        uint16_t bytesPerSector;        //offset: 11 bytes, size: 2 bytes
        uint8_t sectorsPerCluster;      //offset: 13, size: 1
        uint16_t reservedSectorCount;   //offset: 14, size: 2
        uint8_t numFATs;                //offset: 16, size: 1
        uint32_t totalSectors32;        //offset: 32, size: 4
        uint32_t FATSz32;               //offset: 36, size: 4
        unsigned int rootCluster;       //offset: 44, size: 4

        //FirstDataSector = BPB_RsvdSectCnt + (BPB_NumFATs*BPB_FatSz32)
        uint32_t FirstDataSector;

} __attribute__((packed)) BPB;

typedef struct Directory{
        char DIR_Name[32];
        unsigned char DIR_Attr;
        unsigned int DIR_FstClus;
        unsigned int DIR_FileSize;
        unsigned int DIR_EntryByteAddress;
        unsigned char END_OF_ARRAY;

} __attribute__((packed)) Directory;

struct OpenFileList{
        char filename[32];
        char mode[2];
        struct OpenFileList * next;
} __attribute__((packed)) OpenFileList;

struct OpenFileList * root = NULL;

Directory * fillDir(unsigned int cluster);
void writeDirToImage(Directory dir, int cluster);

uint32_t RootDirAddr = 0;
uint32_t CurrentAddr = 0; //current location
unsigned int currentClusterNumber = 2, prevClusterNumber = 2;

unsigned int startOfDataRegion;
unsigned int endOfDataRegion;

FILE *fatImage;
char pwd[80] = "/";
BPB bs;
Directory dir[16];

//RootDirAddr = (bs.numFATs*bs.FATSz32*bs.bytesPerSector)+(bs.reservedSectorCount*bs.bytesPerSector);

int FindClusterAddress(int32_t n){
        if (n==0) return RootDirAddr;
        //FirstDataSector = BPB_RsvdSectCnt + (BPB_NumFATs*BPB_FatSz32)
        bs.FirstDataSector = bs.reservedSectorCount + (bs.numFATs*bs.FATSz32);
        // printf("first data sec: %d\n", bs.FirstDataSector);
        //FirstSectorofCluster = FirstDataSector + ((N - 2)*BPB_SecPerClus)
        // x = bs.FirstDataSector + ((n-2)*bs.sectorsPerCluster);
        // printf("sec per clus: %d\n", bs.sectorsPerCluster);
        //return RootDirAddr + ((n-2)*bs.sectorsPerCluster);
        // printf("new clus address: %d\n", x);
        // x = (bs.bytesPerSector * bs.reservedSectorCount)
        //      + ((n - 2) * bs.bytesPerSector)
        //      + (bs.bytesPerSector * bs.numFATs * bs.FATSz32);
        return (bs.bytesPerSector * bs.reservedSectorCount)
                + ((n - 2) * bs.bytesPerSector)
                + (bs.bytesPerSector * bs.numFATs * bs.FATSz32);
}
int FindSectorofCluster(int clus){
        return (((clus-2)*bs.sectorsPerCluster)+bs.FirstDataSector)
                *bs.bytesPerSector;
}
unsigned int LittleToBigEndian(unsigned char * x, int num){
        unsigned int y = 0;
        int i = 0;
        while(i < num) {
                y += (unsigned int)(x[i])<<(i*8);
                i++;
        }
        return y;
}
unsigned char* BigToLittleEndian(unsigned int value, unsigned int size){
        static unsigned char y[4];
        memset(y, 0, 4);
        unsigned int i = 0;
        unsigned int mask = 0x000000FF;
        unsigned int temp = value;
        while(i < size) {
                y[i] = temp&mask;
                temp = temp >> 8;
                i++;
        }
        return y;
}
int FindFreeCluster(void){
        unsigned int value;
        unsigned char dataRead[4];
        unsigned int pos = startOfDataRegion;
        unsigned int num = 0;
        do{
                fseek(fatImage, pos, SEEK_SET);
                fread(dataRead, sizeof(char), 4, fatImage);
                value = LittleToBigEndian(dataRead, 4);
                pos += 4;
                ++num;
        }while(value != 0x00000000 || pos >= endOfDataRegion);
        num--;
        return num;
}
unsigned int FindNextCluster(unsigned int currCluster) {
        unsigned char temp1[4];
        unsigned int temp = startOfDataRegion + currCluster*4;
        fseek(fatImage,temp,SEEK_SET);
        fread(temp1,sizeof(unsigned char),4,fatImage);
        return LittleToBigEndian(temp1,4);
}
int ValidFile(Directory * d){
        int i;
        for(i=0; i<16; i++){
                if(d[i].DIR_Attr == 1 ||d[i].DIR_Attr == 16||d[i].DIR_Attr == 32 ){
                        return 1;
                }
                else return 0;
        }
}
void WriteToImage(unsigned int cluster, unsigned int next){
        unsigned int offset1 = startOfDataRegion+cluster*4,
                     offset2 = endOfDataRegion+cluster*4;
        fseek(fatImage, offset1, SEEK_SET);
        unsigned char* tmp = BigToLittleEndian(next, 4);
        int i = 0;
        while(i < 4) {
              fputc(tmp[i], fatImage);
              i++;
        }
        fseek(fatImage, offset2, SEEK_SET);
        unsigned char* tmp2 = BigToLittleEndian(next, 4);
        int j = 0;
        while(j < 4) {
              fputc(tmp2[j], fatImage);
              j++;
        }
}
void removeFromImage(unsigned int clus){
        unsigned int totalclusters[bs.totalSectors32/2];
        unsigned short i = 0;
        int j = i-1;
        unsigned int temp = clus;
        do {
                totalclusters[i] = temp;
                temp = FindNextCluster(temp);
                i++;
        }
        while(temp < 0x0FFFFFF8);

        while(j >= 0) {
                unsigned int addr = FindSectorofCluster(totalclusters[j]);
                unsigned int totalbytes = bs.bytesPerSector*bs.sectorsPerCluster;
                fseek(fatImage, addr, SEEK_SET);
                unsigned int k = 0;
                while(k < totalbytes) {
                      fputc('\0', fatImage);
                      k++;
                }
                WriteToImage(totalclusters[j], 0);
                j--;
        }

}
void AddOpenFile(char* fileName,char* mode){
        if(CheckIsOpen(fileName, mode))
                printf("File is already open.\n");
        else{
                struct OpenFileList * tempFileList = calloc(1, sizeof(struct OpenFileList));
                strcpy(tempFileList->filename, fileName);
                strcpy(tempFileList->mode, mode);
                tempFileList->next = NULL;
                if(root == NULL){

                        root = tempFileList;
                }
                else{
                        struct OpenFileList * itrList = root;
                        while(itrList->next != NULL){
                                itrList = itrList->next;
                        }
                        itrList->next = tempFileList;
                }
        }
}

void RemoveFile(const char* fileName){
        struct OpenFileList * temp1;

        struct OpenFileList * temp2;

        temp1 = root;
        temp2 = NULL;

        while(temp1 != NULL){
                if(strcmp(temp1->filename, fileName) == 0){
                        if(temp2 == NULL){
                                root = temp1->next;
                        }
                        else{
                                temp2->next = temp1->next;
                        }
                        //free(temp1);
                        return;
                }
                temp2 = temp1;
                temp1 = temp1->next;
        }
}
int CheckIsOpen(const char* fileName){
        struct OpenFileList * temp;
        temp = root;
        while(temp!=NULL){
                if((strncmp(fileName, temp->filename, strlen(fileName)) == 0)){
                        return 1;

                }
                temp = temp->next;
        }
        return 0;
}
char* upperstr(char * str){
        unsigned char *p = (unsigned char *)str;
        while(*p){
                *p = toupper((unsigned char)*p);
                p++;
        }
        return str;
}
void openImageFileInit(char* fname){
        fatImage = fopen(fname, "r+b");
        if (fatImage == NULL){
                printf("error in opening file\n");
                exit(1);
        }
        fseek(fatImage, 11, SEEK_SET);
        fread(&bs.bytesPerSector, 1, 2, fatImage);
        fread(&bs.sectorsPerCluster, 1, 1, fatImage);
        fread(&bs.reservedSectorCount, 1, 2, fatImage);
        fread(&bs.numFATs, 1, 1, fatImage);
        fseek(fatImage, 32, SEEK_SET);
        fread(&bs.totalSectors32, 1, 4, fatImage);
        fread(&bs.FATSz32, 1, 4, fatImage);
        fseek(fatImage, 44, SEEK_SET);
        fread(&bs.rootCluster, 1, 4, fatImage);

        RootDirAddr = (bs.numFATs*bs.FATSz32*bs.bytesPerSector)+(bs.reservedSectorCount*bs.bytesPerSector);
        bs.FirstDataSector = bs.reservedSectorCount + (bs.numFATs*bs.FATSz32);
        printf("root addr: %x\n", RootDirAddr);
        CurrentAddr = RootDirAddr;
        startOfDataRegion = (bs.reservedSectorCount + (bs.rootCluster*4)/bs.bytesPerSector) * bs.bytesPerSector;
        currentClusterNumber = bs.rootCluster;
        endOfDataRegion = bs.FATSz32 * bs.bytesPerSector + startOfDataRegion;
        //fillDirectory(CurrentAddr, dir);
        //fillDir(bs.rootCluster);

}
void displayInfo(){
        printf("bytes per sec: %d\n", bs.bytesPerSector);
        printf("sectors per cluster: %d\n", bs.sectorsPerCluster);
        printf("reserved sector count: %d\n", bs.reservedSectorCount);
        printf("number of FATs: %d\n", bs.numFATs);
        printf("total sectors: %d\n", bs.totalSectors32);
        printf("FAT size: %d\n", bs.FATSz32);
        printf("root cluster: %x\n", bs.rootCluster);
}
void displaySize(char * name){
        Directory * temp = fillDir(currentClusterNumber);
        int i=0;
        while (temp[i].END_OF_ARRAY == 0){
                if(!strncmp(temp[i].DIR_Name, name, strlen(name))){
                        if (temp[i].DIR_Attr == 16){
                                printf("Error: directory\n");
                                return;
                        }
                        printf("File size: %d bytes\n", temp[i].DIR_FileSize);
                        return;
                }
                i++;
        }
        printf("Error: file does not exist\n");
}
Directory * fillDir(unsigned int cluster){

        static Directory temp[1024];
        unsigned int i = 0;
        char readCluster[32];
        unsigned int nextcluster = cluster;

        do {
                unsigned int index = FindSectorofCluster(nextcluster);
                unsigned int offset = 0;
                do {
                        fseek(fatImage, index + offset, SEEK_SET);
                        fread(readCluster, sizeof(char), 32, fatImage);
                        if (readCluster[0] == 0x00 || (unsigned char)readCluster[0] == 0xE5) {
                                offset += 32;
                                continue;
                        }
                        else {
                                if (readCluster[11] & 0x0F){
                                }
                                else {
                                        int j = 0;
                                        while(j < 11) {
                                                temp[i].DIR_Name[j] = readCluster[j];
                                                j++;
                                        }
                                        temp[i].DIR_Name[11] = '\0';
                                        temp[i].DIR_Attr = readCluster[11];
                                        unsigned char tempArray[4];
                                        tempArray[0] = readCluster[26];
                                        tempArray[1] = readCluster[27];
                                        tempArray[2] = readCluster[20];
                                        tempArray[3] = readCluster[21];
                                        temp[i].DIR_FstClus = LittleToBigEndian(tempArray, 4);
                                        temp[i].DIR_FileSize = LittleToBigEndian(readCluster+28, 4);
                                        temp[i].DIR_EntryByteAddress = index+offset;
                                        temp[i++].END_OF_ARRAY = 0;
                                }
                        }
                        offset += 32;
                } while (readCluster[0] != 0x00 && offset < (bs.bytesPerSector*bs.sectorsPerCluster));
                nextcluster = FindNextCluster(nextcluster);
        } while (nextcluster < 0x0FFFFFF8);
        temp[i].END_OF_ARRAY = 1;
        return temp;
}
void listCurDirectory(){
        Directory * temp = fillDir(currentClusterNumber);
        int i = 0;
        while(!temp[i].END_OF_ARRAY){
                char name[32];
                memset(name, '\0', 32);
                strcpy(name, temp[i].DIR_Name);
                printf("%s\n", name);
                i++;
        }
}
void listDirectory(char * path){
	int i;
	int flag = 0;
	Directory tempdir[16];
	int32_t tempAddr = CurrentAddr;

	if(!strcmp(path, ".")){
		listCurDirectory();
		return;
	}
	else if (!strcmp(path, "..")){
		if (CurrentAddr == RootDirAddr){
			flag = 1;
			printf("You are in the root directory\n");
		}
		else{
			Directory * temp = fillDir(prevClusterNumber);
		}
	}
	else{
                Directory * temp = fillDir(currentClusterNumber);
                while(!temp[i].END_OF_ARRAY){
                        if (!strncmp(temp[i].DIR_Name, path, strlen(path)) && temp[i].DIR_Attr == 16){
                                flag = 1;

                                Directory * temp2 = fillDir(temp[i].DIR_FstClus);
                                int j = 0;
                                while(!temp2[j].END_OF_ARRAY){
                                        char name[32];
                                        memset(name, '\0', 32);
                                        strcpy(name, temp2[j].DIR_Name);
                                        printf("%s\n", name);
                                        j++;
                                }
                        }
                        i++;
                }		
	}
	if (flag == 0) printf("Directory does not exist\n");

}

void changeDirHome(){                   //just goes to home/root directory
        CurrentAddr = RootDirAddr;
        currentClusterNumber = bs.rootCluster;
        Directory * temp = fillDir(bs.rootCluster);
}
void changeDirectory(char * path){
        int i = 0;

        int flag = 0;

        Directory * tempdir = fillDir(currentClusterNumber);

        int32_t tempAddr = CurrentAddr;
        if (!strcmp(path, "..")){
                if (currentClusterNumber == bs.rootCluster){
        	        flag = 1;
		        printf("You are in the root directory\n");
                }
                else{

		        currentClusterNumber = prevClusterNumber;


                        flag =1 ;
                }	

        }
        else if (!strcmp(path, ".")){flag = 1;}
        else{

                while(tempdir[i].END_OF_ARRAY == 0){

                        if (!strncmp(tempdir[i].DIR_Name, path, strlen(path)) && tempdir[i].DIR_Attr == 16){

		                flag = 1;

                                prevClusterNumber = currentClusterNumber;
        
	                        currentClusterNumber = tempdir[i].DIR_FstClus;

	                }

                        i++;
                }

        }

        if (flag == 0) printf("Directory does not exist\n");

}
void creat(char * name){

        int i = 0;
        Directory * temp = fillDir(currentClusterNumber);
        Directory newFile;
        while(temp[i].END_OF_ARRAY == 0){

                if (!strncmp(temp[i].DIR_Name, name, strlen(name))){
                        printf("Error: file already exists\n");
                        return;
                }
                i++;
        }
        newFile.DIR_Attr = 0;
        strcpy(newFile.DIR_Name, name);
        unsigned int firstfree = FindFreeCluster();
        if(firstfree == 0xFFFFFFFF) return;
        WriteToImage(firstfree, 0x0FFFFFF8);
        newFile.DIR_FstClus = firstfree;
        newFile.DIR_FileSize = 0x0000;
        writeDirToImage(newFile, currentClusterNumber);
}
void rm(char * name){
        Directory* temp = fillDir(currentClusterNumber);
        unsigned int i = 0;
        int found = 0;
	
//*******************************************ERROR HANDLING*************************************************

	//------------------------Check if given name is a directory-------------------------
	upperstr(name);
        while (temp[i].END_OF_ARRAY == 0) {
                if(strncmp(temp[i].DIR_Name, name, strlen(name)) == 0){
                        if (temp[i].DIR_Attr & 0x10){
                                printf("Error: Cannot use \"rm\" to remove a directory.\n");
                                return;
                        }
                        else {
                                found = 1;
				break;
                        }
                }
                i++;
        }
	//-----------------------Check if given file is in directory-----------------------
        if(found == 0){
                printf("Error: File not found in current directory.\n");
                return;	
	}

        fseek(fatImage, temp[i].DIR_EntryByteAddress, SEEK_SET);
        fputc(0xE5, fatImage);
        removeFromImage(temp[i].DIR_FstClus);
}
int FindFreeDir(int cluster){
	unsigned int curr = cluster, prev = cluster, new;
	do {
		int start = FindSectorofCluster(curr), offset = 0;
		char read[1];
		do {
			fseek(fatImage, start+offset, SEEK_SET);
			fread(read, sizeof(char), 1, fatImage);
			offset += 32;
		} while(*read != 0xE5 && *read != 0x00 && offset < bs.bytesPerSector*bs.sectorsPerCluster);
		if (*read == 0xE5 || *read == 0x00) return start+offset-32;
		prev = curr;
		curr = FindNextCluster(curr);

	} while(curr < 0x0FFFFFF8);
	new = FindFreeCluster();
	WriteToImage(prev, new);
	WriteToImage(new, 0x0FFFFFF8);
	return FindSectorofCluster(new);
}
void writeDirToImage(Directory dir, int cluster){
        unsigned int offset = FindFreeDir(cluster);
        int i = 0;

        while(i < 11) {
                fseek(fatImage, offset+i, SEEK_SET);

                fputc(dir.DIR_Name[i], fatImage);

                i++;
		
        }
        unsigned int DIR_FstClusLO = dir.DIR_FstClus&0x00FF;

        unsigned int DIR_FstClusHI = dir.DIR_FstClus>>16;

        fseek(fatImage, offset+26, SEEK_SET);
        unsigned char* temp = BigToLittleEndian(DIR_FstClusLO, 2);
        i = 0;
        while(i < 2) {
                fputc(temp[i], fatImage);
                i++;
        }
        fseek(fatImage, offset+20, SEEK_SET);
        temp = BigToLittleEndian(DIR_FstClusHI, 2);
        i = 0;
        while(i < 2) {
                fputc(temp[i], fatImage);
	        i++;
        }
        fseek(fatImage, offset+11, SEEK_SET);
        fputc(dir.DIR_Attr, fatImage);
        fseek(fatImage, offset+28, SEEK_SET);
        temp = BigToLittleEndian(dir.DIR_FileSize, 4);
        i = 0;
        while(i < 4) {
              fputc(temp[i], fatImage);
	      i++;

        }

}
void mkdir(char * name){
	int i = 0;
	Directory * temp = fillDir(currentClusterNumber);
	Directory newDir;
//*******************************************ERROR HANDLING*************************************************
	
	while(temp[i].END_OF_ARRAY == 0){
		if (!strncmp(temp[i].DIR_Name, name, strlen(name))){
			if(temp[i].DIR_Attr == 16){
				printf("Error: directory already exists\n");
				return;
			}
		}
		i++;
	}
//****************************************ERROR HANDLING DONE**********************************************

        newDir.DIR_Attr = 16;
        strcpy(newDir.DIR_Name, name);

        unsigned int firstfree = FindFreeCluster();
        WriteToImage(firstfree, 0x0FFFFFF8);
        newDir.DIR_FstClus = firstfree;
        newDir.DIR_FileSize = 0x0000;

        writeDirToImage(newDir, currentClusterNumber);
        strcpy(newDir.DIR_Name, ".          ");
        writeDirToImage(newDir, firstfree);

        strcpy(newDir.DIR_Name, "..         ");
	firstfree = FindFreeCluster();
        newDir.DIR_FstClus = currentClusterNumber;
        writeDirToImage(newDir, firstfree);
}





void rm_dir(char * name){
        int i = 0;
	upperstr(name);
	int found = 0;
	char * parentDir = "..";
        Directory * temp = fillDir(currentClusterNumber);

//*******************************************ERROR HANDLING*************************************************
	
	//-------------------check if value passed in is not a directory-------------------
        while(temp[i].END_OF_ARRAY == 0){
	        if (!strncmp(temp[i].DIR_Name, name, strlen(name))){
			if (!(temp[i].DIR_Attr & 0x10)){
				printf("Error: Not a directoy.\n");
				found = 0;
				return;
			}
			found = 1;
                }
                i++;

        }
	//-------------------check if directory doesn't exist------------------
	if(found == 0){
		printf("Error: Directory does not exist.\n");
		return;
	}


	//-------------------check if the directory has files-------------------
	changeDirectory(name);				//change directory to directory named name to see if empty
	temp = fillDir(currentClusterNumber);
	for(i = 0; temp[i].END_OF_ARRAY == 0; i++);	//counts number of files in directory
	if(i >= 3){
		printf("Error: Directory is not empty.\n");
		changeDirectory("..");		//go back to parent directory
		return;					//can't do exit(1) because it exits entire program and executable
	}
	changeDirectory(parentDir);
//****************************************ERROR HANDLING DONE**********************************************
	temp = fillDir(currentClusterNumber);
        fseek(fatImage, temp[i].DIR_EntryByteAddress, SEEK_SET);
        fputc(0xE5, fatImage);
        removeFromImage(temp[i].DIR_FstClus);

//---------------------Now we remove the directory-----------------------

}
void open(char * name, char * mode){
	if((strcmp(mode, "r") != 0) && (strcmp(mode, "w") !=0 ) && (strcmp(mode, "rw") != 0) && (strcmp(mode, "wr")!=0)){
		
		printf("Error: Invalid mode.\n");
		return;
	}
	else if (CheckIsOpen(name) == 1){
		printf("File has already been opened.\n");
		return;
	}
	else{
		
		Directory* tempDir = fillDir(currentClusterNumber);
		upperstr(name);	
		unsigned int i = 0;

		while (tempDir[i].END_OF_ARRAY == 0) {
			if(strncmp(tempDir[i].DIR_Name, name, strlen(name)) == 0){
		     		if (tempDir[i].DIR_Attr & 0x10){
		 	  		printf("Error: Cannot open a directory\n");
		 		}
		 		else {
		 	  		AddOpenFile(name, mode);
		 		}
		 		return;
		 	}
		   	i++;
		 }
		 printf("Error: File not found in current directory.\n");
	}


}

void closefile(char * name){
        upperstr(name);
        if (CheckIsOpen(name))
                RemoveFile(name);
        else
                printf("File is not open.\n");
}

//----------------------------------------Check is open if read or write----------------------------------------
int CheckIsOpenR(const char* fileName){
        struct OpenFileList * temp;
        temp = root;
        while(temp!=NULL){
                if((strncmp(fileName, temp->filename, strlen(fileName)) == 0)
                        && ( strcmp(temp->mode, "r") == 0|| strcmp(temp->mode, "rw") == 0||
                                strcmp(temp->mode, "wr") == 0)){
                        return 1;

                }
                temp = temp->next;
        }
        return 0;
}

int CheckIsOpenW(const char* fileName){
        struct OpenFileList * temp;
        temp = root;
        while(temp!=NULL){
                if((strncmp(fileName, temp->filename, strlen(fileName)) == 0)
                        && ( strcmp(temp->mode, "w") == 0|| strcmp(temp->mode, "rw") == 0||
                                strcmp(temp->mode, "wr") == 0)){
                        return 1;

                }
                temp = temp->next;
        }
        return 0;
}

//------------------------------------------------------------------------------------------------------------

void readfile(char * name, int offset, int size){
	Directory* tempDir = fillDir(currentClusterNumber);
	upperstr(name);
	unsigned int i = 0;
	int found = 0;		


//*******************************************ERROR HANDLING*************************************************
	while (tempDir[i].END_OF_ARRAY == 0) {
		if(strncmp(tempDir[i].DIR_Name, name, strlen(name)) == 0){
			if (tempDir[i].DIR_Attr & 0x10){
				printf("Error: Cannot read a directory.\n");
				return;
			}
			else {
				found = 1;
				if(tempDir[i].DIR_FileSize < offset){
					printf("Error: Cannot read file because offset is larger than size of file.\n");
					return;
				}
			}
			//return;
		}
		i++;
	}
	if(found == 0){
		printf("Error: File not found in current directory.\n");
		return;
	}
	if(CheckIsOpenR(name) == 0){
		printf("Error: The file cannot be read because it is not open or because it is not open in read mode.\n");
		return;
	}
//****************************************ERROR HANDLING DONE**********************************************

}


void writefile(char * name, int offset, int size, char * str){
        Directory* tempDir = fillDir(currentClusterNumber);
        upperstr(name);
        unsigned int i = 0;
        int found = 0;

//*******************************************ERROR HANDLING*************************************************
        while (tempDir[i].END_OF_ARRAY == 0) {
                if(strncmp(tempDir[i].DIR_Name, name, strlen(name)) == 0){
                        if (tempDir[i].DIR_Attr & 0x10){
                                printf("Error: Cannot read a directory.\n");
                                return;
                        }
                        else {
                                found = 1;
                        }
                        //return;
                }
                i++;
        }
        if(found == 0){
                printf("Error: File not found in current directory.\n");
                return;
        }
        if(CheckIsOpenW(name) == 0){
                printf("Error: The file cannot be read because it is not open or because it is not opened in write mode.\n");
                return;
        }

//****************************************ERROR HANDLING DONE**********************************************

}

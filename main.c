#include "fat.h"


typedef struct		//just reused the parser code from project 1
{
	char** tokens;
	int numTokens;
} instruction;

void addToken(instruction* instr_ptr, char* tok);
void clearInstruction(instruction* instr_ptr);
void addNull(instruction* instr_ptr);
void searchCommands(instruction* instr_ptr);

int main(int argc, char *argv[]){

	char *imageString;

	char* token = NULL;
	char* temp = NULL;

	instruction instr;
	instr.tokens = NULL;
	instr.numTokens = 0;

	if (argc != 2){
        	printf("error: usage: %s <fat32 image>\n",argv[0]);
        	exit(1);
	}

	imageString = argv[1];
	openImageFileInit(imageString);


	while(1){

	        printf("[%s]> ", pwd);

	        do{
		        scanf("%ms", &token);
		        temp = (char*)malloc((strlen(token) + 1) * sizeof(char));

		        int i;
		        int start = 0;
		        for (i = 0; i < strlen(token); i++) {
		                //pull out special characters and make them into a separate token in the instruction
		                if (token[i] == '|' || token[i] == '>'
				|| token[i] == '<' || token[i] == '&') {
		        		if (i-start > 0) {
				                memcpy(temp, token + start, i - start);
				                temp[i-start] = '\0';
				                addToken(&instr, temp);
		                	}

		                	char specialChar[2];
		                	specialChar[0] = token[i];
		                	specialChar[1] = '\0';

		                	addToken(&instr,specialChar);

		                	start = i + 1;
		                }
		        }

			if (start < strlen(token)) {
				memcpy(temp, token + start, strlen(token) - start);
				temp[i-start] = '\0';
				addToken(&instr, temp);
			}

			//free and reset variables
			free(token);
			free(temp);

			token = NULL;
			temp = NULL;

	        } while ('\n' != getchar());    //until end of line is reached

	        addNull(&instr);
	        searchCommands(&instr);
	        clearInstruction(&instr);
	}

	return 0;
}

void addToken(instruction* instr_ptr, char* tok)
{
        if (instr_ptr->numTokens == 0)
                instr_ptr->tokens = (char**) malloc(sizeof(char*));
        else
                instr_ptr->tokens = (char**) realloc(instr_ptr->tokens, (instr_ptr->numTokens+1) * sizeof(char*));

        instr_ptr->tokens[instr_ptr->numTokens] = (char *)malloc((strlen(tok)+1) * sizeof(char));
        strcpy(instr_ptr->tokens[instr_ptr->numTokens], tok);

        instr_ptr->numTokens++;
}

void addNull(instruction* instr_ptr)
{
        if (instr_ptr->numTokens == 0)
                instr_ptr->tokens = (char**)malloc(sizeof(char*));
        else
                instr_ptr->tokens = (char**)realloc(instr_ptr->tokens, (instr_ptr->numTokens+1) * sizeof(char*));

        instr_ptr->tokens[instr_ptr->numTokens] = (char*) NULL;
        instr_ptr->numTokens++;
}

void clearInstruction(instruction* instr_ptr)
{
        int i;
        for (i = 0; i < instr_ptr->numTokens; i++)
                free(instr_ptr->tokens[i]);

        free(instr_ptr->tokens);

        instr_ptr->tokens = NULL;
        instr_ptr->numTokens = 0;
}

void searchCommands(instruction* instr_ptr)
{
        char temp[12];
	char open_mode[2];
	char* str;

        if(strcmp((instr_ptr->tokens)[0], "exit") == 0){
		printf("Exiting...\n");
		exit(0);
        }


        else if(strcmp((instr_ptr->tokens)[0], "ls") == 0){

        	if(instr_ptr->numTokens == 3){
			strcpy(temp, (instr_ptr->tokens[1]));
			listDirectory(upperstr(temp));
			}
		else if(instr_ptr->numTokens == 2){
			listCurDirectory();
		}
		else
			printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
		}


        else if(strcmp((instr_ptr->tokens)[0], "info") == 0){
		displayInfo();
        }


        else if(strcmp((instr_ptr->tokens)[0], "size") == 0){
			if(instr_ptr->numTokens == 3){
				strcpy(temp, (instr_ptr->tokens[1]));
				displaySize(upperstr(temp));
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "cd") == 0){
		if(instr_ptr->numTokens == 3){
			strcpy(temp, (instr_ptr->tokens[1]));
			changeDirectory(upperstr(temp));
		}
		else if(instr_ptr->numTokens == 2){
			changeDirHome();	//just go to home dir
		}
		else
			printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "creat") == 0){
		if(instr_ptr->numTokens == 3){
			strcpy(temp, (instr_ptr->tokens[1]));
			creat(upperstr(temp));
		}
		else
			printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "mkdir") == 0){
		if(instr_ptr->numTokens == 3){
			strcpy(temp, (instr_ptr->tokens[1]));
			mkdir(upperstr(temp));
		}
		else
			printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "open") == 0){
			if(instr_ptr->numTokens == 4){
				strcpy(temp, (instr_ptr->tokens[1]));
				strcpy(open_mode,(instr_ptr->tokens[2]));
				if (strcmp(open_mode, "r") == 0
				|| strcmp(open_mode, "w") == 0
				|| strcmp(open_mode, "rw") == 0
				|| strcmp(open_mode, "wr") == 0){
					open(temp, open_mode);
				}else{
					printf("Error: invalid mode\n");
				}
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "close") == 0){
			if(instr_ptr->numTokens == 3){
				strcpy(temp, (instr_ptr->tokens[1]));
				closefile(temp);
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "read") == 0){
			if(instr_ptr->numTokens == 5){

				strcpy(temp, (instr_ptr->tokens[1]));
				int offset = atoi(instr_ptr->tokens[2]);
				int sizeOfFile = atoi(instr_ptr->tokens[3]);
				
				readfile(temp, offset, sizeOfFile);
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "write") == 0){
			if(instr_ptr->numTokens == 6){	
				strcpy(temp, (instr_ptr->tokens[1]));
				int offset = atoi(instr_ptr->tokens[2]);
				int sizeOfFile = atoi(instr_ptr->tokens[3]);

				//strcpy(str, instr_ptr->tokens[4]);
				str = instr_ptr->tokens[4];
				writefile(temp, offset, sizeOfFile, str);
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "rm") == 0){
			if(instr_ptr->numTokens == 3){
				strcpy(temp, (instr_ptr->tokens[1]));
				rm(upperstr(temp));
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else if(strcmp((instr_ptr->tokens)[0], "rmdir") == 0){
			if(instr_ptr->numTokens == 3){
				strcpy(temp, (instr_ptr->tokens[1]));
				rm_dir(upperstr(temp));
			}
			else
				printf("Error in %s usage\n", (instr_ptr->tokens)[0]);
        }


        else{
        	printf("Not a supported command\n");
        }

}

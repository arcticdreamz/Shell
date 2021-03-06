/******************************************************************************************
*
* Antoine Louis & Tom Crasset
*
* Operating systems : Projet 2 - shell with built-in's
*******************************************************************************************/

#include <sys/types.h> 
#include <sys/wait.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define IS_COMMAND 1
#define IS_VARIABLE 0
/*************************************Prototypes*********************************************/
int split_line(char* line, char** args);
int get_paths(char** paths);
void remove_delimiters(char** args,int type);
bool find_in_file(const char* path, char* searched_str, char** output_str, int number);
int manage_dollar(char** args, int prev_return, int prev_pid);
int check_variable(char** args);



/****************************************Structures*****************************************/
struct variable{
    char name[256];
    char value[256];
};
//Structure saving previous variables
static struct variable var[256];
static int count = 0;


/*************************************split_line*****************************************
*
* Split the line line entered by the user
*
* ARGUMENT :
*   - line : a string entered by the user as a line line
*
* RETURN : an array of strings reprensenting each token entered by the user
*
*******************************************************************************************/
int split_line(char* line, char** args){

    int nb_args = 0;
    char* token;
    
    token = strtok(line, "\n");
    token = strtok(line, " ");

    while(token != NULL){

        args[nb_args] = token;
        nb_args++;        
        token = strtok(NULL, " ");
    }

    args[nb_args] = (char*) NULL;

    return nb_args;

}


/*************************************get_paths****************************************
*
* Split the full path into all possible paths and get the number of total paths
*
* ARGUMENT :
*   - paths : an array that will contain all the paths
*
* RETURN : the number of paths
*
*******************************************************************************************/
int get_paths(char** paths) {

    char* pathstring = getenv("PATH"); //get the $PATH environment variable
    int nb_paths = 0;

    char* path = strtok(pathstring,":"); //Parse the string for a path delimited by ":"

    while(path != NULL){
        paths[nb_paths] = path;
        nb_paths++;
        path = strtok(NULL,":"); //Parse the array for the next path delimited by ":"
    }

    return nb_paths;
}



/*************************************remove_delimiters************************************
*
* Convert a string with special characters to a directory/folder with whitespaces
*
* ARGUMENT :
*   - args : an array containing all the args of the line  entered by the user
*   - type : IS_COMMAND or IS_VARIABLE
* RETURN : /
*
* NB: it will clear all args except args[0] and args[1], the latter containing the concatenation
*       of all the following arguments, without delimiters
*
*******************************************************************************************/
void remove_delimiters(char** args, int type){

    //Start at 
    int j=type;

    char path[256];
    strcpy(path,"");
    char* token;
    char delimiters[] = "\"\'\\";

    while(args[j] != NULL){

        //Get the first token delimited by one of the delimiters
        token = strtok(args[j], delimiters);

        while(token != NULL){
            //Add this token to the path
            strcat(path, token);
            //Get the next token
            token = strtok(NULL, delimiters);
        }

        //Add a whitespace
        strcat(path, " ");

        j++;
    }

    //Removing the last whitespace
    path[strlen(path)-1] = 0;

    //Cleaning all arguments except cmd and directory (args[0] and args[1])
    memset(&args[2], 0, sizeof(args)-2);
    
    //Copy the path to the unique argument of cd
    //if(!strcmp(args[0],"cd")){
        strcpy(args[1], path);

    //}
}



/*************************************find_in_file*****************************************
*
* Find a certain string in a file 
*
* ARGUMENT :
*   - path : the corresponding path of the file
*   - searched_str : the searched string in the file
*   - output_str : the corresponding output to the searched string
*   - number : 
*
* RETURN : true if the string has been found, false otherwise.
*
*******************************************************************************************/
bool find_in_file(const char* path, char* searched_str, char** output_str, int number){

    FILE* file;
    char* line = NULL;
    size_t len = 0;
    bool result = false;

    //Opening the file
    file = fopen(path, "r");
    if(file == NULL){
        perror("File couldn't be opened");
        printf("1");
        return false;
    }

    
    while(getline(&line, &len, file) != -1){

        if(!strcmp(searched_str, "hostname")){
            *output_str = line;
            fclose(file);
            return true;
        }

        if(strstr(line, searched_str)){

            if(number != 0){
                number--;
                continue;
            }

            *output_str = strtok(line,":");
            *output_str = strtok(NULL, "");
            memmove(*output_str, *output_str+1, strlen(*output_str)); //Remove the whitespace after :
            result = true;
            break;
        }

    }

    fclose(file);

    return result;
}


/*************************************check_variable*****************************************
*
* Check if one tries to assign a variable. If so, store it.
*
* ARGUMENT :
*   - args : arguments entered as a command

*
* RETURN : 1 if no assignement, 0 if assignement successfull, -1 if unsuccessful
*
*******************************************************************************************/
int check_variable(char** args){

    //Get a pointer starting at the '='
    char* ptr = strchr(args[0],'=');

    //No variable assignement
    if(ptr == NULL){
        return 1; 
    }

    //Checking that '=' is surrounded by something
    if(ptr+1 != NULL && ptr-1 != NULL) {
            
            char* name;
            char* value;

            //Value is surrouned by quotes
            if(args[1] != NULL){
                remove_delimiters(args,IS_VARIABLE);
                name = strtok(args[1],"=");
            }else{
                name = strtok(args[0],"=");
            }
            
            //Extracting the name
            value = strtok(NULL, "");

            //Check if the variable alrady exists
            for(int j=0;j < count;j++){
                if(!strcmp(var[j].name,name)){
                    //Replace the old value with the new
                    strcpy(var[j].value,value);
                    return 0;
                }
            }
            
            //Create new variable if it doesn't already exist
            strcpy(var[count].name,name);
            strcpy(var[count].value,value);
            count++;
            return 0;
    }

    //Wrong syntax
    return -1;
}


/*************************************manage_dollar*****************************************
*
* Replace the dollar terms ($? and $!) by their corresponding value, i.e. :
*   - $? : Stores the exit value of the last command that was executed.
*   - $! : Contains the process ID of the most recently executed background pipeline
*
* ARGUMENT :
*   - args : an array containing all the args of the line  entered by the user
*   - prev_return : the previous return value of the foreground command
*   - prev_pid : the previous pid of the background pipeline
*
* RETURN : 0 if replaced a variable, -1 if it doesn't exist, 1 otherwise
*
*******************************************************************************************/
int manage_dollar(char** args, int prev_return, int prev_pid){

    //Check all the arguments and replace the dollar signs by their value
    for(int i=0; args[i] != NULL; i++){

        if(!strcmp(args[i], "$?")){
            snprintf(args[i], 256, "%d", prev_return);
            return 0;
        }

        else if(!strcmp(args[i], "$!")){

            if(prev_pid != 0)
                snprintf(args[i], 256, "%d", prev_pid);
            else
                args[i] = NULL;
            return 0;
        }
        else{
            int cnt = 0;
            char buffer[256] = "";
            int k = 0;
            
            //For all arguments
            for(int j = 0; j < strlen(args[i]);j++){
                //Check if any of them contain $
                if(args[i][j] == '$'){

                    j++; //Don't take $

                    //Extract the following variable name
                    while(j < strlen(args[i])) {
                        buffer[k++] = args[i][j++]; 
                    }

                    //Removing '\"'
                    char* ptr = strchr(buffer,'\"');
                    if(ptr != NULL) 
                        *ptr = 0;
                    
                    //Check if this name exists in the database
                    while(!strcmp(var[cnt].name,"") == false){
                        if((!strcmp(var[cnt].name,buffer)) == true){
                            //Replace the argument with the stored variable
                            args[i] = var[cnt].value; 
                            return 0; //Exit the function
                        }

                        cnt++;
                    }
                    //Clean arguments
                    memset(&args[i],0,sizeof(args[i]));
                    //The variable doesn't exist
                    return -1;
                }
            }
        }
    }
    return 1;
}


/*************************************print_failure*****************************************
*
* Change the value of the previous return value to 1 when there is an error, then print 1.
*
* ARGUMENT :
*   - prev_return : the previous return value
*   - return_nb : a string corresponding to the error value
*
* RETURN : /
*
*******************************************************************************************/
void print_failure(char* return_nb, int* prev_return){
    *prev_return = atoi(return_nb);
    printf("%s", return_nb);
}




/******************************************main**********************************************/
int main(int argc, char** argv){

    bool stop = false;
    int prev_return;
    int prev_pid;

    char line[65536]; 
    char* args[256];
    
    pid_t pid;
    int status;
    
    char* output_str = NULL;

    while(!stop){

        //Clear the variables
        strcpy(line,"");
        memset(args, 0, sizeof(args));

        //Prompt
        printf("> ");
        fflush(stdout);

        //User wants to quit (using Ctrl+D or exit())
        if(fgets(line,sizeof(line),stdin) == NULL || !strcmp(line,"exit\n")){
            stop = true;
            break;
        }

        //User presses "Enter"
        if(!strcmp(line,"\n"))
            continue;

        //User enters a line 
        int nb_args = split_line(line, args);

        //Check if the user enters a variable
        int result = check_variable(args);
        //Syntax error during assignement
        if(result == -1){
            print_failure("1", &prev_return);
            continue;
        }//We stored a variable in our database
        else if(result == 0){
            printf("0");
            continue;
        }


        //Replace $!, $? or $variable by the corresponding term
        if(manage_dollar(args,prev_return, prev_pid) == -1){
            print_failure("1", &prev_return);
            continue;
        }

        


        //The command is cd
        if(!strcmp(args[0], "cd")){

            // Case 1 : cd or cd ~
            if(args[1] == NULL || !strcmp(args[1],"~"))
                args[1] = getenv("HOME");

            //Case 2 : cd ..
            else if(!strcmp(args[1],"..")){

                char* new_dir = strrchr(args[1],'/');

                if(new_dir != NULL)
                    *new_dir = '\0';
            }


            /*Case 3 :  cd FirstDir/"My directory"/DestDir
                        cd FirstDir/'My directory'/DestDir
                        cd FirstDir/My\ directory/DestDir
            */
            if(nb_args > 2){ //Means that there is/are (a) folder(s) with whitespace

                remove_delimiters(args,IS_COMMAND);
            }

            int ret = chdir(args[1]);
            if(ret == -1)
                print_failure("-1", &prev_return);
            else
                printf("%d",ret);
            continue;
        }      


        //The command is sys
        else if(!strcmp(args[0], "sys")){


            //Gives the hostname without using a system call
            if ((args[1]!=NULL)&&(!strcmp(args[1], "hostname"))){

                if(!find_in_file("/proc/sys/kernel/hostname", "hostname", &output_str, 0)){
                    print_failure("1", &prev_return);
                    continue;
                }

                printf("%s0", output_str);
                continue;

            }


            //Gives the CPU model
            if ((args[1]!=NULL)&&(args[2]!=NULL)&&
                (!strcmp(args[1], "cpu"))&&(!strcmp(args[2], "model"))){

                if(!find_in_file("/proc/cpuinfo", "model name", &output_str, 0)){
                    print_failure("1", &prev_return);
                    continue;
                }

                printf("%s0", output_str);
                continue;
            }


            //Gives the CPU frequency of Nth processor
            if ((args[1]!=NULL)&&(args[2]!=NULL)&&
                (!strcmp(args[1], "cpu"))&&(!strcmp(args[2], "freq"))&&
                (args[3]!= NULL)&&(args[4]==NULL)){

                if(!find_in_file("/proc/cpuinfo", "cpu MHz", &output_str, atoi(args[3]))){
                    print_failure("1", &prev_return);
                    continue;
                }

                printf("%s0", output_str);
                continue;

            }


            //Set the frequency of the CPU N to X (in HZ)
            else if ((args[1]!=NULL)&&
                    (args[2]!=NULL)&&
                    (!strcmp(args[1], "cpu"))&&
                    (!strcmp(args[2], "freq"))&&
                    (args[3]!= NULL)&&
                    (args[4]!=NULL)){

                int number = atoi(args[3]);
                char path[256];
                
                //Convert frequency from Hz to kHz
                int frequency = atoi(args[4])/1000;
                snprintf(path,256,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed",number);
                
                FILE* file = fopen(path,"w");
                if(file == NULL){
                    perror("File couldn't be opened");
                    print_failure("1", &prev_return);
                    continue;
                }

                fprintf(file,"%d",frequency);
                fclose(file);

                printf("0");
                continue;

            }


            //Get the ip and mask of the interface DEV
            else if ((args[1] != NULL)&&
                    (args[2] != NULL)&&
                    (!strcmp(args[1], "ip"))&&
                    (!strcmp(args[2], "addr"))&&
                    (args[3] != NULL)&&
                    (args[4] == NULL)){

                    char* dev = args[3];

                    // Create a socket in UDP mode
                    int socket_desc = socket(AF_INET, SOCK_DGRAM, 0);
     
                    //If socket couldn't be created
                    if (socket_desc == -1){
                        perror("Socket couldn't be created\n");
                        print_failure("1", &prev_return);
                        continue;
                    }

                    //Creating an interface structure
                    struct ifreq my_ifreq; 
                    //IPv4
                    my_ifreq.ifr_addr.sa_family = AF_INET;

                    size_t length_if_name= strlen(dev);
                    //Check that the ifr_name is big enough
                    if (length_if_name < IFNAMSIZ){ 

                        memcpy(my_ifreq.ifr_name,dev,length_if_name);
                        my_ifreq.ifr_name[length_if_name]=0; //End the name with terminating char
                    }
                    else{
                        perror("The interface name is too long");
                        print_failure("1", &prev_return);
                        continue;
                    }

                    // Get the IP address, if successful, adress is in  my_ifreq.ifr_addr
                    if(ioctl(socket_desc,SIOCGIFADDR,&my_ifreq) == -1){

                        perror("Couldn't retrieve the IP address");
                        print_failure("1", &prev_return);
                        close(socket_desc);
                        continue;
                    }

                    //Extract the address
                    struct sockaddr_in* IP_address = (struct sockaddr_in*) &my_ifreq.ifr_addr;
                    printf("%s",inet_ntoa(IP_address->sin_addr));

                    // Get the mask, if successful, mask is in my_ifreq.ifr_netmask
                    if(ioctl(socket_desc, SIOCGIFNETMASK, &my_ifreq) == -1){

                        perror("Couldn't retrieve the mask");
                        print_failure("1", &prev_return);
                        close(socket_desc);
                        continue;
                    }

                    //Cast and extract the mask
                    struct sockaddr_in* mask = (struct sockaddr_in*) &my_ifreq.ifr_addr;
                    printf(".%s\n",inet_ntoa(mask->sin_addr));
                    close(socket_desc);

                    printf("0");
                    continue;


            }

        
            //Set the ip of the interface DEV to IP/MASK
            else if ((args[1]!=NULL)&&
                (args[2]!=NULL)&&
                (!strcmp(args[1], "ip"))&&
                (!strcmp(args[2], "addr"))&&
                (args[3]!= NULL)&&
                (args[4]!=NULL)&&
                (args[5]!=NULL)){


                //Interface name and length
                char* name = args[3]; 
                size_t length_if_name= strlen(name); 

                char* address = args[4];
                char* mask = args[5];

                // Create a socket in UDP mode
                int socket_desc = socket(AF_INET, SOCK_DGRAM, 0);
 
                //If socket couldn't be created
                if (socket_desc == -1){
                    perror("Socket couldn't be created\n");
                    print_failure("1", &prev_return);
                    continue;
                }

                //Creating an interface structure
                struct ifreq my_ifreq; 
                my_ifreq.ifr_addr.sa_family = AF_INET;


                //Check that the ifr_name is big enough
                if (length_if_name < IFNAMSIZ){ 
                    //Set the name of the interface you want to look at
                    memcpy(my_ifreq.ifr_name,name,length_if_name);
                    //End the name with terminating char
                    my_ifreq.ifr_name[length_if_name]=0;

                }
                else{
                    perror("The interface name is too long");
                    print_failure("1", &prev_return);
                    close(socket_desc);
                    continue;
                }
                //Creating an address structure;
                struct sockaddr_in* address_struct = (struct sockaddr_in*)&my_ifreq.ifr_addr;

                // Converting from string to address structure
                inet_pton(AF_INET, address, &address_struct->sin_addr);

                //Setting the new IP address
                if(ioctl(socket_desc, SIOCSIFADDR, &my_ifreq) == -1){

                    perror("Couldn't set the address. NOTE: must be in super used mode");
                    print_failure("1", &prev_return);
                    close(socket_desc);
                    continue;
                }

                //Creating a mask structure;
                struct sockaddr_in* mask_struct = (struct sockaddr_in*)&my_ifreq.ifr_netmask;

                // Converting from string to mask structure
                inet_pton(AF_INET, mask,  &mask_struct->sin_addr);

                //Setting the mask
                if(ioctl(socket_desc, SIOCSIFNETMASK, &my_ifreq) == -1){

                    perror("Couldn't set the mask. NOTE: must be in super used mode");
                    print_failure("1", &prev_return);
                    close(socket_desc);
                    continue;
                }


                ioctl(socket_desc, SIOCGIFFLAGS, &my_ifreq); //Load flags
                my_ifreq.ifr_flags |= IFF_UP | IFF_RUNNING; //Change flags
                ioctl(socket_desc, SIOCSIFFLAGS, &my_ifreq); //Save flags
                close(socket_desc);

                printf("0");
                continue;

            }

            //In all other cases, error
            else{
                print_failure("1", &prev_return);
                continue;
            }

        }  

        //The command isn't a built-in command
        pid = fork();

        //Error
        if(pid < 0){
            perror("Process creation failed");
            exit(EXIT_FAILURE);
        }

        //This is the son
        if(pid == 0){

            //Absolute path of command
            if(args[0][0] == '/'){
                if(execv(args[0],args) == -1){
                    perror("Instruction failed");
                }
            }

            //Relative path -- Need to check the $PATH environment variable
            else{
                char* paths[256];
                int j = 1;            

                int nb_paths = get_paths(paths);

                /*In the case of commands like mkdir/rmdir, if the first argument is a directory with whitespaces ("a b", 'a b', a\ b),
                  we need to change this directory in something understandable for the shell*/
                if(nb_args > 2){
                    if(args[1][0] == '\"' || args[1][0] == '\'' || args[1][strlen(args[1])-1] == '\\')
                        remove_delimiters(args,IS_COMMAND);
                }

                //If executable, don't need to add path
                if(args[0][0] == '.'){
                    if(execv(args[0],args) == -1){
                        perror("Instruction failed");
                    }
                    break;    
                    
                }
                else{           
                    //Taking a path from paths[] and concatenating with the command
                    for(j = 0; j < nb_paths; j++){
                        char path[256] = "";
                        strcat(path,paths[j]);
                        strcat(path,"/");
                        strcat(path,args[0]);
                    
                        //Check if path contains the command to execute
                        if(access(path,X_OK) == 0){

                            if(execv(path,args) == -1){
                                perror("Instruction failed");
                            }
                            
                            break;
                        }

                    }
                    printf("Command does not exist\n");
                }
            }
            
            exit(EXIT_FAILURE);
        }

        //This is the father
        else{
            prev_pid = waitpid(-1, &status,0);
            prev_return = WEXITSTATUS(status);
            printf("\n%d",prev_return);
        }

    }

    return 0;
}

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <strings.h>
#include <string.h>
#include <dirent.h>
#include <sys/wait.h>

#define MY_PORT_NUMBER 49999
#define debug true
#define true (1==1)
#define false (1!=1)

int processUserCommand(char*, char*);
void processDataCommand(int, char*,char*);
void processRCD(int, char*);
int show_or_get = 1; // 1 == show , 2 == get

int main(int ac, char*av[]){

  // make control connection to server
  // Make a connection from the client
  int control_fd;
  control_fd = socket(AF_INET,SOCK_STREAM,0);
  if(control_fd == -1){
    perror("Failed to make connection from client.");
    exit(1);
  }

  struct sockaddr_in servAddr;
  struct hostent* hostEntry;
  struct in_addr **pptr;

  memset(&servAddr,0,sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(MY_PORT_NUMBER);
  hostEntry = gethostbyname(av[1]);

  pptr = (struct in_addr **) hostEntry->h_addr_list;
  memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

  if(hostEntry == NULL){
    perror("Failed to get hostname");
    exit(1);
  }
  if(connect(control_fd, (struct sockaddr*)&servAddr, sizeof(servAddr)) !=0){
    perror("connect to control_fd");
    exit(1);
  }
  while(1){
    // create a prompt, get user commands
    char buffer[BUFSIZ];
    int n_read;
    char user_input[35];
    char user_control_command[35];

    printf("\n<mftp>");
    fgets(user_input, 35, stdin);

    // check if user command can be done locally or needs data connection
    int do_i_process = processUserCommand(user_input, user_control_command);

    // if data connection processing is needed, do it
    if(do_i_process == 1){
      processDataCommand(control_fd, user_control_command, av[1]);
    }
    // if rcd
    if(do_i_process == 3){
      processRCD(control_fd, user_control_command);
    }
  }
}

// Handle C (rcd)
void processRCD(int control_fd, char * user_control_command){

  // write C<pathname> to server
  if(write(control_fd, user_control_command, strlen(user_control_command)) < 0){
    perror("Failed to write to control_fd in processRCD()\n");
    exit(1);
  }

  // Read acknowledgement A or error E from server
  char buffer [256];
  if (read(control_fd, buffer, sizeof(buffer)) < 0){
    perror("Failed to read from control_fd in processRCD()\n");
    exit(1);
  }

  // Output rcd result to client
  if (buffer[0] == 'A'){
    printf("Successfully changed server directory\n");
  } else {
    printf("Could not change to that directory\n");
  }

}


// processUserCommand()
// compare user_input to user commands
// if it matches a local command, return 0
// if it matches a data connection command, return 1
// modify user_control_command with corresponding letter if returning 1
// call corresponding function if local command
int processUserCommand(char* user_input, char* user_control_command){

  // tokenize input command and argument
  // token: token_input
  // argument: token_arg
  char * tokenized_userinput;
  char * tokenized_userarg;
  int nullarg = 0;
  char copy_user_input[35];
  strcpy(copy_user_input, user_input);
  tokenized_userinput = strtok(copy_user_input," ");
  if(strlen(tokenized_userinput) != strlen(user_input)){
    tokenized_userarg   = strtok(NULL," ");
  }
  else { nullarg = 1; }

  char token_input[35];
  char token_arg[35];
  strcpy(token_input,tokenized_userinput);
  if (nullarg != 1){
    strcpy(token_arg,tokenized_userarg);
  }
  else {
    strcpy(token_arg,"\n");
  }
  if (strcmp(token_arg,"\n")){
    strcat(token_input,"\n");
  }
  if(debug){printf("Token:%s",token_input);}
  if(debug){printf("Argument:%s", token_arg);}

  // Find a user command from input
  // Local: Perform operation, return 0
  // Data connection: Create control command, return 1
  if (strcmp(token_input,"exit\n") == 0){
    if(debug){printf("Token matches command: exit\n");}
    strcpy(user_control_command,"Q\n\0");
    if(debug){printf("user_control_command set: %s", user_control_command);}
    return 1;
  }

  //// cd change directory locally
  if (strcmp(token_input,"cd\n") == 0){
    if(debug){printf("Token matches command: cd\n");}
    if(token_arg[0] == '\n'){
      strcpy(token_arg,".\n");
    }
    int len = strlen(token_arg);
    if(token_arg[len-1] == '\n'){
      token_arg[strcspn(token_arg,"\n")] = '\0';
    }
    if(chdir(token_arg) != 0){
      perror("Failed to change the working directory\n");
    } else {
      printf("Changed working directory to %s\n",token_arg);
    }
    return 0;
  }
  //// rcd change directory remotely
  if (strcmp(token_input,"rcd\n") == 0){
    if(debug){printf("Token matches command: rcd\n");}
    strcpy(user_control_command,"C");
    strcat(user_control_command,token_arg);
    strcat(user_control_command,"\0");
    if(debug){printf("user_control_command set: %s",user_control_command);}
    return 3;
  }

  //// ls list directory locally
  if (strcmp(token_input,"ls\n") == 0){
    if(debug){printf("Token matches command: ls\n");}

    // fork a child process for exec'ing ls -l
    int ls_pid;
    int ls_exit_status;
    char * ls_args[] = {"ls","-l", ".", NULL};
    if(debug){ printf("%s %s %s", ls_args[0], ls_args[1], ls_args[2]); }
    if ((ls_pid = fork()) == -1){
      perror("forking for rls");
    }
    else if (ls_pid == 0){
      execvp(ls_args[0],ls_args);
      perror("failed to execvp rls");
    }
    else {
      while(wait(&ls_exit_status) != ls_pid){;}
    }


  }
  if (strcmp(token_input,"rls\n") == 0){
    if(debug){printf("Token matches command: rls\n");}
    strcpy(user_control_command,"L\n\0");
    if(debug){printf("user_control_command set: %s",user_control_command);}
    return 1;
  }
  if (strcmp(token_input,"show\n") == 0){
    if(debug){printf("Token matches command: show\n");}
    show_or_get = 1;
    strcpy(user_control_command,"G");
    strcat(user_control_command,token_arg);
    strcat(user_control_command,"\0");
    if(debug){printf("user_control_command set: %s",user_control_command);}
    return 1;
  }
  if (strcmp(token_input,"get\n") == 0){
    show_or_get = 2;
    if(debug){printf("Token matches command: get\n");}
    strcpy(user_control_command,"G");
    strcat(user_control_command,token_arg);
    strcat(user_control_command,"\0");
    if(debug){printf("user_control_command set: %s",user_control_command);}
    return 1;
  }
  if (strcmp(token_input,"put\n") == 0){
    if(debug){printf("Token matches command: put\n");}
    strcpy(user_control_command,"P");
    strcat(user_control_command,token_arg);
    strcat(user_control_command,"\0");
    if(debug){printf("user_control_command set: %s",user_control_command);}
    return 1;
  }
}

void processDataCommand(int control_fd, char* user_control_command, char* hostname){
  // Send D through control connection, asking for a wee bit of data connection
  char control_input[3] = "D\n\0";
  if(debug){printf("strlen of control_input:%ld",strlen(control_input));}
  if(write(control_fd, control_input, strlen(control_input)) == -1){
    perror("write");
    exit(1);
  }
  if(debug){printf("control_input:%s",control_input);}

  // Read back the Acknowledgement A + Port number for a new data connection from the control connection
  int bytes_read;
  char buffer[BUFSIZ] ;
  if((bytes_read = read(control_fd, buffer, BUFSIZ)) <= 0){
    perror("reading port number");
    exit(1);
  }
  strcat(buffer,"\0");
  if(debug){printf("! buffer for port:%s",buffer);}
  // Connect to the data connection socket

  if(buffer[0] == 'E'){
    perror("Error creating a data connection");
    exit(1);
  }
  char port_num[BUFSIZ];
  int i = 0;
  while(buffer[i+2] != '\n'){
    port_num[i] = buffer[i+1];
    i++;
  }
  port_num[i] = buffer[i+1];
  strcat(port_num,"\0");

  int data_fd;
  data_fd = socket(AF_INET,SOCK_STREAM,0);
  if(data_fd == -1){
    perror("Failed to make connection from client.");
    exit(1);
  }

  struct sockaddr_in servAddr;
  struct hostent* hostEntry;
  struct in_addr **pptr;

  int port_int = atoi(port_num);
  memset(&servAddr,0,sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_port = htons(port_int);
  hostEntry = gethostbyname(hostname);

  if(debug){printf("port_int:%d",port_int);}
  fflush(stdout);


  pptr = (struct in_addr **) hostEntry->h_addr_list;
  memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));

  if(hostEntry == NULL){
    perror("Failed to get hostname");
    exit(1);
  }
  if(connect(data_fd, (struct sockaddr*)&servAddr, sizeof(servAddr)) !=0){
    perror("connect to data_fd");
    exit(1);
  }


  // Send the user_control_command to the control connection

  if(debug){printf("about to write the user_control_command to control_fd\n");}
  if(debug){printf("user_control_command:%s",user_control_command);}
  char adjusted_control_command[256] = "";
  if(user_control_command[0] != 'P'){

    strcpy(adjusted_control_command,user_control_command);
    strcat(adjusted_control_command,"\0");

    // send to control socket
    if(write(control_fd, adjusted_control_command, strlen(adjusted_control_command)) == -1){
      perror("Sending user_control_command to control connection for data connection");
      exit(1);
    }
    if(debug){printf("done writing the control command\n");}
  } else {
    // get the filename from full path
    char * token;
    char filename[256];
    char appended_filename[256];
    char temp_control_command[256] = "";
    strcpy(temp_control_command,user_control_command);
    strcpy(adjusted_control_command,user_control_command);
    strcat(adjusted_control_command,"\0");

    token = strtok(temp_control_command,"/");
    while(token != NULL){
      strcpy(filename,token);
      token = strtok(NULL,"/");
    }
    int len = strlen(filename);
    filename[len-1] = '\0';

    // form the control command without path
    strcpy(appended_filename,"P");
    strcat(appended_filename,filename);
    strcat(appended_filename, "\n\0");
    if(debug){ printf("appended_filename adjusted to:%s", appended_filename); }

    // send to control socket
    if(write(control_fd, appended_filename, strlen(appended_filename)) == -1){
      perror("Sending appended filename to control connection for data connection");
      exit(1);
    }
    if(debug){printf("done writing the control command\n");}

  }

  //// Read back an A

  char buff[1];
  int we_can = 1;
  char message[35] = "";
  int iii = 0;
  while(we_can == 1){
    bytes_read = read(control_fd, buff, 1);
    if(bytes_read < 0){ perror("read"); exit(1); }
    if(bytes_read > 0){
      if (buff[0] == '\n'){
        we_can = 0;
      }
      message[iii] = buff[0];
      message[iii+1] = '\0';
      iii++;
    }
  }

  if(debug){printf("done reading message:%s",message);}

  // Read and output from data connection socket

  // L Output (rls)
  //// Reads until we get an error message (data connection closes)
  if (adjusted_control_command[0] == 'L'){
    char buffy[2];
    while((bytes_read = read(data_fd, buffy, 2)) > 0){
      if(bytes_read > 0){
        if(write(1,buffy,2) < 0){ perror("write to stdout\n"); exit(1);}
      }
    }
  }

  // Q Output (exit)
  if (adjusted_control_command[0] == 'Q'){
    printf("Exiting\n");
    close(control_fd);
    exit(1);

  }

  // G Output (get)
  // S Output (show)
  // Works but doesn't output same filename
  if (adjusted_control_command[0] == 'G'){

    // get the filename
    char * token;
    char filename[256];
    token = strtok(adjusted_control_command,"/");
    while(token != NULL){
      strcpy(filename,token);
      token = strtok(NULL,"/");
    }
    int len = strlen(filename);
    filename[len-1] = '\0';

    // get - read from socket and write to file
    int total_bytes_read = 0;
    FILE *fp;
    fp = fopen(filename,"w");

    char bufft[1];
    while((bytes_read = read(data_fd, bufft, 1)) > 0){
      if(bytes_read > 0){
        total_bytes_read++;
        fwrite(bufft,1,sizeof(bufft), fp);
      }
    }
    if (total_bytes_read == 0 ){printf("No file exists to read");}
    fclose(fp);

    // perform show by execvp'ing more -20
    if(show_or_get == 1){
      show_or_get = 2; // reset
      int show_pid;
      int show_exit_status;
      char * show_args[] = {"more","-20", filename, NULL};
      if(debug){ printf("%s %s %s", show_args[0], show_args[1], show_args[2]); }
      if ((show_pid = fork()) == -1){
        perror("forking for show");
      }
      else if (show_pid == 0){
        execvp(show_args[0],show_args);
        perror("failed to execvp show");
      }
      else {
        while(wait(&show_exit_status) != show_pid){;}
      }
    }

  }

  // P Output (put)
  if (adjusted_control_command[0] == 'P'){

    // remove P and \n from control command "P<pathname>"
    // store in 'pathname'
    char pathname[256];
    char message[256];
    strcpy(message,adjusted_control_command);
    printf("message:%s",message);
    if(message[0] == 'P'){
      memmove(message,message+1, strlen(message));
      int i = 0;
      for (i=0; i<strlen(message); i++){
        if(message[i] != '\n'){
          pathname[i] = message[i];
        }
      }
      pathname[i-1] = '\0';
      if(debug){ printf("pathname stripped of P, now:%s",pathname); }
    }
    else {
      perror("Error P<pathname> does not match input");
    }

    // put - read from file and write to socket
    int total_bytes_read = 0;
    FILE *fp;
    fp = fopen(pathname,"r");

    char bufft[1];
    while((bytes_read = fread(bufft,1,sizeof(bufft),fp)) > 0){
      if(bytes_read > 0){
        total_bytes_read++;
        if(write(data_fd, bufft, 1) < 0){
          perror("Error writing in put()\n");
        }
      }
    }
    if (total_bytes_read == 0 ){printf("No file exists to read");}
    fclose(fp);
  }

  fflush(stdout);

}

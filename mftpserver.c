#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#define PORTNUM 49999
#define HOSTLEN 256
#define debug true // true: enable debug messages. false: disable
#define true (1==1)
#define false (1!=1)


void processDataConnectionCommand(int,int);
void newDataConnection(int);
void changeDirectory(char*,int);
void sendError(int,char*);
void sendAcknowledge(int);

// main()
// Creates a server and waits for connections.
// Receives and handles control commands:
//   C : Change the server's directory
//   D : Create a data connection
//   E : Client disconnect request
int main(int ac, char *av[]){
  // create a server
  struct sockaddr_in saddr;
  struct hostent *hp;
  char hostname[HOSTLEN];
  char user_input[BUFSIZ];
  int sock_id, sock_fd;

  sock_id = socket(PF_INET, SOCK_STREAM, 0);
  if(sock_id == -1){
    perror("socket");
  }
  bzero((void*)&saddr,sizeof(saddr));
  gethostname(hostname, HOSTLEN);
  hp = gethostbyname(hostname);
  bcopy((void*)hp->h_addr, (void*)&saddr.sin_addr,hp->h_length);
  saddr.sin_port = htons(PORTNUM);
  saddr.sin_family = AF_INET;
  if(bind(sock_id,(struct sockaddr *)&saddr,sizeof(saddr)) != 0){
    perror("bind");
  }
  if(listen(sock_id,1)!=0){
    perror("listen");
  }
  // wait for messages
  while(1){
    pid_t con_pid;
    int con_status;
    waitpid(-1,&con_status,WNOHANG);

    sock_fd = accept(sock_id, NULL,NULL);
    if(sock_fd == -1){
      perror("accept");
    }

    if ((con_pid = fork()) == -1){
      perror("forking a new client connection");
    }

    // Continuously read for a control command until newline is read.
    // Handle C, Q, D control commands
    else if(con_pid == 0){

      while(1){
        int bytes_read;
        char buffer[1];
        int reached_newline = 1;
        char message[256] = "";
        int i = 0;
        while(reached_newline == 1){
          bytes_read = read(sock_fd, buffer, 1);
          if(bytes_read < 0 ){ perror("read"); exit(1);}
          if(bytes_read > 0){
            if (buffer[0] == '\n'){
              reached_newline = 0;
            }
            message[i] = buffer[0];
            message[i+1] = '\0';
          }
          i++;
        }
        strcpy(user_input,message);
        if (debug){ printf("user_input:%s\n",user_input); }

        // Handle control command Q - disconnect client
        if(user_input[0] == 'Q'){
          sendAcknowledge(sock_fd);
          close(sock_fd);
          close (sock_id);
          exit(1);
        }

        // Handle control command C - change directory
        if(user_input[0] == 'C'){
          changeDirectory(message, sock_fd);
        }

        // Handle control command D - create data connection
        if(strcmp(user_input,"D\n\0") == 0){
          newDataConnection(sock_fd);
        } else if ((user_input[0] != 'C')){printf("something went wrong\n");}
      }
    }
  }
}

// Send an error message to the user
// Format: E<message>
void sendError(int socket_fd, char * error_message){
  // Append error message to E then write to client
  char acknowledge_error[BUFSIZ] = {'E'};
  strcat(acknowledge_error, error_message);
  strcat(acknowledge_error,"\n\0");
  if(write(socket_fd, acknowledge_error, strlen(acknowledge_error)) == -1){
      perror("write");
      exit(1);
  }
}

// Send an acknowledgement 'A' to client
void sendAcknowledge(int socket_fd){
  char acknowledge[BUFSIZ] = {'A','\n','\0'};
  if(write(socket_fd, acknowledge, strlen(acknowledge)) == -1){
      perror("write");
      exit(1);
  }
}
// Change the server's directory.
// message: C<pathname>
void changeDirectory(char * message, int control_fd){

  // checks if we can send acknowledge 'A'
  int send_acknowledge = true;

  // remove C from control command "C<pathname>"
  // store in 'pathname'
  char pathname[256];
  if(message[0] == 'C'){
    message++;
    for (int i=0; i<strlen(message); i++){
      if(message[i] != '\n'){
        pathname[i] = message[i];
      }
    }
    if(debug){ printf("pathname striped of C, now:%s",message); }
  }
  else {
    send_acknowledge = false;
    perror("Error occured in changeDirectory() when removing C from C<pathname>\n");
  }

  // Change directory to 'pathname'
  if(chdir(pathname) != 0){
    send_acknowledge = false;
    perror("Error occured in changeDirectory() when changing directory\n");
  } else {
    printf("Changed working directory to %s\n",pathname);
  }

  // Verify directory changed (debugger function)
  char cwd[PATH_MAX];
  if(getcwd(cwd,sizeof(cwd)) != NULL){
    if(debug){printf("Current working dir: %s\n", cwd);}
  } else {
    perror("getcwd() error");
  }

  // Send error or acknowledgement to client
  if(send_acknowledge){
    sendAcknowledge(control_fd);
  } else {
    sendError(control_fd,"Error: Unable to change to that directory");
  }
}

// newDataConnection()
// Creates a data connection
// Sends A + Port to client
// Calls processDataConnectionCommand()
void newDataConnection(int control_fd){
  // create a data connection
  struct sockaddr_in saddr;
  struct hostent *hp;
  char hostname[HOSTLEN];
  int data_id, data_fd;
  int sockname, port_num;

  data_id = socket(PF_INET, SOCK_STREAM, 0);
  if(data_id == -1){
    perror("socket");
  }

  bzero((void*)&saddr,sizeof(saddr));
  gethostname(hostname, HOSTLEN);
  hp = gethostbyname(hostname);
  bcopy((void*)hp->h_addr, (void*)&saddr.sin_addr,hp->h_length);
  saddr.sin_port = htons(0);
  saddr.sin_family = AF_INET;
  if(bind(data_id,(struct sockaddr *)&saddr,sizeof(saddr)) != 0){
    perror("bind");
  }
  int len = sizeof(saddr);
  sockname = getsockname(data_id, (struct sockaddr *)&saddr,&len);
  port_num = ntohs(saddr.sin_port);
  printf("port: %d\n", port_num);

  if(listen(data_id,4)!=0){
    perror("listen");
  }
  // send acknowledgment A with port via control connection
  char acknowledge[BUFSIZ] = {'A'};
  char port_num_str[25];
  memset(&port_num_str,0,sizeof(port_num_str));
  sprintf(port_num_str,"%d",port_num);
  strcat(acknowledge,port_num_str);
  strcat(acknowledge,"\n\0");
  if(write(control_fd, acknowledge, strlen(acknowledge)) == -1){
      perror("write");
      exit(1);
  }

  data_fd = accept(data_id, NULL,NULL);
  if(data_fd == -1){
    perror("accept");
  }

  // perform data required user commands
  processDataConnectionCommand(data_fd, control_fd);
  close(data_fd);
  // return exit status
}

// processDataConnectionCommand()
// Receives and processes data connection commands:
//   L: List server's current working directory
//   G: Get a server file to the client
//   P: Put a client file to the server
void processDataConnectionCommand(int data_fd,int control_fd){

  // continuously read from control_fd for control commands
  // until a newline character
  FILE *fpo;
  int bytes_read;
  char buffer[1];
  int continuer = true;
  char message[256] = "";
  int send_acknowledge = true;
  int i = 0;
  while(continuer){
    bytes_read = read(control_fd, buffer, 1);
    if(bytes_read < 0 ){
      send_acknowledge = false;
      perror("processDataConnectionCommand()");
    }
    if(bytes_read > 0){
      if (buffer[0] == '\n'){
        continuer = false;
      }
      message[i] = buffer[0];
      i++;
    }
  }
  message[i] = '\0';
  if(debug){printf("message:%s", message);}


  // Begin processing command received in "message"
  // Control Commands: L (rls), G(get), P(put)

  // L (rls) list directory
  if (message[0] == 'L'){

    int send_acknowledge = true;
    // remove newline from pathname (token_arg)
    int len = strlen(message);
    if(message[len-1] == '\n'){
      message[strcspn(message,"\n")] = '\0';
    }
    // fork a child process for exec'ing ls -l
    int ls_pid;
    int ls_exit_status;
    char * ls_args[] = {"ls","-l", ".", NULL};
    if(debug){ printf("%s %s %s", ls_args[0], ls_args[1], ls_args[2]); }
    if ((ls_pid = fork()) == -1){
      perror("forking for rls");
      sendError(control_fd, "forking for rls");
    }
    else if (ls_pid == 0){
      close(STDOUT_FILENO);
      dup2(data_fd,STDOUT_FILENO);
      execvp(ls_args[0],ls_args);
      perror("failed to execvp rls");
    }
    else {
      while(wait(&ls_exit_status) != ls_pid){;}
    }
    sendAcknowledge(control_fd);

  }

  // G (get) file to the client
  if (message[0] == 'G'){
    // remove G and \n from control command "G<pathname>"
    // store in 'pathname'
    char pathname[256];
    if(message[0] == 'G'){
      memmove(message,message+1, strlen(message));
      int i = 0;
      for (i=0; i<strlen(message); i++){
        if(message[i] != '\n'){
          pathname[i] = message[i];
        }
      }
      pathname[i-1] = '\0';
      if(debug){ printf("pathname striped of G, now:%s",pathname); }
    }
    else {
      perror("Error G<pathname> does not match input");
    }
    printf("pathname:%s",pathname);
    fflush(stdout);
    int total_bytes_read = 0;
    FILE *fp;

    char pathcopy[256];
    strcpy(pathcopy,pathname);

    // open the file for reading
    if((fp = fopen(pathname,"r")) == NULL){
      perror("error opening file for get");
      sendError(control_fd, "Could not open file");
    }
    else {
      // read 1 data from the file and write to data_fd
      int buf[1];
      while(fread(&buf, sizeof(int), 1, fp) == 1){ // while we can read 1 int into buf
        const void* bufp = &buf;
        if(write(data_fd,bufp,sizeof(buf)) < 0){ // write to data_fd fread's contents
          perror("Failed to write to data_fd in get");
        }
      }
    }
    sendAcknowledge(control_fd);
    fclose(fp);
  }
  // P (put) put file from the client
  if (message[0] == 'P'){
    // remove 'P' from control command, yielding filename 'pathname'
    char pathname[25];
    if(message[0] == 'P'){
      memmove(message,message+1, strlen(message));
      int i = 0;
      for (i=0; i<strlen(message); i++){
        if(message[i] != '\n'){
          pathname[i] = message[i];
        }
      }
      pathname[i-1] = '\0';
      if(debug){ printf("pathname striped of G, now:%s",pathname); }
    }
    else {
      perror("Error P<pathname> does not match input");
    }
    printf("pathname:%s",pathname);

    // write to file while we can still read from data_fd
    FILE * fp;
    if((fp = fopen(pathname,"w")) == NULL){
      perror("error opening file for put");
      sendError(control_fd,"Could not open specified file");
    }
    else {
      int buf[1];
      const void* bufp = &buf;
      while(read(data_fd, buf, sizeof(buf)) > 0){
        fwrite(buf, sizeof(int), 1, fp);
      }
    }
    sendAcknowledge(control_fd);
    fclose (fp);
  }
}

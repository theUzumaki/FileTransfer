#include "myFTserver.h"

int occurrence(char *string, char *to_find){
	// counts the occurrence inside a string
	int j= 0, counter= 0;
	while (string[j]!= 0){
		if (string[j]==*to_find){ counter++; }
		j++;
	}
	return counter;
}

int str_in_str(char *str1, char *str2, int index, int remove){
	// inserts a string inside a string at a certain index
	char result[64]= "";
	strncat(result, str1, index);
	strcat(result, str2);
	if (remove){ strcat(result, str1+index+strlen(str2)); }
	else { strcat(result, str1+index); }
	strcpy(str1, result);
}

int recursive_mkdir(char *buffer, char *basedir){
	int iterations= occurrence(buffer, "/") - 1;
	char filedir[512]= "", buffer_copy[256]= "";
	DIR *pdir;

	strcpy(buffer_copy, buffer);
	char *pbuffer= buffer_copy;

	// iterates one time for each "/" found and creates
	// the directory if it doesn't already exists
	if (basedir== NULL){ basedir= ""; strcat(filedir, "/"); }
	strcat(filedir, basedir);

	while (iterations> 0){
		strcat(filedir, strtok_r(pbuffer, "/", &pbuffer));
		strcat(filedir, "/");

		// dir creation
		pdir= opendir(filedir);
		if (pdir== NULL){
			int dir_error= mkdir(filedir, 0777);
			if (dir_error< 0){ perror("> dir creation error"); }
		}
		closedir(pdir);
		iterations--;
	}
}

int write_local(char dir[], int client_sock){
	char client_buffer[1024]= "";

	// receiving file name
	int recv_error= recv(client_sock, (char *)&client_buffer, sizeof(client_buffer), 0);
	if (recv_error< 0){ perror("> receive error"); return -1; }
	send(client_sock, "success", 16, 0);

	char filedir[1024]= "", filename[64]= "";
	FILE *pfile;

	// start of critical section
	sem_wait(&sem);

	// directory creation
	strcat(filedir, dir);
	recursive_mkdir((char *)&client_buffer, filedir);
	if (occurrence(client_buffer, "/")== 0){ strcat(filedir, client_buffer); }
	else { strcat(filedir, client_buffer + 1); }

	// solving same name problem
	// searches the index of string before suffix (.txt)
	int index, final_index;
	for (index= strlen(filedir)-1; index> -1; index--){
		if (filedir[index]==*"."){
			final_index= index;
		}
	}
	// generates new name if already existing
	int j= 1; char copyend[4]= "";
	while (access(filedir, F_OK)== 0){
		sprintf(copyend, "(%d)", j);
		if (j==1){ str_in_str(filedir, copyend, index, 0); }
		else { str_in_str(filedir, copyend, index, 1); }
		j++;
	}

	// write on disk
	struct statvfs fs_stat;
	pfile= fopen(filedir, "w");
	memset(&client_buffer, 0, sizeof(client_buffer));

	while ((recv_error= recv(client_sock, client_buffer, sizeof(client_buffer), 0))> 16){
		printf("> received: %s\n", client_buffer);
		statvfs(filedir, &fs_stat);
		if ((fs_stat.f_bsize*fs_stat.f_bfree)<= 1024){ printf("> no disk space\n"); remove(filedir); sem_post(&sem); return -1; }
		fprintf(pfile, (char *)&client_buffer);
		memset(&client_buffer, 0, sizeof(client_buffer));
		send(client_sock, "success", 16, 0);
	}
	if (recv_error< 0){ perror("> receive error"); remove(filedir); sem_post(&sem); return -1; }

	fclose(pfile);
	close(client_sock);
	printf("> file received\n");
	// end of critical section

	sem_post(&sem);
}

int read_local(char *dir, int client_sock){

	// receiving file name
	char buffer[1024]= "", newdir[128]= "", filedir[256]= "";
	int recv_error= recv(client_sock, &newdir, sizeof(newdir), 0);
	if (recv_error< 0){ perror("> receive error"); return -1; }

	strcat(filedir, dir);
	if (occurrence(newdir, "/")==0){ strcat(filedir, newdir); }
	else { strcat(filedir, newdir + 1); }

	// start of critical section
	sem_wait(&sem);

	// retrieving file
	FILE *pfile= fopen(filedir, "r");
	if (pfile!= NULL){
		// confirming file existing
		char ack[16]= "";
		int errcode_send= send(client_sock, "success", 16, 0);
		if (errcode_send< 0){ perror("> sending error"); sem_post(&sem); return -1; }

		// sending contents
		char buffer[1024]= "";
		while(fgets(buffer, 1024, pfile)){
			errcode_send= send(client_sock, (char *)&buffer, sizeof(buffer), 0);
			if (errcode_send< 0){ perror("> sending error"); sem_post(&sem); return -1; }
			recv(client_sock, &ack, sizeof(ack), 0);
			memset(buffer, 0, 1024);
		}
		errcode_send= send(client_sock, "success", 16, 0);
		if (errcode_send< 0){ perror("> sending error"); sem_post(&sem); return -1; }
		printf("> file sent\n");
	}
	else {
		// signaling file not found
		printf("> file not found\n");
		int errcode_send= send(client_sock, "not found", 16, 0);
		if (errcode_send< 0){ perror("> sending error"); sem_post(&sem); return -1; }
		recv(client_sock, &buffer, sizeof(buffer), 0);
	}
	// end of critical section
	sem_post(&sem);
}

int ls_local(char *dir, int client_sock){
	char buffer[1024]= "", filedir[256]= "", filepath[256]= "";
	DIR *pdir;
	printf("> retrieving directory\n");

	// receiving directory name
	int recv_error= recv(client_sock, buffer, sizeof(buffer), 0);
	if (recv_error< 0){ perror("recv_error"); return -1; }
	send(client_sock, "success", 16, 0);

	strcat(filedir, dir);
	strcat(filedir, buffer + 1);

	// start of critical section
	sem_wait(&sem);

	// validating directory
	char ack[16]= "";
	if (opendir(filedir)){ send(client_sock, "displaying directory", 32, 0); }
	else { send(client_sock, "directory not found", 1024, 0); return -1; }
	recv(client_sock, &ack, sizeof(ack), 0);

	// sending directory
	int pid, link[2];
	pipe(link);
	pid= fork();
	if (pid== 0){ // child process
		dup2(link[1], STDOUT_FILENO);
		close(link[1]);
		close(link[0]);
		char *argv[]= {"ls", "-la", filedir, NULL};
		execvp("/bin/ls", argv);
	}
	else { // parent process
		waitpid(pid, NULL, 0);
		// setting read non-blocking
		int flags= fcntl(link[0], F_GETFL, 0);
		fcntl(link[0], F_SETFL, flags | O_NONBLOCK);

		while ((read(link[0], buffer, sizeof(buffer)))> 0){
			int send_error= send(client_sock, buffer, sizeof(buffer), 0);
			if (send_error< 0){ perror("> sending error"); exit(1); }
			memset(buffer, 0, sizeof(buffer));
			recv(client_sock, ack, sizeof(ack), 0);
		}
		send(client_sock, "success", 16, 0);
		close(link[0]);
	}
	// end of critical section
	sem_post(&sem);
}

int main(int argc, char *argv[]){
	int a= 0, p= 0, d= 0;
	int server_sock= socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in server_addr, client_addr;
	socklen_t addr_size;
	char dir[256]= "";
	pid_t childpid;

	// retrieving args
	for (int j= 0; j< argc; j++){
		if (!strcmp(argv[j], "-a")){
			if (j+1< argc){ j++; a= 1; int errcode_addr= inet_aton(argv[j], &server_addr.sin_addr); if (errcode_addr== 0){ perror("> address error"); exit(1); }}
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-p")){
			if (j+1< argc){ j++; p= 1; server_addr.sin_port= atoi(argv[j]); }
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-d")){
			if (j+1< argc){ j++; d= 1; strcpy(dir, argv[j]); }
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (strcmp(argv[j], "./myFTserver.o")!= 0){
			printf("> invalid arguments\n");
			exit(1);
		}
	}
	if (!(a && p && d)){ printf("> invalid arguments\n"); exit(1); }
	if (dir[strlen(dir)-1]!= *"/"){ printf("> invalid directory\n"); exit(1); }
	server_addr.sin_family= AF_INET;

	// directory of server creation
	recursive_mkdir((char *)&dir, NULL);

	// bind
	int errcode_bind= bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (errcode_bind< 0){ perror("> bind error"); exit(1); }

	unsigned int value= 1;
	int sem_error= sem_init(&sem, 1, value);
	if (sem_error== -1){ perror("semaphore"); exit(1); }

	// accepting
	listen(server_sock, 5);
	printf("> waiting for connection...\n");
	while(1){
		addr_size= sizeof(client_addr);
		int client_sock= accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);

		if (client_sock< 0){ printf("> attempt to connection failed\n"); }

		else {
			// forking a child process to handle the client request
			printf("> client connected\n");
			if ((childpid= fork())== 0){

				// receiving of type of request
				char code[2]= "", ack[16]= "";
				recv(client_sock, &code, sizeof(code), 0);
				send(client_sock, "success", 16, 0);

				if (!strcmp(code, "E")){ printf("> client error\n"); }
				else if (!strcmp(code, "w")){
					int error_code= write_local(dir, client_sock);
				if (error_code== -1){ printf("> write on disk failed\n"); }
				}
				else if (!strcmp(code, "r")){
					int error_code= read_local(dir, client_sock);
					if (error_code== -1){ printf("> read on disk failed\n"); }
				}
				else {
					int error_code= ls_local(dir, client_sock);
					if (error_code== -1){ printf("> display of directory failed\n"); }
				}
				printf("> client disconnected\n> ...\n");
				exit(0);
			}
		}
	}
}

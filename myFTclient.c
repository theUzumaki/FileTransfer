#include "myFTclient.h"

int occurrence(char *string, char *to_find){
	// counts how many occurrence of to_find in string
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

int main(int argc,char *argv[]){
	// socket creation
	int client_sock= socket(AF_INET, SOCK_STREAM, 0);
	int w= 0, r= 0, l= 0, local= 0, remote= 0;
	struct sockaddr_in server_addr, client_addr;
	char localpath[128]= "", remotepath[128]= "";

	// retrieving arguments
	for (int j= 0; j< argc; j++){
		if (!strcmp(argv[j], "-w")){
			if (r || l){ printf("> invalid arguments\n"); exit(1); }
			w= 1;
		}
		else if (!strcmp(argv[j], "-r")){
			if (w || l){ printf("> invalid arguments\n"); exit(1); }
			r= 1;
		}
		else if (!strcmp(argv[j], "-a")){
			if (j+1< argc){ j++; int errcode_addr= inet_aton(argv[j], &server_addr.sin_addr); if (errcode_addr== 0){ printf("> invalid address\n"); exit(1); }}
			else { printf("invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-p")){
			if (j+1< argc){ j++; server_addr.sin_port= atoi(argv[j]); }
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-f")){
			if (j+1< argc){ j++; strcpy(localpath, argv[j]); local= 1; }
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-o")){
			if (j+1< argc && !l){ j++; strcpy(remotepath, argv[j]); remote= 1; }
			else { printf("> invalid arguments\n"); exit(1); }
		}
		else if (!strcmp(argv[j], "-l")){
			if (w || r || remote){ printf("> invalid arguments\n"); exit(1); }
			l= 1;
		}
	}
	if (!(w || r || l)){ printf("> invalid arguments\n"); exit(1); }
	if (local && !remote){ strcpy(remotepath, localpath); }
	if (l && (localpath[strlen(localpath)-1]!= *"/" || localpath[0]!= *"/")){
		printf("> invalid arguments\n");
		exit(1);
	}
	server_addr.sin_family= AF_INET;

	// connection to server
	int errcode_connection= connect(client_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
	if (errcode_connection== -1){ perror("> connection error"); exit(1); }
	printf("> successful connection\n");

	// write file to server
	if (w){
		printf("> sending request...\n");
		FILE *pfile= fopen(localpath, "r");
		if (pfile!= NULL){
			char ack[16]= "", code[2]= "w";

			// sending request kind to server
			int errcode_send= send(client_sock, &code, sizeof(code), 0);
			if (errcode_send< 0){ perror("> sending error"); exit(1); }
			recv(client_sock, &ack, sizeof(ack), 0);

			// location where to write file
			errcode_send= send(client_sock, &remotepath, sizeof(remotepath), 0);
			if (errcode_send< 0){ perror(" sending error"); exit(1); }
			recv(client_sock, &ack, sizeof(ack), 0);

			// sending contents
			char buffer[1024]= "";
			while(fgets(buffer, 1024, pfile)){
				errcode_send= send(client_sock, (char *)&buffer, sizeof(buffer), 0);
				if (errcode_send< 0){ perror("> sending error"); exit(1); }
				recv(client_sock, &ack, sizeof(ack), 0);
				memset(buffer, 0, 1024);
			}
			errcode_send= send(client_sock, "success", 16, 0);
			if (errcode_send< 0){ perror("> sending error"); exit(1); }
			printf("> file sent\n");
		}
		else {
			// client error code caused by file not found
			char ack[16]= "";
			send(client_sock, "E", 2, 0);
			recv(client_sock, ack, sizeof(ack), 0);
			printf("> file not found\n");
		}
	}
	// read file from server
	else if (r){
		FILE *pfile; DIR *pdir;
		char ack[16]= "", code[2]= "r";

		// sending request kind to server
		printf("> requesting file...\n");
		int errcode_send= send(client_sock, &code, sizeof(code), 0);
		if (errcode_send< 0){ perror("sending error"); exit(1); }
		recv(client_sock, &ack, sizeof(ack), 0);

		// location where to retrieve file from server
		errcode_send= send(client_sock, &localpath, sizeof(localpath), 0);
		if (errcode_send< 0){ perror("sending error"); exit(1); }
		recv(client_sock, &ack, sizeof(ack), 0);
		if (strcmp(ack, "success")){
			printf("> file not found\n");
			send(client_sock, "success", 16, 0);
			exit(1);
		}

		char *ppath, remotepath_copy[128]= "", filedir[128]= "/";
		strcpy(remotepath_copy, remotepath);
		ppath= remotepath_copy;


		// creating directory if it doesn't already exists
		int iterations= occurrence((char *)&remotepath, "/") - 1;
		strcat(filedir, "/");
		while (iterations){
			strcat(filedir, strtok_r(ppath, "/", &ppath));
			strcat(filedir, "/");

			pdir= opendir(filedir);
			if (pdir== NULL){
				int dir_error= mkdir(filedir, 0777);
				if (dir_error< 0){ perror("> dir creation error"); }
			}
			closedir(pdir);
			iterations--;
		}

		// solving same name problem
		int index, final_index;
		for (index= strlen(remotepath)-1; index> -1; index--){
			if (remotepath[index]==*"."){
				final_index= index;
			}
		}

		int j= 1; char copyend[4]= "";
		while (access(remotepath, F_OK)== 0){
			sprintf(copyend, "_%d", j);
			if (j==1){ str_in_str(remotepath, copyend, index, 0);}
			else { str_in_str(remotepath, copyend, index, 1); }
			j++;
		}

		// writing contents
		struct statvfs fs_stat;
		pfile= fopen(remotepath, "w");
		char buffer[1024]= "";
		int recv_error= 0;
		while ((recv_error= recv(client_sock, buffer, sizeof(buffer), 0))> 16){
			printf("> received: %s\n", buffer);
			statvfs(remotepath, &fs_stat);
			if ((fs_stat.f_bsize*fs_stat.f_bfree)<= 1024){ printf("> no disk space\n"); remove(remotepath); exit(1); }
			fprintf(pfile, (char *)&buffer);
			memset(&buffer, 0, sizeof(buffer));
			send(client_sock, "success", 16, 0);
		}
		if (recv_error< 0){ perror("> receive error"); remove(remotepath); exit(1); }

		fclose(pfile);
		close(client_sock);
		printf("> file received\n");
	}
	// ls -la on server
	else {
		char code[2]= "l", ack[16]= "";
		char buffer[1024]= "";

		// sending request kind to server
		int send_error= send(client_sock, code, sizeof(code), 0);
		if (send_error< 0){ perror("> sending error"); exit(1); }
		recv(client_sock, ack, sizeof(ack), 0);

		// path of directory to explore
		send_error= send(client_sock, localpath, sizeof(localpath), 0);
		if (send_error< 0){ perror("> sending error"); exit(1); }
		recv(client_sock, ack, sizeof(ack), 0);

		// directory found or not
		recv(client_sock, buffer, sizeof(buffer), 0);
		printf("> %s\n", buffer);
		send(client_sock, ack, sizeof(ack), 0);
		if (!strcmp(buffer, "directory not found")){ exit(1); }

		// print of directory contents
		int recv_error= 0;
		memset(buffer, 0, sizeof(buffer));
		printf("-------\n");
		while ((recv_error= recv(client_sock, buffer, sizeof(buffer), 0))> 16){
			printf("%s", buffer);
			memset(buffer, 0, sizeof(buffer));
			send(client_sock, "success", 16, 0);
		}
		if (recv_error< 0){ perror("> receive error"); exit(1); }

		close(client_sock);
		printf("\n> all displayed\n");

	}
	close(client_sock);
}

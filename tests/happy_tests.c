// #include "includeme.h"
// #include <criterion/criterion.h>
// #include <pthread.h>
// #include <stdio.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <signal.h>
// #include <wait.h>

// /* Directory in which to create test output files. */
// #define TEST_OUTPUT "test_output/"
// #define SERVER_PID "8888"

// typedef struct command {
//   char *cmd;
//   int delay;
// } CMD;

// typedef struct client_connection {
//   int pid;
//   int read;
//   int write;
// } CLIENT_C;

// sem_t wait_for_server;
// int is_wait_for_server_init = 0;

// /*
//  * Create a process to connect to the server with jclient.
//  * 
//  */
// static CLIENT_C* create_connection() {
//   // fork and connect to server using execvp and util/jclient -p serverpid
//   // return the pid of the forked process

//   // create a pipe to communicate with the server
//   int pipefd[2];
//   pipe(pipefd);

//   // create a client connection node
//   CLIENT_C *client = calloc(1,sizeof(CLIENT_C));

//   // create two pipes
//   int read_fd_pipe[2], write_fd_pipe[2];
//   if (pipe(read_fd_pipe) < 0 || pipe(write_fd_pipe) < 0) {
//     error("pipe err");
//     return NULL;
//   }

//   // set watcher read and write
//   client->read = read_fd_pipe[0];    // 3 / read to 3
//   client->write = write_fd_pipe[1];  // 6 / write to 6

//   debug("pipe created: %d, %d", read_fd_pipe[0], write_fd_pipe[1]);
//   char *args[] = {"util/jclient", "-p", SERVER_PID, NULL};

//   // fork the process
//   pid_t pid = fork();
//   if (pid == -1) {
//     debug("fork err");
//     return NULL;
//   } else if (pid == 0) {
//     // close the unused ends of the pipe
//     // warn("close parent ends of the pipe, %d, %d", write_fd_pipe[1],
//     //      read_fd_pipe[0]);

//     close(write_fd_pipe[1]);  // close 3 / child does not read from parent
//     close(read_fd_pipe[0]);   // close 6
//     // child process
//     if (dup2(write_fd_pipe[0], STDIN_FILENO) == -1) {
//       // instead of stdin, parent should write to child
//       debug("dup2 write pipe err");
//       return NULL;
//     }
//     // redirect stdout to pipe
//     if (dup2(read_fd_pipe[1], STDOUT_FILENO) == -1) {
//       // instead of stdout, child should write to parent
//       debug("dup2 read pipe err");
//       return NULL;
//     }
//     // execute the command
//     execvp(args[0], args);
//     debug("execvp err");
//     return NULL;
//   } else {
//     // parent process

//     // save process id
//     // debug("parent process id: %d", pid);
//     client->pid = pid;

//     // close child ends of the pipe
//     // warn("close child ends of the pipe, %d, %d", write_fd_pipe[0],
//     close(write_fd_pipe[0]);  // close 3 / child does not read from parent
//     close(read_fd_pipe[1]);   // close 6
//   }
//   return client;
// }

// static int write_cmds(CLIENT_C *client, CMD** commands) {
//   for (int i = 1; *commands != NULL; i++) {
//     CMD* command = *commands;
//     // debug("write cmd: %s", command->cmd);
//     // wait specified delay
//     sleep(command->delay);
//     // check. if client->write is open
//     if (fcntl(client->write, F_GETFD) == -1) {
//       debug("client write closed");
//       return -1;
//     }
//     // write to the pipe
//     write(client->write, command->cmd, strlen(command->cmd));
//     free(command);
//     commands++;
//   }
//   sleep(1);
//   return 0;
// }

// static char* read_client_pipe(CLIENT_C *client) {
//   // read from the pipe
//   char *buf = calloc(1024, sizeof(char));
//   int n = read(client->read, buf, 1024);
//   if (n < 0) {
//     debug("read err");
//     return NULL;
//   }
//   // debug("read %d bytes from pipe", n);
//   // debug("read: %s", buf);
//   return buf;
// }

// /*
//  * Use this to start the server in the background.
//  */
// static int start_server(char* args[]) {
//   int pid = fork();
//   if (pid == 0) {
//     // char *args[] = {"valgrind", "jeux", "--leak-check=full", "--track-fds=yes", "--show-leak-kinds=all",
//     //     "--error-exitcode=37", "--log-file="TEST_OUTPUT"valgrind.out", "bin/jeux", "-p", SERVER_PID, NULL};
//     execvp(args[0], args);
//     fprintf(stderr, "Failed to exec server\n");
//     abort();
//   }
//   return pid;
// }

// static int kill_server(int pid) {
//   kill(pid, SIGHUP);
//   // wait for server to exit
//   waitpid(pid, NULL, 0);
//   return 0;
// }

// static int kill_client(CLIENT_C *client) {
//   // close write end of pipe
//   // close(client->write);
//   // close(client->read);

//   shutdown(client->read, SHUT_RD);
//   shutdown(client->write, SHUT_WR);
//   kill(client->pid, SIGINT);

//   // this will send eof to the client
//   // wait for client to exit
//   waitpid(client->pid, NULL, 0);
//   error("killed client");
//   free(client);
//   sleep(4);
//   return 0;
// }

// int kill_prev_server() {
//   int ret = system("netstat -an | fgrep '0.0.0.0:"SERVER_PID"' > /dev/null");
//   if (ret == 0) {
//     // server is running
//     // kill it
//     system("fuser -k "SERVER_PID"/tcp");
//   }
//   return 0;
// }

// int server_wrapper(char* valgrind_out) {
//     kill_prev_server();
//     int server_pid = 0;

//     char* outfile = calloc(1024, sizeof(char));
//     sprintf(outfile, "--log-file=%s%s", TEST_OUTPUT, valgrind_out);

//     char* args[] = {"valgrind", "--leak-check=full", "--track-fds=yes", "--show-leak-kinds=all",
// 	       "--error-exitcode=37", outfile, "bin/jeux", "-p", SERVER_PID, NULL};
//     // char* args[] = {"bin/jeux",  "-p",  SERVER_PID, NULL};
//     server_pid = start_server(args);
//     cr_assert_neq(server_pid, 0, "Failed to start server");
//     sleep(1);
//     return server_pid;
// }

// void cleanup(int server_pid, CLIENT_C* client) {
//     error("Cleanup");
//     kill_server(server_pid);
//     kill_client(client);
// }

// #define USERNAMEA "a"

// Test(module_suite, users_before_login_error, .timeout = 30) {
//     printf("test users_before_login_error\n");
//     int server_pid = server_wrapper("valgrind_users_then_login.out");
//     // connect clients to server
//     CLIENT_C *client = create_connection();
//     cr_assert_neq(client, NULL, "Failed to create client connection");
//     // create commands
//     CMD** commands = calloc(2, sizeof(CMD*));
//     commands[0] = calloc(1, sizeof(CMD));
//     commands[0]->cmd = "users\n\0";
//     commands[0]->delay = 1;
//     commands[1] = NULL;
//     // read everything before sending commands
//     char* buf = read_client_pipe(client);
//     free(buf);
//     // write commands to client
//     write_cmds(client, commands);
//     // read from client pipe
//     buf = read_client_pipe(client);
//     printf("buf: %s\n", buf);
//     // assert that "ERROR" in buf
//     cr_assert_str_not_empty(buf, "Failed to read from client pipe");
//     // make sure the end of the pipe is ERROR\n>\n
//     cr_assert((strstr(buf, "ERROR") != NULL), "ERROR NOT IN PIPE");
//     free(buf);
//     cleanup(server_pid, client);
// }

// Test(module_suite, login_then_users, .timeout = 30) {
//     printf("test login then users\n");
//     int server_pid = server_wrapper("valgrind_login_then_users.out");
//     // connect clients to server
//     CLIENT_C *client = create_connection();
//     cr_assert_neq(client, NULL, "Failed to create client connection");
//     // create commands
//     CMD** commands = calloc(3, sizeof(CMD*));
//     commands[0] = calloc(1, sizeof(CMD));
//     commands[0]->cmd = "login "USERNAMEA"\n\0";
//     commands[0]->delay = 1;
//     commands[1] = calloc(1, sizeof(CMD));
//     commands[1]->cmd = "users\n\0";
//     commands[1]->delay = 2;
//     commands[2] = NULL;
//     // read everything before sending commands
//     char* buf = read_client_pipe(client);
//     free(buf);
//     // write commands to client
//     write_cmds(client, commands);
//     // read from client pipe
//     buf = read_client_pipe(client);
//     printf("buf: %s\n", buf);
//     // assert that "ERROR" in buf
//     cr_assert_str_not_empty(buf, "Failed to read from client pipe");
//     // make sure the pipe is OK
//     cr_assert((strstr(buf, "OK") != NULL), "OK NOT IN PIPE");
//     // make sure the pipe is users command
//     cr_assert((strstr(buf, USERNAMEA"\t1500") != NULL), "USERS CMD NOT IN PIPE");
//     free(buf);
//     cleanup(server_pid, client);
// }

// // login 65 clients
// Test(module_suite, clients_65, .timeout=30) {
//   printf("test clients_65\n");
//   int server_pid = server_wrapper("valgrind_65.out");
//   // connect clients to server
//   int size = 65;
//   CLIENT_C *clients[size];
//   for (int i = 0; i < size; i++) {
//     clients[i] = create_connection();
//     cr_assert_neq(clients[i], NULL, "Failed to create client connection");
//   }
//   sleep(5);
//   // kill one client
//   error("killing client 0");
//   kill_client(clients[0]);
//   error("server_pid: %d", server_pid);
//   // now last client should be able to login
//   cleanup(server_pid, clients[2]);
// }


// Test(module_suite, two_users_playing, .timeout = 30) {
//     printf("test two_users_playing\n");
//     int server_pid = server_wrapper("valgrind_two_users_playing.out");
//     // connect clients to server
//     CLIENT_C *client = create_connection();
//     cr_assert_neq(client, NULL, "Failed to create client connection");
//     // create commands
//     CMD** commands = calloc(3, sizeof(CMD*));
//     commands[0] = calloc(1, sizeof(CMD));
//     commands[0]->cmd = "login "USERNAMEA"\n\0";
//     commands[0]->delay = 1;
//     commands[1] = calloc(1, sizeof(CMD));
//     commands[1]->cmd = "users\n\0";
//     commands[1]->delay = 1;
//     commands[2] = NULL;
//     // read everything before sending commands
//     char* buf = read_client_pipe(client);
//     free(buf);
//     // write commands to client
//     write_cmds(client, commands);
//     // read from client pipe
//     buf = read_client_pipe(client);
//     printf("buf: %s\n", buf);

//     // assert that "ERROR" not in buf
//     cr_assert_str_not_empty(buf, "Failed to read from client pipe");
//     cr_assert((strstr(buf, "ERROR") == NULL), "ERROR IN PIPE");
//     // make sure the pipe is OK
//     cr_assert((strstr(buf, "OK") != NULL), "OK NOT IN PIPE");
//     // make sure the pipe is users command
//     cr_assert((strstr(buf, USERNAMEA"\t1500") != NULL), "USERS CMD NOT IN PIPE");
//     free(buf);


//     CLIENT_C *c2 = create_connection();
//     cr_assert_neq(c2, NULL, "Failed to create client connection");
//     // create commands
//     CMD** commands2 = calloc(3, sizeof(CMD*));
//     commands2[0] = calloc(1, sizeof(CMD));
//     commands2[0]->cmd = "login b\n\0";
//     commands2[0]->delay = 1;
//     commands2[1] = calloc(1, sizeof(CMD));
//     commands2[1]->cmd = "invite "USERNAMEA" 1\n\0";
//     commands2[1]->delay = 2;
//     commands2[2] = NULL;
//     // read everything before sending commands
//     buf = read_client_pipe(c2);
//     free(buf);
//     // write commands to client
//     write_cmds(c2, commands2);
//     // read from client pipe
//     buf = read_client_pipe(c2);
//     printf("buf: %s\n", buf);
//     // assert that "ERROR" not in buf
//     cr_assert_str_not_empty(buf, "Failed to read from client pipe");
//     cr_assert((strstr(buf, "ERROR") == NULL), "ERROR IN PIPE");
//     // assert OK in pipe
//     cr_assert((strstr(buf, "OK") != NULL), "OK NOT IN PIPE");
//     free(buf);

//     // accept invite in client 1
//     CMD** commands3 = calloc(2, sizeof(CMD*));
//     commands3[0] = calloc(1, sizeof(CMD));
//     commands3[0]->cmd = "accept 0\n\0";
//     commands3[0]->delay = 1;
//     commands3[1] = NULL;
//     // assert OK in pipe

//     // write commands
//     write_cmds(client, commands3);

//     buf = read_client_pipe(client);
//     printf("buf: %s\n", buf);
//     cr_assert((strstr(buf, "You have been invited to play 'X' in a game (#0) with b") != NULL), "You have been invited to play 'X' in a game (#0) with b NOT IN PIPE");
//     cr_assert((strstr(buf, "X to move") != NULL), "X to move NOT IN PIPE");
//     free(buf);
//     // quit client 1
//     kill_client(client);
//     // read from client pipe
//     buf = read_client_pipe(c2);
//     printf("buf: %s\n", buf);
//     // assert that "ERROR" not in buf
//     cr_assert_str_not_empty(buf, "Failed to read from client pipe");
//     cr_assert((strstr(buf, "ERROR") == NULL), "ERROR IN PIPE");
//     // assert Your opponent has resigned game #0 in pipe
//     cr_assert((strstr(buf, "Your opponent has resigned game #0") != NULL), "Your opponent has resigned game #0 NOT IN PIPE");
//     free(buf);
//     cleanup(server_pid, c2);
// }
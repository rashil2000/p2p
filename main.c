#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <unistd.h>

#define MAXLINE 1024
#define MAXPEERS 5
#define LOG 0 // Change to `1` to enable logging

typedef struct _message
{
  char content[MAXLINE];
  int port;
} message;

typedef struct _fd_elem
{
  int fd;
  int port;
} fd_elem;

/* Shared information table */
char *machine_ip = "127.0.0.1";
char *user_info_name[MAXPEERS] = {
    "peer-one",
    "peer-two",
    "peer-three",
    "peer-four",
    "peer-five"};
int user_info_port[MAXPEERS] = {
    5001,
    5002,
    5003,
    5004,
    5005};

int port_lookup(char *name)
{
  for (int i = 0; i < MAXPEERS; i++)
    if (strcmp(user_info_name[i], name) == 0)
      return user_info_port[i];

  return -1;
}

char *name_lookup(int port)
{
  for (int i = 0; i < MAXPEERS; i++)
    if (user_info_port[i] == port)
      return user_info_name[i];

  return NULL;
}

message extract_message(char *text)
{
  int i = 0, j = 0;
  char name[MAXLINE];
  message retval;
  retval.content[0] = 0;
  retval.port = -1;

  strcpy(name, text);
  for (; name[i] != 0; i++)
    if (name[i] == '/')
    {
      name[i++] = 0;
      retval.port = port_lookup(name);
      break;
    }

  while (text[i] != 0)
    retval.content[j++] = text[i++];

  retval.content[j] = 0;

  return retval;
}

int main(int argc, char **argv)
{
  int listenfd, recvfd, stdinfd = STDIN_FILENO, maxfd, my_port, opt = 1, i, curr_port, curr_error;
  char buffer1[MAXLINE], buffer2[MAXLINE];
  fd_set rset;
  socklen_t len;
  struct sockaddr_in srvraddr, recvaddr, sendaddr;
  fd_elem myfdarr[MAXPEERS];

  if (argc < 2)
  {
    printf("\033[3;33m\nProvide the name of the peer.\033[0m\n");
    exit(EXIT_FAILURE);
  }
  my_port = port_lookup(argv[1]);
  if (my_port < 0)
  {
    printf("\033[3;33m\n%s is not in the peers group. This incident will be reported.\033[0m\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  // Defining server address
  bzero(&srvraddr, sizeof(srvraddr));
  srvraddr.sin_family = AF_INET;
  srvraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvraddr.sin_port = htons(my_port);

  // Defining sender address
  bzero(&sendaddr, sizeof(sendaddr));
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_addr.s_addr = inet_addr(machine_ip);

  // Creating sending socket collection
  for (i = 0; i < MAXPEERS; i++)
  {
    myfdarr[i].fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(myfdarr[i].fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    myfdarr[i].port = -1;

    // Binding fd to (own) address
    if (bind(myfdarr[i].fd, (struct sockaddr *)&srvraddr, sizeof(srvraddr)) != 0)
      printf("\033[3;33mError binding 'fd %d'\033[0m\n", i);
  }

  // Creating listening socket
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Binding listenfd to server address
  if (bind(listenfd, (struct sockaddr *)&srvraddr, sizeof(srvraddr)) != 0)
    printf("\033[3;33mError binding 'listenfd'\033[0m\n");

  // Start listening for incoming connections
  listen(listenfd, MAXPEERS);

  // Initializing the descriptor set
  FD_ZERO(&rset);

  // Print welcome message
  printf("\n\033[0;35m++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\033[0m\n");
  printf("\033[0;32m    Hi \033[0m\033[3;32m%s\033[0m\033[0;32m! Welcome to this simple \033[1;32mpeer-to-peer chat\033[0;32m program.\033[0m\n", argv[1]);
  printf("\033[3;36m   Type the \033[4;36mname\033[0m \033[3;36mof the peer, followed by a \033[4;36m/\033[0m\033[3;36m, and then your message.\033[0m\n");
  printf("\033[0;34m[Recompile the program after setting the `\033[1;34mLOG\033[0;34m` macro to enable logging.]\033[0m\n");
  printf("\033[0;35m++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\033[0m\n\n");

  for (;;)
  {
    // Filling descriptors in readset
    FD_SET(listenfd, &rset);
    FD_SET(stdinfd, &rset);
    maxfd = listenfd > stdinfd ? listenfd : stdinfd;
    for (i = 0; i < MAXPEERS; i++)
      if (myfdarr[i].port != -1)
      {
        if (recv(myfdarr[i].fd, NULL, 1, MSG_PEEK | MSG_DONTWAIT) == 0)
        {
          // Disconnect socket if the peer is lost
          myfdarr[i].port = -1;
          sendaddr.sin_family = AF_UNSPEC;
          if (connect(myfdarr[i].fd, (struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0)
            perror("disconnect");

          sendaddr.sin_family = AF_INET;
          continue;
        }

        FD_SET(myfdarr[i].fd, &rset);
        if (myfdarr[i].fd > maxfd)
          maxfd = myfdarr[i].fd;
      }

    // Selecting the ready descriptor
    select(maxfd + 1, &rset, NULL, NULL, NULL);

    // Handling receivers
    for (i = 0; i < MAXPEERS; i++)
    {
      if (myfdarr[i].port != -1 && FD_ISSET(myfdarr[i].fd, &rset))
      {
        bzero(buffer1, sizeof(buffer1));
        char *sender_name = name_lookup(myfdarr[i].port);
        if (!sender_name)
          sender_name = "(unknown)";
        printf("\033[0;34m%s\\\033[0m", sender_name);
        read(myfdarr[i].fd, buffer1, sizeof(buffer1));
        puts(buffer1);
        if (LOG)
          write(myfdarr[i].fd, buffer1, sizeof(buffer1));
      }
    }

    // Handling connection listener
    if (FD_ISSET(listenfd, &rset))
    {
      len = sizeof(recvaddr);
      recvfd = accept(listenfd, (struct sockaddr *)&recvaddr, &len);
      curr_port = ntohs(recvaddr.sin_port);

      for (i = 0; i < MAXPEERS; i++)
        if (curr_port == user_info_port[i])
          break;

      myfdarr[i].fd = recvfd;
      myfdarr[i].port = curr_port;
    }

    // Handling console input and sender
    if (FD_ISSET(stdinfd, &rset))
    {
      bzero(buffer2, sizeof(buffer2));
      read(stdinfd, buffer2, sizeof(buffer2));
      buffer2[strcspn(buffer2, "\n")] = 0; // Newline zapper
      message prntval = extract_message(buffer2);
      if (prntval.port == -1)
        printf("\033[3;33mUnknown peer.\033[0m\n");
      else if (prntval.port == my_port)
        printf("\033[0;34m(self)\\\033[0m%s\n", prntval.content);
      else
      {
        if (LOG)
          printf("\033[3;36mSending to port %d: \033[0m%s\n", prntval.port, prntval.content);
        for (i = 0; i < MAXPEERS; i++)
          if (prntval.port == user_info_port[i])
            break;

        if (myfdarr[i].port == -1)
        {
          sendaddr.sin_port = htons(prntval.port);
          if (connect(myfdarr[i].fd, (struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0)
          {
            printf("\033[3;33mPeer offline.\033[0m\n");
            continue;
          }
          myfdarr[i].port = prntval.port;
        }
        write(myfdarr[i].fd, prntval.content, sizeof(prntval.content));
        if (LOG)
        {
          printf("\033[3;36mReceived from port %d: \033[0m", prntval.port);
          read(myfdarr[i].fd, buffer2, sizeof(buffer2));
          puts(buffer2);
        }
      }
    }
  }
}

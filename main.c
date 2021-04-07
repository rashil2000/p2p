#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/select.h>

#define MAXLINE 1024
#define MAXPEERS 5
#define TIMEOUT 600
#define BROADCAST_ID 999
#define LOG 0 // Change to `1` to enable logging

typedef struct
{
  int port;
  char *name;
} peer;

typedef struct
{
  char content[MAXLINE];
  int peer_index;
} message;

typedef struct
{
  int fd;
  int live;
  unsigned long last_active_time;
} fd_elem;

fd_elem myfdarr[MAXPEERS];

/* Shared information table */
char *machine_ip = "127.0.0.1";
peer peersgroup[MAXPEERS] = {
    {5001, "peer-one"},
    {5002, "peer-two"},
    {5003, "peer-three"},
    {5004, "peer-four"},
    {5005, "peer-five"}};

int index_lookup(char *name)
{
  for (int i = 0; i < MAXPEERS; i++)
    if (strcmp(peersgroup[i].name, name) == 0)
      return i;

  return -1;
}

message extract_message(char *text)
{
  int i = 0, j = 0;
  char name[MAXLINE];
  message retval;
  retval.content[0] = 0;
  retval.peer_index = -1;

  strcpy(name, text);
  for (; name[i] != 0; i++)
    if (name[i] == '/')
    {
      name[i++] = 0;
      retval.peer_index = index_lookup(name);
      if (retval.peer_index == -1 && strcmp(name, "[broadcast]") == 0)
        retval.peer_index = BROADCAST_ID;
      break;
    }

  while (text[i] != 0)
    retval.content[j++] = text[i++];

  retval.content[j] = 0;

  return retval;
}

int main(int argc, char **argv)
{
  int listenfd, recvfd, stdinfd = STDIN_FILENO, maxfd, my_index, curr_index, fails = 1, opt = 1, i, timed_out, lost_out;
  char buffer1[MAXLINE], buffer2[MAXLINE];
  fd_set rset;
  socklen_t len;
  struct sockaddr_in srvraddr, recvaddr, sendaddr;
  struct timeval instant;

  if (argc < 2)
  {
    printf("\033[3;33m\nProvide the name of the peer.\033[0m\n");
    exit(EXIT_FAILURE);
  }
  my_index = index_lookup(argv[1]);
  if (my_index == -1)
  {
    printf("\033[3;33m\n%s is not in the peers group. This incident will be reported.\033[0m\n", argv[1]);
    exit(EXIT_FAILURE);
  }

  // Defining server address
  bzero(&srvraddr, sizeof(srvraddr));
  srvraddr.sin_family = AF_INET;
  srvraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  srvraddr.sin_port = htons(peersgroup[my_index].port);

  // Defining sender address
  bzero(&sendaddr, sizeof(sendaddr));
  sendaddr.sin_family = AF_INET;
  sendaddr.sin_addr.s_addr = inet_addr(machine_ip);

  // Creating sending socket collection
  for (i = 0; i < MAXPEERS; i++)
  {
    myfdarr[i].fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(myfdarr[i].fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    myfdarr[i].live = 0;
    myfdarr[i].last_active_time = 0;
  }

  // Creating listening socket
  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  // Binding listenfd to server address
  if (bind(listenfd, (struct sockaddr *)&srvraddr, sizeof(srvraddr)) != 0)
  {
    printf("\033[3;33mError binding 'listenfd'. Peer port already in use.\033[0m\n");
    exit(EXIT_FAILURE);
  }

  // Start listening for incoming connections
  listen(listenfd, MAXPEERS);

  // Initializing the descriptor set
  FD_ZERO(&rset);

  // Print welcome message
  {
    printf("\n\033[0;35m++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\033[0m\n");
    printf("\033[0;32m    Hi \033[0m\033[3;32m%s\033[0m\033[0;32m! Welcome to this simple \033[1;32mpeer-to-peer chat\033[0;32m program.\033[0m\n", argv[1]);
    printf("\033[3;36m   Type the \033[4;36mname\033[0m \033[3;36mof the peer, followed by a \033[4;36m/\033[0m\033[3;36m, and then your message.\033[0m\n");
    printf("\033[3;36m     Type \033[4;36m[broadcast]\033[0m \033[3;36minstead of \033[4;36mname\033[0m\033[3;36m to send to all active peers.\033[0m\n");
    printf("\033[0;34m[Recompile the program after setting the `\033[1;34mLOG\033[0;34m` macro to enable logging.]\033[0m\n");
    printf("\033[0;35m++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\033[0m\n\n");
  }

  for (;;)
  {
    // Filling descriptors in readset
    FD_SET(listenfd, &rset);
    FD_SET(stdinfd, &rset);
    maxfd = listenfd > stdinfd ? listenfd : stdinfd;
    for (i = 0; i < MAXPEERS; i++)
      if (myfdarr[i].live)
      {
        // Check last active time
        gettimeofday(&instant, NULL);

        lost_out = recv(myfdarr[i].fd, NULL, 1, MSG_PEEK | MSG_DONTWAIT) == 0;
        timed_out = instant.tv_sec - myfdarr[i].last_active_time >= TIMEOUT;
        if (lost_out || timed_out)
        {
          // Disconnect socket if the peer is lost
          myfdarr[i].live = 0;
          sendaddr.sin_family = AF_UNSPEC;
          if (connect(myfdarr[i].fd, (struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0)
            perror("disconnect");

          if (lost_out)
            printf("\033[3;33mDisconnected from %s. Peer not responsive.\033[0m\n", peersgroup[i].name);
          if (timed_out)
            printf("\033[3;33mDisconnected from %s. Peer inactive for too long.\033[0m\n", peersgroup[i].name);

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
      if (myfdarr[i].live && FD_ISSET(myfdarr[i].fd, &rset))
      {
        bzero(buffer1, sizeof(buffer1));
        printf("\033[0;34m%s\\\033[0m", peersgroup[i].name);
        read(myfdarr[i].fd, buffer1, sizeof(buffer1));
        puts(buffer1);
        if (LOG)
          write(myfdarr[i].fd, buffer1, sizeof(buffer1));

        // Update last active time for socket
        gettimeofday(&instant, NULL);
        myfdarr[i].last_active_time = instant.tv_sec;
      }

    // Handling connection listener
    if (FD_ISSET(listenfd, &rset))
    {
      len = sizeof(recvaddr);
      recvfd = accept(listenfd, (struct sockaddr *)&recvaddr, &len);

      // Read identity of connector
      if (read(recvfd, &curr_index, sizeof(curr_index)) < 0)
      {
        perror("accept");
        continue;
      }

      myfdarr[curr_index].fd = recvfd;
      myfdarr[curr_index].live = 1;

      // Update last active time for socket
      gettimeofday(&instant, NULL);
      myfdarr[curr_index].last_active_time = instant.tv_sec;
    }

    // Handling console input and sender
    if (FD_ISSET(stdinfd, &rset))
    {
      bzero(buffer2, sizeof(buffer2));
      read(stdinfd, buffer2, sizeof(buffer2));
      buffer2[strcspn(buffer2, "\n")] = 0; // Newline zapper
      message prntval = extract_message(buffer2);

      if (prntval.peer_index == -1)
        printf("\033[3;33mUnknown peer.\033[0m\n");
      else if (prntval.peer_index == my_index)
        printf("\033[0;34m(self)\\\033[0m%s\n", prntval.content);
      else if (prntval.peer_index == BROADCAST_ID)
      {
        for (i = 0; i < MAXPEERS; i++)
          if (myfdarr[i].live)
          {
            char temp[MAXLINE] = "(broadcast) ";
            strcat(temp, prntval.content);

            if (LOG)
              printf("\033[3;36mSending to %s: \033[0m%s\n", peersgroup[i].name, temp);

            write(myfdarr[i].fd, temp, sizeof(temp));

            if (LOG)
            {
              printf("\033[3;36mReceived from %s: \033[0m", peersgroup[i].name);
              read(myfdarr[i].fd, buffer2, sizeof(buffer2));
              puts(buffer2);
            }

            // Update last active time for socket
            gettimeofday(&instant, NULL);
            myfdarr[i].last_active_time = instant.tv_sec;
          }
      }
      else
      {
        if (LOG)
          printf("\033[3;36mSending to %s: \033[0m%s\n", peersgroup[prntval.peer_index].name, prntval.content);

        if (!myfdarr[prntval.peer_index].live)
        {
          sendaddr.sin_port = htons(peersgroup[prntval.peer_index].port);
          if (connect(myfdarr[prntval.peer_index].fd, (struct sockaddr *)&sendaddr, sizeof(sendaddr)) < 0)
          {
            printf("\033[3;33mPeer offline.\033[0m\n");
            // Print useful tip after every three connection failures
            if (!(fails++ % 3))
              printf("\033[3;36mTip: Make sure you actually have someone to talk to...\033[0m\n");
            continue;
          }
          // Reveal my identity to the acceptor
          write(myfdarr[prntval.peer_index].fd, &my_index, sizeof(my_index));
          myfdarr[prntval.peer_index].live = 1;
        }

        write(myfdarr[prntval.peer_index].fd, prntval.content, sizeof(prntval.content));

        if (LOG)
        {
          printf("\033[3;36mReceived from %s: \033[0m", peersgroup[prntval.peer_index].name);
          read(myfdarr[prntval.peer_index].fd, buffer2, sizeof(buffer2));
          puts(buffer2);
        }

        // Update last active time for socket
        gettimeofday(&instant, NULL);
        myfdarr[prntval.peer_index].last_active_time = instant.tv_sec;
      }
    }
  }
}

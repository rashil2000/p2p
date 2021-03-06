## Peer to Peer Chat Program

This program enables a number of users (upto `MAXPEERS`) to chat with each other using network sockets. Each user has a pre-defined port and IP address and this information is public.

### Running the program

The program files include a _makefile_, which can be used for compilation:
```
make p2p
```
The binary can now be run by providing your (own) username:
```
./p2p peer-one
```

This outputs the following:
```
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
     Hi peer-one! Welcome to this simple peer-to-peer chat program.
   Type the name of the peer, followed by a /, and then your message.
     Type [broadcast] instead of name to send to all active peers.
[Recompile the program after setting the `LOG` macro to enable logging.]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

As mentioned, the user can start chatting by providing the name of the **receiver**, following that by **/** and then the actual message.

A sample input/output is shown here:

| _peer-one_'s screen                                                                                                                                                      | _peer-two_'s screen                                                                                                                                                      |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `...`<br>`peer-two/Hey, you online?`<br>`peer-two\Yeah man, what's cooking?`<br>`peer-two/Nothing much, wanna play some Halo?`<br>`peer-two\Sure, let's start!`<br>`...` | `...`<br>`peer-one\Hey, you online?`<br>`peer-one/Yeah man, what's cooking?`<br>`peer-one\Nothing much, wanna play some Halo?`<br>`peer-one/Sure, let's start!`<br>`...` |

### Shared information

`MAXPEERS = 5`

| Port | IP        | Name       |
| ---: | --------- | ---------- |
| 5001 | 127.0.0.1 | peer-one   |
| 5002 | 127.0.0.1 | peer-two   |
| 5003 | 127.0.0.1 | peer-three |
| 5004 | 127.0.0.1 | peer-four  |
| 5005 | 127.0.0.1 | peer-five  |

### Algorithm

The program starts by defining the address structs, one for self and one for peers. It then initializes a collection of structs of sockets which will be used to read and write messages. A listener socket is bound to the self address, which listens for incoming connection requests.

At this point, the file descriptor read set is initialized for use in the `select()` call. An infinite loop now runs, in which the following take place:
1. The standard input and TCP listener sockets are inserted into the read set.
2. The collection of sockets we initialized earlier is checked for three things: one, whether a socket has been put in use yet (its `live` value is 1), two, whether the socket has reached a certain period of inactivity (10 minutes by default), and three, whether the connection on that socket is still alive. If all conditions are true, the socket is inserted into the read set. If the connection has been timed out or lost, the socket is reset.
3. The `select()` function is called with the read set.
4. Another loop runs on the collection of sockets, basically checking whether they're ready to read from. If yes, the `read()` function is called and the message is displayed on the console.
5. The listener socket is now checked as to whether it is ready to read from. If yes, the `accept()` function is called and a new connection is saved.
6. The standard input file descriptor is now checked whether it is ready to read from. If yes, the index of the receiver is extracted from the input. A conditional check runs to see whether we already have a connection to the aforementioned peer index. If not, the `connect()` function is called and a new connection is saved. The message is then written to the new connection using the `write()` function.

## Peer to Peer Chat Program

This program enables a number of users (upto `MAXPEERS`) located on the same machine to chat with each other using network sockets. Each user has a pre-defined port and this information is public.

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
[Recompile the program after setting the `LOG` macro to enable logging.]
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
```

As mentioned, the user can start chatting by providing the name of the **receiver**, following that by **/** and then the actual message.

A sample input/output is shown here:

| _peer-one_'s screen                                                                                                                                                      | _peer-two_'s screen                                                                                                                                                      |
| ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `...`<br>`peer-two/Hey, you online?`<br>`peer-two\Yeah man, what's cooking?`<br>`peer-two/Nothing much, wanna play some Halo?`<br>`peer-two\Sure, let's start!`<br>`...` | `...`<br>`peer-one\Hey, you online?`<br>`peer-one/Yeah man, what's cooking?`<br>`peer-one\Nothing much, wanna play some Halo?`<br>`peer-one/Sure, let's start!`<br>`...` |


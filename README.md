# Online chat on UNIX
Simple server for online chat. Server working like daemon, it contains parallel processing of clients.

Server contains 5 commands:

- /members_count - the number of chat members on the server.
- /members_list - the list of members on the server.
- /message_all <message> - send message to each member on the server.
- /message <username> <message> - send message to <username>
- /close - disconnecting from the server

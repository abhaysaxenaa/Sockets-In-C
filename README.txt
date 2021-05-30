Systems Programming - CS214 - Spring 2021
Project 3 - Networking


Name: Jesse Fisher
NetID: jaf490


Name: Abhay Saxena
NetID: ans192


IMPORTANT NOTE: We only considered the message code to be an ‘uppercase’ code, since this was the specified format in the project writeup. So, the server will only accept, for instance, “SET” and not “set”.


Testing Strategy:
The testing strategy for the program "server.c" revolved around exploring all possible input cases the server may encounter. We tested the server to ensure that it handles valid input correctly, and we also tested a variety of invalid inputs. We attempted to break the program by entering invalid entries such as “SETTING\n”, “SET\nhello\n”, and “SET\n6\nKEY\nthis is too long\n”. The program responds to each of these entries appropriately. We checked to ensure that when the client closes the connection, the corresponding thread terminates as well. In order to find and resolve any internal errors, we utilized various macros to check return values and provide useful debugging information. The server is able to handle multiple users interacting with it simultaneously, with the data stored in the linked list in a synchronized manner, retaining its integrity and avoiding any race conditions or non-deterministic behavior.


Compilation: Successfully ran and tested the program using UBSan, Address Sanitizer and Valgrind.
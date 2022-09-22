# Overview

In this assignment, we will convert an existing single-threaded web server into a multi-threaded web server. Note this is NOT a kernel project, and you should just develop your code on onyx, not in your virtual machine. Submissions fail to compile or run on onyx, will not be graded.

## Learning Objectives

- Gain more experience writing concurrent code.
- Understand a well-known concurrent programming problem - the producer-consumer problem.
- Explore the pthread library, and learn how to use locks and condition variables.

## Book References

Read these chapters carefully in order to prepare yourself for this assignment:

- [Threads API](http://pages.cs.wisc.edu/~remzi/OSTEP/threads-api.pdf) 
- [Locks](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-locks.pdf)
- [Condition Variables](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf)

In particular, the producer-consumer problem covered in the [Condition Variables](https://pages.cs.wisc.edu/~remzi/OSTEP/threads-cv.pdf) chapter is directly related to this assignment. The chapter actually used the multi-threaded web server as an example of the producer-consumer problem.

## Starter Code

The starter code looks like this:

```console
(base) [jidongxiao@onyx cs452-web-server]$ ls -R
.:
client.c  concurrent_server.c  index.html  Item.c  Item.h  list  Makefile  README.md  server.c  spin.c

./list:
include  lib  Makefile  README.md  SimpleTestList.c

./list/include:
List.h  Node.h

./list/lib:
libmylib.a  libmylib.so
```

You will be modifying *concurrent_server.c*. You should not modify other files in the same folder.

## Specification

You are required to implement the following two functions:

```c
void producer(int fd);
void *consumer(void *ptr);
```

You also need to modify the main() function.

To facilitate your implementation of the producer/consumer functions, a doubly linked list library is provided. As shown in the starter code, *./list/lib/libmylib.a* is the provided library. It is pre-compiled, meaning that you can use the library but you have no access to its source code. Coming with the library are two of its header files: List.h and Node.h, both are located in the list/include subfolder.

This library does not support multiple threads, thus when a program with multiple threads attempts to access this library - manipulating the doubly linked list, there will be race conditions and the results may not be deterministic, and that is why in this assignment we need to use locks so as to avoid race conditions.

## Data Structures Defined in the Doubly Linked List Library

The original list library defines *struct node* in *list/include/Node.h*:

```c
typedef struct node * NodePtr;

struct node {
        void *obj;
        struct node *next;
        struct node *prev;
};
```

And it defines *struct list* in *list/include/List.h*:

```c
typedef struct list * ListPtr;

struct list {
  int size;
  struct node *head;
  struct node *tail;
  int (*equals)(const void *, const void *);
  char * (*toString)(const void *);
  void (*freeObject)(void *);
};
```

The internal of these two structures are not really important to you. But you need to be aware of the *size* field of the *struct list*, which gives you the current size of the list. Knowing the current size of the list tells you if the list is full or not, and if the list is empty or not.

```c
struct list* createList(int (*equals)(const void *, const void *),
                        char *(*toString)(const void *),
                        void (*freeObject)(void *));
```

You do not really need to understand what these arguments are doing, but you can call *createList*() as following:

```c
struct list *list;
list=createList(compareToItem, toStringItem, freeItem); // yes, the name of the three arguments of this createList() must be exactly the same as what are used here, as they are actually three pre-defined functions, defined in *Item.c*.
```

## APIs Provided by the Doubly Linked List Library

You are recommended to use the following functions from the doubly linked list library:

```c
void addAtRear(struct list *list, struct node *node);
struct node *removeFront(struct list *list);
struct list* createList(int (*equals)(const void *, const void *),
                        char *(*toString)(const void *),
                        void (*freeObject)(void *));
struct node* createNode (void *obj);
void freeNode(struct node *node, void (*freeObject)(void *));
```

## Pthread APIs

I used the following pthread APIs:

- pthread_mutex_init()
- pthread_mutex_lock()
- pthread_mutex_unlock()
- pthread_mutex_destroy()
- pthread_cond_init()
- pthread_cond_wait()
- pthread_cond_signal()
- pthread_cond_broadcast()
- pthread_cond_destroy()

For each pthread API, read its man page to find out how to use it. 

## Testing and Expected Results

1. When running the default single thread web server, we get the following results:


server side, run this command to start the server and listen on port 8080.

```console
(base) [jidongxiao@onyxnode60 webserver]$ ./server -d ./ -p 8080
```

client side, run this command to send one http request:

```console
(base) [jidongxiao@onyxnode60 webserver]$ time ./client localhost 8080 /spin.cgi?5
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>

real	0m5.010s
user	0m0.000s
sys	0m0.004s
```

As can be seen that this one single request takes about 5 seconds to finish.

now, run this command to send 4 http requests:

```console
(base) [jidongxiao@onyxnode60 webserver]$ time seq 4 | xargs -n 1 -P 4 -I{} ./client localhost 8080 /spin.cgi?5
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
Header: HTTP/1.0 200 OK
Header: Server: WebServer
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
Header: HTTP/1.0 200 OK
Header: Server: WebServer
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
Header: HTTP/1.0 200 OK
Header: Server: WebServer
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>

real	0m20.038s
user	0m0.007s
sys	0m0.021s
```

And we can see that to satisfy these 4 requests, in total it takes about 20 seconds.

2. When running the multiple-threaded web server, we expect to get results similar to this:

server side, run this command to start the web server with 4 threads and all listen on port 8080.

```console
(base) [jidongxiao@onyxnode60 webserver]$ ./concurrent_server -d ./ -p 8080 -t 4
```

client side, run this command to send one http request:

```console
(base) [jidongxiao@onyxnode60 webserver]$ time ./client localhost 8080 /spin.cgi?5
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: Content-Length: 127
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>

real	0m5.013s
user	0m0.001s
sys	0m0.005s
```

it again takes about 5 seconds.

now, run this command to send 4 http requests:

```console
(base) [jidongxiao@onyxnode60 webserver]$ time seq 4 | xargs -n 1 -P 4 -I{} ./client localhost 8080 /spin.cgi?5
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: HTTP/1.0 200 OK
Header: Server: WebServer
Header: Content-Length: 127
Header: Content-Length: 127
Header: Content-Length: 127
Header: Content-Length: 127
Header: Content-Type: text/html
Header: Content-Type: text/html
Header: Content-Type: text/html
Header: Content-Type: text/html
<p>Welcome to the CGI program (5)</p>
<p>Welcome to the CGI program (5)</p>
<p>Welcome to the CGI program (5)</p>
<p>Welcome to the CGI program (5)</p>
<p>My only purpose is to waste time on the server!</p>
<p>My only purpose is to waste time on the server!</p>
<p>My only purpose is to waste time on the server!</p>
<p>My only purpose is to waste time on the server!</p>
<p>I spun for 5.00 seconds</p>
<p>I spun for 5.00 seconds</p>
<p>I spun for 5.00 seconds</p>
<p>I spun for 5.00 seconds</p>

real	0m5.021s
user	0m0.001s
sys	0m0.025s
```

this time we can see the improvements - even for 4 requests, our web server can still serve all of them in about 5 seconds, as opposed to 20 seconds.

## Submission 

23:59pm, October 20th, 2022. Late submissions will not be accepted/graded. 

## Project Layout

All files necessary for compilation and testing need to be submitted, this includes source code files, header files, the original library, the bash script, and Makefile. The structure of the submission folder should be the same as what was given to you.

## Grading Rubric (for Undergraduate and Graduate students)

All grading will be executed on onyx.boisestate.edu. Submissions that fail to compile on onyx will not be graded.
                                                                                     
- [80 pts] Functional Requirements:
  - [40 pts] testing program produces expected results, when testing with 4 clients, and 4 consumers, linked list capacity is 4, the testing result is approximately 5 seconds.
  - [40 pts] testing program produces expected results, when testing with 8 clients, and 4 consumers, linked list capacity is 4, the testing result is approximately 10 seconds.
- [10 pts] Compiling
  - Each compiler warning will result in a 3 point deduction.
  - You are not allowed to suppress warnings.
- [10 pts] Documentation:
  - README.md file: replace this current README.md with a new one using the template. Do not check in this current README.
  - You are required to fill in every section of the README template, missing 1 section will result in a 2-point deduction.

Note: Running valgrind is not required anymore for this and future assignments.

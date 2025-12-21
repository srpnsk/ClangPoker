# University Clang project

BREAKING UPDATE

Now, there's no more poker, as we were banned from creating gambling games.

And there's no more UNO, because we failed all the deadlines.

This is a chat server with rooms! Wow!

## But remember

Dear contributors, please, name binary files like "*.out"! (Это всегда надо делать?)

Unless you will make this repository full of trash!

## How to compile

Have a look at directory `src`.
There you can see 6 useful files.

### Server

You can get `server.out` binary this way:

```bash
gcc server.c msg.c utils.c -Wall -o server.out 
```

### Client

And to compile `client.out` you should run this command:

```bash
gcc client.c msg.c -Wall -o client.out -lncurses
```


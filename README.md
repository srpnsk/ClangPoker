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
gcc src/server.c src/msg.c src/utils.c -Wall -o server.out 
```

### Client

And to compile `client.out` you should run this command:

```bash
gcc src/client.c src/msg.c -Wall -o client.out -lncurses
```


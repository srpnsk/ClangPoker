# University Clang project

BREAKING UPDATE

Now, it's no more poker, as we were banned from creating gambling games.

Maybe, we will create Uno game, (because it's not gambling LOL).

## But remember

Dear contributors, please, name binary files like "*.out"! (Это всегда надо делать?)

Unless you will make this repository full of trash!

## How to compile

Have a look at directory `examples/pthreads` (we don't use pthreads lol).
There you can see a lot of useful (or useless) files.

### Server

You can get `server.out` binary this way:

```bash
gcc server.c protocol.c json_utils.c -lcjson -Wall -o server.out 
```

### Client

And to compile `ncurses_client.out` you should run this command:

```bash
gcc client_ncurses.c json_utils.c protocol.c client_ui.c -Wall -lncurses -lcjson -o ncurses_cli.out
```

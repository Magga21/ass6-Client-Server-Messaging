# Client and server


## Development Environment
The program was developed and tested on Ubuntu through WSL on Windows.

## Compile program
Compile both programs by running:

```bash
make
```

## Run the server
Run the program using:
```bash
./tsamserver <port>
```

For example:
```bash
./tsamserver 4044
```

## Run the client
Run the program using:
```bash
./tsamclient <IPaddress> <port>
```

For example:
```bash
./tsamclient 127.0.0.1 4044
```

## Commands implemented
```bash
SENDMSG,<from>,<to>,<message>
GETMSG,<username> 
STATUSREQ
```
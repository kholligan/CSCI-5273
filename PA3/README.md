# Distributed File System

## Build and make
```
$make clean ; make
$./dfs DFS1 10001&
$./dfs DFS2 10002&
$./dfs DFS3 10003&
$./dfs DFS4 10004&
$./dfc dfc.conf
```
Input commands are in the form of `%s %s` so for `list` and `exit` enter any second string to prevent it from hanging.

The client will split files being uploaded to the server into 4 equal parts. It will also use a simple cypher to "encrypt" the file. The file servers will create folders under their name ("DFS1" to "DFS4"), as well as sub folders for each user that successfully logs in to the server.

The client will list `[incomplete]` when there are not enough servers to recreate a file.

The `list` command only shows files placed on the server once the client was started. Needs to be rewritten.

Commands are `get [FILENAME]`, `put [FILENAME]`, `list [any char]`, `exit [any char]`

# C Webserver

## Build and make
```
$make clean ; make
$./webserver
```

By default, `ws.conf` has the port number set to `8000`. Feel free to configure this to a different port if you want to.

Establishes connections using TCP and forks new processes to handle concurrent requests. Auto forces all connections to keep-alive of 10s; have not implemented checking against the requested keep-alive header.

Only processes `GET` requests. All other requests result in a 501 error.

Error checking is not super robust. 400 Bad Request: Method gets captured under 501, etc.

Access the web page using telnet or your browser. Set. GO!

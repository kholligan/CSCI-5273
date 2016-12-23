# Web proxy server

Link to demo video: https://www.youtube.com/watch?v=_uPn9mI4iXE

Test results:
pages_from_server - PASS
pages_from_proxy  - PASS
pages_from_server_after_cache_timeout - FAIL
prefetching       - FAIL
multithreading  - PASS
connection keepalive - FAIL - Persistent connections to the proxy are supported, but did not explicitly set or handle connection keep-alives with the webserver

## Build and make
```
$make clean ; make
$./webproxy <PORT NUMBER> <TIMEOUT>
```

This is a simple webproxy server that reads in an HTTP request and sends it out to a webserver.
The webserver supports multiple persistent connections by forking the process to handle incoming requests.
When a new request comes in, we first check to see if the page is in the cache, and load from cache if it is. If not, we parse the host name from the URL and pass it to gethostbyname() to create a connection with the web server. We open a socket on port 80, send the request and read the response back. Each time we get a response we store the results in the singly linked cache, so that we can access it if we make the same request before the timeout. (Note: The cache is partially implemented. Add and retrieve features are implemented, but the pages do not timeout).

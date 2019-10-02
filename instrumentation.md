## Instrumentation

Bjoern can export connection and request metrics to statsd. You can enable statsd metrics on server boot by setting `enable_statsd=True`.

Following metrics are exposed:

| Name | Type | Description |
|:------:|:------:|:-------------|
| `conn.accept.error` | increment | Incremented if bjoern fails to accept the socket connection from client |
| `conn.accept.success` | increment | Incremented when a socket connection is accepted successfully |
| `req.error.client_disconnected` | increment | Incremented if client disconnects before bjoern could read request |
| `req.error.read` | increment | Incremented if bjoern fails to read request data from a valid connection |
| `req.error.parse` | increment | Incremented if bjoern fails to parse the content as valid HTTP request |
| `req.success.read` | increment | Incremented every time a request is read and parsed successfully |
| `req.error.internal` | increment | Incremented if bjoern fails to successfully call the WSGI application |
| `req.active` | increment | Incremented when a request is accepted as a valid one and bjoern is expecting more data from client |
| `req.done` | increment | Incremented when a request is read and parsed completely and no more data is expected from client, server is expected to send response now |
| `req.aborted` | increment | Incremented if the request is aborted before it was read or parsed completely. At the moment this is same as `req.error.read` but logically these are two different things and will diverge in future |
| `resp.active` | increment | Incremented when bjoern is sending response and |
| `resp.done` | increment | Incremented when bjoern is done sending the response |
| `resp.done.keepalive` | increment | Incremented when the response is sent but the connection is kept alive |
| `resp.conn.close` | increment | Incremented when the response is sent and connection is closed |
| `resp.aborted` | increment | Incremented when there is an error in sending response. At this point the connection is also closed |


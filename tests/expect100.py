from __future__ import print_function
import bjoern, socket, threading, time, json, sys, requests


responses = 0


def app(e, s):
    s('200 OK', [("Content-Length", "14")])
    return b"hello response"


def make_request(host, port, reports, i):
    global responses

    content_length = 1 << i
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    client.connect((host, port))
    request = ("GET /{} HTTP/1.1\r\n"
               "Accept: */*\r\n"
               "Expect: 100-continue\r\n"
               "Content-Length: {}\r\n\r\n").format(i, content_length)

    client.send(request.encode("utf-8"))
    print("Request for iteration {}".format(i))
    resp = client.recv(1 << 10)

    if resp == b"HTTP/1.1 100 Continue\r\n\r\n":
        client.send("".join("x" for x in range(0, content_length)).encode("utf-8"))
        resp = client.recv(1 << 10)
        reports.append({"request": request, "response": resp.decode("utf-8")})

    client.close()

    print("Response for iteration {}".format(i))
    responses = responses + 1


if __name__ == "__main__":
    host, port = "0.0.0.0", 8081
    sock = bjoern.listen(app, host, port, reuse_port=True)

    t = threading.Thread(target=bjoern.server_run, args=[sock, app])
    t.setDaemon(True)
    t.start()

    reports = []
    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 1

    for i in range(0, iterations):
        t = threading.Thread(target=make_request, args=[host, port, reports, i + 1])
        t.setDaemon(True)
        t.start()

    while responses < iterations:
        time.sleep(1)

    if len(reports) < iterations:
        print(json.dumps(reports), file=sys.stderr)
        sys.exit(1)


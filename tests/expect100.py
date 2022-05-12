from __future__ import print_function
import bjoern, socket, threading, time, json, sys, logging


def app(e, s):
    s('200 OK', [("Content-Length", "0")])
    return b""


def record_report(reports, request, resp):
    reports.append({"request": request, "response": resp.decode("utf-8")})


def expect_100_continue(test_num, i, request, resp, reports, client, content_length):
    if resp == b"HTTP/1.1 100 Continue\r\n\r\n":
        body = "".join("x" for x in range(0, content_length))

        logging.debug("Request body for test {}, iteration {}: {}".format(test_num, i, body))

        client.send(body.encode("utf-8"))

        logging.info("Request body sent for test {}, iteration {}".format(test_num, i))

        resp = client.recv(1 << 10)

        logging.info("Response 2 for test, {}, iteration {}".format(test_num, i))
    else:
        record_report(reports, request, resp)


def expect_417_expectation_failed(test_num, i, request, resp, reports, *args):
    if resp != b"HTTP/1.1 417 Expectation Failed\r\n\r\n":
        record_report(reports, request, resp)


def expect_200_ok(test_num, i, request, resp, reports, *args):
    if resp != b"HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: Keep-Alive\r\n\r\n":
        record_report(reports, request, resp)


def make_request(host, port, reports, responses, test_num, i,
                 expectation="100-continue", assertion=expect_100_continue,
                 body=None):

    content_length = 1 << i if not body else len(body)
    client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    client.connect((host, port))
    request = ("GET /{} HTTP/1.1\r\n"
               "Expect: {}\r\n"
               "Content-Length: {}\r\n\r\n").format(i, expectation, content_length)

    if body is not None:
        request = "{}{}".format(request, body)

    logging.info("Request for test {}, iteration {}: {}".format(test_num, i, request))
    client.send(request.encode("utf-8"))

    logging.info("Request sent for test {}, iteration {}".format(test_num, i))
    resp = client.recv(1 << 10)

    logging.info("Response 1 for test {}, iteration {}".format(test_num, i))

    assertion(test_num, i, request, resp, reports, client, content_length)

    client.close()

    logging.info("Response recv'ed for test {}, iteration {}".format(test_num, i))
    responses.append(resp)


def usage():
    print("usage: python {} $iterations $log_level".format(sys.argv[0]))


if __name__ == "__main__":

    if len(sys.argv) > 1 and sys.argv[1] in {"-h", "--help"}:
        usage()
        sys.exit(0)

    iterations = int(sys.argv[1]) if len(sys.argv) > 1 else 1
    level = getattr(logging, sys.argv[2].upper()) if len(sys.argv) > 2 else logging.WARNING

    host, port = "0.0.0.0", 8081
    sock = bjoern.listen(app, host, port, reuse_port=True)

    t = threading.Thread(target=bjoern.server_run, args=[sock, app])
    t.setDaemon(True)
    t.start()

    reports = []
    responses = []

    tests = [
        {"expectation": "100-continue", "assertion": expect_100_continue},
        {"expectation": "100-continue", "assertion": expect_100_continue, "body": ""},
        {"expectation": "badness", "assertion": expect_417_expectation_failed},
        {"expectation": "100-continue", "assertion": expect_200_ok, "body": "test"},
        {"expectation": "badness", "assertion": expect_417_expectation_failed, "body": "test"}
    ]

    logging.basicConfig(
        level=level,
        format='%(name)s - %(levelname)s - %(message)s'
    )

    for idx, test in enumerate(tests):
        request_count = iterations * (idx + 1)

        for i in range(0, iterations):
            t = threading.Thread(
                target=make_request,
                args=[host, port, reports, responses, idx + 1, i + 1],
                kwargs=test
            )
            t.start()

        while len(responses) < request_count:
            time.sleep(1)

    if reports:
        logging.error(json.dumps(reports, indent=4))
        sys.exit(1)


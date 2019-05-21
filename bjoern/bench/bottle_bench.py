import json

import bjoern
import bottle

bottle.BaseRequest.MEMFILE_MAX = 1024 * 1024
app = bottle.Bottle(__name__)


@app.get("/a/b/c")
def bench_get():
    k = bottle.request.params.get("k")
    k2 = bottle.request.params.get("k2")

    bottle.response.content_type = "application/json"
    return json.dumps({"k": k, "k2": k2})


@app.post("/a/b/c")
def bench_post():
    k = bottle.request.params.get("k")
    k2 = bottle.request.params.get("k2")
    asdfghjkl = bottle.request.forms["asdfghjkl"]
    image = bottle.request.forms["image"]

    bottle.response.content_type = "application/json"
    return json.dumps({"k": k, "k2": k2, "asdfghjkl": asdfghjkl, "image": image})


if __name__ == "__main__":
    bjoern.run(app, "0.0.0.0", 8080)

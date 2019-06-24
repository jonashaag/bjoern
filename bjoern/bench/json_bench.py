import bjoern
from flask import Flask, jsonify, request
from werkzeug.exceptions import abort

app = Flask(__name__)


@app.route("/a/b/c", methods=("GET", "POST"))
def bench():
    if request.method == "GET":
        k = request.args.get("k")
        k2 = request.args.get("k2")

        return jsonify({"k": k, "k2": k2})
    elif request.method == "POST":
        k = request.args.get("k")
        k2 = request.args.get("k2")
        _json = request.get_json()
        image = _json.get("data")

        return jsonify({"k": k, "k2": k2, "image": image})

    abort(400)


if __name__ == "__main__":
    bjoern.run(app, "0.0.0.0", 8080)

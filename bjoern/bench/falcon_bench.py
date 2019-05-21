import json

import bjoern
import falcon


class AppResource:
    def on_get(self, request, response):
        k = request.get_param("k")
        k2 = request.get_param("k2")

        response.status_code = 200
        response.content_type = "application/json"
        response.body = json.dumps({"k": k, "k2": k2})

    def on_post(self, request, response):
        k = request.get_param("k")
        k2 = request.get_param("k2")
        asdfghjkl = request.get_param("asdfghjkl")
        qwerty = request.get_param("qwerty")
        image = request.get_param("image")

        response.status = falcon.HTTP_200
        response.content_type = "application/json"
        response.body = json.dumps(
            {"k": k, "k2": k2, "asdfghjkl": asdfghjkl, "qwerty": qwerty, "image": image}
        )


app = falcon.API()
app.req_options.auto_parse_form_urlencoded = True

resource = AppResource()


app.add_route("/a/b/c", resource)


if __name__ == "__main__":
    bjoern.run(app, "0.0.0.0", 8080)

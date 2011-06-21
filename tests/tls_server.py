from hello import wsgi_app

# openssl genrsa 1024 > ssl.key
# openssl req -new -key ssl.key -out ssl.csr
# openssl req -days 36500 -x509 -key ssl.key -in ssl.csr > ssl.crt 

import bjoern
bjoern.key_path = "ssl.key"
bjoern.cert_path = "ssl.crt"

bjoern.run(wsgi_app, '0.0.0.0', 8080)

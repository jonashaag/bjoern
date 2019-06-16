SHELL=/bin/bash

.PHONY: default setup test all again clean
default: test

SOURCE_DIR	:= src
BUILD_DIR	:= build
PYTHON35	:= /.py35-venv/bin/python3
PIP35		:= /.py35-venv/bin/pip3
PYTHON36	:= /.py36-venv/bin/python3
PIP36		:= /.py36-venv/bin/pip3
GUNICORN36	:= /.py36-venv/bin/gunicorn
GUNICORN37	:= /.py37-venv/bin/gunicorn
PYTHON37	:= /.py37-venv/bin/python3
PIP37		:= /.py37-venv/bin/pip3
DEBUG 		:= DEBUG=True

PYTHON35_INCLUDE	:= $(shell python3-config --includes | sed s/-I/-isystem\ /g)
PYTHON35_LDFLAGS	:= $(shell python3-config --ldflags)
PYTHON36_INCLUDE	:= $(shell python3-config --includes | sed s/-I/-isystem\ /g)
PYTHON36_LDFLAGS	:= $(shell python3-config --ldflags)
PYTHON37_INCLUDE	:= $(shell python3.7-config --includes | sed s/-I/-isystem\ /g)
PYTHON37_LDFLAGS	:= $(shell python3.7-config --ldflags)


HTTP_PARSER_DIR	:= vendors/http-parser
HTTP_PARSER_OBJ := $(HTTP_PARSER_DIR)/http_parser.o
HTTP_PARSER_SRC := $(HTTP_PARSER_DIR)/http_parser.c

HTTP_PARSER_URL_DIR	:= vendors/http-parser
HTTP_PARSER_URL_OBJ := $(HTTP_PARSER_URL_DIR)/url_parser

objects		:= 	$(HTTP_PARSER_OBJ) $(HTTP_PARSER_URL_OBJ) \
		  		$(patsubst $(SOURCE_DIR)/%.c, $(BUILD_DIR)/%.o, \
		             $(wildcard $(SOURCE_DIR)/*.c))
FEATURES :=
ifneq ($(WANT_SENDFILE), no)
FEATURES	+= -D WANT_SENDFILE
endif

ifneq ($(WANT_SIGINT_HANDLING), no)
FEATURES	+= -D WANT_SIGINT_HANDLING
endif

ifneq ($(WANT_SIGNAL_HANDLING), no)
FEATURES	+= -D WANT_SIGNAL_HANDLING
endif

ifndef SIGNAL_CHECK_INTERVAL
FEATURES	+= -D SIGNAL_CHECK_INTERVAL=0.1
endif
CC 				:= gcc
CPPFLAGS_35		+= $(PYTHON35_INCLUDE) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR)
CPPFLAGS_36		+= $(PYTHON36_INCLUDE) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR)
CPPFLAGS_37		+= $(PYTHON37_INCLUDE) -I . -I $(SOURCE_DIR) -I $(HTTP_PARSER_DIR)
CFLAGS			+= $(FEATURES) -std=c99 -fno-strict-aliasing -fcommon -fPIC -Wall -D DEBUG
LDFLAGS_35		+= $(PYTHON35_LDFLAGS) -shared -fcommon
LDFLAGS_36		+= $(PYTHON36_LDFLAGS) -shared -fcommon
LDFLAGS_37		+= $(PYTHON37_LDFLAGS) -shared -fcommon

AB			:= ab -c 100 -n 10000
TEST_URL	:= "http://127.0.0.1:8080/a/b/c?k=v&k2=v2"

IMAGE_B64 						:= $(shell cat bjoern/tests/charlie.jpg | base64 | xargs urlencode)
IMAGE_B64_LEN 					:= $(shell cat bjoern/tests/charlie.jpg | base64 | xargs urlencode | wc -c)
flask_bench_36 					:= bjoern/bench/flask_py36.txt
flask_gworker_bench_36 			:= bjoern/bench/flask_gworker_py36.txt
flask_gunicorn_bench_36 		:= bjoern/bench/flask_gunicorn_py36.txt
flask_gworker_bench_thread_36 	:= bjoern/bench/flask_gworker_thread_py36.txt
flask_gworker_bench_multi_36 	:= bjoern/bench/flask_gworker_multi_py36.txt
bottle_bench_36 				:= bjoern/bench/bottle_py36.txt
falcon_bench_36 				:= bjoern/bench/falcon_py36.txt
flask_valgrind_36			 	:= bjoern/bench/flask_valgrind_py36.mem
flask_callgrind_36			 	:= bjoern/bench/flask_callgrind_py36.mem
flask_bench_37 					:= bjoern/bench/flask_py37.txt
bottle_bench_37 				:= bjoern/bench/bottle_py37.txt
falcon_bench_37 				:= bjoern/bench/falcon_py37.txt
flask_gunicorn_bench_37 		:= bjoern/bench/flask_gunicorn_py37.txt
flask_gworker_bench_37 			:= bjoern/bench/flask_gworker_py37.txt
flask_gworker_bench_thread_37 	:= bjoern/bench/flask_gworker_thread_py37.txt
flask_gworker_bench_multi_37 	:= bjoern/bench/flask_gworker_multi_py37.txt
flask_valgrind_37			 	:= bjoern/bench/flask_valgrind_py37.mem
flask_callgrind_37			 	:= bjoern/bench/flask_callgrind_py37.mem
ab_post 						:= /tmp/bjoern.post

# Targets
setup-35: clean prepare-build reqs-35
setup-36: clean prepare-build reqs-36
setup-37: clean prepare-build reqs-37

all-35: setup-35 $(objects) _bjoernmodule_35 test-35
all-36: setup-36 $(objects) _bjoernmodule_36 test-36
all-37: setup-36 $(objects) _bjoernmodule_37 test-37

print-env:
	@echo CFLAGS=$(CFLAGS)
	@echo CPPFLAGS_36=$(CPPFLAGS_36)
	@echo LDFLAGS_36=$(LDFLAGS_36)
	@echo CPPFLAGS_37=$(CPPFLAGS_37)
	@echo LDFLAGS_37=$(LDFLAGS_37)
	@echo args=$(HTTP_PARSER_SRC) $(wildcard $(SOURCE_DIR)/*.c)
	@echo FEATURES=$(FEATURES)

_bjoernmodule_35:
	@$(CC) $(CPPFLAGS_35) $(CFLAGS) $(LDFLAGS_35) $(objects) -o $(BUILD_DIR)/_bjoern.so -lev
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON35) -c 'import bjoern;print(f"Bjoern version: {bjoern.__version__}");'

_bjoernmodule_36:
	@$(CC) $(CPPFLAGS_36) $(CFLAGS) $(LDFLAGS_36) $(objects) -o $(BUILD_DIR)/_bjoern.so -lev
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) -c 'import bjoern;print(f"Bjoern version: {bjoern.__version__}");'

_bjoernmodule_37:
	@$(CC) $(CPPFLAGS_37) $(CFLAGS) $(LDFLAGS_37) $(objects) -o $(BUILD_DIR)/_bjoern.so -lev
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) -c 'import bjoern;print(f"Bjoern version: {bjoern.__version__}");'

again-36: all-36
again-37: all-37

$(BUILD_DIR)/%.o: $(SOURCE_DIR)/%.c
	@echo ' -> ' $(CC) $(CPPFLAGS_36) $(CFLAGS) -c $< -o $@
	@$(CC) $(CPPFLAGS_36) $(CFLAGS) -c $< -o $@

# foo.o: shortcut to $(BUILD_DIR)/foo.o
%.o: $(BUILD_DIR)/%.o

reqs-35:
	@bash install-requirements $(PIP35)

reqs-36:
	@bash install-requirements $(PIP36)

reqs-37:
	@bash install-requirements $(PIP37)

fmt:
	@$(PYTHON36) -m isort --settings-path=/.isort.cfg **/*.py
	@$(PYTHON36) -m black .

prepare-build:
	@mkdir -p $(BUILD_DIR)

clean:
	@rm -rf $(BUILD_DIR)/*
	@rm -rf _bjoern.*.so
	@rm -rf *.egg-info
	@rm -rf dist/*
	@rm -f /tmp/*.tmp

# Test
test-35: fmt clean reqs-36 install-debug-35
	@$(PYTHON35) -m pytest

# Test
test-36: fmt clean reqs-36 install-debug-36
	@$(PYTHON36) -m pytest

test-37: fmt clean reqs-37 install-debug-37
	@$(PYTHON37) -m pytest

test: test-37 test-36

# Benchmarks
$(ab_post):
	@echo 'asdfghjkl=asdfghjkl&qwerty=qwertyuiop&image=$(IMAGE_B64)' > "$@"
	@echo $(IMAGE_B64_LEN)

$(flask_bench_36):
	@$(PYTHON36) bjoern/bench/flask_bench.py --log-level inf & jobs -p >/var/run/flask_bjoern.bench.pid
	@sleep 2

flask-ab-36: $(flask_bench_36) $(ab_post)
	@echo -e "\n====== Flask(Python3.6) ======\n" | tee -a $(flask_bench_36)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_bench_36)
	@$(AB) $(TEST_URL) | tee -a $(flask_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_bench_36)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_bench_36)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_bench_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_bench_36)
	@killall -9 $(PYTHON36)

$(flask_gworker_bench_multi_36):
	@$(GUNICORN36) bjoern.bench.flask_bench:app --bind localhost:8080 --log-level info -w 4 --backlog 2048 --timeout 1800 --worker-class bjoern.gworker.BjoernWorker &
	@sleep 2

flask-ab-gworker-multi-36: $(flask_gworker_bench_multi_36) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn-BjoernWorker-multiworkers (Python3.6) ======\n" | tee -a $(flask_gworker_bench_multi_36)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gworker_bench_multi_36)
	@$(AB) $(TEST_URL) | tee -a $(flask_gworker_bench_multi_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_multi_36)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gworker_bench_multi_36)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gworker_bench_multi_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_multi_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gworker_bench_multi_36)
	@killall -9 gunicorn

$(flask_gworker_bench_36):
	@$(GUNICORN36) bjoern.bench.flask_bench:app --bind localhost:8080 --log-level info --backlog 2048 --timeout 1800 --worker-class bjoern.gworker.BjoernWorker &
	@sleep 2

flask-ab-gworker-36: $(flask_gworker_bench_36) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn-BjoernWorker(Python3.6) ======\n" | tee -a $(flask_gworker_bench_36)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gworker_bench_36)
	@$(AB) $(TEST_URL) | tee -a $(flask_gworker_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_36)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gworker_bench_36)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gworker_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gworker_bench_36)
	@killall -9 gunicorn

$(flask_gunicorn_bench_36):
	@$(GUNICORN36) bjoern.bench.flask_bench:app --bind localhost:8080 --log-level info --backlog 2048 --timeout 1800 &
	@sleep 2

flask-ab-gunicorn-36: $(flask_gunicorn_bench_36) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn(Python3.6) ======\n" | tee -a $(flask_gunicorn_bench_36)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gunicorn_bench_36)
	@$(AB) $(TEST_URL) | tee -a $(flask_gunicorn_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gunicorn_bench_36)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gunicorn_bench_36)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gunicorn_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gunicorn_bench_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gunicorn_bench_36)
	@killall -9 gunicorn

$(bottle_bench_36):
	@$(PYTHON36) bjoern/bench/bottle_bench.py &
	@sleep 2

bottle-ab-36: $(bottle_bench_36) $(ab_post)
	@echo -e "\n====== Bottle(Python3.6) ======\n" | tee -a $(bottle_bench_36)
	@echo -e "\n====== GET ======\n" | tee -a $(bottle_bench_36)
	@$(AB) $(TEST_URL) | tee -a $(bottle_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(bottle_bench_36)
	@$(AB) -k $(TEST_URL) | tee -a $(bottle_bench_36)
	@echo -e "\n====== POST ======\n" | tee -a $(bottle_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(bottle_bench_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(bottle_bench_36)
	@killall -9 $(PYTHON36)

$(falcon_bench_36):
	@$(PYTHON36) bjoern/bench/falcon_bench.py &
	@sleep 2

falcon-ab-36: $(falcon_bench_36) $(ab_post)
	@echo -e "\n====== Falcon(Python3.6) ======\n" | tee -a $(falcon_bench_36)
	@echo -e "\n====== GET ======\n" | tee -a $(falcon_bench_36)
	@$(AB) $(TEST_URL) | tee -a $(falcon_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(falcon_bench_36)
	@$(AB) -k $(TEST_URL) | tee -a $(falcon_bench_36)
	@echo -e "\n====== POST ======\n" | tee -a $(falcon_bench_36)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(falcon_bench_36)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(falcon_bench_36)
	@killall -9 $(PYTHON36)

_clean_bench_36:
	@rm -rf bjoern/bench/*36.txt

bjoern-bench-36: _clean_bench_36 setup-36 install-36-bench flask-ab-36 bottle-ab-36 falcon-ab-36 flask-ab-gunicorn-36 flask-ab-gworker-36 flask-ab-gworker-multi-36
bjoern-legacy-bench-36: _clean_bench_36 setup-36 install-legacy-bench-36 flask-ab-36 bottle-ab-36 falcon-ab-36


$(flask_bench_37):
	@$(PYTHON37) bjoern/bench/flask_bench.py & jobs -p >/var/run/flask_bjoern.bench.pid
	@sleep 2

flask-ab-37: $(flask_bench_37) $(ab_post)
	@echo -e "\n====== Flask(Python3.7) ======\n" | tee -a $(flask_bench_37)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_bench_37)
	@$(AB) $(TEST_URL) | tee -a $(flask_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_bench_37)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_bench_37)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_bench_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_bench_37)
	@killall -9 $(PYTHON37)

$(flask_gworker_bench_multi_37):
	@$(GUNICORN37) bjoern.bench.flask_bench:app --bind localhost:8080 --log-level info -w 4 --backlog 2048 --timeout 1800 --worker-class bjoern.gworker.BjoernWorker &
	@sleep 2

flask-ab-gworker-multi-37: $(flask_gworker_bench_multi_37) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn-BjoernWorker-multiworkers (Python3.7) ======\n" | tee -a $(flask_gworker_bench_multi_37)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gworker_bench_multi_37)
	@$(AB) $(TEST_URL) | tee -a $(flask_gworker_bench_multi_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_multi_37)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gworker_bench_multi_37)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gworker_bench_multi_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_multi_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gworker_bench_multi_37)
	@killall -9 gunicorn

$(flask_gworker_bench_37):
	@$(GUNICORN37) bjoern.bench.flask_bench:app --backlog 2048 --timeout 1800 --worker-class bjoern.gworker.BjoernWorker &
	@sleep 2

flask-ab-gworker-37: $(flask_gworker_bench_37) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn-BjoernWorker(Python3.7) ======\n" | tee -a $(flask_gworker_bench_37)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gworker_bench_37)
	@$(AB) $(TEST_URL) | tee -a $(flask_gworker_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_37)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gworker_bench_37)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gworker_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gworker_bench_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gworker_bench_37)
	@killall -9 gunicorn

$(flask_gunicorn_bench_37):
	@$(GUNICORN37) bjoern.bench.flask_bench:app --backlog 2048 --timeout 1800 &
	@sleep 2

flask-ab-gunicorn-37: $(flask_gunicorn_bench_37) $(ab_post)
	@echo -e "\n====== Flask-Gunicorn(Python3.7) ======\n" | tee -a $(flask_gunicorn_bench_37)
	@echo -e "\n====== GET ======\n" | tee -a $(flask_gunicorn_bench_37)
	@$(AB) $(TEST_URL) | tee -a $(flask_gunicorn_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gunicorn_bench_37)
	@$(AB) -k $(TEST_URL) | tee -a $(flask_gunicorn_bench_37)
	@echo -e "\n====== POST ======\n" | tee -a $(flask_gunicorn_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(flask_gunicorn_bench_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(flask_gunicorn_bench_37)
	@killall -9 gunicorn

$(bottle_bench_37):
	@$(PYTHON37) bjoern/bench/bottle_bench.py & jobs -p >/var/run/bottle_bjoern.bench.pid
	@sleep 2

bottle-ab-37: $(bottle_bench_37) $(ab_post)
	@echo -e "\n====== Falcon(Python3.7) ======\n" | tee -a $(bottle_bench_37)
	@echo -e "\n====== GET ======\n" | tee -a $(bottle_bench_37)
	@$(AB) $(TEST_URL) | tee -a $(bottle_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(bottle_bench_37)
	@$(AB) -k $(TEST_URL) | tee -a $(bottle_bench_37)
	@echo -e "\n====== POST ======\n" | tee -a $(bottle_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(bottle_bench_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(bottle_bench_37)
	@killall -9 $(PYTHON37)

$(falcon_bench_37):
	@$(PYTHON37) bjoern/bench/falcon_bench.py & jobs -p >/var/run/falcon_bjoern.bench.pid
	@sleep 2

falcon-ab-37: $(falcon_bench_37) $(ab_post)
	@echo -e "\n====== Falcon(Python3.7) ======\n" | tee -a $(falcon_bench_37)
	@echo -e "\n====== GET ======\n" | tee -a $(falcon_bench_37)
	@$(AB) $(TEST_URL) | tee -a $(falcon_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(falcon_bench_37)
	@$(AB) -k $(TEST_URL) | tee -a $(falcon_bench_37)
	@echo -e "\n====== POST ======\n" | tee -a $(falcon_bench_37)
	@echo -e "\n~~~~~ Keep Alive ~~~~~\n" | tee -a $(falcon_bench_37)
	$(AB) -T 'application/x-www-form-urlencoded' -T 'Expect: 100-continue' -k -p $(ab_post) $(TEST_URL) | tee -a $(falcon_bench_37)
	@killall -9 $(PYTHON37)

_clean_bench_37:
	@rm -rf bjoern/bench/*37.txt

bjoern-bench-37: _clean_bench_37 setup-37 install-37-bench flask-ab-gunicorn-37 flask-ab-37 bottle-ab-37 falcon-ab-37 flask-ab-gworker-37 flask-ab-gworker-multi-37
bjoern-legacy-bench-37: _clean_bench_37 setup-37 install-legacy-bench-37 flask-ab-37 bottle-ab-37 falcon-ab-37

bjoern-bench: bjoern-bench-37 bjoern-bench-36

# Memory checks
flask-valgrind-36: install-debug-36
	valgrind --leak-check=full --show-reachable=yes $(PYTHON36) bjoern/tests/test_flask.py > $(flask_valgrind_36) 2>&1

flask-callgrind-36: install-debug-36
	valgrind --tool=callgrind $(PYTHON36) bjoern/tests/test_flask.py > $(flask_callgrind_36) 2>&1

memwatch-36:
	watch -n 0.5 \
	  'cat /proc/$$(pgrep -n $(PYTHON36))/cmdline | tr "\0" " " | head -c -1; \
	   echo; echo; \
	   tail -n +25 /proc/$$(pgrep -n $(PYTHON36))smaps'

flask-valgrind-37: install-debug-37
	valgrind --leak-check=full --show-reachable=yes $(PYTHON37) bjoern/tests/test_flask.py > $(flask_valgrind_37) 2>&1

flask-callgrind-37: install-debug-37
	valgrind --tool=callgrind $(PYTHON37) bjoern/tests/test_flask.py  > $(flask_callgrind_37) 2>&1

memwatch-37:
	watch -n 0.5 \
	  'cat /proc/$$(pgrep -n $(PYTHON37))/cmdline | tr "\0" " " | head -c -1; \
	   echo; echo; \
	   tail -n +25 /proc/$$(pgrep -n $(PYTHON37))smaps'

valgrind: flask-valgrind-36 flask-callgrind-36 flask-valgrind-37 flask-callgrind-37

flask-valgrind-legacy-36: install-legacy-bench-36
	valgrind --leak-check=full --show-reachable=yes $(PYTHON36) bjoern/tests/test_flask.py > $(flask_valgrind_36) 2>&1

flask-callgrind-legacy-36: install-legacy-bench-36
	valgrind --tool=callgrind $(PYTHON36) bjoern/tests/test_flask.py > $(flask_callgrind_36) 2>&1

flask-valgrind-legacy-37: install-legacy-bench-37
	valgrind --leak-check=full --show-reachable=yes $(PYTHON37) bjoern/tests/test_flask.py > $(flask_valgrind_37) 2>&1

flask-callgrind-legacy-37: install-legacy-bench-37
	valgrind --tool=callgrind $(PYTHON37) bjoern/tests/test_flask.py > $(flask_callgrind_37) 2>&1

valgrind-legacy: flask-valgrind-legacy-36 flask-callgrind-legacy-36 flask-valgrind-legacy-37 flask-callgrind-legacy-37

# Pypi
extension-36:
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) setup.py build_ext

extension-37:
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) setup.py build_ext

install-debug-35:
	@DEBUG=True PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON35) -m pip install --editable .

install-debug-36:
	@DEBUG=True PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) -m pip install --editable .

uninstall-36: clean
	@DEBUG=False PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) -m pip uninstall -y bjoern

install-36:
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) -m pip install --editable .

install-36-bench: uninstall-36 extension-36
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON36) setup.py install

install-legacy-bench-36: uninstall-36
	@$(PIP36) install --upgrade --index-url=https://pypi.org/simple bjoern

install-legacy-bench-37: uninstall-37
	@$(PIP37) install --upgrade --index-url=https://pypi.org/simple bjoern

upload-36:
	$(PYTHON36) setup.py sdist
	$(PYTHON36) -m twine upload --repository=robbie-pypi dist/*.tar.gz

wheel-36:
	$(PYTHON36) setup.py bdist_wheel

install-debug-37:
	@DEBUG=True PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) -m pip install --editable .

uninstall-37: clean
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) -m pip uninstall -y bjoern

install-37:
	@DEBUG=False @PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) -m pip install --editable .

install-37-bench: uninstall-37 extension-37
	@PYTHONPATH=$$PYTHONPATH:$(BUILD_DIR) $(PYTHON37) setup.py install

upload-37:
	$(PYTHON36) setup.py sdist
	$(PYTHON36) -m twine upload --repository=robbie-pypi dist/*.tar.gz

wheel-37:
	$(PYTHON37) setup.py bdist_wheel

upload-wheel-36: wheel-36
	@$(PYTHON36) -m twine upload --skip-existing dist/*.whl

upload-wheel-37: wheel-37
	@$(PYHHON37) -m twine upload --skip-existing dist/*.whl

# Vendors
http-parser:
	# http-parser 2.9.2
	@cd $(HTTP_PARSER_DIR) && git checkout 5c17dad400e45c5a442a63f250fff2638d144682

$(HTTP_PARSER_OBJ): http-parser
	$(MAKE) -C $(HTTP_PARSER_DIR) http_parser.o url_parser CFLAGS_DEBUG_EXTRA=-fPIC CFLAGS_FAST_EXTRA="-pthread -fPIC -march='core-avx2' -mtune='core-avx2'"

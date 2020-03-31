all: manylinux2014 manylinux2010 manylinux1

manylinux%:
	IMAGE=quay.io/pypa/$@_x86_64 docker-compose run --rm builder

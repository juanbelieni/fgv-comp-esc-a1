VERSION := 1.55.0
INSTALL_DIR := $(HOME)/.local
NUM_JOBS := 4

install-apt:
	sudo apt install -y cmake               \
						build-essential     \
						autoconf            \
						libtool             \
						pkg-config

install-grpc-python:
	pip3 install grpcio grpcio-tools

configure-grpc-cpp:
	mkdir -p $(INSTALL_DIR) && \
	cd $(INSTALL_DIR) && \
	git clone -b v$(VERSION) --depth 1 https://github.com/grpc/grpc && \
	cd grpc && \
	git submodule update --init && \

	mkdir -p cmake/build

# Demora uma eternidade
install-grpc-cpp:
	cd cmake/build && \
	cmake ../.. && \
	make -j $(NUM_JOBS) && \

	cmake ../.. -DgRPC_INSTALL=ON               \
				-DCMAKE_BUILD_TYPE=Release      \
				-DgRPC_ABSL_PROVIDER=module     \
				-DgRPC_CARES_PROVIDER=module    \
				-DgRPC_PROTOBUF_PROVIDER=module \
				-DgRPC_RE2_PROVIDER=module      \
				-DgRPC_SSL_PROVIDER=module      \
				-DgRPC_ZLIB_PROVIDER=package && \

	make -j $(NUM_JOBS) && \
	make -j $(NUM_JOBS) install

generate-simulator:
	python3 -m grpc_tools.protoc \
		-I. \
		--python_out=./highway-simulator \
		--pyi_out=./highway-simulator \
		--grpc_python_out=./highway-simulator \
		proto/simulation.proto

build-server:
	mkdir -p ETL/proto && \
	mkdir -p build && \
	cd build && \
	cmake .. -Wno-dev && \
	make -j $(NUM_JOBS)

start-simulator:
	python3 main.py

start-server:
	./server

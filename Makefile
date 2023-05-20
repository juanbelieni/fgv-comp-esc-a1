
generate-cpp:
	protoc -I. \
		--cpp_out=./ETL \
		--grpc_out=./ETL \
		--plugin=protoc-gen-grpc=`which grpc_cpp_plugin` \
		./proto/*.proto

generate-python:
	protoc -I. \
		--python_out=./highway-simulator \
		--grpc_out=./highway-simulator \
		--plugin=protoc-gen-grpc=`which grpc_python_plugin` \
		./proto/*.proto

generate: generate-cpp generate-python

start-simulator:
	cd highway-simulator && python3 main.py

compilacion

g++ client.cpp mensaje.pb.cc -lprotobuf -pthread -o client

g++ server.cpp mensaje.pb.cc -lprotobuf -pthread -o server

protoc -I=. --cpp_out=. mensaje.proto
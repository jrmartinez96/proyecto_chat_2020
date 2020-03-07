# PROYECTO CHAT

# Compilacion de proto
`protoc -I=. --cpp_out=. mensaje.proto`

# Compilacion de cliente y servidor

`g++ client.cpp mensaje.pb.cc -lprotobuf -pthread -o client`

`g++ server.cpp mensaje.pb.cc -lprotobuf -pthread -o server`

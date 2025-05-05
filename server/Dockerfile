FROM gcc
RUN apt-get update && apt-get install -y \
    net-tools \
    bash

WORKDIR /app
COPY server.cpp .
RUN g++ server.cpp -o server -l pthread
EXPOSE 8080/udp

CMD ["./server"]

FROM gcc
RUN apt-get update && apt-get install -y \
    net-tools \
    bash \
    iptables \
    iproute2
WORKDIR /app
COPY ./client.cpp .
RUN g++ client.cpp -o client -l pthread

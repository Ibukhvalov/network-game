FROM gcc
WORKDIR /app
COPY ./client.cpp .
RUN g++ client.cpp -o client -l pthread

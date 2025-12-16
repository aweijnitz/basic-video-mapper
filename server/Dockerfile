FROM ubuntu:22.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential \
        cmake \
        libsqlite3-dev \
        sqlite3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN cmake -S . -B build \
    && cmake --build build

EXPOSE 8080

VOLUME ["/data/db", "/data/assets"]

CMD ["/app/build/server/lumi_server"]

FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends build-essential perl ca-certificates curl && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY get-sources.sh /usr/local/bin/get-sources.sh
RUN sh /usr/local/bin/get-sources.sh /src/openssl-1.0.2u.tar.gz && tar xzf openssl-1.0.2u.tar.gz && rm openssl-1.0.2u.tar.gz
WORKDIR /src/openssl-1.0.2u
RUN ./config no-shared no-asm enable-ssl3 enable-ssl3-method enable-weak-ssl-ciphers enable-rc4 --prefix=/opt/legacy && make -j2 && make install_sw
COPY proxy.c /src/proxy.c
RUN cc -O2 -Wall -Wextra /src/proxy.c -I/opt/legacy/include /opt/legacy/lib/libssl.a /opt/legacy/lib/libcrypto.a -ldl -pthread -o /opt/legacy/proxy
FROM debian:bookworm-slim
COPY --from=build /opt/legacy /opt/legacy
COPY entrypoint.sh /entrypoint.sh
ENTRYPOINT ["/bin/sh","/entrypoint.sh"]

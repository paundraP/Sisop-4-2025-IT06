FROM alpine:latest

RUN apk update && \
    apk add --no-cache gcc make fuse3-dev fuse3 pkgconf musl-dev

COPY antink.c /app/antink.c

WORKDIR /app

RUN gcc antink.c -o antink_fs $(pkg-config fuse3 --cflags --libs)

RUN mkdir -p /it24_host /antink_mount /var/log

CMD ["./antink_fs", "/it24_host", "/antink_mount"]
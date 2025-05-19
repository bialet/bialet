# Install library dependencies
FROM alpine:latest AS base
RUN apk add musl-dev sqlite-dev openssl-dev curl-dev

# Install compilation tools
FROM base AS build
RUN apk add gcc make
COPY . /usr/src
WORKDIR /usr/src
RUN make clean && make

# Copy bialet binary
FROM base as run
COPY --from=build /usr/src/build/bialet /usr/local/bin/bialet
ENTRYPOINT ["bialet", "-h", "0.0.0.0", "/var/www/"]

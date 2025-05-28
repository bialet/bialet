# Install library dependencies
FROM alpine:latest AS base
RUN apk add sqlite-dev curl

# Install compilation tools
FROM base AS build
RUN apk add gcc make musl-dev openssl-dev curl-dev
COPY . /usr/src
WORKDIR /usr/src
RUN make clean && make

# Copy bialet binary
FROM base AS run
COPY --from=build /usr/src/build/bialet /usr/local/bin/bialet
EXPOSE 7001
ENTRYPOINT ["bialet", "-h", "0.0.0.0", "/var/www/"]

# syntax=docker/dockerfile:1
FROM alpine:3 AS build

# Setup the build environment
RUN apk add --no-cache git cmake make pipx build-base icu-dev
RUN pipx install conan

ENV PATH="/root/.local/bin:$PATH"

# Build the server
WORKDIR /build/alicia-server

# Prepare the source
COPY . .
RUN git submodule update --init --recursive

# Configure conan
RUN conan profile detect --force
RUN conan install . -s build_type=Release --build=missing

# Configure cmake and build
RUN cmake --preset conan-default
RUN cmake --build ./build --parallel

# Install the binary
RUN cmake --install ./build --prefix /usr/local

# Copy the resources
RUN mkdir /var/lib/alicia-server/
RUN cp -r ./resources/* /var/lib/alicia-server/

FROM alpine:3

LABEL author="Serkan Sahin" maintainer="dev@storyofalicia.com"
LABEL org.opencontainers.image.source=https://github.com/Story-Of-Alicia/alicia-server
LABEL org.opencontainers.image.description="Dedicated server implementation for the Alicia game series"

# Setup the runtime environent
RUN apk add --no-cache libstdc++ icu icu-data-full

WORKDIR /opt/alicia-server

COPY --from=build /usr/local /usr/local
COPY --from=build /var/lib/alicia-server/ /var/lib/alicia-server/

ENTRYPOINT ["/usr/local/bin/alicia-server", "/var/lib/alicia-server"]
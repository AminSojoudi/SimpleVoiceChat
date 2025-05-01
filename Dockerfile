# Build stage
FROM ubuntu:22.04 AS build

WORKDIR /app

# Install required build tools and dependencies
RUN apt-get update && apt-get install -y \
    git \
    cmake \
    g++ \
    curl \
    zip \
    unzip \
    tar \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Install vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
RUN /opt/vcpkg/bootstrap-vcpkg.sh
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Install dependencies
RUN vcpkg install gamenetworkingsockets:x64-linux

# Copy source code
COPY . .

# Build the project
WORKDIR /app/VoiceChatServer
RUN cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build --config Release

# Runtime stage
FROM ubuntu:22.04 AS runtime

WORKDIR /app

# Copy the built binary and required libraries
COPY --from=build /app/VoiceChatServer/build/VoiceChatServer /app/
COPY --from=build /opt/vcpkg/installed/x64-linux/lib/libGameNetworkingSockets.so /usr/lib/

# Install runtime dependencies
RUN apt-get update && apt-get install -y \
    libssl3 \
    && rm -rf /var/lib/apt/lists/*

# Expose the port the server will listen on
EXPOSE 27020

# Run the server
CMD ["./VoiceChatServer"]
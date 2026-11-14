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

# Copy source code
COPY . .

# Build only the server. vcpkg installs the shared and server deps from vcpkg.json.
RUN cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DVOICECHAT_BUILD_CLIENT=OFF \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake
RUN cmake --build build

# Runtime stage
FROM ubuntu:22.04 AS runtime

WORKDIR /app

# vcpkg links everything statically on Linux, so the binary is all we need.
COPY --from=build /app/build/VoiceChatServer/VoiceChatServer /app/

# Expose the port the server will listen on
EXPOSE 27020

# Run the server
CMD ["./VoiceChatServer"]
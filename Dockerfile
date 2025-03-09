FROM debian:latest AS build

# Install required system dependencies
RUN apt-get update && apt-get install -y \
    cmake g++ git curl zip tar unzip make ninja-build pkg-config

# Set the working directory
WORKDIR /app

# Clone and set up vcpkg
RUN git clone https://github.com/Microsoft/vcpkg.git /opt/vcpkg
RUN bash /opt/vcpkg/bootstrap-vcpkg.sh

# Set vcpkg environment variables
ENV VCPKG_ROOT=/opt/vcpkg
ENV PATH="${VCPKG_ROOT}:${PATH}"

# Copy project files
COPY /VoiceChatServer /app/VoiceChatServer
COPY /Common /app/Common


# Install dependencies using vcpkg manifest mode
RUN cd VoiceChatServer && /opt/vcpkg/vcpkg install --triplet x64-linux

# Configure and build
RUN cmake -B build -S VoiceChatServer  \
    -DCMAKE_TOOLCHAIN_FILE=/opt/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build

RUN echo $(ls) && pwd

# Remove not used folders
RUN rm -rf /app/VoiceChatServer/build/vcpkg_installed


FROM debian:latest

# Set work directory
WORKDIR /app

# Copy only the compiled binary from the build stage
COPY --from=build /app/VoiceChatServer/build /app/

RUN echo $(ls) && pwd

# Expose UDP port
EXPOSE 27020/udp

# Run the serverl
CMD ["./VoiceChatServer"]
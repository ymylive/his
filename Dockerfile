FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    libasound2-dev \
    libgl1-mesa-dev \
    libwayland-dev \
    libx11-dev \
    libxcursor-dev \
    libxi-dev \
    libxinerama-dev \
    libxkbcommon-dev \
    libxrandr-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build -G Ninja \
    && cmake --build build \
    && ctest --test-dir build --output-on-failure

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    libasound2 \
    libgl1 \
    libwayland-client0 \
    libx11-6 \
    libxcursor1 \
    libxi6 \
    libxinerama1 \
    libxkbcommon0 \
    libxrandr2 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/his /app/his
COPY --from=build /app/build/his_desktop /app/his_desktop
COPY --from=build /app/data /app/data
COPY --from=build /app/README.md /app/README.md
COPY --from=build /app/docs /app/docs

CMD ["/app/his"]

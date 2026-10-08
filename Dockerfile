FROM debian:bookworm AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++ cmake make git ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j

FROM debian:bookworm-slim
COPY --from=build /src/build/simple_task_manager /usr/local/bin/
ENV HOST=0.0.0.0 PORT=8080 TASK_TIMEOUT=300
EXPOSE 8080
CMD ["simple_task_manager"]
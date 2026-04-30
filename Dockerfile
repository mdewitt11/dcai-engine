# ===== Builder =====
FROM golang:latest AS builder

WORKDIR /app
COPY . .

RUN apt-get update && apt-get install -y make gcc

RUN make clean || true
RUN make -j1

# ===== Runtime =====
FROM debian:bookworm-slim

WORKDIR /app
COPY --from=builder /app/dcai .

ENV RUNTIME_MODE=docker

RUN useradd -m appuser && chown -R appuser:appuser /app

USER appuser

CMD ["./dcai"]

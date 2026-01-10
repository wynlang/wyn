# Multi-stage Dockerfile for Wyn Language
# Stage 1: Build environment
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    clang-15 \
    llvm-15-dev \
    libllvm15 \
    llvm-15-tools \
    git \
    cmake \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

# Set LLVM environment
ENV LLVM_CONFIG=llvm-config-15
ENV CC=clang-15

# Create build directory
WORKDIR /build

# Copy source code
COPY . .

# Build Wyn compiler
RUN make wyn-llvm

# Stage 2: Runtime environment
FROM ubuntu:22.04 AS runtime

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y \
    libllvm15 \
    && rm -rf /var/lib/apt/lists/*

# Create app directory
WORKDIR /app

# Copy built compiler from builder stage
COPY --from=builder /build/wyn-llvm /usr/local/bin/wyn
COPY --from=builder /build/packages /app/packages

# Create non-root user
RUN useradd -m -u 1000 wynuser && chown -R wynuser:wynuser /app
USER wynuser

# Set default command
ENTRYPOINT ["/usr/local/bin/wyn"]
CMD ["--help"]

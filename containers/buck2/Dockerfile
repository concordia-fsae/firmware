# syntax=docker/dockerfile:1.7
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ENV PATH="/root/.local/bin:/usr/local/bin:${PATH}"

# Base dev tooling + common build deps
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates curl git unzip zstd xz-utils \
    build-essential pkg-config \
    python3-minimal python3-venv \
    zstd \
    sudo \
    vim \
    wget \
    git \
    && rm -rf /var/lib/apt/lists/*

# --- Install uv (Python package & project manager) ---
RUN curl -fsSL https://astral.sh/uv/install.sh | sh
RUN mv /root/.local/bin/uv /bin/uv

# --- Install Buck2 (official Meta build system) ---
# Fetch latest release binary from GitHub (linux-x86_64)
RUN curl -L https://github.com/facebook/buck2/releases/download/latest/buck2-x86_64-unknown-linux-gnu.zst \
    -o buck2.zst && zstd --decompress buck2.zst && \
    chmod +x buck2 && mv buck2 /usr/local/bin/buck2

RUN usermod -l builduser ubuntu
RUN groupmod -n builduser ubuntu

# allow sudo group to use sudo command without password
RUN echo "%sudo ALL=(ALL:ALL) NOPASSWD: ALL" > /etc/sudoers.d/sudoers
RUN echo "alias buck2='sudo buck2'" > /home/ubuntu/.bashrc

# --- Get the firmware sources (or mount them at runtime) ---
COPY entrypoint.sh /opt/entrypoint.sh
RUN chown builduser:builduser /opt/entrypoint.sh
RUN chmod u+x /opt/entrypoint.sh

WORKDIR /firmware

# Default shell
ENTRYPOINT /opt/entrypoint.sh

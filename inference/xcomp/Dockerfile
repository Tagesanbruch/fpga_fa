# 使用 linux/arm64 架构的 Ubuntu 基础镜像，与目标板保持一致
FROM --platform=linux/arm64 ubuntu:22.04

# 避免交互式配置卡住
ENV DEBIAN_FRONTEND=noninteractive

# 在 update 前切换到中科大 ARM64 (ubuntu-ports) 源
RUN sed -i 's|http://ports.ubuntu.com/ubuntu-ports|http://mirrors.ustc.edu.cn/ubuntu-ports|g' /etc/apt/sources.list

# 安装编译必需的工具链
RUN apt-get update && apt-get install -y \
    ca-certificates \
    build-essential \
    cmake \
    git \
    python3 \
    python3-pip \
    wget \
    && rm -rf /var/lib/apt/lists/*

# 设置工作目录
WORKDIR /workspace

CMD ["/bin/bash"]
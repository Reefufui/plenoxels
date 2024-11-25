FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get install -y cmake g++ git wget curl build-essential locales \
    nlohmann-json3-dev && \
    locale-gen en_US.UTF-8 && \
    update-locale LANG=en_US.UTF-8

RUN apt-get install -y software-properties-common
RUN wget https://apt.llvm.org/llvm.sh
RUN chmod +x llvm.sh && ./llvm.sh 18
RUN apt-get install -y llvm-18 llvm-18-dev llvm-18-tools clang-18 libclang-18-dev

RUN apt-get install -y ninja-build lsb-release gnupg libzstd-dev
RUN git clone https://github.com/EnzymeAD/Enzyme.git /opt/Enzyme
RUN mkdir /opt/Enzyme/enzyme/build
WORKDIR /opt/Enzyme/enzyme/build
RUN cmake -G Ninja .. -DClang_DIR=/usr/lib/llvm-18/lib/cmake/clang -DLLVM_DIR=/usr/lib/llvm-18/lib/cmake/llvm -DLLVM_EXTERNAL_LIT=/usr/lib/llvm-18/build/utils/lit/lit.py
RUN ninja

ENV LANG=en_US.UTF-8
ENV LANGUAGE=en_US:en
ENV LC_ALL=en_US.UTF-8

WORKDIR /app

COPY . .

COPY model.dat /app/model.dat

RUN mkdir -p build && cd build && \
        cmake cmake -G Ninja .. && \
        ninja


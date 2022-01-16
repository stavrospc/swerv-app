FROM ubuntu:20.04

# Initialization
RUN apt update \
    && apt install -y jq git wget make g++ gcc libz-dev libboost-program-options-dev verilator

# Download git repos
RUN cd /home \
    && git clone https://github.com/riscv-collab/riscv-gnu-toolchain.git \
    && git clone https://github.com/chipsalliance/Cores-SweRV.git \
    && git clone https://github.com/chipsalliance/SweRV-ISS.git

# Setup riscv-gnu-toolchain
RUN cd /home/riscv-gnu-toolchain\
    && DEBIAN_FRONTEND=noninteractive apt -y install autoconf automake autotools-dev curl python3 libmpc-dev libmpfr-dev libgmp-dev gawk build-essential bison flex texinfo gperf libtool patchutils bc zlib1g-dev libexpat-dev \
    && mkdir -p out/Multilib \
    && ./configure --prefix=/home/riscv-gnu-toolchain/out/Multilib --enable-multilib \
    && make

# swerv-app
RUN mkdir -p /home/swerv-app
COPY swerv-app /home/swerv-app

# Setup whisper
RUN cd /home \
    && wget "https://boostorg.jfrog.io/artifactory/main/release/1.74.0/source/boost_1_74_0.tar.bz2" \
    && tar --bzip2 -xf /home/boost_1_74_0.tar.bz2 \
    && cd SweRV-ISS && git checkout 9058ea5 && patch -p1 < /home/swerv-app/util/whisper.patch && make BOOST_DIR=/home/boost_1_74_0

ENV PATH="/home/riscv-gnu-toolchain/out/Multilib/bin:/home/SweRV-ISS/build-Linux:${PATH}"
ENV RV_ROOT="/home/Cores-SweRV"

# Setup project
RUN cd /home/swerv-app \
    && export PATH=$PATH:/home/riscv-gnu-toolchain/out/Multilib/bin \
    && export RV_ROOT=/home/Cores-SweRV \
    && $RV_ROOT/configs/swerv.config -snapshot=swerv-app -set=icache_enable=1 -set=icache_size=32 -set=fpga_optimize=0 -set=dccm_enable=0 -set=iccm_enable=0 \
    && jq '.csr += {"mrac":{"number":"0x7c0","exists":"true","reset":"0x0","mask":"0xffffffff"}}' \
       snapshots/swerv-app/whisper.json > snapshots/swerv-app/whisper-mrac.json \
       && mv snapshots/swerv-app/whisper-mrac.json snapshots/swerv-app/whisper.json \
    && make verilator

CMD ["/home/swerv-app/util/run.sh"]
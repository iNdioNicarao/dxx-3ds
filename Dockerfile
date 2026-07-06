from devkitpro/devkitarm:latest

RUN apt-get update && apt-get install -y gcc g++ zip cmake

# Install makerom
RUN cd / && git clone --recursive https://github.com/profi200/Project_CTR
RUN cd /Project_CTR/makerom && make deps && make
ENV PATH="/Project_CTR/makerom/bin:${PATH}"

# Install bannertool
RUN apt-get install -y wget unzip && \
    wget https://github.com/Epicpkmn11/bannertool/releases/download/v1.2.2/bannertool.zip && \
    unzip bannertool.zip && \
    mv linux-x86_64/bannertool /usr/local/bin/ && \
    chmod +x /usr/local/bin/bannertool && \
    rm *.zip

# Install PHYSFS
RUN cd / && git clone https://github.com/RossMeikleham/physfs-3ds
RUN cd physfs-3ds && mkdir build && cd build &&\
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../Toolchain.cmake -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=$DEVKITPRO/portlibs/3ds \
        -DPHYSFS_BUILD_SHARED=OFF -DPHYSFS_BUILD_TEST=OFF &&\
    make &&\
    make install

# Build D1X & D2X .3dsx and .cia files
RUN mkdir /dxx/
ADD  d1 /dxx/d1/
ADD  d2 /dxx/d2/
ADD  libs /dxx/libs/
WORKDIR /dxx/

CMD cd libs/picaGL && make && cd ../../d1 && make && cp d1x-3ds.3dsx /mnt &&\ 
    cd 3ds_data && ./make_cia.sh && cp d1x-3ds.cia /mnt &&\
    cd ../../d2 && make && cp d2x-3ds.3dsx /mnt &&\
    cd 3ds_data && ./make_cia.sh && cp d2x-3ds.cia /mnt

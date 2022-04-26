FROM ubuntu:latest

RUN apt-get -qq update \
        && apt-get -y install git scons bzr vim python3.5 gcc-arm-none-eabi

RUN apt install python3-pip -y

WORKDIR /usr/project

# keep container running for debugging
#ENTRYPOINT ["tail", "-f", "/dev/null"]

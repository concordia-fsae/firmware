FROM ubuntu:latest

RUN apt-get -qq update \
        && apt-get -y install scons python3.5 wget vim 

RUN apt install python3-pip -y

# download compiler files 
RUN wget https://developer.arm.com/-/media/Files/downloads/gnu/11.2-2022.02/binrel/gcc-arm-11.2-2022.02-x86_64-arm-none-eabi.tar.xz

# extract tar and remove .xz 
RUN  tar -xf /gcc-arm-11.2-2022.02-x86_64-arm-none-eabi.tar.xz \
	&& rm -f /gcc-arm-11.2-2022.02-x86_64-arm-none-eabi.tar.xz

# define default dir when entering shell to be dir of project 
WORKDIR /usr/project/

# keep container running for debugging
#ENTRYPOINT ["tail", "-f", "/dev/null"]

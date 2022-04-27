#!/bin/bash

IMAGE_NAME="scons_image"
CONTAINER_NAME="scons_container"

# local directories 
C_CODE=$(cd ../ && pwd)
WORKDIR=/usr/project/

# wrapper variables 
COMPONENT=""

# TODO: find dockerfile --> will fail if Dockerfile not in directory script is executed from 
# build base ubuntu image  
docker build -t ${IMAGE_NAME} .

# start and mount program files to container workdir 
docker run -t -d -v ${C_CODE}:${WORKDIR} --name ${CONTAINER_NAME} ${IMAGE_NAME}

# install requirements 
docker exec ${CONTAINER_NAME} pip3 install -r ${WORKDIR}/requirements.txt

# rename and move compiler files to appropriate dir 
docker exec ${CONTAINER_NAME} mv /gcc-arm-11.2-2022.02-x86_64-arm-none-eabi/ /usr/project/embedded/platforms/toolchain-gccarmnoneeabi/

# open containers shell
docker exec -it ${CONTAINER_NAME} bash

# add wrapper feature?
#docker exec ${CONTAINER_NAME} scons --target=${COMPONENT}

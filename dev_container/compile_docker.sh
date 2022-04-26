#!/bin/bash

# TODO: add installation and starting of docker daemon if necessary

IMAGE_NAME="image_scons"
CONTAINER_NAME="container_scons"

C_CODE=/Users/garyfodi/Desktop/PROJECTS/SAE/DAQ/firmware/
WORKDIR=/usr/project


COMPONENT=""

# build base ubuntu image  
docker build -t ${IMAGE_NAME} .

# start and mount program files to container workdir 
docker run -t -d -v ${C_CODE}:${WORKDIR} --name ${CONTAINER_NAME} ${IMAGE_NAME}

# install requirements 
docker exec ${CONTAINER_NAME} pip3 install -r /usr/project/requirements.txt

# open containers shell
docker exec -it ${CONTAINER_NAME} bash

# add wrapper feature?
#docker exec ${CONTAINER_NAME} scons --target=${COMPONENT}

#!/bin/bash

POSITIONAL_ARGS=()
CURRENT_TAG="v1.0.0"
IMAGE_TAG=$CURRENT_TAG
RUN_COMMAND=""

print_help () {
    echo "Usage 'buildroot.sh [OPTIONS] COMMAND"
    echo "Buildroot help..."
    echo "Options:"
    echo "  -h --help            Displays this help message"
    echo "  -l --latest          Displays this help message"
    echo "  -r --run COMMAND     Command to run in the docker container once started"
    echo "  -t --tag TAG         The tag to use for the buildroot container. Default's to $IMAGE_TAG"
}

validate_tag_change () {
    if [ $IMAGE_TAG != $CURRENT_TAG ]; then \
        echo "Cannot specify tag and latest in the same command. Exiting..." && \
        exit 1;
    fi

}

for var in "$@"
do
    case $1 in
        -h|--help)
            print_help
            exit 0
            ;;
        -l|--latest)
            validate_tag_change
            IMAGE_TAG="latest"
            ;;
        -t|--tag)
            validate_tag_change
            shift
            IMAGE_TAG="$1"
            shift
            ;;
        -r|--run)
            shift
            RUN_COMMAND="$1"
            shift
            ;;
        -*|--*) # catch any unknown args and exit
            echo "Unknown option $1"
            print_help
            exit 1
            ;;
        *)
            break
            ;;
    esac
done

for var in "$@"
do
    case $1 in
        *)
            POSITIONAL_ARGS+=("$1") # save positional arg
            shift # past argument
            ;;
    esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters
# one day we can do things here with positional args or params
# for now, just always start the container
echo "Pulling buildroot image tag '$IMAGE_TAG'"
docker-compose pull buildroot
docker-compose up -d
IMAGE_TAG=${IMAGE_TAG} docker-compose run --rm buildroot $RUN_COMMAND

EXIT_CODE=$?
if [ $EXIT_CODE != 0 ]; then
    echo "Container exited with error code $EXIT_CODE..."
    exit $EXIT_CODE;
fi

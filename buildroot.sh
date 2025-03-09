#!/bin/bash

POSITIONAL_ARGS=()
CONTAINER_NAME="buildroot"

print_help () {
    echo "Buildroot help..."
    echo "  -h --help            Displays this help message"
    echo "  -l --latest          Displays this help message"
}

for var in "$@"
do
    case $1 in
        -h|--help)
            print_help
            exit 0
            ;;
        -l|--latest)
            echo "Running latest version of buildroot"
            CONTAINER_NAME+="-latest"
            ;;
        -*|--*) # catch any unknown args and exit
            echo "Unknown option $1"
            print_help
            exit 1
            ;;
        *)
        POSITIONAL_ARGS+=("$1") # save positional arg
        shift # past argument
        ;;
    esac
done

set -- "${POSITIONAL_ARGS[@]}" # restore positional parameters
# one day we can do things here with positional args or params
# for now, just always start the container
docker-compose pull $CONTAINER_NAME
docker-compose up -d
docker-compose run --rm $CONTAINER_NAME

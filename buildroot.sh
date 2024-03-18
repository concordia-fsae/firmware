#!/bin/bash
is_arg=false

for args in "$@"
do
  is_arg=true
  
  case $args in
    -h|--help)
    echo "Buildroot help..."
    echo "  --help          Displays this help message"

  ;;
  *)
    echo "Unknown option: $args"; exit 1
  ;;
  ## If option with variable
  #-p=*|--prefix=*)
  #OPTION="${i#*=}"

  #;;
  esac
done

if [ "$is_arg" = false ]; then
  docker-compose pull
  docker-compose up -d
  docker-compose run --rm buildroot
fi

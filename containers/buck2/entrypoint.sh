#!/bin/bash

# if no arguments are passed, run bash
# otherwise, execute the argument
if [[ "${#}" -eq 0 ]]; then
    exec /bin/bash
else
    exec "${@}"
fi

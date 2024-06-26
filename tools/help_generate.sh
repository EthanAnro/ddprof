#!/bin/bash

# Unless explicitly stated otherwise all files in this repository are licensed under the Apache License Version 2.0.
# This product includes software developed at Datadog (https://www.datadoghq.com/). Copyright 2021-Present Datadog, Inc.

# http://redsymbol.net/articles/unofficial-bash-strict-mode/
set -euo pipefail
IFS=$'\n\t'

### Set directory names
CURRENTDIR=$PWD
SCRIPTPATH=$(readlink -f "$0")
SCRIPTDIR=$(dirname $SCRIPTPATH)
cd $SCRIPTDIR/../
TOP_LVL_DIR=$PWD
cd $CURRENTDIR

BUILD_FOLDER="build_Release"
# Parse parameters 
if [ $# != 0 ] && [ $1 == "-b" ]; then
    shift
    cd $1
    BUILD_FOLDER="$PWD"
    cd $CURRENTDIR
    shift
fi

FILE=${TOP_LVL_DIR}/docs/Commands.md
echo "# ddprof Commands" > ${FILE}
echo "" >> ${FILE}
echo '```bash' >> ${FILE}
${BUILD_FOLDER}/ddprof --help >> ${FILE}
echo '```' >> ${FILE}

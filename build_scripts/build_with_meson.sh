#!/bin/bash


script_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

while [[ $# -gt 0 ]]; do
    case "$1" in
        --name)
            export GWATCH_TARGET_NAME="$2"
            shift 2
            ;;
        --type)
            export GWATCH_TARGET_TYPE="$2"
            shift 2
            ;;
        --log)
            LOG_PATH="$2"
            shift 2
            ;;
        --version)
            export GWATCH_VERSION="$2"
            shift 2
            ;;
        --sources)
            export GWATCH_SRC="$2"
            shift 2
            ;;
        --includes)
            export GWATCH_INC="$2"
            shift 2
            ;;
        --ldflags)
            export GWATCH_LDFLAGS="$2"
            shift 2
            ;;
        --cflags)
            export GWATCH_CFLAGS="$2"
            shift 2
            ;;
        *)
            echo "错误：未知参数 $1"
            ;;
    esac
done

cd $script_dir
if [ ! -d $script_dir/build/$GWATCH_TARGET_NAME ]; then
    mkdir -p $script_dir/build/$GWATCH_TARGET_NAME
    meson build/$GWATCH_TARGET_NAME         > $LOG_PATH    2>&1
    ninja -C build/$GWATCH_TARGET_NAME      >> $LOG_PATH    2>&1
else
    ninja -C build/$GWATCH_TARGET_NAME      > $LOG_PATH    2>&1
fi

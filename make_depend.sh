#!/bin/bash
# Script to build and compile dependencies
set -e  # Exit immediately on error

# Log files
LOG_FILE="make_depend.log"
ERROR_LOG_FILE="make_depend_errors.log"

# Clean log files
> "$LOG_FILE"
> "$ERROR_LOG_FILE"

# Directory definitions
SHDIR=$(dirname `readlink -f $0`)
DEPS_DIR=$SHDIR/deps
BUILD_DEPS_DIR=$SHDIR/deps_build

# Function to copy library files if build directory is empty
copy_library_files() {
    local lib_name=$1
    local src_dir=$2
    local build_dir=$3

    # Create build directory if it doesn't exist
    mkdir -p "$build_dir"

    # Check if build directory is empty
    if [ ! "$(ls -A "$build_dir")" ]; then
        echo "Copying $lib_name files to build directory..."
        rsync -a "$src_dir/" "$build_dir/"
    else
        echo "$lib_name build directory is not empty, skipping copy"
    fi
}

# Get number of CPU cores
COMPILE_NUM=$(cat /proc/cpuinfo | grep "processor" | wc -l)

# Function to check if library exists
check_library_exists() {
    local lib_name=$1
    shift
    local lib_paths=("$@")

    for path in "${lib_paths[@]}"; do
        if [ -f "$path" ]; then
            echo "Library $lib_name already exists: $path"
            return 0
        fi
    done
    return 1
}

# Function to compile libraries
compile_library() {
    local lib_name=$1
    local src_dir=$2
    local build_dir=$3
    local build_cmd=$4
    local lib_paths=("${@:5}")

    # Copy library files if needed
    copy_library_files "$lib_name" "$src_dir" "$build_dir"

    # Check if library already exists
    if check_library_exists "$lib_name" "${lib_paths[@]}"; then
        return 0
    fi

    echo "Compiling $lib_name..."
    if ! (cd "$build_dir" && eval "$build_cmd"); then
        echo "Failed to compile $lib_name"
        return 1
    fi

    return 0
}

# Compile libraries
compile_libraries() {
    # OpenSSL
    compile_library "OpenSSL" \
        "$DEPS_DIR/openssl" \
        "$BUILD_DEPS_DIR/openssl" \
        "./Configure && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/openssl/libcrypto.a" \
        "$BUILD_DEPS_DIR/openssl/libssl.a"

    # RocksDB
    compile_library "RocksDB" \
        "$DEPS_DIR/rocksdb" \
        "$BUILD_DEPS_DIR/rocksdb" \
        "make static_lib USE_RTTI=1 -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/rocksdb/librocksdb.a"

    # Protobuf
    compile_library "Protobuf" \
        "$DEPS_DIR/protobuf" \
        "$BUILD_DEPS_DIR/protobuf" \
        "mkdir -p build && cd build && cmake .. && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/protobuf/build/libprotobuf.a" \
        "$BUILD_DEPS_DIR/protobuf/build/libprotobuf-lite.a"

    # Boost
    compile_library "Boost" \
        "$DEPS_DIR/boost" \
        "$BUILD_DEPS_DIR/boost" \
        "./bootstrap.sh --prefix=$BUILD_DEPS_DIR/boost/stage && ./b2 -j$COMPILE_NUM --build-dir=$BUILD_DEPS_DIR/boost/build_tmp install" \
        "$BUILD_DEPS_DIR/boost/stage/lib/libboost_system.a" \
        "$BUILD_DEPS_DIR/boost/stage/lib/libboost_filesystem.a"

    # libfmt
    compile_library "libfmt" \
        "$DEPS_DIR/libfmt" \
        "$BUILD_DEPS_DIR/libfmt" \
        "rm -rf CMakeCache.txt && cmake -B . -S . -DCMAKE_POSITION_INDEPENDENT_CODE=ON && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/libfmt/libfmt.a"

    # spdlog
    compile_library "spdlog" \
        "$DEPS_DIR/spdlog" \
        "$BUILD_DEPS_DIR/spdlog" \
        "fmt_DIR=$BUILD_DEPS_DIR/libfmt cmake -DSPDLOG_FMT_EXTERNAL=yes . && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/spdlog/libspdlog.a"

    # silkpre
    SILKPRE_LIB_PATHS=("$BUILD_DEPS_DIR/silkpre/build/lib/libsilkpre-standalone.a" "$BUILD_DEPS_DIR/silkpre/build/libsilkpre-standalone.a")
    copy_library_files "Silkpre" "$DEPS_DIR/silkpre" "$BUILD_DEPS_DIR/silkpre"
    
    if check_library_exists "silkpre" "${SILKPRE_LIB_PATHS[@]}"; then
        echo "silkpre library already exists, skipping compilation"
    else
        echo "Installing GMP dependencies..."
        # Install GMP library
        if ! bash ./gmp.sh ; then
            echo "Error: gmp.sh execution failed."
            exit 1
        fi
        
        if cd $BUILD_DEPS_DIR/silkpre && mkdir -p build && cmake -S . -B build -DCMAKE_POSITION_INDEPENDENT_CODE=ON && cd build && make -j$COMPILE_NUM 2>> "$ERROR_LOG_FILE"; then
            if [ -f "$BUILD_DEPS_DIR/silkpre/build/lib/libsilkpre-standalone.a" ]; then
                echo "Silkpre compilation completed successfully"
                echo "Silkpre library location: $BUILD_DEPS_DIR/silkpre/build/lib/libsilkpre-standalone.a"
            else
                FAILED_BUILDS+=("Silkpre")
            fi
        else
            FAILED_BUILDS+=("Silkpre")
        fi
    fi

    # EVMone
    compile_library "EVMone" \
        "$DEPS_DIR/evmone" \
        "$BUILD_DEPS_DIR/evmone" \
        "mkdir -p build && cmake -S . -B build -DBUILD_SHARED_LIBS=OFF && cd build && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/evmone/build/lib64/libevmone-standalone.a" \
        "$BUILD_DEPS_DIR/evmone/build/lib/libevmone-standalone.a"

    # json
    compile_library "json" \
        "$DEPS_DIR/json" \
        "$BUILD_DEPS_DIR/json" \
        "" \
        "" \
        
    # qrcode
    compile_library "qrcode" \
        "$DEPS_DIR/qrcode" \
        "$BUILD_DEPS_DIR/qrcode" \
        "cd cpp && make -j$COMPILE_NUM" \
        "$BUILD_DEPS_DIR/qrcode/cpp/libqrcodegencpp.a" \

}

# Execute compilation
if compile_libraries; then
    echo "All dependency libraries compiled successfully"
else
    echo "Some dependency libraries failed to compile"
    exit 1
fi

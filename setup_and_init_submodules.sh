#!/bin/bash
set -e

LOG_FILE="submodule_setup.log"
ERROR_LOG_FILE="submodule_setup_errors.log"
PROGRESS_FILE="submodule_setup_progress.txt"

log_message() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" | tee -a "$LOG_FILE"
}
log_error() {
    echo "[$(date '+%Y-%m-%d %H:%M:%S')] ERROR: $1" | tee -a "$LOG_FILE" "$ERROR_LOG_FILE"
}

SHDIR=$(dirname `readlink -f $0`)

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

gitmodules_cleanup() {
    if [ -f .gitmodules ]; then
        sed -i '/^\[submodule "/,/^$/d' .gitmodules
    fi
    for mod in boost evmone libfmt openssl protobuf rocksdb silkpre spdlog gmp; do
        git config -f .git/config --remove-section "submodule.deps/$mod" 2>/dev/null || true
    done
}


if [ ! -f .gitmodules ]; then
    touch .gitmodules
    git add .gitmodules
fi

clean_submodule() {
    local path=$1
    local escaped_path=$(printf "%s" "$path" | sed 's/[\/&]/\\&/g')
    
    if [ -f .gitmodules ] && grep -q "$path" .gitmodules; then
        git config -f .gitmodules --remove-section "submodule.$path" 2>/dev/null || true
        sed -i "/submodule \"$escaped_path\"/,/^$/d" .gitmodules || true
    fi
    git config --remove-section "submodule.$path" 2>/dev/null || true
    git rm --cached "$path" 2>/dev/null || true
    git config -f .gitmodules --remove-section submodule."$path" 2>/dev/null || true
    git submodule deinit -f "$path" 2>/dev/null || true
    git rm -f "$path" 2>/dev/null || true
    rm -rf ".git/modules/$path" 2>/dev/null || true
    rm -rf "$path" 2>/dev/null || true
}


add_submodule() {
    local name=$1
    local url=$2
    local version=$3
    local path=$4

    echo "Processing $name ($version)..."
    
    if [ -d "$path" ]; then
        cd "$path"
        git fetch --all
        git fetch --tags
        if git rev-parse --verify "$version" >/dev/null 2>&1 && [ "$(git rev-parse HEAD)" = "$(git rev-parse $version)" ]; then
            echo "$name already at $version, skipping add"
            cd - >/dev/null
            return 0
        fi
        cd - >/dev/null
    fi

    clean_submodule "$path"

    if ! git submodule add -f "$url" "$path"; then
        echo "Failed to add $name submodule"
        return 1
    fi
    git add .gitmodules 2>/dev/null || true

   
    if ! (cd "$path" && git fetch --all && git fetch --tags && git checkout "$version"); then
        echo "Cannot checkout $name version $version"
        return 1
    fi

    git add "$path"
    echo "$name added and checked out to $version successfully"
    return 0
}

add_submodule "silkpre"     "https://github.com/MoonMeshTech/silkpre.git"             "2187d6a"          "deps/silkpre" # silkpre
add_submodule "evmone"      "https://github.com/MoonMeshTech/evmone.git"              "ff59309"          "deps/evmone"  #0.26.0
add_submodule "boost"       "https://github.com/boostorg/boost.git"                   "b7b1371"          "deps/boost"  #boost-1.75.0
add_submodule "libfmt"      "https://github.com/fmtlib/fmt.git"                       "7bdf062"          "deps/libfmt" #7.1.3
add_submodule "openssl"     "https://github.com/openssl/openssl.git"                  "aea7aaf"          "deps/openssl" #3.5.1
add_submodule "protobuf"    "https://github.com/protocolbuffers/protobuf.git"         "90b73ac"          "deps/protobuf" #v3.21.9
add_submodule "rocksdb"     "https://github.com/facebook/rocksdb.git"                 "13d5230"          "deps/rocksdb" #v9.6.1
add_submodule "spdlog"      "https://github.com/gabime/spdlog.git"                    "de0dbfa"          "deps/spdlog"  #1.8.2
add_submodule "threadpool"  "https://github.com/philipphenkel/threadpool.git"         "a7b3e05"          "deps/threadpool"  #boost 3rdlib #0.2.5
add_submodule "utils"       "https://github.com/MoonMeshTech/utils.git"               "13e3938"         "deps/utils"  # ethash & intx
add_submodule "json"        "https://github.com/nlohmann/json.git"                    "e00484f"         "deps/json"   # json v3.12.0
add_submodule "qrcode"      "https://github.com/nayuki/QR-Code-generator.git"         "2c9044d"         "deps/qrcode"   # json v3.12.0

log_message "Starting to update all submodules..."
if ! git submodule update --init --recursive; then
    log_error "Failed to update submodules"
    exit 1
fi
log_message "All submodules setup and initialization completed successfully"

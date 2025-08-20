#!/bin/bash

# Set log file environment variable
export LOGFILE="$(pwd)/build.log"

# Function to log messages to both screen and log file
log_message() {
    echo "$1" | tee -a "$LOGFILE"
}

# Initialize log file
: > "$LOGFILE"
log_message "Build process started at $(date)"

# Check if cmake is installed and version is >= 3.15.0
log_message "Checking cmake installation and version..."
if ! command -v cmake &> /dev/null; then
    log_message "Error: cmake is not installed."
    exit 1
fi

CMAKE_VERSION=$(cmake --version | head -n1 | cut -d" " -f3)
REQUIRED_VERSION="3.15.0"
if [ "$(printf '%s\n' "$CMAKE_VERSION" "$REQUIRED_VERSION" | sort -V | head -n1)" != "$REQUIRED_VERSION" ]; then
    log_message "Error: cmake version $CMAKE_VERSION is lower than required version $REQUIRED_VERSION."
    exit 1
fi
log_message "cmake version $CMAKE_VERSION is sufficient."

# Execute appropriate script based on ISGIT
log_message "Executing setup_and_init_submodules.sh..."
if ! bash ./setup_and_init_submodules.sh ; then
    log_message "Error: setup_and_init_submodules.sh execution failed."
    exit 1
fi
log_message "Submodules setup completed successfully."

# Execute dependency build script
log_message "Executing make_depend.sh to build dependencies..."
if ! bash ./make_depend.sh >> "$LOGFILE" 2>&1; then
    log_message "Error: make_depend.sh execution failed."
    log_message "Please check make_depend.log and make_depend_errors.log for details."
    exit 1
fi
log_message "Dependencies build completed successfully."

# Create build directory and run cmake
log_message "Creating build directory..."
mkdir -p build || { log_message "Error: Failed to create build directory."; exit 1; }
cd build || { log_message "Error: Failed to enter build directory."; exit 1; }

log_message "Running cmake .."
#if ! cmake .. >> "$LOGFILE" 2>&1; then
if ! cmake .. ; then
    log_message "Error: cmake configuration failed."
    exit 1
fi

log_message "Build setup completed successfully at $(date)"

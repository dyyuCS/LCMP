# Base image: Ubuntu 22.04 (amd64 architecture)
FROM --platform=linux/amd64 ubuntu:22.04
# Disable interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Step 1: Install basic certificates and HTTPS support, clean apt cache
RUN apt-get update && \
    apt-get install -y ca-certificates apt-transport-https gnupg && \
    rm -rf /var/lib/apt/lists/*

# Step 2 (Optional): Update apt sources list (use official Ubuntu repos for global compatibility)
RUN sed -i \
    -e 's|http://archive.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' \
    -e 's|http://security.ubuntu.com/ubuntu/|http://mirrors.tuna.tsinghua.edu.cn/ubuntu/|g' \
    /etc/apt/sources.list && \
    apt-get update

# Step 3: Install development dependencies and Python packages
RUN apt-get update && \
    apt-get install -y \
       # Build essentials
       build-essential gcc g++ mercurial \
       # Library dependencies
       libsqlite3-dev libxml2-dev libgtk2.0-0 libgtk2.0-dev uncrustify \
       # Python 2 support
       python2-dev python2 \
       # Build tools
       cmake libboost-all-dev git \
       # Python 3 package manager
       python3-pip python3-tk \
       # Utility tools
       vim wget sudo ca-certificates && \
    # Clean apt cache
    rm -rf /var/lib/apt/lists/* && \
    pip3 install pandas matplotlib

# Step 4: Install Legacy GCC/G++ 5.x (required for older code compatibility)
RUN echo "===== Installing GCC 5/G++ 5 =====" && \
    # Add Xenial (16.04) repositories to sources list
    echo "deb http://us.archive.ubuntu.com/ubuntu/ xenial main" >> /etc/apt/sources.list && \
    echo "deb http://us.archive.ubuntu.com/ubuntu/ xenial universe" >> /etc/apt/sources.list && \
    # Import missing GPG keys for Xenial repos
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 40976EAF437D05B5 3B4FE6ACC0B21F32 && \
    apt-get update && \
    # Install GCC 5 and G++ 5
    apt-get install -y gcc-5 g++-5 && \
    # Clean apt cache to reduce image size
    rm -rf /var/lib/apt/lists/*

# ========== Critical Step: Remove Xenial repos (cleanup post-GCC 5 installation) ==========
RUN sed -i '/xenial/d' /etc/apt/sources.list && \
    # Update apt index (retains only 22.04 repos)
    apt-get update && \
    # Clean apt cache (reduce image size)
    rm -rf /var/lib/apt/lists/* && \
    # Set GCC 5 as default compiler
    update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 50 --slave /usr/bin/g++ g++ /usr/bin/g++-5 

# Step 5: Set Python 2 as default Python interpreter
RUN ln -sf /usr/bin/python2 /usr/bin/python

# Set working directory
WORKDIR /root

# Restore interactive mode (does not affect container operation)
ENV DEBIAN_FRONTEND=dialog
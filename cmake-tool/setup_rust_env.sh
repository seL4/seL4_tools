#!/bin/bash -e
TOOLCHAIN_VERSION=1.3
SYSROOT=x86_64-sel4-sysroot.tar.gz

if grep -e CentOS </etc/os-release >/dev/null 2>&1; then
    COMPILER=rust-centos-x86_64.tar.gz
else
    COMPILER=rust-ubuntu-x86_64.tar.gz
fi

stdbuf -oL printf "Fetching sel4-aware Rust compiler..."
(
  rm -rf rust &&
  mkdir rust &&
  cd rust &&
  curl -OL https://github.com/GaloisInc/sel4-rust/releases/download/$TOOLCHAIN_VERSION/$COMPILER &&
  tar -xf $COMPILER
) >/dev/null 2>&1
echo " done."

stdbuf -oL printf "Fetching sel4 sysroot..."
(
  rm -rf sysroot &&
  mkdir sysroot &&
  cd sysroot &&
  curl -OL https://github.com/GaloisInc/sel4-rust/releases/download/$TOOLCHAIN_VERSION/$SYSROOT &&
  tar -xf $SYSROOT
) >/dev/null 2>&1
echo " done."

stdbuf -oL printf "Setting up tree-local Cargo configuration..."
mkdir -p .cargo
cat >.cargo/config <<EOF
[build]
target = "x86_64-sel4-none"
rustflags = ["--sysroot", "sysroot"]
rustc = "rust/bin/rustc"
EOF
echo " done."

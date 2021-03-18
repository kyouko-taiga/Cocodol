// swift-tools-version:5.3

import PackageDescription

let package = Package(
  name: "Cocodol",
  platforms: [
    .macOS(.v11)
  ],
  dependencies: [
    .package(name: "LLVM", url: "https://github.com/llvm-swift/LLVMSwift.git", from: "0.7.0"),
  ],
  targets: [
    .target(name: "cocodoc", dependencies: ["Cocodol", "CodeGen", "LLVM"]),
    .target(name: "Cocodol", dependencies: ["CCocodol"]),
    .target(name: "CCocodol", exclude: ["build/", "src/main.c", "Makefile"]),
    .target(name: "CodeGen", dependencies: ["Cocodol", "LLVM"]),
    .testTarget(name: "CocodolTests", dependencies: ["Cocodol"]),
  ])

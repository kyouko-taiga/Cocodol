// swift-tools-version:5.3

import PackageDescription

let package = Package(
  name: "Cocodol",
  platforms: [
    .macOS(.v11),
  ],
  products: [
    .executable(name: "cocodoc", targets: ["Driver"]),
    .library(name: "Cocodol", targets: ["Cocodol"]),
  ],
  dependencies: [
    .package(url: "https://github.com/apple/swift-argument-parser.git", from: "0.4.0"),
    .package(name: "LLVM", url: "https://github.com/llvm-swift/LLVMSwift.git", .branch("master")),
  ],
  targets: [
    // The driver's target.
    .target(
      name: "Driver",
      dependencies: [
        "Cocodol", "CodeGen", "LLVM",
        .product(name: "ArgumentParser", package: "swift-argument-parser"),
      ]),

    // Targets related to the C core and its wrapper.
    .target(name: "Cocodol", dependencies: ["CCocodol"]),
    .target(name: "CCocodol", exclude: ["build/", "src/main.c", "Makefile"]),

    // The code generator's target.
    .target(name: "CodeGen", dependencies: ["Cocodol", "LLVM"]),

    // Test targets.
    .testTarget(name: "CocodolTests", dependencies: ["Cocodol"]),
  ])

// swift-tools-version:5.3

import PackageDescription

let package = Package(
  name: "Cocodol",
  dependencies: [],
  targets: [
    .target(name: "cocodoc", dependencies: ["Cocodol"]),
    .target(name: "Cocodol", dependencies: ["CCocodol"]),
    .target(name: "CCocodol", exclude: ["build/", "src/main.c", "Makefile"]),
    .testTarget(name: "CocodolTests", dependencies: ["Cocodol"]),
  ])

import Foundation

import ArgumentParser
import Cocodol
import CodeGen
import LLVM

/// The compiler driver.
struct Cocodoc: ParsableCommand {

  /// The default library search paths.
  static var librarySearchPaths = ["/usr/lib", "/usr/local/lib", "/opt/local/lib"]

  @Argument(help: "The source program.", transform: URL.init(fileURLWithPath:))
  var inputFile: URL

  @Flag(name: [.customShort("O"), .long], help: "Compile with optimizations.")
  var optimize = false

  @Option(name: [.short, .customLong("output")],
          help: "Write the output to <output>.")
  var outputFile: String?

  @Option(name: [.customShort("L"), .customLong("lib")],
          help: ArgumentHelp("Add a custom library search path.", valueName: "path"))
  var customLibrarySearchPath: String?

  @Option(name: [.customLong("clang")], help: "The path to clang.")
  var clangPath: String = {
    (try? exec("/usr/bin/which", args: ["clang"])) ?? "/usr/bin/clang"
  }()

  @Flag(help: "Print the program as it has been parsed without compiling it.")
  var unparse = false

  @Flag(help: "Evaluate the program without compiling it.")
  var eval = false

  @Flag(help: "Emits the LLVM IR of the program.")
  var emitIR = false

  @Flag(help: "Emits an assembly file.")
  var emitAssembly = false

  @Flag(help: "Emits an object file.")
  var emitObject = false

  /// The output path the driver's product.
  var productFile: URL {
    if let path = outputFile {
      return URL(fileURLWithPath: path)
    } else {
      let cwd = FileManager.default.currentDirectoryPath
      return URL(fileURLWithPath: cwd)
        .appendingPathComponent(inputFile.deletingPathExtension().lastPathComponent)
    }
  }

  mutating func run() throws {
    // Open and read the input file.
    let source = try String(contentsOf: inputFile)

    // Parse the program.
    let context = Context(source: source)
    let parser = Parser(in: context)
    let decls = parser.parse()

    // Unparse the program, if requested to.
    if unparse {
      for decl in decls {
        print(decl.unparse())
      }
      return
    }

    // Evaluate the program, if requested to.
    if eval {
      let vm = Interpreter(in: context)
      vm.eval(program: decls)
      return
    }

    // Emit the LLVM IR fo the program.
    let module = try Emitter.emit(program: decls)

    // Apply optimizations, if requested to.
    if optimize {
      let pipeliner = PassPipeliner(module: module)
      pipeliner.addStandardModulePipeline("opt", optimization: .default, size: .default)
      pipeliner.execute()
    }

    // Emit human readable assembly, if requested to.
    var file = productFile
    if emitIR {
      if file.pathExtension.isEmpty {
        file.appendPathExtension("ll")
      }

      try String(describing: module).write(to: file, atomically: true, encoding: .utf8)
      return
    }

    // Compile the program.
    let target = try TargetMachine(optLevel: optimize ? .default : .none)
    module.targetTriple = target.triple

    // Emit the requested output.
    if emitAssembly {
      if file.pathExtension.isEmpty {
        file.appendPathExtension("s")
      }
      try target.emitToFile(module: module, type: .assembly, path: file.path)
      return
    }

    if emitObject {
      if file.pathExtension.isEmpty {
        file.appendPathExtension("o")
      }
      try target.emitToFile(module: module, type: .object, path: file.path)
      return
    }

    try makeExec(target: target, module: module)
  }

  /// Generates an executable.
  func makeExec(target: TargetMachine, module: Module) throws {
    let manager = FileManager.default

    // Create a temporary directory.
    let tmp = try manager.url(
      for: .itemReplacementDirectory,
      in: .userDomainMask,
      appropriateFor: productFile,
      create: true)

    // Search for the runtime library.
    var searchPaths = Cocodoc.librarySearchPaths
    if let customPath = customLibrarySearchPath {
      searchPaths.insert(customPath, at: 0)
    }

    var runtimePath = "libcocodol_rt.a"
    for directory in searchPaths {
      let path = URL(fileURLWithPath: directory).appendingPathComponent("libcocodol_rt.a").path
      if manager.fileExists(atPath: path) {
        runtimePath = path
        break
      }
    }

    // Compile the LLVM module.
    let moduleObject = tmp.appendingPathComponent(module.name).appendingPathExtension("o")
    try target.emitToFile(module: module, type: .object, path: moduleObject.path)

    // Produce the executable.
    try exec(clangPath, args: [moduleObject.path, runtimePath, "-lm", "-o", productFile.path])
  }

}

Cocodoc.main()

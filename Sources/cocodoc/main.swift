import Foundation

import ArgumentParser
import Cocodol
import CodeGen
import LLVM

/// The compiler driver.
struct Cocodoc: ParsableCommand {

  @Argument(help: "The source program.", transform: URL.init(fileURLWithPath:))
  var inputFile: URL

  @Flag(name: [.customShort("O"), .long], help: "Compile with optimizations.")
  var optimize = false

  @Option(name: [.short, .customLong("output")],
          help: "Write the output to <output>.")
  var outputFile: String?

  @Option(name: [.customLong("lib")],
          help: "The path to the runtime library.")
  var runtimePath = "/usr/local/lib/libcocodol_rt.a"

  @Option(name: [.customLong("clang")], help: "The path to clang.")
  var clangPath = "/usr/bin/clang"

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
    // Create a temporary directory.
    let tmp = try FileManager.default.url(
      for: .itemReplacementDirectory,
      in: .userDomainMask,
      appropriateFor: productFile,
      create: true)

    // Compile the LLVM module.
    let moduleObject = tmp.appendingPathComponent(module.name).appendingPathExtension("o")
    try target.emitToFile(module: module, type: .object, path: moduleObject.path)

    // Produce the executable.
    let linkExec = Process()
    linkExec.executableURL = URL(fileURLWithPath: clangPath)
    linkExec.arguments = [moduleObject.path, runtimePath, "-lm", "-o", productFile.path]
    try linkExec.run()
    linkExec.waitUntilExit()
  }

}

Cocodoc.main()

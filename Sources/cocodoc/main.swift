import Foundation

import Cocodol
import CodeGen
import LLVM

/// A wrapper around the system's standard error.
struct StandardError: TextOutputStream {

  mutating func write(_ string: String) {
    _ = string.withCString({ cstr in fputs(cstr, stderr) })
  }

}

/// The mode of the compiler.
enum OutputMode {

  /// Emits a compiled file.
  ///
  /// The "value" of this mode is determined by the extension of the output file given as argument.
  /// The `.bc` extension designates LLVM bitcode, `.o` designates an object file, and finally `.s`
  /// designates an assembly file. For any other file extension, the value of the mode is `nil` and
  /// designates an executable.
  case emit(CodegenFileType?)

  /// Evaluates the program.
  case eval

}

/// An error that occured while executing the driver.
struct DriverError: Error, CustomStringConvertible {

  var description: String

}

/// A set of driver arguments.
struct DriverArguments {

  /// Indicates that the driver should dump the AST of the program, right after it has been parsed.
  var dumpAST = false

  /// Indicates that the driver should dump the LLVM IR of the program.
  var dumpIR = false

  /// Indicates that the driver should compile with optimizations.
  var optimize = false

  /// Inidicates that the driver should evaluate the program, rather than producing an executable.
  var eval = false

  /// The path of the source program.
  var sourcePath = ""

  /// The output path the driver's product.
  var productPath = ""

  /// The path of Cocodol' runtime library.
  var runtimePath = ""

  /// The path of clang's executable.
  var clangPath = "/usr/bin/clang"

  /// The type of file to emit.
  ///
  /// This argument's value is determined by the extension of the output file: `.bc`designates LLVM
  /// bitcode, `.o` designates an object file, and `.s` designates an assembly file. For any other
  /// file extension, the value of the mode is `nil` and designates an executable.
  var fileType: CodegenFileType?

  /// Initializes the driver's arguments.
  ///
  /// - Parameter arguments: An array of raw arguments.
  init(arguments: [String]) throws {
    var args = arguments.dropFirst()

    while let name = args.first {
      args.removeFirst()
      switch name {
      case "--dump-ast":
        dumpAST = true

      case "--dump-ir":
        dumpIR = true

      case "-O", "--optimize":
        optimize = true

      case "--rt-root":
        guard let path = args.popFirst() else {
          throw DriverError(description: "missing directory path")
        }
        guard !path.starts(with: "-") else {
          throw DriverError(description: "missing argument after \(name)")
        }
        runtimePath = path

      case "--clang":
        guard let path = args.popFirst() else {
          throw DriverError(description: "missing directory path")
        }
        guard !path.starts(with: "-") else {
          throw DriverError(description: "missing argument after \(name)")
        }
        clangPath = path

      case "-o", "--output":
        guard let path = args.popFirst() else {
          throw DriverError(description: "missing output file")
        }
        guard !path.starts(with: "-") else {
          throw DriverError(description: "missing argument after \(name)")
        }
        productPath = path

        if let dot = path.lastIndex(of: ".") {
          switch path.suffix(from: path.index(after: dot)) {
          case "bc" : fileType = .bitCode
          case "o"  : fileType = .object
          case "s"  : fileType = .assembly
          default: break
          }
        }

      default:
        guard sourcePath.isEmpty else {
          throw DriverError(description: "unexpected parameter '\(name)'")
        }
        sourcePath = name
      }
    }

    // Make sure there's an input.
    guard !sourcePath.isEmpty else {
      throw DriverError(description: "no input file")
    }
  }

}

/// Generates a product.
func makeExec(target: TargetMachine, module: Module, args: DriverArguments) throws {
  let productURL  = URL(fileURLWithPath: args.productPath)
  let runtimeURL  = URL(fileURLWithPath: args.runtimePath)
  let clangURL    = URL(fileURLWithPath: args.clangPath)

  // Create a temporary directory.
  let tmp = try FileManager.default.url(
    for: .itemReplacementDirectory,
    in: .userDomainMask,
    appropriateFor: productURL,
    create: true)

  // Compile the LLVM module.
  let moduleObject = tmp.appendingPathComponent(module.name).appendingPathExtension("o")
  try target.emitToFile(module: module, type: .object, path: moduleObject.path)

  // Compile the runtime.
  let runtimeObject = tmp.appendingPathComponent("cocodol_rt.o")
  let compileRuntime = Process()
  compileRuntime.executableURL = clangURL
  compileRuntime.arguments = [
    "-I", runtimeURL.appendingPathComponent("include").path,
    runtimeURL.appendingPathComponent("src/cocodol_rt.c").path,
    "-c", "-o", runtimeObject.path
  ]
  try compileRuntime.run()
  compileRuntime.waitUntilExit()

  // Produce the executable.
  let linkExec = Process()
  linkExec.executableURL = clangURL
  linkExec.arguments = [moduleObject.path, runtimeObject.path, "-o", productURL.path]
  try linkExec.run()
}

/// The program's entry point.
func run() throws {
  var standardError = StandardError()

  // Parse the command line.
  let args: DriverArguments
  do {
    args = try DriverArguments(arguments: CommandLine.arguments)
  } catch {
    print("error: \(error)", to: &standardError)
    return
  }

  // Open and read the input file.
  guard let source = try? String(contentsOfFile: args.sourcePath) else {
    print("error: couldn't read file '\(args.sourcePath)'", to: &standardError)
    return
  }

  // Parse the program.
  let context = Context(source: source)
  let parser = Parser(in: context)
  let decls = parser.parse()
  if args.dumpAST {
    decls.forEach({ decl in _ = decl.unparse() })
    return
  }

  // Evaluate the program, if requested to.
  if args.eval {
    let vm = Interpreter(in: context)
    vm.eval(program: decls)
    return
  }

  // Emit the LLVM IR fo the program.
  let module: Module
  do {
    module = try Emitter.emit(program: decls)
  } catch {
    print("error: \(error)", to: &standardError)
    return
  }

  // Apply optimizations, if requested to.
  if args.optimize {
    let pipeliner = PassPipeliner(module: module)
    pipeliner.addStandardModulePipeline("opt", optimization: .default, size: .default)
    pipeliner.execute()
  }

  // Prints the human readable assembly, if requested to.
  if args.dumpIR {
    print(module)
  }

  // Compile the program.
  let target = try TargetMachine(optLevel: args.optimize ? .default : .none)
  module.targetTriple = target.triple

  if let fileType = args.fileType {
    try target.emitToFile(module: module, type: fileType, path: args.productPath)
  } else {
    try makeExec(target: target, module: module, args: args)
  }
}

try run()

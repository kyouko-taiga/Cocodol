import Foundation

import Cocodol
import CodeGen
import LLVM

extension FileHandle: TextOutputStream {

  public func write(_ string: String) {
    let data = string.data(using: .utf8)!
    write(data)
  }

  static func << (handle: FileHandle, string: String) {
    handle.write(string)
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

  /// Indicates that the driver should print the unparsed AST of the program, right after it has
  /// been parsed.
  var unparse = false

  /// Inidicates that the driver should interpret the program, rather than compiling it.
  var eval = false

  /// Indicates that the driver should compile with optimizations.
  var optimize = false

  /// Indicates that the driver should dump the LLVM IR of the program.
  var dumpIR = false

  /// The path of the source program.
  var sourcePath = ""

  /// The output path the driver's product.
  var productPath = ""

  /// The path of Cocodol' runtime library.
  var runtimePath = "/usr/local/lib/libcocodol_rt.a"

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
      case "-u", "--unparse":
        unparse = true

      case "-e", "--eval":
        eval = true

      case "-O", "--optimize":
        optimize = true

      case "--dump-ir":
        dumpIR = true

      case "-l", "--lib":
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

    // Generate a product path if none was given.
    if productPath.isEmpty {
      let cwd = FileManager.default.currentDirectoryPath
      var url = URL(fileURLWithPath: sourcePath)
      url.deletePathExtension()
      productPath = URL(fileURLWithPath: cwd).appendingPathComponent(url.lastPathComponent).path
    }
  }

}

/// Generates a product.
func makeExec(target: TargetMachine, module: Module, args: DriverArguments) throws {
  let productURL = URL(fileURLWithPath: args.productPath)
  let clangURL = URL(fileURLWithPath: args.clangPath)

  // Create a temporary directory.
  let tmp = try FileManager.default.url(
    for: .itemReplacementDirectory,
    in: .userDomainMask,
    appropriateFor: productURL,
    create: true)

  // Compile the LLVM module.
  let moduleObject = tmp.appendingPathComponent(module.name).appendingPathExtension("o")
  try target.emitToFile(module: module, type: .object, path: moduleObject.path)

  // Produce the executable.
  let linkExec = Process()
  linkExec.executableURL = clangURL
  linkExec.arguments = [moduleObject.path, args.runtimePath, "-lm", "-o", productURL.path]
  try linkExec.run()
}

/// The program's entry point.
func run() throws {
  var stderr = FileHandle.standardError

  // Parse the command line.
  let args: DriverArguments
  do {
    if CommandLine.arguments.contains(where: { ($0 == "-h") || ($0 == "--help") }) {
      stderr << """
      usage: \(CommandLine.arguments[0]) [options] file
      options:
        -h, --help          : Print this help message.
        -u, --unparse       : Print the program, right after it has been parsed.
        -e, --eval          : Evaluate the program rather than compiling it.
        -O, --optimize      : Compile with optimizations.
        -l, --lib <file>    : Configure the location of the runtime library.
        -o, --output <file> : Write the output to <file>.
        --dump-ir           : Dump the LLVM IR of the program.
        --clang <file>      : Configure the path to clang.
      """
    }

    args = try DriverArguments(arguments: CommandLine.arguments)
  } catch {
    print("error: \(error)", to: &stderr)
    return
  }

  // Open and read the input file.
  guard let source = try? String(contentsOfFile: args.sourcePath) else {
    print("error: couldn't read file '\(args.sourcePath)'", to: &stderr)
    return
  }

  // Parse the program.
  let context = Context(source: source)
  let parser = Parser(in: context)
  let decls = parser.parse()

  // Unparse the progra, if requested to.
  if args.unparse {
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
    print("error: \(error)", to: &stderr)
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

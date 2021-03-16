import Foundation

import Cocodol
import CodeGen

/// The program's entry point.
func run() {
  // Get the path of the input file.
  guard CommandLine.arguments.count >= 2 else {
    print("error: no input file")
    return
  }

  // Open and read the input file.
  let path = CommandLine.arguments[1]
  guard let source = try? String(contentsOfFile: path) else {
    print("error: couldn't read file '\(path)'")
    return
  }

  // Parse the program.
  let context = Context(source: source)
  let parser = Parser(in: context)
  let decls = parser.parse()

  // Evaluate the program.
  let vm = Interpreter(in: context)
  vm.eval(program: decls)
}

run()

import CCocodol

/// CoCoDoL's interpreter.
public final class Interpreter {

  /// The internal state of the interpreter.
  var state = EvalState()

  /// The context in which the interpreter operates.
  public let context: Context

  /// Creates a new interpreter in the given context.
  ///
  /// - Parameter context: An AST context.
  public init(in context: Context) {
    self.context = context
    eval_init(&state, context.state)
  }

  deinit {
    eval_deinit(&state)
  }

  /// Evaluates the given program.
  ///
  /// - Parameter decls: A sequence of top-level declarations.
  /// - Returns: The interpreter's exit status.
  @discardableResult
  public func eval<S>(program decls: S) -> Int where S: Sequence, S.Element == Decl {
    let ids = decls.map({ $0.handle.id })
    let status = ids.withUnsafeBufferPointer({ buffer in
      eval_program(&state, buffer.baseAddress, buffer.count, reportDiagnostic(error:state:))
    })
    return Int(status)
  }

}

private func reportDiagnostic(error: EvalError, state: UnsafePointer<EvalState>?) {
  let message = String(cString: error.message)
  print("\(error.start): error: \(message)")
}

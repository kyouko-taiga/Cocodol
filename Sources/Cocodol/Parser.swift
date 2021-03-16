import CCocodol

/// CoCoDoL's parser.
public final class Parser {

  /// The internal state of the parser.
  var state = ParserState()

  /// The context in which the parser operates.
  public let context: Context

  /// Creates a new parser in the given context.
  ///
  /// - Parameter context: An AST context.
  public init(in context: Context) {
    self.context = context
    parser_init(&state, context.state, nil)
  }

  deinit {
    parser_deinit(&state)
  }

  /// Parses a sequence of top-level declarations from the input buffer.
  public func parse() -> [Decl] {
    var stmtv: UnsafeMutablePointer<NodeID>?
    let stmtc = CCocodol.parse(&state, &stmtv, reportDiagnostic(error:user:))
    guard stmtc > 0 else { return [] }

    let decls: [Decl] = Array(
      unsafeUninitializedCapacity: stmtc,
      initializingWith: { (storage, count) in
        for i in 0 ..< stmtc {
          storage.baseAddress!.advanced(by: i)
            .initialize(to: NodeHandle(context: context, id: stmtv![i]).adaptAsDecl()!)
        }
        count = stmtc
      })

    stmtv!.deallocate()
    return decls
  }

  /// Parses a single declaration.
  public func parseDecl() -> Decl? {
    let id = CCocodol.parse_decl(&state, reportDiagnostic(error:user:))
    return NodeHandle(context: context, id: id).adaptAsDecl()
  }

  /// Parses a single expression.
  public func parseExpr() -> Expr? {
    let id = CCocodol.parse_expr(&state, reportDiagnostic(error:user:))
    return NodeHandle(context: context, id: id).adaptAsExpr()
  }

  /// Parses a single statement or declaration.
  public func parseStmt() -> Node? {
    let id = CCocodol.parse_stmt(&state, reportDiagnostic(error:user:))
    let handle = NodeHandle(context: context, id: id)
    return handle.adaptAsStmt() ?? handle.adaptAsExpr() ?? handle.adaptAsDecl()
  }

}

private func reportDiagnostic(error: ParseError, user: UnsafePointer<ParserState>?) {
  let message = String(cString: error.message)
  print("\(error.location): error: \(message)")
}

import CCocodol

/// An iterator that tokenizes the contents of a source file.
public final class Lexer: IteratorProtocol {

  /// The internal state of the lexer.
  var state = LexerState()

  /// The input string representing the program source.
  let source: ManagedStringBuffer

  /// Creates a new lexer in the given context.
  ///
  /// - Parameter context: An AST context.
  public init(in context: Context) {
    self.source = context.source
    lexer_init(&state, self.source.data)
  }

  deinit {
    lexer_deinit(&state)
  }

  /// Generates the next token, if any.
  public func next() -> Token? {
    var cToken = CCocodol.Token()
    guard lexer_next(&state, &cToken) else { return nil }
    return Token(cToken: cToken, buffer: source)
  }

}

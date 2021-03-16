import CCocodol

/// A structure that holds AST nodes along with other long-lived metadata.
public final class Context {

  /// The internal state of the context.
  ///
  /// - Remark: This must be stored as a pointer rather than a concrete object, so that we can wrap
  ///   re-entrant C functions that accept a pointer to the context without violating Swift's law
  ///   of exclusivity.
  var state: UnsafeMutablePointer<CCocodol.Context>?

  /// The input string representing the program source.
  var source: ManagedStringBuffer

  /// Creates a new context, initialized with the given program source.
  ///
  /// - Parameter source: A program source.
  public init(source: String) {
    self.source = ManagedStringBuffer(copying: source)
    state = .allocate(capacity: 1)
    context_init(state, self.source.data)
  }

  deinit {
    context_deinit(state)
    state!.deallocate()
  }

}

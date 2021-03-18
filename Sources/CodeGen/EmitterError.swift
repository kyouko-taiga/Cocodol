/// An error that occured during LLVM IR code generation.
public struct EmitterError: Error {

  /// The error's message.
  public var message: String

  /// The error's location in the source program.
  public var range: Range<Int>

}

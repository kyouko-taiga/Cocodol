/// Asserts that the code path is unreachable.
///
/// - Parameters:
///   - file: The file name to report in case the code path was reached. The default is the file
///     where this function is called.
///   - line: The line number to report in case the code path was reached. The default is the line
///     number where this function is called.
@inlinable public func unreachable(
  file: StaticString = #file,
  line: UInt = #line
) -> Never {
  // Small trick to check if we're in debug mode.
  var isDebug = false
  assert({ isDebug = true; return true }())

  if isDebug {
    fatalError(file: file, line: line)
  } else {
    return unsafeBitCast((), to: Never.self)
  }
}

import Foundation

/// Executes the given executable.
///
/// - Parameters:
///   - path: The path to the executable that should be ran.
///   - args: A list of arguments that are passed to the executable.
///
/// - Returns: The standard output of the process, or `nil` if it was empty.
@discardableResult
func exec(_ path: String, args: [String] = []) throws -> String? {
  let pipe = Pipe()
  let process = Process()

  process.executableURL = URL(fileURLWithPath: path).absoluteURL
  process.arguments = args
  process.standardOutput = pipe
  try process.run()
  process.waitUntilExit()

  guard let output = String(
          data: pipe.fileHandleForReading.readDataToEndOfFile(),
          encoding: .utf8)
  else {
    return nil
  }

  let result = output.trimmingCharacters(in: .whitespacesAndNewlines)
  return result.isEmpty ? nil : result
}

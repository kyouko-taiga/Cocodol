/// A view of a string's contents as a collection of characters
public struct CharacterView {

  let buffer: ManagedStringBuffer

  public let startIndex: Int

  public let endIndex: Int

}

extension CharacterView: Equatable {

  public static func == (lhs: CharacterView, rhs: CharacterView) -> Bool {
    return (lhs.buffer.data == rhs.buffer.data) && (lhs.endIndex == rhs.endIndex)
  }

  public static func ==<S> (lhs: CharacterView, rhs: S) -> Bool where S: StringProtocol {
    guard (lhs.count == rhs.count) else { return false }
    for (a, b) in zip(lhs, rhs) {
      guard a == b else { return false }
    }
    return true
  }

}

extension CharacterView: Hashable {

  public func hash(into hasher: inout Hasher) {
    hasher.combine(buffer.data)
    hasher.combine(endIndex)
  }

}

extension CharacterView: BidirectionalCollection {

  public func index(after i: Int) -> Int {
    return i + 1
  }

  public func index(before i: Int) -> Int {
    return i - 1
  }

  public subscript(position: Int) -> Character {
    let scalarValue = UInt8(bitPattern: buffer[position])
    return Character(Unicode.Scalar(scalarValue))
  }

}

extension CharacterView: CustomStringConvertible {

  public var description: String { String(self) }

}

#if !canImport(ObjectiveC)
import XCTest

extension LexerTests {
    // DO NOT MODIFY: This is autogenerated, use:
    //   `swift test --generate-linuxmain`
    // to regenerate.
    static let __allTests__LexerTests = [
        ("testLexFloat", testLexFloat),
        ("testLexInteger", testLexInteger),
        ("testLexName", testLexName),
        ("testLexOperator", testLexOperator),
    ]
}

public func __allTests() -> [XCTestCaseEntry] {
    return [
        testCase(LexerTests.__allTests__LexerTests),
    ]
}
#endif

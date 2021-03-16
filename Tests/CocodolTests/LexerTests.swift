import XCTest
import Cocodol

class LexerTests: XCTestCase {

  func tokenize(_ source: String) -> [Token] {
    var tokens: [Token] = []
    let context = Context(source: source)
    let lexer = Lexer(in: context)
    while let token = lexer.next() {
      tokens.append(token)
    }
    return tokens
  }

  func testLexName() throws {
    var token = try XCTUnwrap(tokenize("CoCoDo").first)
    XCTAssertEqual(token.kind, .name)
    XCTAssert(token.value == "CoCoDo")

    token = try XCTUnwrap(tokenize("_CoD0_D0").first)
    XCTAssertEqual(token.kind, .name)
    XCTAssert(token.value == "_CoD0_D0")
  }

  func testLexInteger() throws {
    let token = try XCTUnwrap(tokenize("42").first)
    XCTAssertEqual(token.kind, .integer)
    XCTAssert(token.value == "42")
  }

  func testLexFloat() throws {
    let token = try XCTUnwrap(tokenize("4.2").first)
    XCTAssertEqual(token.kind, .float)
    XCTAssert(token.value == "4.2")

    let tokens = tokenize("4.")
    XCTAssertEqual(tokens.count, 2)
  }

  func testLexOperator() throws {
    var token = try XCTUnwrap(tokenize("+").first)
    XCTAssertEqual(token.kind, .plus)
    XCTAssert(token.value == "+")

    token = try XCTUnwrap(tokenize("-").first)
    XCTAssertEqual(token.kind, .minus)
    XCTAssert(token.value == "-")

    token = try XCTUnwrap(tokenize("*").first)
    XCTAssertEqual(token.kind, .star)
    XCTAssert(token.value == "*")

    token = try XCTUnwrap(tokenize("/").first)
    XCTAssertEqual(token.kind, .slash)
    XCTAssert(token.value == "/")

    token = try XCTUnwrap(tokenize("%").first)
    XCTAssertEqual(token.kind, .percent)
    XCTAssert(token.value == "%")

    token = try XCTUnwrap(tokenize("|").first)
    XCTAssertEqual(token.kind, .pipe)
    XCTAssert(token.value == "|")

    token = try XCTUnwrap(tokenize("&").first)
    XCTAssertEqual(token.kind, .amp)
    XCTAssert(token.value == "&")

    token = try XCTUnwrap(tokenize("^").first)
    XCTAssertEqual(token.kind, .caret)
    XCTAssert(token.value == "^")

    token = try XCTUnwrap(tokenize("~").first)
    XCTAssertEqual(token.kind, .tilde)
    XCTAssert(token.value == "~")

    token = try XCTUnwrap(tokenize("!").first)
    XCTAssertEqual(token.kind, .not)
    XCTAssert(token.value == "!")

    token = try XCTUnwrap(tokenize("<<").first)
    XCTAssertEqual(token.kind, .lShift)
    XCTAssert(token.value == "<<")

    token = try XCTUnwrap(tokenize(">>").first)
    XCTAssertEqual(token.kind, .rShift)
    XCTAssert(token.value == ">>")

    token = try XCTUnwrap(tokenize("and").first)
    XCTAssertEqual(token.kind, .and)
    XCTAssert(token.value == "and")

    token = try XCTUnwrap(tokenize("or").first)
    XCTAssertEqual(token.kind, .or)
    XCTAssert(token.value == "or")

    token = try XCTUnwrap(tokenize("<").first)
    XCTAssertEqual(token.kind, .lt)
    XCTAssert(token.value == "<")

    token = try XCTUnwrap(tokenize("<=").first)
    XCTAssertEqual(token.kind, .le)
    XCTAssert(token.value == "<=")

    token = try XCTUnwrap(tokenize(">").first)
    XCTAssertEqual(token.kind, .gt)
    XCTAssert(token.value == ">")

    token = try XCTUnwrap(tokenize(">=").first)
    XCTAssertEqual(token.kind, .ge)
    XCTAssert(token.value == ">=")

    token = try XCTUnwrap(tokenize("==").first)
    XCTAssertEqual(token.kind, .eq)
    XCTAssert(token.value == "==")

    token = try XCTUnwrap(tokenize("!=").first)
    XCTAssertEqual(token.kind, .ne)
    XCTAssert(token.value == "!=")

    token = try XCTUnwrap(tokenize("=").first)
    XCTAssertEqual(token.kind, .assign)
    XCTAssert(token.value == "=")
  }

}

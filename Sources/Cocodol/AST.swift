import CCocodol

typealias NodePointer = UnsafePointer<CCocodol.Node>

/// The handle for an AST node stored in a context.
///
/// - Important: This type's storeage should remain lightweight. It is currently 2 words wide.
public struct NodeHandle {

  /// The context in which the node is stored.
  let context: Context

  /// The ID of the node.
  let id: Int

  /// A pointer to the node storage.
  ///
  /// - Important: Do not store this property. The pointer may be invalidated every time the owning
  ///   context is mutated.
  var pointer: NodePointer { UnsafePointer(context_get_nodeptr(context.state, id)) }

  /// The kind of the node.
  var kind: NodeKind { pointer.pointee.kind }

  /// The contents of the node.
  var contents: CCocodol.NodeContents { pointer.pointee.bits }

  /// The range of the node in the source input.
  public var range: Range<Int> { pointer.pointee.start ..< pointer.pointee.end }

  /// A value describing the type of an AST walk event.
  public enum WalkEvent {
    case enter
    case exit
  }

  /// Walks the AST rooted by this node, calling the given closure every time the method enters or
  /// exits a node.
  ///
  /// - Parameter visitor: A closure that is called every time the method enters or exits a node.
  ///   `visitor` must accept a handle for the node being visited and a value indicating whether
  ///   the visit occurs en entry or exit.
  ///
  ///   When `visitor` is called on entry, its return value determines whether this method should
  ///   continue visiting the node's children. When `visitor` is called on exit, its return value
  ///   determines whether this method should abort the walk altogether.
  ///
  /// - Returns: `true` if the walk was aborted.
  @discardableResult
  public func walk(with visitor: @escaping (NodeHandle, WalkEvent) -> Bool) -> Bool {
    struct WalkEnv {
      let context: Context
      let fn: (NodeHandle, WalkEvent) -> Bool
    }

    // Build an environment for the visitor.
    var env = WalkEnv(context: context, fn: visitor)

    // Walk the AST.
    return withUnsafeMutablePointer(to: &env, { envp in
      node_walk(id, context.state, envp, { (id, _, event, envp) -> Bool in
        let env = envp!.load(as: WalkEnv.self)
        return env.fn(NodeHandle(context: env.context, id: id), event ? .enter : .exit)
      })
    })
  }

  /// Accepts the given node walker.
  ///
  /// - Parameter walker: A node walker.
  ///
  /// - Returns: `true` if the walk was aborted.
  @discardableResult
  public func walk<Walker>(with walker: Walker) -> Bool where Walker: NodeWalker {
    return walk(with: { (node, event) -> Bool in
      return event == .enter
        ? walker.willVisit(node)
        : walker.didVisit(node)
    })
  }

  /// Wraps this handle into the specified adapter type.
  ///
  /// - Parameter adapterType: The type of the adapter.
  public func adapt<T>(as adapterType: T.Type) -> T? where T: Node {
    return T(handle: self)
  }

  /// Wraps this handle into a type-erased adapter.
  public func adaptAsAny() -> Node {
    if let decl = adaptAsDecl() { return decl }
    if let expr = adaptAsExpr() { return expr }
    if let stmt = adaptAsStmt() { return stmt }

    assert(kind == nk_error)
    return ErrorNode(handle: self)
  }

  /// Wraps this handle into a declaration adapter.
  public func adaptAsDecl() -> Decl? {
    switch kind {
    case nk_top_decl    : return adapt(as: TopDecl.self)
    case nk_var_decl    : return adapt(as: VarDecl.self)
    case nk_fun_decl    : return adapt(as: FunDecl.self)
    case nk_obj_decl    : return adapt(as: ObjDecl.self)
    default: return nil
    }
  }

  /// Wraps this handle into an expression adapter.
  public func adaptAsExpr() -> Expr? {
    switch kind {
    case nk_declref_expr: return adapt(as: DeclRefExpr.self)
    case nk_bool_expr   : return adapt(as: BoolExpr.self)
    case nk_integer_expr: return adapt(as: IntegerExpr.self)
    case nk_float_expr  : return adapt(as: FloatExpr.self)
    case nk_unary_expr  : return adapt(as: UnaryExpr.self)
    case nk_binary_expr : return adapt(as: BinaryExpr.self)
    case nk_member_expr : return adapt(as: MemberExpr.self)
    case nk_apply_expr  : return adapt(as: ApplyExpr.self)
    case nk_paren_expr  : return adapt(as: ParenExpr.self)
    default: return nil
    }
  }

  /// Wraps this handle into a statement adapter.
  public func adaptAsStmt() -> Stmt? {
    switch kind {
    case nk_brace_stmt  : return adapt(as: BraceStmt.self)
    case nk_expr_stmt   : return adapt(as: ExprStmt.self)
    case nk_if_stmt     : return adapt(as: IfStmt.self)
    case nk_while_stmt  : return adapt(as: WhileStmt.self)
    case nk_brk_stmt    : return adapt(as: BrkStmt.self)
    case nk_nxt_stmt    : return adapt(as: NxtStmt.self)
    case nk_ret_stmt    : return adapt(as: RetStmt.self)
    default: return nil
    }
  }

  /// The "unparsed" representation of the node.
  ///
  /// This method encodes a source representation of the node from its structured contents. The
  /// result might differ from the original program source, as lexing and/or parsing will have
  /// elided some information (e.g., comments and unecessary white spaces).
  public func unparse() -> String {
    return (adaptAsDecl() ?? adaptAsExpr() ?? adaptAsStmt())?.unparse() ?? "$\(kind)"
  }

}

extension NodeHandle: Hashable {

  public func hash(into hasher: inout Hasher) {
    hasher.combine(context.state)
    hasher.combine(id)
  }

  public static func == (lhs: NodeHandle, rhs: NodeHandle) -> Bool {
    return (lhs.context.state == rhs.context.state) && (lhs.id == rhs.id)
  }

}

extension NodeHandle: CustomStringConvertible {

  public var description: String {
    return "\(kind)(id: \(id))"
  }

}

/// A type that handles AST visitation events.
public protocol NodeWalker {

  /// This method is called when the walker is about to visit a node.
  ///
  /// - Parameter node: The node that will be visited.
  ///
  /// - Returns: `true` if the walker should visit `node`, or `false` if it should skip to the next
  ///   node in the AST. The default implementation always returns `true`.
  func willVisit(_ node: NodeHandle) -> Bool

  /// This method is called when the walker finished visiting a node.
  ///
  /// - Parameter node: The node that has been visited.
  ///
  /// - Returns: `true` if the walker should visit the next node in the AST, or `false` if the walk
  ///   should abort. The default implementation always returns `true`.
  func didVisit(_ node: NodeHandle) -> Bool

}

extension NodeWalker {

  public func willVisit(_ node: NodeHandle) -> Bool {
    return true
  }

  public func didVisit(_ node: NodeHandle) -> Bool {
    return true
  }

}

/// An API adapter for an AST node stored in a context.
///
/// Conforming types should serve as API adapters to interact with an AST node represented the
/// handle.
public protocol Node {

  /// The handle to the node.
  var handle: NodeHandle { get }

  /// Creates an adapter from the given handle.
  init?(handle: NodeHandle)

  /// The "unparsed" representation of the node.
  func unparse() -> String

}

extension Node {

  /// Walks the AST rooted by this node, calling the given closure every time the method enters or
  /// exits a node.
  @discardableResult
  public func walk(with visitor: @escaping (NodeHandle, NodeHandle.WalkEvent) -> Bool) -> Bool {
    handle.walk(with: visitor)
  }

  /// Accepts the given node walker.
  @discardableResult
  public func walk<Walker>(with walker: Walker) -> Bool where Walker: NodeWalker {
    handle.walk(with: walker)
  }

}

/// An error node.
public struct ErrorNode: Node {

  public let handle: NodeHandle

  public init(handle: NodeHandle) {
    self.handle = handle
  }

  public func unparse() -> String {
    return "$\(handle.kind)"
  }

}

/// A declaration.
public protocol Decl: Node {}

/// A top-level declaration.
public struct TopDecl: Decl {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_top_decl else { return nil }
    self.handle = handle
  }

  /// The statements that are gathered in this declaration.
  public var stmts: Statements {
    return Statements(
      context: handle.context,
      stmtv: handle.contents.brace_stmt.stmtv,
      endIndex: handle.contents.brace_stmt.stmtc)
  }

  public func unparse() -> String {
    var acc = ""

    for (i, stmt) in stmts.enumerated() {
      acc += stmt.unparse()
      if i < stmts.count - 1 {
        acc += "\n"
      }
    }

    return acc
  }

}

/// A variable declaration.
public struct VarDecl: Decl {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_var_decl else { return nil }
    self.handle = handle
  }

  /// The name of the variable.
  public var name: CharacterView {
    return CharacterView(
      buffer: handle.context.source,
      startIndex: handle.contents.var_decl.name.start,
      endIndex: handle.contents.var_decl.name.end)
  }

  /// The variable's initializer, if any.
  public var initializer: NodeHandle? {
    let id = handle.contents.var_decl.initializer
    return id != ~0
      ? NodeHandle(context: handle.context, id: id)
      : nil
  }

  public func unparse() -> String {
    if let initializer = self.initializer {
      let i = initializer.adaptAsExpr().map({ $0.stringified }) ?? "$\(initializer.kind)"
      return "var \(name) = \(i)"
    } else {
      return "var \(name)"
    }
  }

}

/// A function declaration.
public struct FunDecl: Decl {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_fun_decl else { return nil }
    self.handle = handle
  }

  /// The name of the function.
  public var name: CharacterView {
    return CharacterView(
      buffer: handle.context.source,
      startIndex: handle.contents.fun_decl.name.start,
      endIndex: handle.contents.fun_decl.name.end)
  }

  /// The parameters of the function.
  public var params: [Token] {
    var value: [Token] = []
    value.reserveCapacity(handle.contents.fun_decl.paramc)
    for i in 0 ..< handle.contents.fun_decl.paramc {
      value.append(
        Token(cToken: handle.contents.fun_decl.paramv[i], buffer: handle.context.source))
    }
    return value
  }

  /// The body of the function.
  public var body: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.fun_decl.body)
  }

  /// The identifiers captured by the function.
  public var captures: [CharacterView] {
    let tokens = UnsafeMutablePointer<UnsafeMutablePointer<CCocodol.Token>?>.allocate(
      capacity: Int(truncatingIfNeeded: MAX_CAPTURE_COUNT))
    defer { tokens.deallocate() }

    let count = capture_list(handle.id, handle.context.state, tokens)
    return (0 ..< count).map({ (i) -> CharacterView in
      let cToken = tokens[i]!.pointee
      return CharacterView(
        buffer: handle.context.source, startIndex: cToken.start, endIndex: cToken.end)
    })
  }

  public func unparse() -> String {
    let params = self.params
      .map({ String(describing: $0.value) })
      .joined(separator: ", ")
    let body = self.body.adapt(as: BraceStmt.self).map({ $0.unparse() })
      ?? "$\(self.body.kind)"

    return "fun \(name)(\(params)) \(body)"
  }

}

/// A type declaration.
public struct ObjDecl: Decl {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_obj_decl else { return nil }
    self.handle = handle
  }

  /// The name of the type.
  public var name: CharacterView {
    return CharacterView(
      buffer: handle.context.source,
      startIndex: handle.contents.obj_decl.name.start,
      endIndex: handle.contents.obj_decl.name.end)
  }

  /// The body of the type.
  public var body: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.obj_decl.body)
  }

  public func unparse() -> String {
    let body = self.body.adapt(as: BraceStmt.self).map({ $0.unparse() })
      ?? "$\(self.body.kind)"

    return "obj \(name) \(body)"
  }

}

/// An expression.
public protocol Expr: Node {}

extension Expr {

  var stringified: String {
    let desc = unparse()
    if (handle.kind == nk_apply_expr) || (handle.kind == nk_paren_expr) {
      return desc
    } else if desc.contains(" ") && (desc.first != "(") && (desc.last != ")") {
      return "(" + desc + ")"
    } else {
      return desc
    }
  }

}

/// A reference to a type or value declaration.
public struct DeclRefExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_declref_expr else { return nil }
    self.handle = handle
  }

  /// The name of the declaration being referred.
  public var name: CharacterView {
    return CharacterView(
      buffer: handle.context.source,
      startIndex: handle.contents.declref_expr.start,
      endIndex: handle.contents.declref_expr.end)
  }

  public func unparse() -> String {
    return String(name)
  }

}

/// A Boolean literal expression.
public struct BoolExpr: Expr {

  public let handle: NodeHandle

  /// The value of the literal.
  public var value: Bool {
    return handle.contents.bool_expr
  }

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_bool_expr else { return nil }
    self.handle = handle
  }

  public func unparse() -> String {
    return String(describing: value)
  }

}

/// An integer literal expression.
public struct IntegerExpr: Expr {

  public let handle: NodeHandle

  /// The value of the literal.
  public var value: Int {
    return handle.contents.integer_expr
  }

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_integer_expr else { return nil }
    self.handle = handle
  }

  public func unparse() -> String {
    return String(describing: value)
  }

}

// A float literal expression.
public struct FloatExpr: Expr {

  public let handle: NodeHandle

  /// The value of the literal.
  public var value: Double {
    return handle.contents.float_expr
  }

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_float_expr else { return nil }
    self.handle = handle
  }

  public func unparse() -> String {
    return String(describing: value)
  }

}

/// A unary operator application written with a prefix expression.
public struct UnaryExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_unary_expr else { return nil }
    self.handle = handle
  }

  /// The operator of the expression.
  public var op: Token {
    return Token(cToken: handle.contents.unary_expr.op, buffer: handle.context.source)
  }

  /// The operand.
  public var subexpr: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.unary_expr.subexpr)
  }

  public func unparse() -> String {
    let subexpr = self.subexpr.adaptAsExpr().map({ $0.stringified })
      ?? "$\(self.subexpr.kind)"

    return "\(op.value)\(subexpr)"
  }

}

/// A binary operator application written with an infix expression.
public struct BinaryExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_binary_expr else { return nil }
    self.handle = handle
  }

  /// The operator of the expression.
  public var op: Token {
    return Token(cToken: handle.contents.binary_expr.op, buffer: handle.context.source)
  }

  /// The left operand.
  public var lhs: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.binary_expr.lhs)
  }

  /// The right operand.
  public var rhs: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.binary_expr.rhs)
  }

  public func unparse() -> String {
    let lhs = self.lhs.adaptAsExpr().map({ $0.stringified })
      ?? "$\(self.lhs.kind)"
    let rhs = self.rhs.adaptAsExpr().map({ $0.stringified })
      ?? "$\(self.rhs.kind)"
    return "\(lhs) \(op.value) \(rhs)"
  }

}

/// A member expression (e.g., `foo.bar`).
public struct MemberExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_member_expr else { return nil }
    self.handle = handle
  }

  /// The base expression.
  public var base: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.member_expr.base)
  }

  /// The name of the member.
  public var member: CharacterView {
    return CharacterView(
      buffer: handle.context.source,
      startIndex: handle.contents.member_expr.member.start,
      endIndex: handle.contents.member_expr.member.end)
  }

  public func unparse() -> String {
    let base = self.base.adaptAsExpr().map({ $0.stringified })
      ?? "$\(self.base.kind)"
    return "\(base).\(member)"
  }

}

/// A function application.
public struct ApplyExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_apply_expr else { return nil }
    self.handle = handle
  }

  /// The callee's expression.
  public var callee: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.apply_expr.callee)
  }

  /// The arguments of the call.
  public var args: [NodeHandle] {
    var value: [NodeHandle] = []
    value.reserveCapacity(handle.contents.apply_expr.argc)
    for i in 0 ..< handle.contents.apply_expr.argc {
      value.append(NodeHandle(context: handle.context, id: handle.contents.apply_expr.argv[i]))
    }
    return value
  }

  public func unparse() -> String {
    let callee = self.callee.adaptAsExpr().map({ $0.stringified })
      ?? "$\(self.callee.kind)"

    let args = self.args.map({ arg in
      arg.adaptAsExpr().map({ $0.unparse() })
        ?? "$\(arg.kind)"
    }).joined(separator: ", ")

    return "\(callee)(\(args))"
  }

}

/// A parenthesized expression.
public struct ParenExpr: Expr {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_paren_expr else { return nil }
    self.handle = handle
  }

  /// The sub-expression.
  public var subexpr: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.paren_expr)
  }

  public func unparse() -> String {
    let subexpr = self.subexpr.adaptAsExpr().map({ $0.unparse() })
      ?? "$\(self.subexpr.kind)"
    return "(\(subexpr))"
  }

}

/// A statement.
public protocol Stmt: Node {}

/// A brace statement.
public struct BraceStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_brace_stmt else { return nil }
    self.handle = handle
  }

  /// The statements that reside in this scope.
  public var stmts: Statements {
    return Statements(
      context: handle.context,
      stmtv: handle.contents.brace_stmt.stmtv,
      endIndex: handle.contents.brace_stmt.stmtc)
  }

  /// The parent lexical scope, if any.
  public var parent: NodeHandle? {
    let id = handle.contents.brace_stmt.parent
    return id != ~0
      ? NodeHandle(context: handle.context, id: id)
      : nil
  }

  public func unparse() -> String {
    var acc = "{\n"

    for stmt in stmts {
      for line in stmt.unparse().split(separator: "\n") {
        acc += "  " + line + "\n"
      }
    }

    acc += "}"
    return acc
  }

}

/// A expression statement.
///
/// This node only serve to signal that the contained expression should be interpreted as a
/// statement (i.e., the resulting value does not form a sub-expression).
public struct ExprStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_expr_stmt else { return nil }
    self.handle = handle
  }

  /// The condition of the statement.
  public var expr: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.expr_stmt)
  }

  public func unparse() -> String {
    return expr.unparse()
  }

}

/// A conditional (a.k.a. `if`) statement.
public struct IfStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_if_stmt else { return nil }
    self.handle = handle
  }

  /// The condition of the statement.
  public var cond: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.if_stmt.cond)
  }

  /// The "then" branch of the statement.
  public var then_: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.if_stmt.then_)
  }

  /// The "else" branch of the statement, if any.
  ///
  /// In a valid AST, this is either `nil`, or a brace statement, or another if statement.
  public var else_: NodeHandle? {
    let id = handle.contents.if_stmt.else_
    return id != ~0
      ? NodeHandle(context: handle.context, id: id)
      : nil
  }

  public func unparse() -> String {
    let cond = self.cond.adaptAsExpr().map({ $0.unparse() })
      ?? "$\(self.cond.kind)"
    let then_ = self.then_.adaptAsStmt().map({ $0.unparse() })
      ?? "$\(self.then_.kind)"

    if let else_ = self.else_ {
      let e = else_.adaptAsStmt().map({ $0.unparse() })
        ?? "$\(else_.kind)"
      return "if \(cond) \(then_) else \(e)"
    } else {
      return "if \(cond) \(then_)"
    }
  }

}

/// A loop (a.k.a. `while`) statement.
public struct WhileStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_while_stmt else { return nil }
    self.handle = handle
  }

  /// The condition of the statement.
  public var cond: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.while_stmt.cond)
  }

  /// The body of the statement.
  public var body: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.while_stmt.body)
  }

  public func unparse() -> String {
    let cond = self.cond.adaptAsExpr().map({ $0.unparse() })
      ?? "$\(self.cond.kind)"
    let body = self.body.adaptAsStmt().map({ $0.unparse() })
      ?? "$\(self.body.kind)"

    return "while \(cond) \(body)"
  }

}

/// A break (a.k.a. `brk`) statement.
public struct BrkStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_brk_stmt else { return nil }
    self.handle = handle
  }

  public func unparse() -> String {
    return "brk"
  }

}

/// A next (a.k.a. `nxt`) statement.
public struct NxtStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_nxt_stmt else { return nil }
    self.handle = handle
  }

  public func unparse() -> String {
    return "nxt"
  }

}

/// A return (a.k.a. `ret`) statement.
public struct RetStmt: Stmt {

  public let handle: NodeHandle

  public init?(handle: NodeHandle) {
    guard handle.kind == nk_ret_stmt else { return nil }
    self.handle = handle
  }

  /// The value being returned.
  public var value: NodeHandle {
    return NodeHandle(context: handle.context, id: handle.contents.ret_stmt)
  }

  public func unparse() -> String {
    let value = self.value.adaptAsExpr().map({$0.unparse() })
      ?? "$\(self.value.kind)"

    return "ret \(value)"
  }

}

extension NodeKind: CustomStringConvertible {

  public var description: String {
    switch self {
    case nk_error       : return "Error"

    case nk_top_decl    : return "TopDecl"
    case nk_var_decl    : return "VarDecl"
    case nk_fun_decl    : return "FunDecl"
    case nk_obj_decl    : return "ObjDecl"

    case nk_declref_expr: return "DeclRefExpr"
    case nk_bool_expr   : return "BoolExpr"
    case nk_integer_expr: return "IntegerExpr"
    case nk_float_expr  : return "FloatExpr"
    case nk_unary_expr  : return "UnaryExpr"
    case nk_binary_expr : return "BinaryExpr"
    case nk_member_expr : return "MemberExpr"
    case nk_apply_expr  : return "ApplyExpr"
    case nk_paren_expr  : return "ParenExpr"

    case nk_brace_stmt  : return "BraceStmt"
    case nk_if_stmt     : return "IfStmt"
    case nk_while_stmt  : return "WhileStmt"
    case nk_brk_stmt    : return "BrkStmt"
    case nk_nxt_stmt    : return "NxtStmt"
    case nk_ret_stmt    : return "RetStmt"

    default: unreachable()
    }
  }

}

/// A collection of statements.
public struct Statements: BidirectionalCollection {

  /// The context in which the nodes are stored.
  fileprivate let context: Context

  /// The wrapped buffer.
  fileprivate let stmtv: UnsafeMutablePointer<NodeID>!

  public var startIndex: Int { 0 }

  public let endIndex: Int

  public func index(after i: Int) -> Int {
    return i + 1
  }

  public func index(before i: Int) -> Int {
    return i - 1
  }

  public subscript(position: Int) -> NodeHandle {
    NodeHandle(context: context, id: stmtv[position])
  }

}

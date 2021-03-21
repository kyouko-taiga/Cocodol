import Cocodol
import LLVM

/// A LLVM IR code generator.
public final class Emitter {

  /// The builder that is used to generate LLVM IR instructions.
  let builder: IRBuilder

  /// A collection with information about each function traversed by the code generator.
  var functionContexts = [FunContext(decl: nil)]

  /// A collection with information about each loop traversed by the code generator.
  var loops: [LoopContext] = []

  /// Creates a new generator.
  ///
  /// - Parameter builder: An instruction builder.
  init(builder: IRBuilder) {
    self.builder = builder
  }

  /// The LLVM context owning the module.
  var llvm: LLVM.Context { builder.module.context }

  /// The LLVM module being generated.
  var module: Module { builder.module }

  /// The `i32` type.
  var i32: IntType { IntType(width: 32, in: llvm) }

  /// The `i64` type.
  var i64: IntType { IntType(width: 64, in: llvm) }

  /// The `f64` type.
  var f64: FloatType { FloatType(kind: .double, in: llvm) }

  /// The `void` type.
  var void: VoidType { VoidType(in: llvm) }

  /// The type of any Cocodol object.
  lazy var any: StructType = {
    // let ty = VectorType(elementType: i64, count: 2)
    let ty = builder.createStruct(name: "_Any")
    ty.setBody([i64, i64])
    return ty
  }()

  /// Returns the type of a user function for the given number of parameters.
  func userFunType(paramCount: Int) -> FunctionType {
    return FunctionType(
      Array(repeating: PointerType(pointee: any), count: paramCount + 1),
      any)
  }

  /// C's `malloc` function.
  var mallocFunction: Function {
    if let fun = module.function(named: "malloc") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "malloc", type: FunctionType([i64], PointerType.toVoid))
  }

  /// Cocodol's built-in `drop` function.
  var dropFunction: Function {
    if let fun = module.function(named: "_cocodol_drop") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "_cocodol_drop", type: FunctionType([i64, i64], void))
  }

  /// Cocodol's built-in `copy` function.
  var copyFunction: Function {
    if let fun = module.function(named: "_cocodol_copy") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "_cocodol_copy", type: FunctionType([i64, i64], any))
  }

  /// The function that implements Cocodol's built-in unary operators.
  var unaryOperator: Function {
    if let fun = module.function(named: "_cocodol_unop") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "_cocodol_unop", type: FunctionType([i64, i64, i32], any))
  }

  /// The function that implements Cocodol's built-in binary operators.
  var binaryOperator: Function {
    if let fun = module.function(named: "_cocodol_binop") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "_cocodol_binop", type: FunctionType([i64, i64, i64, i64, i32], any))
  }

  /// Cocodol's built-in `print` function.
  var printFunction: Function {
    if let fun = module.function(named: "_cocodol_print") {
      return fun
    }

    // Forward-declare the function
    return builder.addFunction(
      "_cocodol_print", type: FunctionType([i64, i64], void))
  }

  /// Cocodol's built-in `print` function, wrapped as a function object.
  var printFunctionObject: IRValue {
    if let fun = module.function(named: "_cocodol_print.wrapper") {
      return fun
    }

    // Save the current insertion pointer.
    let current = builder.insertBlock

    var fun = builder.addFunction(
      "_cocodol_print.wrapper", type: userFunType(paramCount: 1))
    fun.linkage = .private
    fun.addAttribute(.argmemonly, to: .function)
    fun.addAttribute(.norecurse , to: .function)
    fun.addAttribute(.nounwind  , to: .function)
    fun.addAttribute(.ssp       , to: .function)
    fun.addAttribute(.nocapture , to: .argument(0))
    fun.addAttribute(.readonly  , to: .argument(0))
    fun.addAttribute(.nocapture , to: .argument(1))
    fun.addAttribute(.readnone  , to: .argument(1))

    let entry = fun.appendBasicBlock(named: "entry")
    builder.positionAtEnd(of: entry)

    let _0 = builder.buildLoad(
      builder.buildStructGEP(fun.parameters[0], type: any, index: 0), type: i64)
    let _1 = builder.buildLoad(
      builder.buildStructGEP(fun.parameters[0], type: any, index: 1), type: i64)
    _ = builder.buildCall(printFunction, args: [_0, _1])
    builder.buildRet(constObject(kind: .junk))

    // Restore the insertion pointer.
    current.map(builder.positionAtEnd(of:))
    return constObject(fun: fun)
  }

  /// Creates an alloca at the beginning of the current function.
  ///
  /// - Parameters:
  ///   - type: The sized type used to determine the amount of stack memory to allocate.
  ///   - name: The name for the newly inserted instruction.
  func addEntryAlloca(type: IRType, name: String = "") -> IRInstruction {
    // Save the current insertion pointer.
    let current = builder.insertBlock

    // Move to the function's entry.
    let entry = builder.currentFunction!.entryBlock!
    if let loc = entry.instructions.first(where: { !$0.isAAllocaInst }) {
      builder.positionBefore(loc)
    } else {
      builder.positionAtEnd(of: entry)
    }

    // Build the alloca.
    let alloca = builder.buildAlloca(type: type, name: name)

    // Restore the insertion pointer.
    current.map(builder.positionAtEnd(of:))
    return alloca
  }

  func constObject(kind: ObjectKind, _0: Int64 = 0, _1: Int64 = 0) -> Constant<Struct> {
    assert((kind.rawValue & _0) == 0)
    let kindValue = kind.rawValue | _0
    return any.constant(values: [i64.constant(kindValue), i64.constant(_1)])
  }

  func constObject(_ value: Bool) -> Constant<Struct> {
    return constObject(kind: .bool, _1: value ? 1 : 0)
  }

  func constObject(_ value: Int64) -> Constant<Struct> {
    return constObject(kind: .integer, _1: value)
  }

  func constObject(_ value: Double) -> Constant<Struct> {
    let kindValue = ObjectKind.float.rawValue
    let data = f64.constant(value)
    return any.constant(values: [i64.constant(kindValue), builder.buildBitCast(data, type: i64)])
  }

  func constObject(fun: Function) -> Constant<Struct> {
    let kindValue = ObjectKind.function.rawValue

    // _0 is the function pointer, tagged by the function tag.
    var _0 = builder.buildPtrToInt(fun, type: i64)
    _0 = builder.buildOr(_0, i64.constant(kindValue))

    return any.constant(values: [_0, i64.zero()])
  }

  /// Emits the given program in the specified context.
  func emit(program decls: [Decl]) throws {
    // Create the program's entry point.
    let main = builder.addFunction("main", type: FunctionType([], i32))
    main.addAttribute(.norecurse, to: .function)
    main.addAttribute(.nounwind , to: .function)
    main.addAttribute(.ssp      , to: .function)

    let entry = main.appendBasicBlock(named: "entry")
    builder.positionAtEnd(of: entry)

    // Scan the top-level declarations to forward-declare global functions.
    for case let decl as FunDecl in decls {
      let name = String(decl.name)
      guard module.function(named: name) == nil else {
        throw EmitterError(message: "duplicate function '\(name)'", range: decl.handle.range)
      }
      _ = builder.addFunction(name, type: userFunType(paramCount: decl.params.count))
    }

    // Emit the program's top-level declarations.
    try decls.forEach({ decl in try emit(any: decl, isTopLevel: true) })

    // Emit the return statement of the main function.
    builder.buildRet(i32.constant(0))
  }

  /// Emits an AST node.
  @discardableResult
  func emit(any node: Node, isTopLevel: Bool = false) throws -> EmitterAction {
    switch node {
    case let d as TopDecl   : try emit(decl: d)
    case let d as VarDecl   : try emit(decl: d, isTopLevel: isTopLevel)
    case let d as FunDecl   : try emit(decl: d, isTopLevel: isTopLevel)

    case let e as Expr      : try _ = emit(expr: e)

    case let s as BraceStmt : return try emit(stmt: s)
    case let s as ExprStmt  : return try emit(stmt: s)
    case let s as IfStmt    : return try emit(stmt: s)
    case let s as WhileStmt : return try emit(stmt: s)
    case let s as BrkStmt   : return try emit(stmt: s)
    case let s as NxtStmt   : return try emit(stmt: s)
    case let s as RetStmt   : return try emit(stmt: s)

    default: unreachable()
    }

    return .proceed
  }

  /// Emits a top-level declaration.
  func emit(decl: TopDecl) throws {
    try decl.stmts.forEach({ hndl in try emit(any: hndl.adaptAsAny()) })
  }

  /// Emits a variable declaration.
  func emit(decl: VarDecl, isTopLevel: Bool = false) throws {
    if (isTopLevel) {
      assert(builder.currentFunction?.name == "main")

      // Emit a global variable.
      let name = String(decl.name)
      guard module.global(named: name) == nil else {
        throw EmitterError(message: "duplicate identifier '\(name)'", range: decl.handle.range)
      }
      let global = builder.addGlobal(name, initializer: constObject(kind: .junk))

      // If the variable has an initializer, wrap it in an function `() -> Any` that's called at
      // the beginning of the `main` function.
      if let initializer = decl.initializer {
        // Save the current insertion pointer.
        let current = builder.insertBlock

        // Create the init function.
        let fun = builder.addFunction("\(name).initializer", type: FunctionType([], any))
        fun.addAttribute(.norecurse , to: .function)
        fun.addAttribute(.nounwind  , to: .function)
        fun.addAttribute(.ssp       , to: .function)

        let entry = fun.appendBasicBlock(named: "entry")
        builder.positionAtEnd(of: entry)

        // Emit the initializer's expression within the init function.
        builder.buildRet(try emit(expr: initializer.adaptAsExpr()!))

        // Restore the insertion pointer.
        current.map(builder.positionAtEnd(of:))

        // Call the initializer from the main function.
        let initValue = builder.buildCall(fun, args: [])
        builder.buildStore(initValue, to: global)
      }
    } else {
      assert(!functionContexts.isEmpty)

      // Allocate space for the variable.
      let name = String(decl.name)
      let local = addEntryAlloca(type: any, name: name)
      functionContexts[functionContexts.count - 1].bind(value: local, to: name)

      // Emit the variable's initialization.
      if let initializer = decl.initializer {
        let initValue = try emit(expr: initializer.adaptAsExpr()!)
        builder.buildStore(initValue, to: local)
      } else {
        builder.buildStore(constObject(kind: .junk), to: local)
      }
    }
  }

  /// Emits a function declaration.
  func emit(decl: FunDecl, isTopLevel: Bool = false) throws {
    // Save the current insertion pointer.
    let current = builder.insertBlock

    // Generate the function's name.
    let fun: Function
    if isTopLevel {
      // Global functions are forward-declared.
      fun = module.function(named: String(decl.name))!
    } else {
      // Mangle the name of the function, unless we're at the top level.
      var name = "_F" + functionContexts[1...].reversed().map({ (ctx) -> String in
        String(repeating: "_", count: ctx.scopes.count - 1) + String(ctx.decl!.name)
      }).joined()
      name.append(contentsOf: decl.name)

      // Check for duplicate declarations.
      guard module.function(named: name) == nil else {
        throw EmitterError(message: "duplicate function '\(name)'", range: decl.handle.range)
      }
      fun = builder.addFunction(name, type: userFunType(paramCount: decl.params.count))
    }

    // Emit the function.
    fun.addAttribute(.nounwind, to: .function)
    fun.addAttribute(.ssp     , to: .function)

    // Capture the function's environment.
    let captures = isTopLevel
      ? []
      : decl.captures

    var env: IRValue
    if !captures.isEmpty {
      // Allocate and populate the function's environment.
      let size = builder.buildAdd(
        emit(sizeOf: i64),
        builder.buildMul(i64.constant(captures.count), emit(sizeOf: any)))
      env = builder.buildCall(mallocFunction, args: [size])

      // Store the size of the environment.
      let count = builder.buildBitCast(env, type: PointerType(pointee: i64))
      builder.buildStore(i64.constant(captures.count), to: count)

      // Store the captured parameters.
      env = builder.buildGEP(env, type: PointerType.toVoid.pointee, indices: [emit(sizeOf: i64)])
      env = builder.buildBitCast(env, type: PointerType(pointee: any))

      for (i, capture) in captures.enumerated() {
        let val = builder.buildLoad(
          functionContexts.last!.value(boundTo: String(capture))!, type: any)
        let loc = builder.buildGEP(env, type: any, indices: [i64.constant(i)])
        builder.buildStore(emit(copy: val), to: loc)
      }
    } else {
      env = PointerType(pointee: any).null()
    }

    // Create a function object if the declaration is local.
    if !isTopLevel {
      let loc = addEntryAlloca(type: any, name: String(decl.name))
      builder.buildStore(constObject(fun: fun), to: loc)
      let _1 = builder.buildStructGEP(loc, type: any, index: 1)
      builder.buildStore(builder.buildPtrToInt(env, type: i64), to: _1)

      functionContexts[functionContexts.count - 1].bind(value: loc, to: String(decl.name))
    }

    var funCtx = FunContext(decl: decl)
    let entry = fun.appendBasicBlock(named: "entry")
    builder.positionAtEnd(of: entry)

    // Configure the local scope to map captured symbols onto the function's environment.
    for (i, capture) in captures.enumerated() {
      let loc = builder.buildGEP(fun.parameters.last!, type: any, indices: [i32.constant(i)])
      funCtx.bind(value: loc, to: String(capture))
    }

    // Configure the parameters.
    for (i, param) in decl.params.enumerated() {
//      fun.addAttribute(.byval     , to: .argument(i))
      fun.addAttribute(.nocapture , to: .argument(i))
      funCtx.bind(value: fun.parameter(at: i)!, to: String(param.value))
    }

    // Emit the function's body.
    functionContexts.append(funCtx)
    defer { functionContexts.removeLast() }

    switch try emit(stmt: decl.body.adapt(as: BraceStmt.self)!) {
    case .unwind(let handle):
      assert(handle == decl.handle)
    case .proceed:
      builder.buildRet(constObject(kind: .junk))
    }

    // Restore the insertion pointer.
    current.map(builder.positionAtEnd(of:))
  }

  /// Emits an expression.
  func emit(expr: Expr) throws -> IRValue {
    switch expr {
    case let e as DeclRefExpr : return try emit(expr: e)
    case let e as BoolExpr    : return emit(expr: e)
    case let e as IntegerExpr : return emit(expr: e)
    case let e as FloatExpr   : return emit(expr: e)
    case let e as UnaryExpr   : return try emit(expr: e)
    case let e as BinaryExpr  : return try emit(expr: e)
    case let e as ApplyExpr   : return try emit(expr: e)
    case let e as ParenExpr   : return try emit(expr: e)
    default: unreachable()
    }
  }

  /// Emits a declaration reference.
  func emit(expr: DeclRefExpr) throws -> IRValue {
    // Emit the built-in `print` function.
    if expr.name == "print" {
      return printFunctionObject
    }

    // Search within the locals.
    let name = String(expr.name)
    if let loc = functionContexts.last?.value(boundTo: name) {
      return builder.buildLoad(loc, type: any)
    }

    // Search within the global variables.
    if let global = module.global(named: name) {
      assert(!global.isAFunction)
      return builder.buildLoad(global, type: any)
    }

    // Search within the global functions.
    if let fun = module.function(named: name) {
      return constObject(fun: fun)
    }

    throw EmitterError(message: "unbound identifier '\(expr.name)'", range: expr.handle.range)
  }

  /// Emits a declaration reference as an l-value.
  func emit(lvalue expr: DeclRefExpr) throws -> IRValue {
    // Search within the locals.
    let name = String(expr.name)
    if let loc = functionContexts.last?.value(boundTo: name) {
      return loc
    }

    // Search within the globals.
    guard let global = module.global(named: name) else {
      throw EmitterError(message: "unbound identifier '\(expr.name)'", range: expr.handle.range)
    }
    return global
  }

  /// Emits a Boolean literal.
  func emit(expr: BoolExpr) -> IRValue {
    return constObject(expr.value)
  }

  /// Emits an integer literal.
  func emit(expr: IntegerExpr) -> IRValue {
    return constObject(Int64(expr.value))
  }

  /// Emits a floating point literal.
  func emit(expr: FloatExpr) -> IRValue {
    return constObject(expr.value)
  }

  /// Emits a unary expression
  func emit(expr: UnaryExpr) throws -> IRValue {
    // Emit the operands.
    let subexpr = try emit(expr: expr.subexpr.adaptAsExpr()!)

    // Emit a call to the built-in unary operator.
    return builder.buildCall(
      unaryOperator,
      args: [
        builder.buildExtractValue(subexpr, index: 0),
        builder.buildExtractValue(subexpr, index: 1),
        i32.constant(expr.op.kind.rawValue),
      ])
  }

  /// Emits a binary expression
  func emit(expr: BinaryExpr) throws -> IRValue {
    // Handle assignments.
    guard expr.op.kind != .assign else {
      return try emit(assignment: expr)
    }

    // Emit the operands.
    let lhs = try emit(expr: expr.lhs.adaptAsExpr()!)
    let rhs = try emit(expr: expr.rhs.adaptAsExpr()!)

    // Emit a call to the built-in binary operator.
    return builder.buildCall(
      binaryOperator,
      args: [
        builder.buildExtractValue(lhs, index: 0),
        builder.buildExtractValue(lhs, index: 1),
        builder.buildExtractValue(rhs, index: 0),
        builder.buildExtractValue(rhs, index: 1),
        i32.constant(expr.op.kind.rawValue),
      ])
  }

  /// Emits an assignment.
  func emit(assignment: BinaryExpr) throws -> IRValue {
    // Emit the left operand as an l-value.
    let lvalue: IRValue
    if let lhs = assignment.lhs.adapt(as: DeclRefExpr.self) {
      lvalue = try emit(lvalue: lhs)
    } else {
      throw EmitterError(message: "invalid l-value", range: assignment.lhs.range)
    }

    // Emit the assignment.
    let rvalue = try emit(expr: assignment.rhs.adaptAsExpr()!)
    builder.buildStore(emit(copy: rvalue), to: lvalue)
    return constObject(kind: .junk)
  }

  /// Emits a function application.
  func emit(expr: ApplyExpr) throws -> IRValue {
    var global: Function?
    var object: IRValue?

    // Handle direct calls to statically known functions.
    if let ref = expr.callee.adapt(as: DeclRefExpr.self) {
      let name = String(ref.name)

      // Handle direct calls to `print`.
      if name == "print" {
        // There should exactly one argument.
        guard expr.args.count == 1 else {
          throw EmitterError(
            message: "invalid argument count: expected 1, got \(expr.args.count)",
            range: ref.handle.range)
        }

        // Emit the call.
        let arg = try emit(expr: expr.args[0].adaptAsExpr()!)
        let _0 = builder.buildExtractValue(arg, index: 0)
        let _1 = builder.buildExtractValue(arg, index: 1)
        _ = builder.buildCall(printFunction, args: [_0, _1])
        return constObject(kind: .junk)
      }

      // Search within the locals.
      if let loc = functionContexts.last?.value(boundTo: name) {
        object = builder.buildLoad(loc, type: any)
      }

      // Search for a global function.
      global = module.function(named: name)

      if (object == nil) && (global == nil) {
        guard let g = module.global(named: name) else {
          throw EmitterError(message: "unbound identifier '\(name)'", range: expr.callee.range)
        }
        object = builder.buildLoad(g, type: any)
      }
    } else {
      object = try emit(expr: expr.callee.adaptAsExpr()!)
    }

    // Emit the arguments.
    var args: [IRValue] = []
    for subexpr in expr.args {
      let arg = addEntryAlloca(type: any)
      args.append(arg)

      let val = try emit(expr: subexpr.adaptAsExpr()!)
      builder.buildStore(emit(copy: val), to: arg)
    }

    // If we found a function object, apply it.
    let result: Call
    if let callee = object {
      // Extract the function pointer.
      emit(assert: callee, isA: .function)
      let fun = emit(
        extractFunFrom: callee,
        type: userFunType(paramCount: args.count))
      let env = emit(extractEnvFrom: callee)

      // Emit a call to a location function.
      result = builder.buildCall(fun, args: args + [env])
    } else if let fun = global {
      // Emit a call to a global function..
      result = builder.buildCall(fun, args: args + [PointerType(pointee: any).null()])
    } else {
      unreachable()
    }

    // Drop the arguments.
    for arg in args {
      emit(dropPointer: arg)
    }

    return result
  }

  /// Emits a parenthesized expression.
  func emit(expr: ParenExpr) throws -> IRValue {
    return try emit(expr: expr.subexpr.adaptAsExpr()!)
  }

  /// Emits a brace statement.
  func emit(stmt: BraceStmt) throws -> EmitterAction {
    functionContexts[functionContexts.count - 1].pushScope(stmt: stmt)
    defer { functionContexts[functionContexts.count - 1].popScope() }

    // Emit all statements in the scope.
    for hndl in stmt.stmts {
      let action = try emit(any: hndl.adaptAsAny())
      guard action == .proceed else {
        return action
      }
    }

    // Drop the local scope.
    let scope = functionContexts.last!.scopes.last!
    emit(drop: scope)

    return .proceed
  }

  /// Emits an expression statement.
  func emit(stmt: ExprStmt) throws -> EmitterAction {
    let value = try emit(expr: stmt.expr.adaptAsExpr()!)
    emit(drop: value)
    return .proceed
  }

  /// Emits a conditional (a.k.a. `if`) statement.
  func emit(stmt: IfStmt) throws -> EmitterAction {
    let fun = builder.currentFunction!

    // Emit the condition.
    let cond = try emit(expr: stmt.cond.adaptAsExpr()!)
    emit(assert: cond, isA: .bool)
    var condValue = builder.buildExtractValue(cond, index: 1)
    condValue = builder.buildICmp(condValue, i64.zero(), .notEqual)

    // Emit the branch.
    let thenBB = fun.appendBasicBlock(named: "then")
    let elseBB = fun.appendBasicBlock(named: "else")
    builder.buildCondBr(condition: condValue, then: thenBB, else: elseBB)
    builder.positionAtEnd(of: thenBB)

    let thenAction = try emit(stmt: stmt.then_.adapt(as: BraceStmt.self)!)

    builder.positionAtEnd(of: elseBB)
    let elseAction: EmitterAction
    if let else_ = stmt.else_?.adapt(as: BraceStmt.self) {
      elseAction = try emit(stmt: else_)
    } else if let else_ = stmt.else_?.adapt(as: IfStmt.self) {
      elseAction = try emit(stmt: else_)
    } else {
      assert(stmt.else_ == nil)
      elseAction = .proceed
    }

    switch (thenAction, elseAction) {
    case (.proceed, .proceed):
      // If both branches need to proceed, we must create a block that "joins" the control flow.
      let joinBB = fun.appendBasicBlock(named: "join")
      builder.buildBr(joinBB)
      builder.positionAtEnd(of: thenBB)
      builder.buildBr(joinBB)
      builder.positionAtEnd(of: joinBB)
      return .proceed

    case (.unwind(let a), .unwind(let b)):
      if (a == b) {
        // If both branches unwind to the same location, we can unwind there as well.
        return .unwind(dest: a)
      } else {
        // Otherwise, since Cocodol has no loop labels, one must be the current function and we can
        // simply unwind to it.
        let decl = functionContexts.last!.decl
        return .unwind(dest: decl!.handle)
      }

    default:
      // If one branch unwinds but the other doesn't, we must proceed at the end of the former.
      if thenAction == .proceed {
        builder.positionAtEnd(of: thenBB)
      } else {
        builder.positionAtEnd(of: elseBB)
      }
      return .proceed
    }
  }

  /// Emits a loop (a.k.a. `while`) statement.
  func emit(stmt: WhileStmt) throws -> EmitterAction {
    let fun = builder.currentFunction!

    // Emit the condition.
    let headBB = fun.appendBasicBlock(named: "head")
    builder.buildBr(headBB)
    builder.positionAtEnd(of: headBB)

    let cond = try emit(expr: stmt.cond.adaptAsExpr()!)
    emit(assert: cond, isA: .bool)
    var condValue = builder.buildExtractValue(cond, index: 1)
    condValue = builder.buildICmp(condValue, i64.zero(), .notEqual)

    // Emit the loop's body.
    let bodyBB = fun.appendBasicBlock(named: "body")
    let tailBB = fun.appendBasicBlock(named: "tail")
    loops.append(LoopContext(loop: stmt, head: headBB, tail: tailBB))
    defer { loops.removeLast() }

    builder.buildCondBr(condition: condValue, then: bodyBB, else: tailBB)
    builder.positionAtEnd(of: bodyBB)
    switch try emit(stmt: stmt.body.adapt(as: BraceStmt.self)!) {
    case .proceed:
      // Emit a terminator instruction that jumps to the loop's head before proceeding.
      builder.buildBr(headBB)

    case .unwind:
      // A terminator instruction has already been emitted for the loop body; we can proceed.
      break
    }

    // Position the builder at the tail of the loop.
    builder.positionAtEnd(of: tailBB)
    return .proceed
  }

  /// Emits a `brk` statement.
  func emit(stmt: BrkStmt) throws -> EmitterAction {
    guard let ctx = loops.last else {
      throw EmitterError(message: "'brk' outside of a loop", range: stmt.handle.range)
    }

    // Drop all local scopes inside the loop.
    for scope in functionContexts.last!.scopes.reversed() {
      emit(drop: scope)
      guard scope.node != ctx.loop.body else { break }
    }

    // Jump to the loop's tail.
    builder.buildBr(ctx.tail)
    return .unwind(dest: ctx.loop.handle)
  }

  /// Emits a `nxt` statement.
  func emit(stmt: NxtStmt) throws -> EmitterAction {
    guard let ctx = loops.last else {
      throw EmitterError(message: "'nxt' outside of a loop", range: stmt.handle.range)
    }

    // Drop all local scopes inside the loop.
    for scope in functionContexts.last!.scopes.reversed() {
      emit(drop: scope)
      guard scope.node != ctx.loop.body else { break }
    }

    // Jump to the loop's head.
    builder.buildBr(ctx.head)
    return .unwind(dest: ctx.loop.handle)
  }

  /// Emits a return statement.
  func emit(stmt: RetStmt) throws -> EmitterAction {
    guard let ctx = functionContexts.last,
          let decl = ctx.decl
    else {
      throw EmitterError(message: "'ret' outside of a function", range: stmt.handle.range)
    }

    // Emit the return value and copy it.
    let rvalue = emit(copy: try emit(expr: stmt.value.adaptAsExpr()!))

    // Drop all local scopes.
    for scope in ctx.scopes.reversed() {
      // Don't drop the argument scope.
      guard scope.node?.adapt(as: FunDecl.self) == nil else { break }
      emit(drop: scope)
    }

    // Emit the return statement.
    builder.buildRet(rvalue)
    return .unwind(dest: decl.handle)
  }

  /// Emits a copy of the given value.
  func emit(copy value: IRValue) -> IRValue {
    let _0 = builder.buildExtractValue(value, index: 0)
    let _1 = builder.buildExtractValue(value, index: 1)
    return builder.buildCall(copyFunction, args: [_0, _1])
  }

  /// Emits the destruction of the given value.
  /// Emits a piece of code that drops (i.e., deinitializes and deallocate) the give value.
  func emit(drop value: IRValue) {
    let _0 = builder.buildExtractValue(value, index: 0)
    let _1 = builder.buildExtractValue(value, index: 1)
    _ = builder.buildCall(dropFunction, args: [_0, _1])
  }

  /// Emits the destruction of value pointed by the given pointer.
  func emit(dropPointer pointer: IRValue) {
    let _0 = builder.buildLoad(
      builder.buildStructGEP(pointer, type: any, index: 0), type: i64)
    let _1 = builder.buildLoad(
      builder.buildStructGEP(pointer, type: any, index: 1), type: i64)
    _ = builder.buildCall(dropFunction, args: [_0, _1])
  }

  /// Emits a piece of code that drops (i.e., deinitializes and deallocate) the values bound to the
  /// given scope.
  func emit(drop scope: FunContext.Scope) {
    scope.bindings.values.forEach(emit(dropPointer:))
  }

  /// Emits a piece of code that checks whether the given tag denotes the specified type.
  func emit(_ tag: IRValue, is kind: ObjectKind) -> IRValue {
    if kind == .function {
      // Function types are encoded in the low bits.
      let tmp = builder.buildAnd(tag, 0b11)
      return builder.buildICmp(tmp, kind.rawValue, .equal)
    } else {
      // Other kinds can be checked with simple equality.
      return builder.buildICmp(tag, i64.constant(kind.rawValue), .equal)
    }
  }

  /// Emits a piece of code that checks whether the given value is of the specified type.
  func emit(_ value: IRValue, isA kind: ObjectKind) -> IRValue {
    let tag = builder.buildExtractValue(value, index: 0)
    return emit(tag, is: kind)
  }

  /// Emits a piece of code that checks whether the value pointed by the given pointer is of the
  /// specified type.
  func emit(_ pointer: IRValue, isPointerTo kind: ObjectKind) -> IRValue {
    let tag = builder.buildStructGEP(pointer, type: any, index: 0)
    return emit(builder.buildLoad(tag, type: i64), is: kind)
  }

  /// Emits a piece of code asserting that the given value is of the specified type.
  func emit(assert value: IRValue, isA kind: ObjectKind) {
    let fun = builder.currentFunction!

    let cond = emit(value, isA: kind)
    let fail = fun.appendBasicBlock(named: "fail")
    let next = fun.appendBasicBlock(named: "next")

    builder.buildCondBr(condition: cond, then: next, else: fail)
    builder.positionAtEnd(of: fail)
    _ = builder.buildCall(module.intrinsic(Intrinsic.ID.llvm_trap)!, args: [])
    builder.buildUnreachable()
    builder.positionAtEnd(of: next)
  }

  /// Emits the extraction of a function pointer from the lower part of an object.
  func emit(extractFunFrom object: IRValue, type: FunctionType) -> IRValue {
    var _0 = builder.buildExtractValue(object, index: 0)
    _0 = builder.buildAnd(_0, i64.constant(~ObjectKind.function.rawValue))
    return builder.buildIntToPtr(_0, type: PointerType(pointee: type))
  }

  /// Emits the extraction of a function's environment from the upper part of an object.
  func emit(extractEnvFrom object: IRValue) -> IRValue {
    let _1 = builder.buildExtractValue(object, index: 1)
    return builder.buildIntToPtr(_1, type: PointerType(pointee: any))
  }

  /// Emits the extraction of a function's environment from the upper part of the object pointed by
  /// the given value.
  func emit(extractEnvFromPointer pointer: IRValue) -> IRValue {
    let _1 = builder.buildStructGEP(pointer, type: any, index: 1)
    return builder.buildIntToPtr(builder.buildLoad(_1, type: i64), type: PointerType(pointee: any))
  }

  /// Emits the size of the given type, as an `i64`.
  ///
  /// The size is computed by subtracting the address of the element at the 1st position from one
  /// at the 2nd position of an array of the given type. Constant folding should optimize this
  /// computation to a single constant.
  ///
  /// See: https://stackoverflow.com/questions/29979023
  func emit(sizeOf type: IRType) -> IRValue {
    let ptr = PointerType(pointee: type)
    let _1 = builder.buildGEP(ptr.null(), type: type, indices: [i32.constant(1)])
    let _0 = builder.buildGEP(ptr.null(), type: type, indices: [i32.constant(0)])
    return builder.buildSub(
      builder.buildPtrToInt(_1, type: i64),
      builder.buildPtrToInt(_0, type: i64))
  }

  /// Emits the LLVM IR of the given program.
  public static func emit(program decls: [Decl]) throws -> Module {
    let module  = Module(name: "main")
    let builder = IRBuilder(module: module)
    let emitter = Emitter(builder: builder)

    try emitter.emit(program: decls)
    try module.verify()
    return module
  }

}

/// A data structure that contains information about the function being emitted.
struct FunContext {

  /// The function declaration.
  let decl: FunDecl?

  /// A collection with the bindings of each lexical scope traversed by the code generator.
  var scopes: [Scope]

  init(decl: FunDecl?) {
    self.decl   = decl
    self.scopes = [Scope(node: decl?.handle)]
  }

  func value(boundTo name: String) -> IRValue? {
    for scope in scopes.reversed() {
      if let value = scope.bindings[name] {
        return value
      }
    }
    return nil
  }

  mutating func bind(value: IRValue, to name: String) {
    scopes[scopes.count - 1].bindings[name] = value
  }

  mutating func pushScope(stmt: BraceStmt) {
    scopes.append(Scope(node: stmt.handle))
  }

  mutating func popScope() {
    scopes.removeLast()
    assert(!scopes.isEmpty)
  }

  struct Scope {

    /// The node that delimits the scope.
    let node: NodeHandle?

    /// The local bindings of the scope.
    var bindings: [String: IRValue] = [:]

  }

}

/// A data structure that contains information about the loop being emitted.
struct LoopContext {

  /// The loop statement.
  let loop: WhileStmt

  /// The head block of the loop, before its condition is evaluated.
  let head: BasicBlock

  /// The tail block of the loop, after its body.
  let tail: BasicBlock

}

/// The action the emitter should take after processing a node.
enum EmitterAction: Equatable {

  /// Continue with the next node.
  case proceed

  /// Climb back the tree without emitting any additional instruction until reaching the specified
  /// loop statement or function declaration.
  case unwind(dest: NodeHandle)

}

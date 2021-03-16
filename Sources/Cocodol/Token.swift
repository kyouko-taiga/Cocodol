import CCocodol

/// A token.
public struct Token {

  /// The kind of a token.
  public enum Kind {

    case name
    case var_
    case fun
    case ret
    case obj
    case if_
    case else_
    case while_
    case brk
    case nxt
    case true_
    case false_
    case integer
    case float
    case plus
    case minus
    case pipe
    case amp
    case caret
    case lShift
    case rShift
    case star
    case slash
    case percent
    case tilde
    case not
    case and
    case or
    case lt
    case le
    case gt
    case ge
    case eq
    case ne
    case assign
    case dot
    case colon
    case comma
    case semicolon
    case lParen
    case rParen
    case lBrace
    case rBrace
    case eof
    case error

  }

  /// The token's kind.
  public let kind: Kind

  /// The token's value.
  public let value: CharacterView

  init(cToken: CCocodol.Token, buffer: ManagedStringBuffer) {
    self.kind = Kind(cToken.kind)
    self.value = CharacterView(buffer: buffer, startIndex: cToken.start, endIndex: cToken.end)
  }

}

extension Token.Kind {

  init(_ kind: CCocodol.TokenKind) {
    switch kind {
    case tk_name      : self = .name
    case tk_var       : self = .var_
    case tk_fun       : self = .fun
    case tk_obj       : self = .obj
    case tk_ret       : self = .ret
    case tk_if        : self = .if_
    case tk_else      : self = .else_
    case tk_while     : self = .while_
    case tk_brk       : self = .brk
    case tk_nxt       : self = .nxt
    case tk_true      : self = .true_
    case tk_false     : self = .false_
    case tk_integer   : self = .integer
    case tk_float     : self = .float
    case tk_plus      : self = .plus
    case tk_minus     : self = .minus
    case tk_pipe      : self = .pipe
    case tk_amp       : self = .amp
    case tk_caret     : self = .caret
    case tk_l_shift   : self = .lShift
    case tk_r_shift   : self = .rShift
    case tk_star      : self = .star
    case tk_slash     : self = .slash
    case tk_percent   : self = .percent
    case tk_tilde     : self = .tilde
    case tk_not       : self = .not
    case tk_and       : self = .and
    case tk_or        : self = .or
    case tk_lt        : self = .lt
    case tk_le        : self = .le
    case tk_gt        : self = .gt
    case tk_ge        : self = .ge
    case tk_eq        : self = .eq
    case tk_ne        : self = .ne
    case tk_assign    : self = .assign
    case tk_dot       : self = .dot
    case tk_colon     : self = .colon
    case tk_semicolon : self = .semicolon
    case tk_comma     : self = .comma
    case tk_l_paren   : self = .lParen
    case tk_r_paren   : self = .rParen
    case tk_l_brace   : self = .lBrace
    case tk_r_brace   : self = .rBrace
    case tk_eof       : self = .eof
    case tk_error     : self = .error
    default: unreachable()
    }
  }

}

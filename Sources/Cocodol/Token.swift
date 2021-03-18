import CCocodol

/// A token.
public struct Token {

  /// The kind of a token.
  public struct Kind: RawRepresentable, Equatable {

    public var rawValue: UInt32

    public init?(rawValue: UInt32) {
      self.rawValue = rawValue
    }

    public static var error    : Kind { Kind(rawValue: tk_error.rawValue)!     }
    public static var name     : Kind { Kind(rawValue: tk_name.rawValue)!      }
    public static var true_    : Kind { Kind(rawValue: tk_true.rawValue)!      }
    public static var false_   : Kind { Kind(rawValue: tk_false.rawValue)!     }
    public static var integer  : Kind { Kind(rawValue: tk_integer.rawValue)!   }
    public static var float    : Kind { Kind(rawValue: tk_float.rawValue)!     }
    public static var dot      : Kind { Kind(rawValue: tk_dot.rawValue)!       }
    public static var colon    : Kind { Kind(rawValue: tk_colon.rawValue)!     }
    public static var semicolon: Kind { Kind(rawValue: tk_semicolon.rawValue)! }
    public static var comma    : Kind { Kind(rawValue: tk_comma.rawValue)!     }
    public static var lParen   : Kind { Kind(rawValue: tk_l_paren.rawValue)!   }
    public static var rParen   : Kind { Kind(rawValue: tk_r_paren.rawValue)!   }
    public static var lBrace   : Kind { Kind(rawValue: tk_l_brace.rawValue)!   }
    public static var rBrace   : Kind { Kind(rawValue: tk_r_brace.rawValue)!   }
    public static var eof      : Kind { Kind(rawValue: tk_eof.rawValue)!       }
    public static var var_     : Kind { Kind(rawValue: tk_var.rawValue)!       }
    public static var fun      : Kind { Kind(rawValue: tk_fun.rawValue)!       }
    public static var obj      : Kind { Kind(rawValue: tk_obj.rawValue)!       }
    public static var if_      : Kind { Kind(rawValue: tk_if.rawValue)!        }
    public static var else_    : Kind { Kind(rawValue: tk_else.rawValue)!      }
    public static var while_   : Kind { Kind(rawValue: tk_while.rawValue)!     }
    public static var brk      : Kind { Kind(rawValue: tk_brk.rawValue)!       }
    public static var nxt      : Kind { Kind(rawValue: tk_nxt.rawValue)!       }
    public static var ret      : Kind { Kind(rawValue: tk_ret.rawValue)!       }
    public static var lShift   : Kind { Kind(rawValue: tk_l_shift.rawValue)!   }
    public static var rShift   : Kind { Kind(rawValue: tk_r_shift.rawValue)!   }
    public static var star     : Kind { Kind(rawValue: tk_star.rawValue)!      }
    public static var slash    : Kind { Kind(rawValue: tk_slash.rawValue)!     }
    public static var percent  : Kind { Kind(rawValue: tk_percent.rawValue)!   }
    public static var plus     : Kind { Kind(rawValue: tk_plus.rawValue)!      }
    public static var minus    : Kind { Kind(rawValue: tk_minus.rawValue)!     }
    public static var pipe     : Kind { Kind(rawValue: tk_pipe.rawValue)!      }
    public static var amp      : Kind { Kind(rawValue: tk_amp.rawValue)!       }
    public static var caret    : Kind { Kind(rawValue: tk_caret.rawValue)!     }
    public static var lt       : Kind { Kind(rawValue: tk_lt.rawValue)!        }
    public static var le       : Kind { Kind(rawValue: tk_le.rawValue)!        }
    public static var gt       : Kind { Kind(rawValue: tk_gt.rawValue)!        }
    public static var ge       : Kind { Kind(rawValue: tk_ge.rawValue)!        }
    public static var eq       : Kind { Kind(rawValue: tk_eq.rawValue)!        }
    public static var ne       : Kind { Kind(rawValue: tk_ne.rawValue)!        }
    public static var and      : Kind { Kind(rawValue: tk_and.rawValue)!       }
    public static var or       : Kind { Kind(rawValue: tk_or.rawValue)!        }
    public static var assign   : Kind { Kind(rawValue: tk_assign.rawValue)!    }
    public static var not      : Kind { Kind(rawValue: tk_not.rawValue)!       }
    public static var tilde    : Kind { Kind(rawValue: tk_tilde.rawValue)!     }

  }

  /// The token's kind      .
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
    case tk_error     : self = .error
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
    default: unreachable()
    }
  }

}

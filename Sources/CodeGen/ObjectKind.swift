/// The value of an object's kind at runtime.
enum ObjectKind: Int64 {

  case junk     = 0b00000
  case function = 0b00001
  // case record   = 0b00010
  // case print    = 0b00111
  case bool     = 0b01011
  case integer  = 0b01111
  case float    = 0b10011

}

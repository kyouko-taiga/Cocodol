/// A managed C string.
final class ManagedStringBuffer {

  var data: UnsafeMutablePointer<CChar>?

  init(copying string: String) {
    guard !string.isEmpty else {
      data = nil
      return
    }

    data = string.utf8CString.withUnsafeBufferPointer({ cstr in
      let buf = UnsafeMutablePointer<CChar>.allocate(capacity: cstr.count)
      buf.assign(from: cstr.baseAddress!, count: cstr.count)
      return buf
    })
  }

  deinit {
    data?.deallocate()
  }

  subscript(position: Int) -> CChar {
    return data![position]
  }

}

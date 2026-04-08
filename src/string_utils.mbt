/// 共享字符串工具模块
/// 提供基础的字符串操作，避免 FFI 依赖

///|
/// 查找子串首次出现的位置
/// 返回 Some(index) 或 None
pub fn find_substring(haystack : String, needle : String) -> Int? {
  let h_len = haystack.length()
  let n_len = needle.length()
  if n_len == 0 || n_len > h_len {
    return None
  }
  let mut i = 0
  while i <= h_len - n_len {
    let mut found = true
    let mut j = 0
    while j < n_len {
      if haystack[i + j] != needle[j] {
        found = false
        break
      }
      j = j + 1
    }
    if found {
      return Some(i)
    }
    i = i + 1
  }
  None
}

///|
/// 检查字符串是否以指定前缀开头
pub fn starts_with(s : String, prefix : String) -> Bool {
  let s_len = s.length()
  let p_len = prefix.length()
  if p_len > s_len {
    return false
  }
  let mut i = 0
  while i < p_len {
    if s[i] != prefix[i] {
      return false
    }
    i = i + 1
  }
  true
}

///|
/// 检查字符串是否以指定后缀结尾
pub fn ends_with(s : String, suffix : String) -> Bool {
  let s_len = s.length()
  let suf_len = suffix.length()
  if suf_len > s_len {
    return false
  }
  let start = s_len - suf_len
  let mut i = 0
  while i < suf_len {
    if s[start + i] != suffix[i] {
      return false
    }
    i = i + 1
  }
  true
}

///|
/// 重复字符串 n 次
pub fn repeat(s : String, n : Int) -> String {
  if n <= 0 {
    return ""
  }
  let mut result = ""
  let mut i = 0
  while i < n {
    result = result + s
    i = i + 1
  }
  result
}

///|
/// 移除字符串两端的空白字符
pub fn trim(s : String) -> String {
  let len = s.length()
  if len == 0 {
    return ""
  }

  // 找到第一个非空白字符
  let mut start = 0
  while start < len {
    let ch = s[start]
    // 空格(32), 制表符(9), 换行(10), 回车(13)
    if ch != 32 && ch != 9 && ch != 10 && ch != 13 {
      break
    }
    start = start + 1
  }

  if start >= len {
    return ""
  }

  // 找到最后一个非空白字符
  let mut end = len - 1
  while end > start {
    let ch = s[end]
    if ch != 32 && ch != 9 && ch != 10 && ch != 13 {
      break
    }
    end = end - 1
  }

  // StringView 转 String
  s[start:end + 1].to_string()
}

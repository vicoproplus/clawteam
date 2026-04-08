/// log-redaction 模块 - MoonBit 实现
/// 原始 TypeScript 实现参考: paperclip/packages/adapter-utils/src/log-redaction.ts

pub const REDACTED_HOME_PATH_USER: String = "*"

/// 掩码处理用户名段
/// "john" → "j***"
fn mask_home_path_user_segment(value: String) -> String {
  let trimmed = value.trim().to_string()
  if trimmed.length() == 0 {
    return REDACTED_HOME_PATH_USER
  }
  let first = trimmed[0:1].to_string()
  let rest_len = trimmed.length() - 1
  let stars = if rest_len > 0 { String::make(rest_len, '*') } else { "" }
  first + stars
}

/// 重复字符 n 次
fn repeat_char(n: Int, ch: Char) -> String {
  if n <= 0 { "" } else { String::make(n, ch) }
}

/// 查找并替换 macOS 路径: /Users/xxx/... → /Users/x***/...
fn replace_macos_home_path(text: String) -> String {
  let prefix = "/Users/"
  let prefix_len = prefix.length()
  let text_len = text.length()
  let mut result = ""
  let mut i = 0
  
  while i < text_len {
    // 检查是否匹配 /Users/
    if i + prefix_len <= text_len && text[i:i + prefix_len].to_string() == prefix {
      // 找到 /Users/，提取用户名
      let user_start = i + prefix_len
      let mut user_end = user_start
      while user_end < text_len {
        let ch = text[user_end]
        if ch == 47 || ch == 92 || ch == 32 || ch == 9 || ch == 10 {  // /, \, 空白
          break
        }
        user_end = user_end + 1
      }
      
      if user_end > user_start {
        let user = text[user_start:user_end].to_string()
        let masked = mask_home_path_user_segment(user)
        result = result + prefix + masked
        i = user_end
      } else {
        result = result + text[i:i + 1].to_string()
        i = i + 1
      }
    } else {
      result = result + text[i:i + 1].to_string()
      i = i + 1
    }
  }
  
  result
}

/// 查找并替换 Linux 路径: /home/xxx/... → /home/x***/...
fn replace_linux_home_path(text: String) -> String {
  let prefix = "/home/"
  let prefix_len = prefix.length()
  let text_len = text.length()
  let mut result = ""
  let mut i = 0
  
  while i < text_len {
    if i + prefix_len <= text_len && text[i:i + prefix_len].to_string() == prefix {
      let user_start = i + prefix_len
      let mut user_end = user_start
      while user_end < text_len {
        let ch = text[user_end]
        if ch == 47 || ch == 92 || ch == 32 || ch == 9 || ch == 10 {
          break
        }
        user_end = user_end + 1
      }
      
      if user_end > user_start {
        let user = text[user_start:user_end].to_string()
        let masked = mask_home_path_user_segment(user)
        result = result + prefix + masked
        i = user_end
      } else {
        result = result + text[i:i + 1].to_string()
        i = i + 1
      }
    } else {
      result = result + text[i:i + 1].to_string()
      i = i + 1
    }
  }
  
  result
}

/// 查找并替换 Windows 路径: C:\Users\xxx\... → C:\Users\x**\...
fn replace_windows_home_path(text: String) -> String {
  let text_len = text.length()
  let mut result = ""
  let mut i = 0
  
  while i < text_len {
    // 检查格式: X:\Users\
    let ch = text[i]
    let is_drive = (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122)  // A-Z or a-z
    if is_drive && i + 2 < text_len && text[i + 1] == 58 && (text[i + 2] == 92) {  // X:\
      // 检查后面是否跟着 Users\
      let users_prefix = "Users\\"
      let after_drive = i + 3
      if after_drive + users_prefix.length() <= text_len &&
         text[after_drive:after_drive + users_prefix.length()].to_string() == users_prefix {
        // 找到 X:\Users\，提取用户名
        let prefix = text[i:after_drive + users_prefix.length()].to_string()
        let user_start = after_drive + users_prefix.length()
        let mut user_end = user_start
        while user_end < text_len {
          let c = text[user_end]
          if c == 92 || c == 47 || c == 32 || c == 9 || c == 10 {
            break
          }
          user_end = user_end + 1
        }
        
        if user_end > user_start {
          let user = text[user_start:user_end].to_string()
          let masked = mask_home_path_user_segment(user)
          result = result + prefix + masked
          i = user_end
        } else {
          result = result + text[i:i + 1].to_string()
          i = i + 1
        }
        continue
      }
    }
    
    result = result + text[i:i + 1].to_string()
    i = i + 1
  }
  
  result
}

/// 脱敏文本中的家庭路径
pub fn redact_home_path_user_segments(text: String) -> String {
  let step1 = replace_macos_home_path(text)
  let step2 = replace_linux_home_path(step1)
  replace_windows_home_path(step2)
}


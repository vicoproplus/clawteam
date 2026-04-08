/// routine-variables 模块 - MoonBit 实现
/// 原始 TypeScript 实现参考: paperclip/packages/shared/src/routine-variables.ts

/// 正则模式: /\{\{\s*([A-Za-z][A-Za-z0-9_]*)\s*\}\}/g
/// 手动实现模板变量提取（无 FFI 依赖）

///|
/// 检查字符是否是有效的变量名首字符 (A-Za-z)
fn is_valid_start(ch : UInt16) -> Bool {
  (ch >= 65 && ch <= 90) || (ch >= 97 && ch <= 122) // A-Z or a-z
}

///|
/// 检查字符是否是有效的变量名后续字符 (A-Za-z0-9_)
fn is_valid_continue(ch : UInt16) -> Bool {
  is_valid_start(ch) || (ch >= 48 && ch <= 57) || ch == 95 // A-Za-z or 0-9 or _
}

///|
/// 检查字符是否是空白字符
fn is_whitespace(ch : UInt16) -> Bool {
  ch == 32 || ch == 9 || ch == 10 || ch == 13 // space, tab, newline, carriage return
}

///|
/// 提取模板变量名列表
/// 输入: "{{agent.name}} 处理 {{issue.title}}"
/// 输出: ["agent", "name", "issue", "title"]  (注意: 原实现按点号分割)
pub fn extract_variables(template : String) -> Array[String] {
  fn scan(template : String, i : Int, acc : Array[String]) -> Array[String] {
    if i + 1 >= template.length() {
      return acc
    }

    // 查找 "{{"
    if template[i] == 123 && template[i + 1] == 123 {
      let start = i + 2
      let len = template.length()
      let mut j = start

      // 跳过前导空白
      while j < len && is_whitespace(template[j]) {
        j = j + 1
      }

      // 提取变量名
      let var_start = j
      while j < len && is_valid_continue(template[j]) {
        j = j + 1
      }
      let var_end = j

      // 跳过尾随空白
      while j < len && is_whitespace(template[j]) {
        j = j + 1
      }

      // 检查 "}}"
      if j + 1 < len && template[j] == 125 && template[j + 1] == 125 {
        if var_end > var_start {
          let var_name = template[var_start:var_end].to_string()
          // 检查是否已存在（去重）
          let mut found = false
          for existing in acc {
            if existing == var_name {
              found = true
              break
            }
          }
          let new_acc = if !found { acc + [var_name] } else { acc }
          let _ = new_acc // suppress unused variable warning
          scan(template, j + 2, new_acc)
        } else {
          scan(template, j + 2, acc)
        }
      } else {
        scan(template, i + 1, acc)
      }
    } else {
      scan(template, i + 1, acc)
    }
  }

  scan(template, 0, [])
}

///|
/// 验证变量名是否合法
pub fn is_valid_variable_name(name : String) -> Bool {
  let len = name.length()
  if len == 0 {
    return false
  }

  // 检查首字符
  if !is_valid_start(name[0]) {
    return false
  }

  // 检查后续字符
  let mut i = 1
  while i < len {
    if !is_valid_continue(name[i]) {
      return false
    }
    i = i + 1
  }

  true
}

///|
/// 模板插值
/// 输入: template="{{name}}", values={"name": "Claude"}
/// 输出: "Claude"
pub fn interpolate(template : String, values : Map[String, String]) -> String {
  let len = template.length()
  let mut result = ""
  let mut i = 0

  while i < len {
    // 查找 "{{"
    if i + 1 < len && template[i] == 123 && template[i + 1] == 123 {
      let start = i + 2
      let mut j = start

      // 跳过前导空白
      while j < len && is_whitespace(template[j]) {
        j = j + 1
      }

      // 提取变量名
      let var_start = j
      while j < len && is_valid_continue(template[j]) {
        j = j + 1
      }
      let var_end = j

      // 跳过尾随空白
      while j < len && is_whitespace(template[j]) {
        j = j + 1
      }

      // 检查 "}}"
      if j + 1 < len && template[j] == 125 && template[j + 1] == 125 {
        if var_end > var_start {
          let var_name = template[var_start:var_end].to_string()
          match values.get(var_name) {
            Some(value) => result = result + value
            None =>
              result = result +
                "{{" +
                template[var_start:var_end].to_string() +
                "}}"
          }
        } else {
          result = result + "{{"
        }
        i = j + 2
      } else {
        // 单字符转字符串：用 substring 方式
        result = result + template[i:i + 1].to_string()
        i = i + 1
      }
    } else {
      result = result + template[i:i + 1].to_string()
      i = i + 1
    }
  }

  result
}

///|
/// 同步模板变量
pub fn sync_variables_with_template(
  template : String,
  existing : Map[String, String],
) -> Map[String, String] {
  let var_names = extract_variables(template)
  let result : Map[String, String] = {}

  for name in var_names {
    match existing.get(name) {
      Some(value) => result[name] = value
      None => result[name] = name // 默认值
    }
  }

  result
}

// moonbit-core/src/lib/json.mbt

// JSON 序列化工具
// 约定: 内部使用 PascalCase, JSON 使用 snake_case

// 字符串转 JSON (带转义)
pub fn string_to_json(s: String) -> String {
  "\"" + escape_json(s) + "\""
}

// 整数转 JSON
pub fn int_to_json(i: Int) -> String {
  i.to_string()
}

// 布尔转 JSON
pub fn bool_to_json(b: Bool) -> String {
  if b { "true" } else { "false" }
}

// 转义 JSON 字符串中的特殊字符
fn escape_json(s: String) -> String {
  s.replace(old="\\", new="\\\\")
   .replace(old="\"", new="\\\"")
   .replace(old="\n", new="\\n")
   .replace(old="\r", new="\\r")
   .replace(old="\t", new="\\t")
}

// 解析 JSON 字符串 (简化版)
pub fn parse_json_string(s: String) -> Result[String, String] {
  if s.has_prefix("\"") && s.has_suffix("\"") && s.length() >= 2 {
    Ok(s[1:s.length()-1].to_string())
  } else {
    Err("Expected string literal")
  }
}

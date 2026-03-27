/// Parser 正则解析模块
///
/// 本模块提供 CLI 输出解析功能。

///|
/// 模式类型
pub enum PatternType {
  Success
  Error
  Progress
  Ready
  Working
  Unknown
} derive(ToJson, FromJson, Show, Eq)

///|
/// 解析结果
pub struct ParseResult {
  matched : Bool
  pattern_type : PatternType
} derive(ToJson, Show)

///|
/// 正则解析器
pub struct RegexParser {
  ready_patterns : Array[String]
  working_patterns : Array[String]
}

///|
pub fn RegexParser::new(ready_patterns : Array[String], working_patterns : Array[String]) -> RegexParser {
  RegexParser::{ ready_patterns, working_patterns }
}

///|
/// 解析输出行
pub fn RegexParser::parse_line(self : RegexParser, line : String) -> ParseResult {
  // 检查就绪模式
  for pattern in self.ready_patterns {
    if line.contains(pattern) {
      return ParseResult::{ matched: true, pattern_type: PatternType::Ready }
    }
  }
  
  // 检查工作中模式
  for pattern in self.working_patterns {
    if line.contains(pattern) {
      return ParseResult::{ matched: true, pattern_type: PatternType::Working }
    }
  }
  
  ParseResult::{ matched: false, pattern_type: PatternType::Unknown }
}

///|
/// 检测最终状态
pub fn RegexParser::detect_final_status(self : RegexParser, output : String) -> PatternType {
  let lines = output.split("\n").to_array()
  let results = lines.map(fn(line) { self.parse_line(line.to_string()) })
    .filter(fn(r) { r.matched })
  
  // 从后向前检查
  let mut i = results.length() - 1
  while i >= 0 {
    let result = results[i]
    match result.pattern_type {
      PatternType::Success => return PatternType::Success
      PatternType::Error => return PatternType::Error
      _ => i = i - 1
    }
  }
  
  PatternType::Unknown
}
/// Parser 模块白盒测试
///
/// 白盒测试可以访问包内部实现

test "RegexParser internal creation" {
  let parser = RegexParser::new([">", "╭─"], ["Thinking..."])
  inspect(parser.ready_patterns.length(), content="2")
  inspect(parser.working_patterns.length(), content="1")
}

test "PatternType Ready to_json" {
  let pt = PatternType::Ready
  let json = pt.to_json()
  inspect(json.stringify(), content="\"Ready\"")
}

test "PatternType Working to_json" {
  let pt = PatternType::Working
  let json = pt.to_json()
  inspect(json.stringify(), content="\"Working\"")
}

test "PatternType Unknown to_json" {
  let pt = PatternType::Unknown
  let json = pt.to_json()
  inspect(json.stringify(), content="\"Unknown\"")
}

test "ParseResult internal creation" {
  let result = ParseResult::{
    matched: true,
    pattern_type: PatternType::Ready,
  }
  inspect(result.matched, content="true")
}

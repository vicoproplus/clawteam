/// project_mentions 模块测试

///|
test "normalize_hex_color 6 digit with hash" {
  let result = normalize_hex_color("#aabbcc")
  @debug.assert_eq(result, Some("#aabbcc"))
}

///|
test "normalize_hex_color 6 digit uppercase" {
  let result = normalize_hex_color("#AABBCC")
  @debug.assert_eq(result, Some("#aabbcc"))
}

///|
test "normalize_hex_color 6 digit without hash" {
  let result = normalize_hex_color("aabbcc")
  @debug.assert_eq(result, Some("#aabbcc"))
}

///|
test "normalize_hex_color 3 digit short" {
  let result = normalize_hex_color("#abc")
  @debug.assert_eq(result, Some("#aabbcc"))
}

///|
test "normalize_hex_color 3 digit no hash" {
  let result = normalize_hex_color("abc")
  @debug.assert_eq(result, Some("#aabbcc"))
}

///|
test "normalize_hex_color invalid" {
  let result = normalize_hex_color("#gggggg")
  @debug.assert_eq(result, None)
}

///|
test "normalize_hex_color empty" {
  let result = normalize_hex_color("")
  @debug.assert_eq(result, None)
}

///|
test "build_project_mention_href no color" {
  let result = build_project_mention_href("proj-123", None)
  @debug.assert_eq(result, "project://proj-123")
}

///|
test "build_project_mention_href with color" {
  let result = build_project_mention_href("proj-123", Some("#aabbcc"))
  @debug.assert_eq(result, "project://proj-123?c=aabbcc")
}

///|
test "parse_project_mention_href valid" {
  let result = parse_project_mention_href("project://proj-123")
  assert_true(result.is_some())
  @debug.assert_eq(result.unwrap().project_id, "proj-123")
}

///|
test "parse_project_mention_href with color" {
  let result = parse_project_mention_href("project://proj-123?c=aabbcc")
  assert_true(result.is_some())
  let mention = result.unwrap()
  @debug.assert_eq(mention.project_id, "proj-123")
  @debug.assert_eq(mention.color, Some("#aabbcc"))
}

///|
test "parse_project_mention_href invalid scheme" {
  let result = parse_project_mention_href("agent://proj-123")
  @debug.assert_eq(result, None)
}

///|
test "normalize_agent_icon valid" {
  let result = normalize_agent_icon("my-icon")
  @debug.assert_eq(result, Some("my-icon"))
}

///|
test "normalize_agent_icon uppercase" {
  let result = normalize_agent_icon("My-Icon")
  @debug.assert_eq(result, Some("my-icon"))
}

///|
test "normalize_agent_icon invalid chars" {
  let result = normalize_agent_icon("my_icon!")
  @debug.assert_eq(result, None)
}

///|
test "normalize_skill_slug valid" {
  let result = normalize_skill_slug("my-skill")
  @debug.assert_eq(result, Some("my-skill"))
}

///|
test "normalize_skill_slug starts with number" {
  let result = normalize_skill_slug("123skill")
  @debug.assert_eq(result, Some("123skill"))
}

///|
test "normalize_skill_slug invalid start" {
  let result = normalize_skill_slug("-skill")
  @debug.assert_eq(result, None)
}

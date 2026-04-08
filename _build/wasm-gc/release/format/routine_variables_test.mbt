/// routine_variables 模块黑盒测试

///|
test "extract_variables single variable" {
  let result = extract_variables("{{name}}")
  let expected : Array[String] = ["name"]
  @debug.assert_eq(result, expected)
}

///|
test "extract_variables multiple variables" {
  let result = extract_variables("{{name}} and {{action}}")
  let expected : Array[String] = ["name", "action"]
  @debug.assert_eq(result, expected)
}

///|
test "extract_variables dedup" {
  let result = extract_variables("{{name}} and {{name}}")
  let expected : Array[String] = ["name"]
  @debug.assert_eq(result, expected)
}

///|
test "extract_variables empty" {
  let result = extract_variables("plain text")
  let expected : Array[String] = []
  @debug.assert_eq(result, expected)
}

///|
test "extract_variables with whitespace" {
  let result = extract_variables("{{  name  }}")
  let expected : Array[String] = ["name"]
  @debug.assert_eq(result, expected)
}

///|
test "is_valid_variable_name valid names" {
  assert_true(is_valid_variable_name("name"))
  assert_true(is_valid_variable_name("agent_name"))
  assert_true(is_valid_variable_name("name123"))
  assert_true(is_valid_variable_name("Name"))
  assert_true(!is_valid_variable_name("_private")) // 下划线开头不合法
}

///|
test "is_valid_variable_name invalid names" {
  assert_true(!is_valid_variable_name(""))
  assert_true(!is_valid_variable_name("123name"))
  assert_true(!is_valid_variable_name("name-with-dashes"))
  assert_true(!is_valid_variable_name("name with space"))
}

///|
test "interpolate single variable" {
  let values : Map[String, String] = { "name": "Claude" }
  let result = interpolate("Hello {{name}}!", values)
  @debug.assert_eq(result, "Hello Claude!")
}

///|
test "interpolate multiple variables" {
  let values : Map[String, String] = { "name": "Claude", "action": "code" }
  let result = interpolate("{{name}} will {{action}}", values)
  @debug.assert_eq(result, "Claude will code")
}

///|
test "interpolate missing variable keeps template" {
  let values : Map[String, String] = {}
  let result = interpolate("{{name}}", values)
  @debug.assert_eq(result, "{{name}}")
}

///|
test "interpolate no variables returns original" {
  let values : Map[String, String] = { "name": "Claude" }
  let result = interpolate("plain text", values)
  @debug.assert_eq(result, "plain text")
}

///|
test "interpolate with whitespace" {
  let values : Map[String, String] = { "name": "World" }
  let result = interpolate("{{  name  }}", values)
  @debug.assert_eq(result, "World")
}

///|
test "sync_variables_with_template" {
  let existing : Map[String, String] = { "name": "existing" }
  let result = sync_variables_with_template("{{name}} and {{new}}", existing)
  @debug.assert_eq(result["name"], "existing")
  @debug.assert_eq(result["new"], "new")
}

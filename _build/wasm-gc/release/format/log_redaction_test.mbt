/// log_redaction 模块测试

///|
test "redact macos home path" {
  let result = redact_home_path_user_segments(
    "Error at /Users/john/secret/key.pem",
  )
  @debug.assert_eq(result, "Error at /Users/j***/secret/key.pem")
}

///|
test "redact macos home path single char" {
  let result = redact_home_path_user_segments("/Users/a/file.txt")
  @debug.assert_eq(result, "/Users/a/file.txt")
}

///|
test "redact linux home path" {
  let result = redact_home_path_user_segments("/home/alice/.bashrc")
  @debug.assert_eq(result, "/home/a****/.bashrc")
}

///|
test "redact windows home path" {
  let result = redact_home_path_user_segments("C:\\Users\\Bob\\Documents")
  @debug.assert_eq(result, "C:\\Users\\B**\\Documents")
}

///|
test "redact multiple paths" {
  let result = redact_home_path_user_segments("/Users/john/a and /home/alice/b")
  @debug.assert_eq(result, "/Users/j***/a and /home/a****/b")
}

///|
test "no paths to redact" {
  let result = redact_home_path_user_segments("plain text")
  @debug.assert_eq(result, "plain text")
}

///|
test "redact path empty username segment" {
  // 路径 /Users//file (双斜杠) 不会匹配，因为用户名段为空
  let result = redact_home_path_user_segments("/Users//file")
  @debug.assert_eq(result, "/Users//file") // 不匹配，保持不变
}

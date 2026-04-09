// moonbit-core/src/server/main.mbt

// stdio JSON-RPC Server 入口点
// 从 stdin 逐行读取 JSON-RPC 请求，处理并写入 stdout

fn main {
  // 打印就绪信号
  println("MOONBIT_READY")
  
  // 主循环
  stdio_loop()
}

fn stdio_loop() -> Unit {
  // TODO: MoonBit 0.1 (2026-04) 的 stdio 读取 API 需要调研
  // 可能的 API:
  // - io::stdin()?.read_line()
  // - Console::read_line()
  // - @builtin.stdin()
  //
  // 当前 MoonBit 版本可能不支持完整的 stdin 读取
  // 以下是框架代码,待 API 可用后实现:
  //
  // loop {
  //   let line = io::read_line()?
  //   if line.length() == 0 {
  //     break
  //   }
  //   let req = parse_json_rpc_request(line)
  //   let resp = handle_request(req.method, req.params, req.id)
  //   println(resp)
  // }
  
  // 占位: 保持函数存在以便后续实现
  let _ = handle_request
}

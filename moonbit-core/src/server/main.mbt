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
  // 注意: MoonBit 0.1 的 stdio 读取 API 需要验证
  // 目前使用简化版本，实际部署时需要适配真实 API
  
  // 占位实现: 实际应使用 io::stdin().read_line() 或类似 API
  // 由于 MoonBit stdio API 尚未完全验证，这里提供框架代码
  
  let _ = () // 占位
}

// 简单的 JSON 解析辅助函数
// 实际应使用 MoonBit JSON 库

fn extract_method(json: String) -> String {
  // 简化: 从 JSON 中提取 method 字段
  // 实际应使用 JSON 解析器
  "unknown"
}

fn extract_id(json: String) -> Int {
  // 简化: 从 JSON 中提取 id 字段
  0
}

fn extract_params(json: String) -> String {
  // 简化: 从 JSON 中提取 params 字段
  "{}"
}

// moonbit-core/src/lib/rpc.mbt

// JSON-RPC 2.0 over stdio
// Request:  {"jsonrpc":"2.0","method":"<method>","params":{...},"id":<number>}
// Response: {"jsonrpc":"2.0","result":{...},"id":<number>}
// Error:    {"jsonrpc":"2.0","error":{"code":-32600,"message":"..."},"id":<number>}

pub struct JsonRpcRequest {
  method_name: String  // renamed from 'method' (reserved keyword)
  params: String  // 原始 JSON 字符串,由 handler 自行解析
  id: Int
}

pub struct JsonRpcResponse {
  result: Option[String]   // Some(JSON字符串) 或 None
  error_msg: Option[String]    // Some(错误消息) 或 None (renamed from 'error')
  id: Int
}

// 格式化 JSON-RPC 响应
pub fn format_response(resp: JsonRpcResponse) -> String {
  match (resp.result, resp.error_msg) {
    (Some(result), None) => {
      "{\"jsonrpc\":\"2.0\",\"result\":" + result + ",\"id\":" + resp.id.to_string() + "}"
    }
    (None, Some(msg)) => {
      "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"" + msg + "\"},\"id\":" + resp.id.to_string() + "}"
    }
    _ => {
      "{\"jsonrpc\":\"2.0\",\"error\":{\"code\":-32603,\"message\":\"Internal error\"},\"id\":" + resp.id.to_string() + "}"
    }
  }
}

// 格式化成功响应
pub fn format_success(result_json: String, id: Int) -> String {
  format_response(JsonRpcResponse::{ result: Some(result_json), error_msg: None, id })
}

// 格式化错误响应
pub fn format_error(message: String, id: Int) -> String {
  format_response(JsonRpcResponse::{ result: None, error_msg: Some(message), id })
}

// stdio 主循环 (占位 - 需要后续实现)
// 注意: MoonBit 的 stdio API 需要调研
pub fn run_stdio_loop(handler: (JsonRpcRequest) -> JsonRpcResponse) -> Unit {
  // TODO: 实现 stdio 循环
  // 1. 逐行读取 stdin
  // 2. 解析 JSON 请求
  // 3. 调用 handler
  // 4. 打印响应到 stdout
  // 暂时留空
  let _ = handler
}

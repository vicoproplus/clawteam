/// ClawTeam 主入口
/// 
/// ClawTeam 是一个基于 MoonBit 构建的纯调度层工具，
/// 核心价值在于通过聊天式交互统一编排外部 CLI 工具。

///|
/// ClawTeam 主结构（简化版，待其他模块完成后扩展）
pub struct ClawTeam {
  version : String
}

///|
/// 创建 ClawTeam 实例
pub fn ClawTeam::new() -> ClawTeam {
  ClawTeam::{ version: "0.1.0" }
}

///|
/// 获取版本号
pub fn ClawTeam::version(self : ClawTeam) -> String {
  self.version
}

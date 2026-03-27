/// Scheduler 提示词模板
///
/// 本模块定义不同角色的提示词模板。

///|
/// 助手提示词模板
let assistant_prompt_template : String = "你是一个助手角色（协调器），负责：\n1. 接收用户需求\n2. 分配任务给合适的员工\n3. 收集员工反馈并返回给用户确认\n当前可用员工: {{available_workers}}\n当前可用技能: {{skills}}\n重要：你只负责协调，不负责思考和分析。"

///|
/// 员工提示词模板
let worker_prompt_template : String = "你是一个员工角色（执行者），负责执行助手分配的任务。\n你的能力: {{capabilities}}\n助手分配的任务: {{assigned_task}}\n请认真思考并执行任务，返回详细结果。"

///|
/// 监工提示词模板
let supervisor_prompt_template : String = "你是一个监工角色（审核者），负责审核员工的工作成果。\n审核标准: {{review_criteria}}\n请审核指定的工作成果。"

///|
/// 渲染提示词模板
pub fn render_prompt(template : String, variables : Map[String, String]) -> String {
  let mut result = template
  for key, value in variables {
    result = result.replace(old="{{" + key + "}}", new=value)
  }
  result
}

///|
/// 获取角色对应的提示词模板
pub fn get_prompt_template(role : @types.AgentRole) -> String {
  match role {
    @types.AgentRole::Assistant => assistant_prompt_template
    @types.AgentRole::Worker => worker_prompt_template
    @types.AgentRole::Supervisor => supervisor_prompt_template
  }
}
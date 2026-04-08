/// project-mentions 模块 - MoonBit 实现
/// 原始 TypeScript 实现参考: paperclip/packages/shared/src/project-mentions.ts

///|
/// 协议常量
const PROJECT_MENTION_SCHEME : String = "project://"

///|
const AGENT_MENTION_SCHEME : String = "agent://"

///|
const SKILL_MENTION_SCHEME : String = "skill://"

///|
/// Mention 解析结果
pub(all) struct ParsedProjectMention {
  project_id : String
  color : String?
} derive(Debug, Eq)

///|
pub(all) struct ParsedAgentMention {
  agent_id : String
  icon : String?
} derive(Debug, Eq)

///|
pub(all) struct ParsedSkillMention {
  skill_id : String
  slug : String?
} derive(Debug, Eq)

///|
/// 十六进制字符表
const HEX_CHARS : String = "0123456789abcdef"

///|
/// 规范化十六进制颜色值
pub fn normalize_hex_color(input : String) -> String? {
  let len = input.length()
  if len == 0 {
    return None
  }

  // 去空白
  let trimmed = input.trim().to_string()
  let t_len = trimmed.length()
  if t_len == 0 {
    return None
  }

  // 检查 # 前缀
  let has_hash = trimmed[0] == 35
  let raw_start = if has_hash { 1 } else { 0 }
  let raw_len = t_len - raw_start

  if raw_len == 6 {
    // 验证6位十六进制并转小写
    let mut result = ""
    let mut i = 0
    while i < 6 {
      let ch = trimmed[raw_start + i]
      let idx : Int = if ch >= 48 && ch <= 57 {
        (ch - 48).to_int()
      } else if ch >= 65 && ch <= 70 {
        (ch - 65 + 10).to_int()
      } else if ch >= 97 && ch <= 102 {
        (ch - 97 + 10).to_int()
      } else {
        return None
      }
      result = result + HEX_CHARS[idx:idx + 1].to_string()
      i = i + 1
    }
    Some("#" + result)
  } else if raw_len == 3 {
    // 3位扩展到6位
    let mut result = ""
    let mut i = 0
    while i < 3 {
      let ch = trimmed[raw_start + i]
      let idx : Int = if ch >= 48 && ch <= 57 {
        (ch - 48).to_int()
      } else if ch >= 65 && ch <= 70 {
        (ch - 65 + 10).to_int()
      } else if ch >= 97 && ch <= 102 {
        (ch - 97 + 10).to_int()
      } else {
        return None
      }
      let c = HEX_CHARS[idx:idx + 1].to_string()
      result = result + c + c
      i = i + 1
    }
    Some("#" + result)
  } else {
    None
  }
}

///|
/// 解析查询参数
fn get_query_param(query : String, key : String) -> String? {
  let len = query.length()
  if len == 0 {
    return None
  }

  let start = if query[0] == 63 { 1 } else { 0 }
  let search_key = key + "="
  let sk_len = search_key.length()

  let mut i = start
  while i < len {
    let mut matched = true
    let mut j = 0
    while j < sk_len && i + j < len {
      if query[i + j] != search_key[j] {
        matched = false
        break
      }
      j = j + 1
    }
    if matched && i + sk_len <= len {
      let vs = i + sk_len
      let mut ve = vs
      while ve < len && query[ve] != 38 {
        ve = ve + 1
      }
      return Some(query[vs:ve].to_string())
    }
    while i < len && query[i] != 38 {
      i = i + 1
    }
    i = i + 1
  }
  None
}

///|
/// 构建 project mention href
pub fn build_project_mention_href(
  project_id : String,
  color : String?,
) -> String {
  let tid = project_id.trim().to_string()
  match color {
    Some(c) =>
      match normalize_hex_color(c) {
        Some(nc) =>
          PROJECT_MENTION_SCHEME + tid + "?c=" + nc[1:nc.length()].to_string()
        None => PROJECT_MENTION_SCHEME + tid
      }
    None => PROJECT_MENTION_SCHEME + tid
  }
}

///|
/// 解析 project mention href
pub fn parse_project_mention_href(href : String) -> ParsedProjectMention? {
  if !starts_with(href, PROJECT_MENTION_SCHEME) {
    return None
  }

  let rest = href[PROJECT_MENTION_SCHEME.length():href.length()].to_string()
  let mut path = rest
  let mut query : String? = None

  let r_len = rest.length()
  let mut i = 0
  while i < r_len {
    if rest[i] == 63 {
      path = rest[0:i].to_string()
      query = Some(rest[i:r_len].to_string())
      break
    }
    i = i + 1
  }

  // 去前导 '/'
  let p_len = path.length()
  let mut ps = 0
  while ps < p_len && path[ps] == 47 {
    ps = ps + 1
  }
  let pid = path[ps:p_len].to_string()
  if pid.length() == 0 {
    return None
  }

  let color = match query {
    Some(q) =>
      match get_query_param(q, "c") {
        Some(v) => normalize_hex_color(v)
        None =>
          match get_query_param(q, "color") {
            Some(v) => normalize_hex_color(v)
            None => None
          }
      }
    None => None
  }

  Some(ParsedProjectMention::{ project_id: pid, color })
}

///|
/// 构建 agent mention href
pub fn build_agent_mention_href(agent_id : String, icon : String?) -> String {
  let tid = agent_id.trim().to_string()
  match icon {
    Some(i) =>
      match normalize_agent_icon(i) {
        Some(ni) => AGENT_MENTION_SCHEME + tid + "?i=" + ni
        None => AGENT_MENTION_SCHEME + tid
      }
    None => AGENT_MENTION_SCHEME + tid
  }
}

///|
/// 解析 agent mention href
pub fn parse_agent_mention_href(href : String) -> ParsedAgentMention? {
  if !starts_with(href, AGENT_MENTION_SCHEME) {
    return None
  }

  let rest = href[AGENT_MENTION_SCHEME.length():href.length()].to_string()
  let mut path = rest
  let mut query : String? = None

  let r_len = rest.length()
  let mut i = 0
  while i < r_len {
    if rest[i] == 63 {
      path = rest[0:i].to_string()
      query = Some(rest[i:r_len].to_string())
      break
    }
    i = i + 1
  }

  let p_len = path.length()
  let mut ps = 0
  while ps < p_len && path[ps] == 47 {
    ps = ps + 1
  }
  let aid = path[ps:p_len].to_string()
  if aid.length() == 0 {
    return None
  }

  let icon = match query {
    Some(q) =>
      match get_query_param(q, "i") {
        Some(v) => normalize_agent_icon(v)
        None =>
          match get_query_param(q, "icon") {
            Some(v) => normalize_agent_icon(v)
            None => None
          }
      }
    None => None
  }

  Some(ParsedAgentMention::{ agent_id: aid, icon })
}

///|
/// 构建 skill mention href
pub fn build_skill_mention_href(skill_id : String, slug : String?) -> String {
  let tid = skill_id.trim().to_string()
  match slug {
    Some(s) =>
      match normalize_skill_slug(s) {
        Some(ns) => SKILL_MENTION_SCHEME + tid + "?s=" + ns
        None => SKILL_MENTION_SCHEME + tid
      }
    None => SKILL_MENTION_SCHEME + tid
  }
}

///|
/// 解析 skill mention href
pub fn parse_skill_mention_href(href : String) -> ParsedSkillMention? {
  if !starts_with(href, SKILL_MENTION_SCHEME) {
    return None
  }

  let rest = href[SKILL_MENTION_SCHEME.length():href.length()].to_string()
  let mut path = rest
  let mut query : String? = None

  let r_len = rest.length()
  let mut i = 0
  while i < r_len {
    if rest[i] == 63 {
      path = rest[0:i].to_string()
      query = Some(rest[i:r_len].to_string())
      break
    }
    i = i + 1
  }

  let p_len = path.length()
  let mut ps = 0
  while ps < p_len && path[ps] == 47 {
    ps = ps + 1
  }
  let sid = path[ps:p_len].to_string()
  if sid.length() == 0 {
    return None
  }

  let slug = match query {
    Some(q) =>
      match get_query_param(q, "s") {
        Some(v) => normalize_skill_slug(v)
        None =>
          match get_query_param(q, "slug") {
            Some(v) => normalize_skill_slug(v)
            None => None
          }
      }
    None => None
  }

  Some(ParsedSkillMention::{ skill_id: sid, slug })
}

///|
/// 手动转小写
fn to_lower(s : String) -> String {
  let alpha_lower = "abcdefghijklmnopqrstuvwxyz"
  let len = s.length()
  let mut result = ""
  let mut i = 0
  while i < len {
    let ch = s[i]
    if ch >= 65 && ch <= 90 {
      // A-Z → 用字母表索引转小写
      let idx = (ch - 65).to_int()
      result = result + alpha_lower[idx:idx + 1].to_string()
    } else {
      result = result + s[i:i + 1].to_string()
    }
    i = i + 1
  }
  result
}

///|
/// 规范化 agent 图标名: [a-z0-9-]+
pub fn normalize_agent_icon(input : String) -> String? {
  let t = to_lower(input.trim().to_string())
  if t.length() == 0 {
    return None
  }
  let len = t.length()
  let mut i = 0
  while i < len {
    let ch = t[i]
    if !((ch >= 97 && ch <= 122) || (ch >= 48 && ch <= 57) || ch == 45) {
      return None
    }
    i = i + 1
  }
  Some(t)
}

///|
/// 规范化 skill slug: [a-z0-9][a-z0-9-]*
pub fn normalize_skill_slug(input : String) -> String? {
  let t = to_lower(input.trim().to_string())
  if t.length() == 0 {
    return None
  }
  let f = t[0]
  if !((f >= 97 && f <= 122) || (f >= 48 && f <= 57)) {
    return None
  }
  let len = t.length()
  let mut i = 1
  while i < len {
    let ch = t[i]
    if !((ch >= 97 && ch <= 122) || (ch >= 48 && ch <= 57) || ch == 45) {
      return None
    }
    i = i + 1
  }
  Some(t)
}

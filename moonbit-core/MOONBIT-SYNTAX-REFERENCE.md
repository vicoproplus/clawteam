# MoonBit 语法参考 (实际验证版)

> 基于 MoonBit 0.1.20260403 实际编译测试
> 创建日期: 2026-04-09

---

## 1. 枚举定义

```moonbit
// ✅ 正确: 使用 pub(all) 使枚举可构造
pub(all) enum CompanyStatus {
  Active
  Paused
  Archived
}

// ⚠️ 注意: pub 枚举是 read-only 类型,外部包无法构造
// pub enum CompanyStatus { ... }  // 错误: "Cannot create values of the read-only type"

// ❌ 错误: 不支持 deriving 语法
// pub enum CompanyStatus { ... } deriving (Eq, Debug)
```

## 2. 枚举转字符串函数

```moonbit
// ✅ 正确: 使用 match 表达式
pub fn company_status_to_string(s: CompanyStatus) -> String {
  match s {
    Active => "active"
    Paused => "paused"
    Archived => "archived"
  }
}
```

## 3. 结构体定义

```moonbit
// ✅ 正确: 简单结构体 (无 derive)
pub struct JsonRpcRequest {
  method_name: String
  params: String
  id: Int
}

// ❌ 错误: 不支持 deriving 语法
// pub struct JsonRpcRequest { ... } deriving (Debug)
```

## 4. 结构体构造

```moonbit
// ✅ 正确: 使用 Type::{field: value} 语法
let req = JsonRpcRequest::{
  method_name: "health",
  params: "{}",
  id: 1
}

// ✅ 正确: 如果类型可推断,可省略 Type::
let req2 = {
  method_name: "health",
  params: "{}",
  id: 1
}

// ❌ 错误: 缺少 ::
// let req = JsonRpcRequest { method_name: "health", ... }
```

## 5. 函数类型

```moonbit
// ✅ 正确: 使用 (T) -> R 语法
pub fn run_stdio_loop(handler: (JsonRpcRequest) -> JsonRpcResponse) -> Unit {
  // ...
}

// ❌ 错误: 不支持 Fn[T] -> R 语法
// pub fn run_stdio_loop(handler: Fn[JsonRpcRequest] -> JsonRpcResponse) -> Unit
```

## 6. 泛型函数

```moonbit
// ✅ 正确: 使用 fn[T] f 语法 (新语法)
pub fn[T] identity(x: T) -> T {
  x
}

// ❌ 错误: 旧语法 fn f[T] 已废弃
// pub fn identity[T](x: T) -> T { ... }
```

## 7. Map API

```moonbit
// 创建
let m: Map[String, Int] = Map::new()

// 设置 (原地修改,返回 Unit)
m.set("key", 42)

// 获取 (返回 Option[T])
let value: Int? = m.get("key")  // Some(42)

// 包含检查
let has_key: Bool = m.contains("key")

// 删除
m.remove("key")

// 容量
let cap: Int = m.capacity()

// 清空
m.clear()
```

## 8. Array API

```moonbit
// 创建
let xs: Array[Int] = [1, 2, 3]

// Fold (注意标签参数)
let sum = xs.fold(fn(acc, x) { acc + x }, init=0)

// 长度
let len: Int = xs.length()

// 访问
let first: Int? = xs.get(0)
```

## 9. String API

```moonbit
// 长度
let len: Int = "hello".length()

// 前缀/后缀检查 (新语法)
let has_prefix: Bool = "hello".has_prefix("hel")  // ✅ true
let has_suffix: Bool = "hello".has_suffix("lo")  // ✅ true

// ❌ 旧语法已废弃
// "hello".starts_with("hel")  // 警告
// "hello".ends_with("lo")     // 警告

// 子字符串 (使用切片语法)
let sub: String = "hello"[1:4].to_string()  // "ell"

// ❌ 旧语法已废弃
// "hello".substring(1, 4)  // 警告

// 替换 (需要标签参数)
let replaced: String = "hello".replace(old="l", new="L")  // "heLLo"

// ❌ 错误: 缺少标签
// "hello".replace("l", "L")  // 编译错误

// 转整数 (方法名需要验证)
// 注意: String.to_int() 不存在,需要使用其他方式
```

## 10. 测试语法

```moonbit
// ✅ 正确: 同包测试不需要导入
// 直接使用 enum 构造函数和函数

test "company status to_string" {
  inspect(company_status_to_string(Active), content="\"active\"")
  inspect(company_status_to_string(Paused), content="\"paused\"")
}

// ✅ 正确: 使用 inspect 进行快照测试
test "map api works" {
  let m: Map[String, Int] = Map::new()
  m.set("key", 42)
  inspect(m.get("key"), content="Some(42)")
}

// ❌ 错误: 不支持 use 导入 (reserved keyword)
// use ./enums.*

// ❌ 错误: assert_eq/ assert_ne 不存在
// assert_eq(Active, Active)
// assert_ne(Active, Paused)
```

## 11. Option 类型

```moonbit
// 模式匹配
match opt {
  Some(v) => println(v.to_string())
  None => println("nothing")
}

// 注意: Option 在 MoonBit 中写作 T? (简写)
let x: Int? = Some(42)
let y: Int? = None
```

## 12. Result 类型

```moonbit
// 定义
pub fn parse(s: String) -> Result[Int, String] {
  // ...
}

// 模式匹配
match result {
  Ok(v) => println(v.to_string())
  Err(e) => println("Error: " + e)
}
```

## 13. loop 循环

```moonbit
// ❌ 旧语法已废弃
// loop {
//   if condition { break }
// }

// ✅ 新语法: 使用 for 循环带状态
for state = init {
  if condition {
    break state
  }
  // 更新 state
}
```

## 14. IO (需要验证)

```moonbit
// TODO: 验证 stdio 读取 API
// 可能的 API:
// - io::read_line()
// - stdin().read_line()
// - Console::read_line()

// 输出
println("hello")  // ✅ 确认存在
```

## 15. 枚举构造限制

```moonbit
// ⚠️ 重要发现: 枚举构造函数可能是 read-only 类型
// 错误: "Cannot create values of the read-only type: Active"

// 可能需要在同一包内才能构造枚举值
// 或者需要使用特殊的构造语法

// 解决方案:
// 1. 测试文件与定义在同一包 (已满足)
// 2. 或者枚举需要标记为 pub
```

---

## 常见错误对照表

| 错误语法 | 正确语法 | 原因 |
|---------|---------|------|
| `deriving (Debug)` | (无,不支持) | MoonBit 不支持 derive 宏 |
| `Fn[T] -> R` | `(T) -> R` | 函数类型语法变更 |
| `use ./module.*` | (不需要) | 同包自动可见 |
| `assert_eq(a, b)` | `inspect(a, content="...")` | 使用快照测试 |
| `str.starts_with()` | `str.has_prefix()` | API 重命名 |
| `str.substring(1, 4)` | `str[1:4].to_string()` | 切片语法 |
| `str.replace("a", "b")` | `str.replace(old="a", new="b")` | 需要标签参数 |
| `Type { field: v }` | `Type::{ field: v }` | 需要 `::` |
| `fn f[T](x: T)` | `fn[T] f(x: T)` | 泛型语法变更 |
| `loop { ... }` | `for state = init { ... }` | 循环语法变更 |

---

## 待验证项目

- [ ] stdio 读取 API (read_line)
- [ ] String.to_int() 是否存在
- [ ] 枚举 read-only 类型的正确构造方式
- [ ] assert_eq/assert_ne 是否存在或需要自定义
- [ ] JSON 解析库 (是否有内置 @json 模块)

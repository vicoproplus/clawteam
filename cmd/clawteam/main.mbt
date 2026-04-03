///|
async fn main {
  @backtrace.initialize()
  let args = @os.args()
  match args[1:] {
    ["conversations"] => @conversation.list()
    ["conversation", .. rest] => @conversation.show(rest)
    ["exec", .. rest] => @exec.exec(rest)
    ["tui", .. rest] => @tui.tui_interactive(rest)
    ["version"] => println(@buildinfo.version)
    [.. rest] => @interactive.interactive(rest)
  }
}

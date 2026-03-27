// #region Define traits with default implementations

///|
/// is_being_cancelled may be unused if there's no async test
#warnings("-unused_value")
trait MoonBit_Async_Test_Driver {
  run_async_tests(@moonbitlang/core/prelude.Array[async () -> Unit]) -> Unit = _
  is_being_cancelled() -> Bool = _
}

///|
impl MoonBit_Async_Test_Driver with run_async_tests(_) {
  ()
}

///|
impl MoonBit_Async_Test_Driver with is_being_cancelled() {
  false
}

///|
trait MoonBit_Test_Driver {
  /// returns if the test is found and executed
  run_test(
    @moonbitlang/core/prelude.Array[async () -> Unit],
    String,
    Int,
    (String, String, Bool) -> Unit,
    (Error) -> String,
  ) -> Bool raise MoonBitTestDriverInternalSkipTest = _
}

///|
impl MoonBit_Test_Driver with run_test(_, _, _, _, _) {
  false
}

// #endregion
// #region Define abstract types for namespace with implementation declarations

///|
priv type MoonBit_Test_Driver_Internal_No_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_No_Args

///|
priv type MoonBit_Test_Driver_Internal_With_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_With_Args

///|
priv type MoonBit_Test_Driver_Internal_Async_No_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_No_Args

///|
priv type MoonBit_Test_Driver_Internal_Async_With_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_With_Args

///|
priv type MoonBit_Test_Driver_Internal_With_Bench_Args

///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_With_Bench_Args

///|
priv type MoonBit_Async_Test_Driver_Impl

///|
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl

// #endregion

// #region Utility Types

///|
/// Maybe unused if there's no tests
#warnings("-unused_type_declaration")
priv struct MoonBitTestDriverInternalTestMap[F](
  @moonbitlang/core/prelude.Map[
    String,
    @moonbitlang/core/prelude.Map[Int, (F, ReadOnlyArray[String])],
  ]
)

///|
priv suberror MoonBitTestDriverInternalSkipTest {
  MoonBitTestDriverInternalSkipTest(name~ : String)
}

// #endregion
///|
fn moonbit_test_driver_internal_error_to_string(x : Error) -> String = "%error.to_string"

///|
#cfg(target="native")
fn string_get(s : String, index : Int) -> Int = "%string_get"

///|
#cfg(not(target="js"))
fn moonbit_unsafe_char_from_int(x : Int) -> Char = "%identity"

///|
#cfg(target="native")
const MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE = true

///|
#cfg(not(target="native"))
const MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE = false

// =========================================================
// ================== BEGIN WASM SPECIFIC ==================
// =========================================================

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_get_file_name(
  file_name : MoonbitTestDriverInternalExternString,
) -> String {
  moonbit_test_driver_internal_string_from_extern(file_name)
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
#external
type MoonbitTestDriverInternalStringReadHandle

///|
#cfg(any(target="wasm", target="wasm-gc"))
#external
type MoonbitTestDriverInternalExternString

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_begin_read_string(
  s : MoonbitTestDriverInternalExternString,
) -> MoonbitTestDriverInternalStringReadHandle = "__moonbit_fs_unstable" "begin_read_string"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_string_read_char(
  handle : MoonbitTestDriverInternalStringReadHandle,
) -> Int = "__moonbit_fs_unstable" "string_read_char"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_finish_read_string(
  handle : MoonbitTestDriverInternalStringReadHandle,
) = "__moonbit_fs_unstable" "finish_read_string"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn fixedarray_to_bytes(arr : FixedArray[Byte]) -> Bytes = "%identity"

///|
#cfg(any(target="wasm", target="wasm-gc"))
fn moonbit_test_driver_internal_string_from_extern(
  e : MoonbitTestDriverInternalExternString,
) -> String {
  let buf = FixedArray::make(512, Byte::default())
  let mut len = 0
  let handle = moonbit_test_driver_internal_begin_read_string(e)
  while true {
    let ch = moonbit_test_driver_internal_string_read_char(handle)
    if ch == -1 {
      break
    } else {
      let written_bytes_len = buf.set_utf16le_char(
        len,
        moonbit_unsafe_char_from_int(ch),
      )
      len += written_bytes_len
    }
  }
  moonbit_test_driver_internal_finish_read_string(handle)
  fixedarray_to_bytes(buf).to_unchecked_string(offset=0, length=len).to_string()
}

// =========================================================
// =================== END WASM SPECIFIC ===================
// =========================================================

// =========================================================
// ================= BEGIN NATIVE SPECIFIC =================
// =========================================================

///|
#cfg(target="native")
fn moonbit_test_driver_internal_native_parse_args() -> @moonbitlang/core/prelude.Array[
  (String, Int),
] {
  fn moonbit_test_driver_internal_parse_int_(s : String) -> Int {
    let mut res = 0
    let len = s.length()
    for i = 0; i < len; i = i + 1 {
      res = res * 10 + (string_get(s, i) - '0')
    }
    res
  }

  fn moonbit_test_driver_internal_utf8_bytes_to_mbt_string(
    bytes : Bytes,
  ) -> String {
    let res = @moonbitlang/core/prelude.StringBuilder::new()
    let len = bytes.length()
    let mut i = 0
    while i < len {
      let mut c = bytes[i].to_int()
      if c < 0x80 {
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 1
      } else if c < 0xE0 {
        if i + 1 >= len {
          break
        }
        c = ((c & 0x1F) << 6) | (bytes[i + 1].to_int() & 0x3F)
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 2
      } else if c < 0xF0 {
        if i + 2 >= len {
          break
        }
        c = ((c & 0x0F) << 12) |
          ((bytes[i + 1].to_int() & 0x3F) << 6) |
          (bytes[i + 2].to_int() & 0x3F)
        res.write_char(moonbit_unsafe_char_from_int(c))
        i += 3
      } else {
        if i + 3 >= len {
          break
        }
        c = ((c & 0x07) << 18) |
          ((bytes[i + 1].to_int() & 0x3F) << 12) |
          ((bytes[i + 2].to_int() & 0x3F) << 6) |
          (bytes[i + 3].to_int() & 0x3F)
        c -= 0x10000
        res.write_char(moonbit_unsafe_char_from_int((c >> 10) + 0xD800))
        res.write_char(moonbit_unsafe_char_from_int((c & 0x3FF) + 0xDC00))
        i += 4
      }
    }
    res.to_string()
  }

  fn moonbit_test_driver_internal_get_cli_args_internal() -> @moonbitlang/core/prelude.Array[
    String,
  ] {
    let tmp = moonbit_test_driver_internal_get_cli_args_ffi()
    let res = @moonbitlang/core/prelude.Array::new(capacity=tmp.length())
    for i = 0; i < tmp.length(); i = i + 1 {
      res.push(moonbit_test_driver_internal_utf8_bytes_to_mbt_string(tmp[i]))
    }
    res
  }

  fn moonbit_test_driver_internal_split_mbt_string(
    s : String,
    sep : Char,
  ) -> @moonbitlang/core/prelude.Array[String] {
    let res = []
    let mut i = 0
    let mut start = 0
    while i < s.length() {
      if moonbit_unsafe_char_from_int(string_get(s, i)) == sep {
        res.push(s.unsafe_substring(start~, end=i))
        start = i + 1
      }
      i += 1
    }
    if start < s.length() {
      res.push(s.unsafe_substring(start~, end=s.length()))
    }
    res
  }

  let file_and_index = []
  let cli_args = moonbit_test_driver_internal_get_cli_args_internal()
  let test_args = moonbit_test_driver_internal_split_mbt_string(
    cli_args[1],
    '/',
  )
  for arg in test_args {
    let file_and_range = moonbit_test_driver_internal_split_mbt_string(arg, ':')
    let file = file_and_range[0]
    let range = file_and_range[1]
    let start_and_end = moonbit_test_driver_internal_split_mbt_string(
      range, '-',
    )
    let start = moonbit_test_driver_internal_parse_int_(start_and_end[0])
    let end = moonbit_test_driver_internal_parse_int_(start_and_end[1])
    for i = start; i < end; i = i + 1 {
      file_and_index.push((file, i))
    }
  }
  file_and_index
}

///|
#cfg(target="native")
fn moonbit_test_driver_internal_get_cli_args_ffi() -> FixedArray[Bytes] = "$moonbit.get_cli_args"

// =========================================================
// ================== END NATIVE SPECIFIC ==================
// =========================================================

// =========================================================
// -================== BEGIN JS SPECIFIC ===================
// =========================================================

///|
#cfg(target="js")
#external
type MoonBitTestDriverInternalTestEntry

///|
#cfg(target="js")
extern "js" fn MoonBitTestDriverInternalTestEntry::filename(
  self : Self,
) -> String =
  #| (entry) => entry[0]

///|
#cfg(target="js")
extern "js" fn MoonBitTestDriverInternalTestEntry::index(self : Self) -> Int =
  #| (entry) => entry[1]

///|
pub(all) suberror MoonBitTestDriverInternalJsError {
  MoonBitTestDriverInternalJsError(String)
}

///|
/// Maybe unused if there's no tests
#cfg(target="js")
#warnings("-unused_value")
fn moonbit_test_driver_internal_catch_error(
  f : () -> Unit raise,
  on_ok~ : () -> Unit,
  on_err~ : (Error) -> Unit,
) -> Unit {
  moonbit_test_driver_internal_js_catch(
    on_err=err_str => on_err(MoonBitTestDriverInternalJsError(err_str)),
    () => {
      try f() catch {
        err => on_err(err)
      } noraise {
        _ => on_ok()
      }
    },
  )
}

///|
#cfg(target="js")
extern "js" fn moonbit_test_driver_internal_js_catch(
  f : () -> Unit,
  on_err~ : (String) -> Unit,
) =
  #| (f, on_err) => {
  #|   try {
  #|     f()
  #|   } catch (err) {
  #|     const msg = err.stack.toString()
  #|     on_err(msg)
  #|   }
  #| }

///|
/// Maybe unused if there's no tests
#cfg(not(target="js"))
#warnings("-unused_value")
fn moonbit_test_driver_internal_catch_error(
  f : () -> Unit raise,
  on_ok~ : () -> Unit,
  on_err~ : (Error) -> Unit,
) -> Unit {
  try f() catch {
    err => on_err(err)
  } noraise {
    _ => on_ok()
  }
}

// =========================================================
// -=================== END JS SPECIFIC ====================
// =========================================================
// This file is introduced when tests without args is needed

///|
let moonbit_test_driver_internal_no_args_tests : MoonBitTestDriverInternalTestMap[
  () -> Unit raise,
] = {
  "workflow_engine_wbtest.mbt": {
  },
  "ui_snapshot_wbtest.mbt": {
  },
  "ui_events_wbtest.mbt": {
    0: (__test_75695f6576656e74735f7762746573742e6d6274_0, ["run_events_as_timeline sorts and maps events"]),
  },
  "system_storage_wbtest.mbt": {
  },
  "subjob_wbtest.mbt": {
  },
  "runtime_wbtest.mbt": {
  },
  "run_workspace_wbtest.mbt": {
  },
  "proposal_wbtest.mbt": {
    0: (__test_70726f706f73616c5f7762746573742e6d6274_0, ["draft_job_proposal creates non-executing draft with steps"]),
    2: (__test_70726f706f73616c5f7762746573742e6d6274_2, ["proposal_as_workflow builds generic analysis workflow with notify config"]),
    9: (__test_70726f706f73616c5f7762746573742e6d6274_9, ["proposal_as_workflow preserves generic drafts as direct proposal workflows"]),
    10: (__test_70726f706f73616c5f7762746573742e6d6274_10, ["render_job_proposal_for_chat includes proposal steps"]),
    11: (__test_70726f706f73616c5f7762746573742e6d6274_11, ["ensure_draft_proposal rejects non-draft proposals"]),
  },
  "profile_wbtest.mbt": {
  },
  "memory_wbtest.mbt": {
  },
  "memory_capture_wbtest.mbt": {
  },
  "executor_wbtest.mbt": {
  },
  "display_wbtest.mbt": {
    0: (__test_646973706c61795f7762746573742e6d6274_0, ["human readable ids include slug and short suffix"]),
    1: (__test_646973706c61795f7762746573742e6d6274_1, ["definition_display_title prefers stored title"]),
    2: (__test_646973706c61795f7762746573742e6d6274_2, ["unwrapped_epoch_ms_near restores wrapped timestamps near current era"]),
  },
  "control_service_wbtest.mbt": {
  },
  "compiler_wbtest.mbt": {
    0: (__test_636f6d70696c65725f7762746573742e6d6274_0, ["compile_job_proposal preserves proposal step structure for generic requests"]),
    1: (__test_636f6d70696c65725f7762746573742e6d6274_1, ["compile_job_proposal builds direct workflow for generic requests"]),
    2: (__test_636f6d70696c65725f7762746573742e6d6274_2, ["compile_job_proposal preserves custom step kinds directly"]),
    3: (__test_636f6d70696c65725f7762746573742e6d6274_3, ["compile_job_proposal applies analysis metadata from proposal steps"]),
    4: (__test_636f6d70696c65725f7762746573742e6d6274_4, ["compile_job_proposal preserves analysis execution routing hints"]),
  },
  "chat_wbtest.mbt": {
  },
  "chat_service_wbtest.mbt": {
    0: (__test_636861745f736572766963655f7762746573742e6d6274_0, ["decide_active_job_notifications emits verbose progress and heartbeat"]),
    1: (__test_636861745f736572766963655f7762746573742e6d6274_1, ["decide_active_job_notifications warns once for long-running jobs"]),
    2: (__test_636861745f736572766963655f7762746573742e6d6274_2, ["decide_active_job_notifications emits step start for normal verbosity"]),
    3: (__test_636861745f736572766963655f7762746573742e6d6274_3, ["resolve_job_notify_target applies persisted preference overrides"]),
    4: (__test_636861745f736572766963655f7762746573742e6d6274_4, ["parse_job_notify_target accepts legacy optional-json array encoding"]),
    5: (__test_636861745f736572766963655f7762746573742e6d6274_5, ["render_confirmed_job_reply includes title and created time"]),
    8: (__test_636861745f736572766963655f7762746573742e6d6274_8, ["resolve_target_proposal prefers explicit id and falls back to reply lookup"]),
    9: (__test_636861745f736572766963655f7762746573742e6d6274_9, ["proposal command render helpers produce expected text"]),
  },
  "artifact_store_wbtest.mbt": {
  },
  "application_wbtest.mbt": {
  },
  "analysis_wbtest.mbt": {
    5: (__test_616e616c797369735f7762746573742e6d6274_5, ["requested_model_name_or_none treats default as auto-detect"]),
  },
}


///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_No_Args with run_test(
  _,
  filename,
  index,
  handle_result,
  error_to_string,
) {
  if moonbit_test_driver_internal_no_args_tests.0.get(filename)
    is Some(index_map) &&
    index_map.get(index) is Some((f, attrs)) {
    let name = if attrs is [name, ..] { name } else { "" }
    if MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      for attr in attrs {
        if attr is [.. "panic", ..] {
          raise MoonBitTestDriverInternalSkipTest(name~)
        }
      }
    }
    moonbit_test_driver_internal_catch_error(
      f,
      on_ok=() => handle_result(name, "", false),
      on_err=err => handle_result(name, error_to_string(err), false),
    )
    true
  } else {
    false
  }
}
// This file is included if async test is needed
// This also introduces the dependency on async package

///|
#cfg(not(any(target="wasm", target="wasm-gc")))
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl with run_async_tests(
  tests,
) {
  let moonbit_test_driver_internal_max_concurrent_tests : Int = 10
  @moonbitlang/async.run_async_main(() => {
    @moonbitlang/async.with_task_group(group => {
      let mut i = 0
      for _ in 0..<moonbit_test_driver_internal_max_concurrent_tests {
        group.spawn_bg(() => {
          while i < tests.length() {
            let f = tests[i]
            i += 1
            f()
          }
        })
      }
    })
    
  })
}

///|
#cfg(not(any(target="wasm", target="wasm-gc")))
impl MoonBit_Async_Test_Driver for MoonBit_Async_Test_Driver_Impl with is_being_cancelled() {
  @moonbitlang/async.is_being_cancelled()
}
// This file is introduced when tests without args is needed but when async is needed
// This also introduces the dependency on async package

///|
let moonbit_test_driver_internal_async_tests : MoonBitTestDriverInternalTestMap[
  async () -> Unit,
] = {
  "workflow_engine_wbtest.mbt": {
    0: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_0, ["WorkflowEngine executes steps in order and passes outputs"]),
    1: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_1, ["WorkflowEngine retries a failing step and persists retry count"]),
    2: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_2, ["WorkflowEngine fails the run when a step exhausts retries"]),
    3: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_3, ["WorkflowEngine persists controller decisions from step output when policy allows it"]),
    4: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_4, ["WorkflowEngine accepts looser controller decision output shapes"]),
    5: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_5, ["WorkflowEngine cancels before the next step when control requests stop"]),
    6: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_6, ["WorkflowEngine warns on unexpected verdicts but still persists them"]),
    7: (__test_776f726b666c6f775f656e67696e655f7762746573742e6d6274_7, ["WorkflowEngine uses configured next_iteration field mappings"]),
  },
  "ui_snapshot_wbtest.mbt": {
    0: (__test_75695f736e617073686f745f7762746573742e6d6274_0, ["build_job_ui_snapshot includes proposal workflow steps child runs and timeline"]),
    1: (__test_75695f736e617073686f745f7762746573742e6d6274_1, ["build_job_ui_snapshot includes explicit handoffs when provided"]),
    2: (__test_75695f736e617073686f745f7762746573742e6d6274_2, ["build_job_ui_snapshot adds split and merge nodes for controller runs"]),
  },
  "ui_events_wbtest.mbt": {
  },
  "system_storage_wbtest.mbt": {
    0: (__test_73797374656d5f73746f726167655f7762746573742e6d6274_0, ["SystemStore persists definitions workflows runs steps and artifacts"]),
    1: (__test_73797374656d5f73746f726167655f7762746573742e6d6274_1, ["SystemStore keeps indexes and per-run files in expected layout"]),
    2: (__test_73797374656d5f73746f726167655f7762746573742e6d6274_2, ["SystemStore persists job proposals and run events"]),
    3: (__test_73797374656d5f73746f726167655f7762746573742e6d6274_3, ["SystemStore persists notification preferences"]),
  },
  "subjob_wbtest.mbt": {
    0: (__test_7375626a6f625f7762746573742e6d6274_0, ["subjob_step_handler creates a child run with lineage and executes it"]),
    1: (__test_7375626a6f625f7762746573742e6d6274_1, ["controller delegate step records iteration state and child lineage"]),
    2: (__test_7375626a6f625f7762746573742e6d6274_2, ["subjob_step_handler applies explicit child profile to the planned child job"]),
  },
  "runtime_wbtest.mbt": {
    0: (__test_72756e74696d655f7762746573742e6d6274_0, ["RuntimeManager persists checkpoints and prevents duplicate active runs"]),
    1: (__test_72756e74696d655f7762746573742e6d6274_1, ["RuntimeManager returns due scheduled definitions from checkpoints"]),
    2: (__test_72756e74696d655f7762746573742e6d6274_2, ["RuntimeManager triggers due runs and exposes active and recent status"]),
    3: (__test_72756e74696d655f7762746573742e6d6274_3, ["RuntimeManager schedules retry runs with backoff and resets retry state on success"]),
    4: (__test_72756e74696d655f7762746573742e6d6274_4, ["RuntimeManager initializes controller state for controller jobs"]),
  },
  "run_workspace_wbtest.mbt": {
    0: (__test_72756e5f776f726b73706163655f7762746573742e6d6274_0, ["prepare_run_workspace creates dedicated git-backed workspace with inherited memory"]),
    1: (__test_72756e5f776f726b73706163655f7762746573742e6d6274_1, ["prepare_run_workspace nests child runs under parent workspace"]),
  },
  "proposal_wbtest.mbt": {
    1: (__test_70726f706f73616c5f7762746573742e6d6274_1, ["plan_job_proposal applies configured controller profile steps"]),
    3: (__test_70726f706f73616c5f7762746573742e6d6274_3, ["plan_job_proposal uses model JSON when available"]),
    4: (__test_70726f706f73616c5f7762746573742e6d6274_4, ["profile json drives controller policy through compilation into runtime"]),
    5: (__test_70726f706f73616c5f7762746573742e6d6274_5, ["plan_job_proposal includes relevant memory context in planner prompt"]),
    6: (__test_70726f706f73616c5f7762746573742e6d6274_6, ["revise_job_proposal updates draft from model JSON"]),
    7: (__test_70726f706f73616c5f7762746573742e6d6274_7, ["revise_job_proposal includes memory context in revise prompt"]),
    8: (__test_70726f706f73616c5f7762746573742e6d6274_8, ["revise_job_proposal carries prior revision history into later revisions"]),
    12: (__test_70726f706f73616c5f7762746573742e6d6274_12, ["save_confirmed_job_proposal persists confirmed status and run id"]),
    13: (__test_70726f706f73616c5f7762746573742e6d6274_13, ["save_rejected_job_proposal persists rejected status"]),
  },
  "profile_wbtest.mbt": {
    0: (__test_70726f66696c655f7762746573742e6d6274_0, ["load_job_profile reads controller profile from moonclaw.jobs.json"]),
    1: (__test_70726f66696c655f7762746573742e6d6274_1, ["suggest_job_profile_family matches configured request rules"]),
  },
  "memory_wbtest.mbt": {
    0: (__test_6d656d6f72795f7762746573742e6d6274_0, ["MemoryService persists and reloads scoped memory records"]),
    1: (__test_6d656d6f72795f7762746573742e6d6274_1, ["MemoryService search filters by scope and ignores archived records"]),
  },
  "memory_capture_wbtest.mbt": {
    0: (__test_6d656d6f72795f636170747572655f7762746573742e6d6274_0, ["capture_successful_run_memories stores run summary and report artifacts"]),
  },
  "executor_wbtest.mbt": {
    0: (__test_6578656375746f725f7762746573742e6d6274_0, ["execute_run_lifecycle runs workflow and invokes hooks"]),
    1: (__test_6578656375746f725f7762746573742e6d6274_1, ["execute_run_lifecycle fails missing workflow_id runs"]),
    2: (__test_6578656375746f725f7762746573742e6d6274_2, ["execution_notification_hooks sends start and finish notifications"]),
  },
  "display_wbtest.mbt": {
  },
  "control_service_wbtest.mbt": {
    0: (__test_636f6e74726f6c5f736572766963655f7762746573742e6d6274_0, ["render_job_status_command_reply resolves run targets"]),
    1: (__test_636f6e74726f6c5f736572766963655f7762746573742e6d6274_1, ["request_job_stop resolves run ids before calling cancel"]),
    2: (__test_636f6e74726f6c5f736572766963655f7762746573742e6d6274_2, ["render_job_workspace_command_reply shows workspace path"]),
  },
  "compiler_wbtest.mbt": {
  },
  "chat_wbtest.mbt": {
    0: (__test_636861745f7762746573742e6d6274_0, ["ChatService retrieves active artifacts and returns citations"]),
    1: (__test_636861745f7762746573742e6d6274_1, ["ChatService applies persisted chat bindings to artifact scope"]),
  },
  "chat_service_wbtest.mbt": {
    6: (__test_636861745f736572766963655f7762746573742e6d6274_6, ["save_job_notification_preference preserves created_at when updating"]),
    7: (__test_636861745f736572766963655f7762746573742e6d6274_7, ["save_presented_job_proposal stores presented message id"]),
    10: (__test_636861745f736572766963655f7762746573742e6d6274_10, ["dispatch_job_chat_action routes commands through callbacks"]),
  },
  "artifact_store_wbtest.mbt": {
    0: (__test_61727469666163745f73746f72655f7762746573742e6d6274_0, ["ArtifactStore writes reads and filters artifacts"]),
    1: (__test_61727469666163745f73746f72655f7762746573742e6d6274_1, ["ArtifactStore supports append-only versions and mutable overwrite"]),
    2: (__test_61727469666163745f73746f72655f7762746573742e6d6274_2, ["ArtifactStore archives and cleans up artifacts by retention policy"]),
  },
  "application_wbtest.mbt": {
    0: (__test_6170706c69636174696f6e5f7762746573742e6d6274_0, ["GatewayJobApp::revise_draft_proposal rejects non-draft proposals"]),
    1: (__test_6170706c69636174696f6e5f7762746573742e6d6274_1, ["GatewayJobApp::confirm_proposal registers workflow and persists proposal state"]),
    2: (__test_6170706c69636174696f6e5f7762746573742e6d6274_2, ["GatewayJobApp::confirm_proposal initializes generic controller records"]),
  },
  "analysis_wbtest.mbt": {
    0: (__test_616e616c797369735f7762746573742e6d6274_0, ["analysis_step_handler prefers run workspace over config cwd"]),
    1: (__test_616e616c797369735f7762746573742e6d6274_1, ["analysis_step_handler persists report and result artifacts"]),
    2: (__test_616e616c797369735f7762746573742e6d6274_2, ["analysis_step_handler passes execution routing hints into executor"]),
    3: (__test_616e616c797369735f7762746573742e6d6274_3, ["analysis_step_handler loads local skill file content into prompt"]),
    4: (__test_616e616c797369735f7762746573742e6d6274_4, ["analysis_step_handler includes relevant memory context in prompt"]),
    6: (__test_616e616c797369735f7762746573742e6d6274_6, ["analysis_step_handler skips when matching artifact already exists"]),
    7: (__test_616e616c797369735f7762746573742e6d6274_7, ["analysis_step_handler enforces timeout and token budget policies"]),
    8: (__test_616e616c797369735f7762746573742e6d6274_8, ["setup_analysis_agent registers execute_command tool"]),
  },
}


///|
impl MoonBit_Test_Driver for MoonBit_Test_Driver_Internal_Async_No_Args with run_test(
  async_tests,
  filename,
  index,
  handle_result,
  error_to_string,
) {
  if moonbit_test_driver_internal_async_tests.0.get(filename) is Some(index_map) &&
    index_map.get(index) is Some((f, attrs)) {
    let name = if attrs is [name, ..] { name } else { "" }
    if MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      for attr in attrs {
        if attr is [.. "panic", ..] {
          raise MoonBitTestDriverInternalSkipTest(name~)
        }
      }
    }
    async_tests.push(() => {
      try f() catch {
        err if MoonBit_Async_Test_Driver_Impl::is_being_cancelled() => raise err
        err => handle_result(name, error_to_string(err), false)
      } noraise {
        _ => handle_result(name, "", false)
      }
    })
    true
  } else {
    false
  }
}
///|
#cfg(target="native")
#test_entry
fn main {
  let async_tests = []
  for arg in moonbit_test_driver_internal_native_parse_args() {
    moonbit_test_driver_internal_do_execute(async_tests, arg.0, arg.1)
  }
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(target="js")
#test_entry
fn main {
  let test_params = moonbit_test_driver_js_parse_args()
  moonbit_test_driver_internal_execute(test_params)
  moonbit_test_driver_finish()
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
#test_entry
fn main {
  ()
}

///|
#cfg(target="js")
extern "js" fn moonbit_test_driver_js_parse_args() -> Array[
  MoonBitTestDriverInternalTestEntry,
] =
  #|() => {
  #|  try {
  #|    let testParams = []
  #|    // check if there is command line argument
  #|    if (process.argv.length > 2) {
  #|      const testArgs = JSON.parse(process.argv[2])
  #|      testParams = testArgs.file_and_index.flatMap(([file, ranges]) => {
  #|        let arr = []
  #|        for (let range of ranges) {
  #|          for (let i = range.start; i < range.end; i++) {
  #|            arr.push([file, i])
  #|          }
  #|        }
  #|        return arr
  #|      })
  #|      return testParams
  #|    }
  #|  } catch (error) {
  #|    console.error('failed to parse args: ', error.message)
  #|    process.exit(1)
  #|  }
  #|}

///|
#cfg(target="js")
pub fn moonbit_test_driver_internal_execute(
  tests : Array[MoonBitTestDriverInternalTestEntry],
) -> Unit {
  let async_tests = []
  for i in 0..<tests.length() {
    let entry = tests[i]
    moonbit_test_driver_internal_do_execute(
      async_tests,
      entry.filename(),
      entry.index(),
    )
  }
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(any(target="wasm", target="wasm-gc"))
pub fn moonbit_test_driver_internal_execute(
  filename : MoonbitTestDriverInternalExternString,
  index : Int,
) -> Unit {
  let filename = moonbit_test_driver_internal_get_file_name(filename)
  let async_tests = []
  moonbit_test_driver_internal_do_execute(async_tests, filename, index)
  MoonBit_Async_Test_Driver_Impl::run_async_tests(async_tests)
}

///|
#cfg(not(target="native"))
pub fn moonbit_test_driver_finish() -> Unit {
  
}

///|
fn moonbit_test_driver_internal_do_execute(
  async_tests : Array[async () -> Unit],
  filename : String,
  index : Int,
) -> Unit {
  // These two are internalized to avoid unused warnings

  fn handle_result(testname : String, message : String, skipped : Bool) {
    // In WASM, this function is called by the external test driver, but here
    // in native mode there's nothing that calls it, so we call it manually.
    if not(skipped) && MOONBIT_TEST_DRIVER_INTERNAL_IS_NATIVE {
      
    }
    let file_name = filename.escape()
    let test_name = testname.escape()
    let message = message.escape()
    @moonbitlang/core/prelude.println("----- BEGIN MOON TEST RESULT -----")
    @moonbitlang/core/prelude.println(
      "{\"package\": \"clawteam/clawteam/job\", \"filename\": \{file_name}, \"index\": \"\{index}\", \"test_name\": \{test_name}, \"message\": \{message}}",
    )
    @moonbitlang/core/prelude.println("----- END MOON TEST RESULT -----")
  }

  fn error_to_string(err : Error) -> String {
    match err {
      @moonbitlang/core/prelude.Failure::Failure(e)
      | @moonbitlang/core/prelude.InspectError::InspectError(e)
      | @moonbitlang/core/prelude.SnapshotError::SnapshotError(e)
      | MoonBitTestDriverInternalJsError(e) => e
      e => moonbit_test_driver_internal_error_to_string(e)
    }
  }

  try {
    if !(MoonBit_Test_Driver_Internal_No_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_With_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_Async_No_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_Async_With_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      ) ||
      MoonBit_Test_Driver_Internal_With_Bench_Args::run_test(
        async_tests, filename, index, handle_result, error_to_string,
      )) {
      raise MoonBitTestDriverInternalSkipTest(name="")
    }
  } catch {
    err => {
      guard err is MoonBitTestDriverInternalSkipTest(name~)
      handle_result(name, "skipped test", true)
    }
  }
}

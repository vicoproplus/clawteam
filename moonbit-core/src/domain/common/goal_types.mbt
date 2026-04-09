// moonbit-core/src/domain/common/goal_types.mbt

// Goal 领域类型

pub(all) struct GoalCreateInput {
  title: String
  level: GoalLevel
  status: GoalStatus
  company_id: String
  parent_goal_id: Option[String]
}

pub(all) struct GoalUpdateInput {
  title: Option[String]
  level: Option[GoalLevel]
  status: Option[GoalStatus]
  parent_goal_id: Option[String]
}

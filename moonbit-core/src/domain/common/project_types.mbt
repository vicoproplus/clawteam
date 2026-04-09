// moonbit-core/src/domain/common/project_types.mbt

// Project 领域类型

pub(all) struct ProjectCreateInput {
  name: String
  status: ProjectStatus
  company_id: String
  goal_id: Option[String]
  color: Option[String]
}

pub(all) struct ProjectUpdateInput {
  name: Option[String]
  status: Option[ProjectStatus]
  goal_id: Option[String]
  color: Option[String]
}

pub(all) struct ValidationError {
  field: String
  message: String
}

// Project 验证器

pub fn validate_project_create(input: ProjectCreateInput) -> Result[Unit, Array[ValidationError]] {
  let errors: Array[ValidationError] = []

  if input.name.length() == 0 {
    errors.push(ValidationError::{ field: "name", message: "Project name is required" })
  }
  if input.name.length() > 100 {
    errors.push(ValidationError::{ field: "name", message: "Project name must be less than 100 characters" })
  }
  if input.company_id.length() == 0 {
    errors.push(ValidationError::{ field: "companyId", message: "Company ID is required" })
  }

  if errors.length() == 0 {
    Ok(())
  } else {
    Err(errors)
  }
}

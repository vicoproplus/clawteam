// moonbit-core/src/domain/common/company_types.mbt

// Company 领域类型

pub(all) struct CompanyCreateInput {
  name: String
  status: CompanyStatus
  deployment_mode: String
  deployment_exposure: String
}

pub(all) struct CompanyUpdateInput {
  name: Option[String]
  status: Option[CompanyStatus]
  deployment_mode: Option[String]
  deployment_exposure: Option[String]
}

/// ClawTeam 主入口
pub struct ClawTeam {
  logger : @pino.Logger
  config : @config.ClawTeamConfig
  scheduler : @scheduler.Scheduler
}

pub fn ClawTeam::new(
  config : @config.ClawTeamConfig,
  logger : @pino.Logger,
) -> ClawTeam {
  ClawTeam::{
    logger,
    config,
    scheduler: @scheduler.Scheduler::new(config, logger),
  }
}

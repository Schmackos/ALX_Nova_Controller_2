# Concept: AI Automation & Development Workflow

| Field | Value |
|---|---|
| Workflow | `raw` |
| Priority | `---` |
| Effort | `---` |
| Sources | Aitomatic roadmap exexution.m4a, Github managing Issue, auto close and himan intervention.m4a, What are we missing in lroject structure.m4a |
| Audio | [`inbox/processed/`](inbox/processed/) |
| Last updated | 2026-03-26 |

## Problem / Opportunity

As the ALX Nova platform approaches launch, the project needs to scale development throughput while maintaining quality — handling a growing roadmap of features, devices, and software updates alongside an expected influx of user-reported GitHub issues. Currently, plan execution, issue triage, and bug resolution require significant manual intervention at every step. Establishing an automated pipeline from roadmap planning through execution, combined with intelligent issue triage and duplicate detection, would dramatically accelerate delivery while preserving human oversight where it matters most.

## Sub-topics

### Automated roadmap execution with session continuity

#### What we know
- The project already has an established workflow for planning and executing features via Claude Code (GSD skills, worktrees, CI/CD gates)
- Plans should be reviewable and approvable by a human before automated execution begins
- Execution should use bypassed permissions so Claude implements approved plans without prompts
- Session limits are a real constraint — the system needs to detect when a session is exhausted and automatically start a new one for the next roadmap item
- The goal is continuous, unattended execution of approved roadmap items with human involvement only at the review/approval stage

#### What needs research
- What mechanism can reliably detect Claude Code session limits (token exhaustion, rate limits, context window) and trigger a new session?
- Can Claude Code's scheduled triggers (`/gsd:autonomous`, cron-based `RemoteTrigger`) be chained to process a roadmap queue?
- What is the best format for a machine-readable roadmap queue that tracks item status (pending review → approved → in-progress → complete → verified)?
- How should rollback work if an automated execution fails CI gates — auto-revert, park the item, or alert for human intervention?
- What guardrails prevent an automated session from drifting off-plan or making destructive changes without approval?

### GitHub issue triage and auto-resolution pipeline

#### What we know
- All code changes must go through the standard GitHub cycle: PR → CI/CD → review → merge
- Post-launch, a significant volume of GitHub issues is expected from users
- Issues meeting defined criteria should be automatically triaged and resolved by Claude, resulting in PRs for human review
- Duplicate issues should be detected, labeled, clustered, and auto-closed with context merged into a primary issue
- Human involvement should be reserved for issues that genuinely require human judgment or architectural decisions
- Users should receive timely feedback on issue status regardless of resolution path

#### What needs research
- What criteria determine whether an issue is auto-resolvable vs. requires human intervention (e.g., severity, affected subsystem, reproduction clarity, scope of change)?
- How should the auto-resolution pipeline be triggered — GitHub Actions workflow on issue creation, scheduled polling, or webhook-based?
- What duplicate detection approach works best — semantic similarity via embeddings, keyword matching, or a hybrid with GitHub's existing duplicate label?
- How should auto-generated PRs be distinguished from human PRs in the review queue (labels, assignees, PR template)?
- What feedback loop ensures users are notified when their issue is picked up, being worked on, or resolved?
- How do we prevent the auto-resolver from attempting fixes that are too large in scope or that touch critical subsystems (audio pipeline, HAL, security)?

### Project setup benchmarking against best-in-class AI workflows

#### What we know
- The project already uses Claude Code with GSD skills, worktrees, agent teams, pre-commit hooks, and 4 CI quality gates
- There is a comprehensive test suite (3790+ C++ tests, 464 E2E tests, 205 on-device tests)
- The project has CLAUDE.md, memory system, concerns tracking, and structured planning artifacts
- The goal is to identify gaps by benchmarking against best-in-class open source projects using Claude Code or similar AI automation

#### What needs research
- What are the top open source projects effectively using Claude Code or AI-assisted development, and what patterns do they use that ALX Nova does not?
- Are there codebase health metrics (code coverage percentage, lint scores, dependency freshness) that could be tracked automatically and surfaced in dashboards?
- Would automated architecture decision records (ADRs) or changelog generation improve project quality?
- Is there value in adding automated security scanning (SAST/DAST) beyond the current ESLint and manual review?
- Could automated performance regression testing (heap usage, task timing, boot time) be added to the CI pipeline for firmware?
- What documentation automation (API docs generation, test coverage reports, dependency graphs) would reduce maintenance burden?

## Action Items

- [ ] Research Claude Code session limit detection and automated chaining. Investigate how to detect when a Claude Code session is approaching its token or rate limit, and design a mechanism (shell script, cron job, or GitHub Actions workflow) that automatically starts a new session to continue processing the next approved roadmap item. Document the approach including session state handoff, roadmap queue format, and failure handling. Write findings to `docs-internal/planning/research-session-continuity.md`.
- [ ] Design a machine-readable roadmap queue format for automated execution. Create a JSON or YAML schema for roadmap items that includes fields for status (draft → reviewed → approved → in-progress → complete → verified), priority, dependencies, plan file path, and execution constraints. Include a CLI or script that advances items through the pipeline. Write the schema and tooling spec to `docs-internal/planning/roadmap-queue-schema.md`.
- [ ] Build a GitHub Actions workflow for automated issue triage. Create a workflow triggered on `issues.opened` that: (1) classifies the issue by subsystem and severity using the issue body and labels, (2) checks for duplicates by comparing title and body against open issues, (3) auto-labels duplicates and posts a comment linking to the primary issue, (4) for auto-resolvable issues, triggers a Claude Code session with the issue context and repository access to generate a fix PR. Include configurable criteria for what qualifies as auto-resolvable. Write the workflow to `.github/workflows/issue-triage.yml` and document the configuration in `docs-internal/planning/issue-triage-pipeline.md`.
- [ ] Research best-in-class AI-automated open source projects. Search for open source projects that effectively use Claude Code, Cursor, Copilot Workspace, or similar AI automation tools. Document their project structure, automation patterns, CI/CD integration, and any tooling the ALX Nova project is missing. Focus on patterns that improve codebase quality, speed up bug resolution, and accelerate feature delivery. Write findings to `docs-internal/backlog/research-ai-workflow-benchmarks.md`.
- [ ] Design a duplicate issue detection and clustering system. Research approaches for detecting duplicate GitHub issues including semantic similarity (embeddings), keyword extraction, and label-based matching. Design a system that auto-labels duplicates, merges context into the primary issue, auto-closes duplicates with a comment explaining the closure, and notifies the original reporter. Write the design to `docs-internal/planning/duplicate-issue-detection.md`.
- [ ] Audit the current CI pipeline for automation gaps. Review the existing 4 CI quality gates (cpp-tests, cpp-lint, js-lint, e2e-tests) and identify missing automated checks that best-in-class projects use: firmware performance regression tests (heap usage, boot time, task timing), automated SAST/DAST scanning, dependency freshness checks, code coverage tracking with minimum thresholds, and automated changelog generation. Write a gap analysis with prioritized recommendations to `docs-internal/planning/ci-pipeline-gaps.md`.

## Original Transcripts

<details>
<summary>Source: Aitomatic roadmap exexution.m4a</summary>

> I need to create a rope map for the Alex NOVA carrier board platform project and populate these with with the right topics. So new features, new devices, new software updates, things like that. And then how can I prepare these in adequate plans? And when I've reviewed the plan and I approved it, that I can schedule the automatic execution. So bypassing the permissions and actually for plot to create the code and implementing the plan according to our workflow in an automated way. Now here's the thing. How can we continuously ping if we've reached our session limits? And if the session limit is not invoked, then auto start the next item from the rope map. Making this a continuous process as we go without basically human intervention to do the execution bit. How can we do this? What do we need? You know, let's me to do this.

</details>

<details>
<summary>Source: Github managing Issue, auto close and himan intervention.m4a</summary>

> What the project I want all co-changes to go through the proper get up cycle. When we launch I do expect a significant amount of new issues being raised through get up and how can we streamline this process so that based on criteria either the issues get automatically solved by you and go through our CICD pipeline as we do with everything and how can these be flagged as pull requests that need to be reviewed. How can we streamline this process to make it efficient and include me where their issues and human intervention is required to resolve these issues moving forward. This so we keep the pace and making sure that bugs and issues get either solved quickly or they get feedback so that the user knows where we stand. I also need then the functionality when issues are raised that when they are duplicates they get labeled accordingly and and clustered together and also that the context is then taken over into a main item and that the duplicate issues will be automatically closed. Based on this what other improvements to this workflow should we implement to keep the pace and use automation as best as we can without impacting our overall quality and keeping the human attention where it's needed and valued most.

</details>

<details>
<summary>Source: What are we missing in lroject structure.m4a</summary>

> I want you to evaluate our current project and look online for best-in-class projects that use claw code or AI automation. What are we currently missing in our setup that would be very beneficial to improving the project, improving the codebase, and speeding up the delivery of bug fixes and new features from the roadmap.

</details>

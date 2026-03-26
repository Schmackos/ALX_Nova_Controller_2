# Voice Note: Github managing Issue, auto close and himan intervention

## Summary
- All code changes must go through the proper GitHub cycle (PR → CI/CD → review → merge)
- Post-launch, expect a significant volume of new GitHub issues from users
- Want automated triage: issues meeting certain criteria should be auto-resolved by Claude, go through CI/CD, and appear as PRs for review
- Human involvement should be reserved for issues that genuinely require human judgment
- Need duplicate detection: auto-label duplicates, cluster them, merge context into a primary issue, and auto-close duplicates
- Goal: maintain development pace while ensuring bugs get resolved quickly or users get timely feedback on status
- Seeking additional workflow automation ideas that maximize quality without sacrificing human oversight where it matters

## Cleaned Transcript

What I want for the project is all code changes to go through the proper GitHub cycle. When we launch, I do expect a significant amount of new issues being raised through GitHub. How can we streamline this process so that, based on criteria, either the issues get automatically solved by you and go through our CI/CD pipeline as we do with everything, and how can these be flagged as pull requests that need to be reviewed?

How can we streamline this process to make it efficient and include me where there are issues and human intervention is required to resolve these issues moving forward? This way we keep the pace and make sure that bugs and issues either get solved quickly or they get feedback so that the user knows where we stand.

I also need the functionality where, when issues are raised that are duplicates, they get labeled accordingly and clustered together. The context should then be taken over into a main item and the duplicate issues will be automatically closed.

Based on this, what other improvements to this workflow should we implement to keep the pace and use automation as best as we can without impacting our overall quality, keeping the human attention where it's needed and valued most?

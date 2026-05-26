# Contributing to IdeaWalker

## Baseline

All relevant changes should preserve the DocOps boundary, ADR traceability and the invariant checks already registered in CI.

Before opening a change:

1. Describe the intent and affected boundary.
2. Link or create the related ADR/DAI entry when the change affects architecture, methodology or governance.
3. Run the headless tests locally when possible.
4. Attach evidence for quality gates touched by the change.

## Expected Evidence

- Build/test result or CI run.
- Updated ADR/DAI when the change alters decisions, risks or operating rules.
- Updated documentation when the change affects user-visible workflow or analytical interpretation.

## Review Focus

Reviewers should check boundary preservation, traceability, test evidence and compatibility with the current governance policies.

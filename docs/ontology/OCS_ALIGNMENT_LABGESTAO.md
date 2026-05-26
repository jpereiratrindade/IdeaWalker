# OCS Alignment - IdeaWalker x LabGestao

## Context

LabGestao reported IdeaWalker with `Top OCS=64` against LabGestao. This indicates an alignment opportunity, not automatic integration readiness.

The purpose of this note is to make the compatibility explicit by mapping equivalent entities, shared relations and divergent rules before any integration decision.

## Equivalent Entities

| IdeaWalker | LabGestao | Alignment |
| --- | --- | --- |
| `ArchitectureDecision` / ADR catalog | `AdrRecord` / ADR inventory signal | Both represent architectural decisions and their traceability. |
| `DocOpsWorkspace` | `WorkspaceConfig` / monitored workspace | Both define the operational boundary used to interpret files and project context. |
| `Evaluation` / audit artifact | `GovernanceProfile` evidence | Both preserve evidence used to qualify a project or repository. |
| `NarrativeBundle` | `Report` / exported characterization | Both package interpreted observations for later review. |
| `Policy` / invariant | `Policy` / governance signal | Both constrain safe operation and interpretation. |
| `ToolContract` | `ToolContract` / governance signal | Both describe expected behavior and preconditions for tool/agent execution. |

## Shared Relations

| Relation | IdeaWalker Reading | LabGestao Reading |
| --- | --- | --- |
| `records` | ADR/catalog records architectural decisions. | Project/inventory records ADR, DAI and governance evidence. |
| `validates` | Invariants and audits validate DocOps consistency. | CI, tests and governance checks validate project discipline. |
| `references` | Documents reference decisions, evaluations and artifacts. | Profiles reference repository documents, code and evidence. |
| `produces` | Services produce bundles, indexes and reports. | Inventory produces metrics, reports and action plans. |
| `aligns_with` | DocOps aligns method, evidence and project structure. | OCS aligns entities, relations and rules across repositories. |

## Divergent Rules

| Rule Area | IdeaWalker | LabGestao | Treatment |
| --- | --- | --- | --- |
| Primary object | Writing/reasoning/DocOps project. | Portfolio/inventory/analysis project. | Keep distinct; align only analytical artifacts. |
| Evaluation target | Internal methodological and narrative artifacts. | External repositories and project inventory. | Share evidence vocabulary, not runtime workflow. |
| Identity | Decisions and artifacts are identified by catalog/doc paths. | Repositories and projects are identified by path, id and inventory profile. | Map identifiers through explicit source paths. |
| Integration readiness | Requires methodological boundary and invariant consistency. | Requires OCI, OCS, governance and reliability evidence. | Use LabGestao readiness as external gate, not sole decision. |

## Integration Candidate

The strongest integration candidate is not direct feature merging. It is evidence exchange:

1. IdeaWalker exports ADR/catalog/evaluation evidence.
2. LabGestao imports or scans that evidence as governance and ontology signals.
3. Shared reports retain source paths so identity remains auditable.

## Open Questions

- Which IdeaWalker artifacts should be canonical evidence for LabGestao?
- Should LabGestao treat `NarrativeBundle` as an entity, evidence package, or report artifact?
- Which OCS mappings should become machine-readable in a future ontology profile?

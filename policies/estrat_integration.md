# ESTRAT Integration Policy — IdeaWalker

## Scope
This policy governs IdeaWalker's participation in the ESTRAT protocol.

## Role in Protocol
IdeaWalker is the **epistemic seed producer** of the pipeline.
- Produces: `ESTRAT-SEED`
- May consume: DAI feedback artifacts from LabGP (as new inbox entries)

## Production Rules (ESTRAT-SEED)

### Gate: VE-IW Approval
A ESTRAT-SEED may only be exported if the Epistemic Validator (VE-IW) passes all checks:
- `has_teleology: false`
- `has_normative_claim: false`
- `has_linear_causality: false`

If any flag is `true`, the note is returned to the internal refinement cycle.
No ESTRAT-SEED is written to disk in this case.

### Content Rules
- `raw_intuition` must carry the unprocessed source — no pre-normalization
- `structured_note_path` must point to an existing file in `notas/`
- `ve_iw_seal` must be the SHA-256 hash of the note file content at export time
- Export destination: `strata/consumables/estrat/SEED-<timestamp>.yaml`

## Consumption Rules (DAI Feedback from LabGP)
- LabGP may produce DAI artifacts signaling convergence failures (decision: iterar/descartar)
- These DAI artifacts may be dropped into `inbox/` as `.yaml` or `.md` files
- IdeaWalker treats them as new raw inputs and processes them through the standard AI pipeline
- The `concept_ref` and `seed_ref` fields in the DAI must be preserved in the resulting note

## Restrictions
- No autonomous agent may write ESTRAT-SEED without the VE-IW gate passing
- The `raw_intuition` field must never be altered before export
- DocOps workspaces and writing trajectories are excluded from ESTRAT export

## Violations
- Writing ESTRAT-SEED when `ve_iw_approved: false`: **prohibited**
- Omitting `raw_intuition` from the seed: **prohibited**
- Exporting pre-structured content as `raw_intuition`: **prohibited**
